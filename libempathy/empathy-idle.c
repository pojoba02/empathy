/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007-2008 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>

#include <string.h>

#include <glib/gi18n-lib.h>
#include <dbus/dbus-glib.h>
#ifdef HAVE_NM
#include <nm-client.h>
#endif

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/util.h>

#include "empathy-account-manager.h"
#include "empathy-idle.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include "empathy-debug.h"

/* Number of seconds before entering extended autoaway. */
#define EXT_AWAY_TIME (30*60)

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyIdle)
typedef struct {
	DBusGProxy     *gs_proxy;
#ifdef HAVE_NM
	NMClient       *nm_client;
#endif

	TpConnectionPresenceType      state;
	gchar          *status;
	TpConnectionPresenceType      flash_state;
	gboolean        auto_away;
	gboolean        use_nm;

	TpConnectionPresenceType      away_saved_state;
	TpConnectionPresenceType      nm_saved_state;
	gchar          *nm_saved_status;

	gboolean        is_idle;
	gboolean        nm_connected;
	guint           ext_away_timeout;

	EmpathyAccountManager *manager;
} EmpathyIdlePriv;

typedef enum {
	SESSION_STATUS_AVAILABLE,
	SESSION_STATUS_INVISIBLE,
	SESSION_STATUS_BUSY,
	SESSION_STATUS_IDLE,
	SESSION_STATUS_UNKNOWN
} SessionStatus;

enum {
	PROP_0,
	PROP_STATE,
	PROP_STATUS,
	PROP_FLASH_STATE,
	PROP_AUTO_AWAY,
	PROP_USE_NM
};

G_DEFINE_TYPE (EmpathyIdle, empathy_idle, G_TYPE_OBJECT);

static EmpathyIdle * idle_singleton = NULL;

static void
idle_presence_changed_cb (EmpathyAccountManager *manager,
			  TpConnectionPresenceType state,
			  gchar          *status,
			  gchar          *status_message,
			  EmpathyIdle    *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	if (state == TP_CONNECTION_PRESENCE_TYPE_UNSET)
		/* Assume our presence is offline if MC reports UNSET */
		state = TP_CONNECTION_PRESENCE_TYPE_OFFLINE;

	DEBUG ("Presence changed to '%s' (%d) \"%s\"", status, state,
		status_message);

	g_free (priv->status);
	priv->state = state;
	if (EMP_STR_EMPTY (status_message))
		priv->status = NULL;
	else
		priv->status = g_strdup (status_message);

	g_object_notify (G_OBJECT (idle), "state");
	g_object_notify (G_OBJECT (idle), "status");
}

static gboolean
idle_ext_away_cb (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	DEBUG ("Going to extended autoaway");
	empathy_idle_set_state (idle, TP_CONNECTION_PRESENCE_TYPE_EXTENDED_AWAY);
	priv->ext_away_timeout = 0;

	return FALSE;
}

static void
idle_ext_away_stop (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	if (priv->ext_away_timeout) {
		g_source_remove (priv->ext_away_timeout);
		priv->ext_away_timeout = 0;
	}
}

static void
idle_ext_away_start (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	if (priv->ext_away_timeout != 0) {
		return;
	}
	priv->ext_away_timeout = g_timeout_add_seconds (EXT_AWAY_TIME,
							(GSourceFunc) idle_ext_away_cb,
							idle);
}

static void
idle_session_status_changed_cb (DBusGProxy    *gs_proxy,
				SessionStatus  status,
				EmpathyIdle   *idle)
{
	EmpathyIdlePriv *priv;
	gboolean is_idle;

	priv = GET_PRIV (idle);

	is_idle = (status == SESSION_STATUS_IDLE);

	DEBUG ("Session idle state changed, %s -> %s",
		priv->is_idle ? "yes" : "no",
		is_idle ? "yes" : "no");

	if (!priv->auto_away ||
	    (priv->nm_saved_state == TP_CONNECTION_PRESENCE_TYPE_UNSET &&
	     (priv->state <= TP_CONNECTION_PRESENCE_TYPE_OFFLINE ||
	      priv->state == TP_CONNECTION_PRESENCE_TYPE_HIDDEN))) {
		/* We don't want to go auto away OR we explicitely asked to be
		 * offline, nothing to do here */
		priv->is_idle = is_idle;
		return;
	}

	if (is_idle && !priv->is_idle) {
		TpConnectionPresenceType new_state;
		/* We are now idle */

		idle_ext_away_start (idle);

		if (priv->nm_saved_state != TP_CONNECTION_PRESENCE_TYPE_UNSET) {
		    	/* We are disconnected, when coming back from away
		    	 * we want to restore the presence before the
		    	 * disconnection. */
			priv->away_saved_state = priv->nm_saved_state;
		} else {
			priv->away_saved_state = priv->state;
		}

		new_state = TP_CONNECTION_PRESENCE_TYPE_AWAY;
		if (priv->state == TP_CONNECTION_PRESENCE_TYPE_EXTENDED_AWAY) {
			new_state = TP_CONNECTION_PRESENCE_TYPE_EXTENDED_AWAY;
		}

		DEBUG ("Going to autoaway. Saved state=%d, new state=%d",
			priv->away_saved_state, new_state);
		empathy_idle_set_state (idle, new_state);
	} else if (!is_idle && priv->is_idle) {
		const gchar *new_status;
		/* We are no more idle, restore state */

		idle_ext_away_stop (idle);

		if (priv->away_saved_state == TP_CONNECTION_PRESENCE_TYPE_AWAY ||
		    priv->away_saved_state == TP_CONNECTION_PRESENCE_TYPE_EXTENDED_AWAY) {
			priv->away_saved_state = TP_CONNECTION_PRESENCE_TYPE_AVAILABLE;
			new_status = NULL;
		} else {
			new_status = priv->status;
		}

		DEBUG ("Restoring state to %d, reset status to %s",
			priv->away_saved_state, new_status);

		empathy_idle_set_presence (idle,
					   priv->away_saved_state,
					   new_status);

		priv->away_saved_state = TP_CONNECTION_PRESENCE_TYPE_UNSET;
	}

	priv->is_idle = is_idle;
}

#ifdef HAVE_NM
static void
idle_nm_state_change_cb (NMClient         *client,
			 const GParamSpec *pspec,
			 EmpathyIdle      *idle)
{
	EmpathyIdlePriv *priv;
	gboolean         old_nm_connected;
	gboolean         new_nm_connected;
	NMState          state;

	priv = GET_PRIV (idle);

	if (!priv->use_nm) {
		return;
	}

	state = nm_client_get_state (priv->nm_client);
	old_nm_connected = priv->nm_connected;
	new_nm_connected = !(state == NM_STATE_CONNECTING ||
			     state == NM_STATE_DISCONNECTED);
	priv->nm_connected = TRUE; /* To be sure _set_state will work */

	DEBUG ("New network state %d", state);

	if (old_nm_connected && !new_nm_connected) {
		/* We are no more connected */
		DEBUG ("Disconnected: Save state %d (%s)",
				priv->state, priv->status);
		priv->nm_saved_state = priv->state;
		g_free (priv->nm_saved_status);
		priv->nm_saved_status = g_strdup (priv->status);
		empathy_idle_set_state (idle, TP_CONNECTION_PRESENCE_TYPE_OFFLINE);
	}
	else if (!old_nm_connected && new_nm_connected
			&& priv->nm_saved_state != TP_CONNECTION_PRESENCE_TYPE_UNSET) {
		/* We are now connected */
		DEBUG ("Reconnected: Restore state %d (%s)",
				priv->nm_saved_state, priv->nm_saved_status);
		empathy_idle_set_presence (idle,
				priv->nm_saved_state,
				priv->nm_saved_status);
		priv->nm_saved_state = TP_CONNECTION_PRESENCE_TYPE_UNSET;
		g_free (priv->nm_saved_status);
		priv->nm_saved_status = NULL;
	}

	priv->nm_connected = new_nm_connected;
}
#endif

static void
idle_finalize (GObject *object)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->status);

	if (priv->gs_proxy) {
		g_object_unref (priv->gs_proxy);
	}

	if (priv->manager != NULL)
		g_object_unref (priv->manager);

#ifdef HAVE_NM
	if (priv->nm_client) {
		g_object_unref (priv->nm_client);
	}
#endif

	idle_ext_away_stop (EMPATHY_IDLE (object));
}

static GObject *
idle_constructor (GType type,
		  guint n_props,
		  GObjectConstructParam *props)
{
	GObject *retval;

	if (idle_singleton) {
		retval = g_object_ref (idle_singleton);
	} else {
		retval = G_OBJECT_CLASS (empathy_idle_parent_class)->constructor
			(type, n_props, props);

		idle_singleton = EMPATHY_IDLE (retval);
		g_object_add_weak_pointer (retval, (gpointer) &idle_singleton);
	}

	return retval;
}

static void
idle_get_property (GObject    *object,
		   guint       param_id,
		   GValue     *value,
		   GParamSpec *pspec)
{
	EmpathyIdlePriv *priv;
	EmpathyIdle     *idle;

	priv = GET_PRIV (object);
	idle = EMPATHY_IDLE (object);

	switch (param_id) {
	case PROP_STATE:
		g_value_set_enum (value, empathy_idle_get_state (idle));
		break;
	case PROP_STATUS:
		g_value_set_string (value, empathy_idle_get_status (idle));
		break;
	case PROP_FLASH_STATE:
		g_value_set_enum (value, empathy_idle_get_flash_state (idle));
		break;
	case PROP_AUTO_AWAY:
		g_value_set_boolean (value, empathy_idle_get_auto_away (idle));
		break;
	case PROP_USE_NM:
		g_value_set_boolean (value, empathy_idle_get_use_nm (idle));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
idle_set_property (GObject      *object,
		   guint         param_id,
		   const GValue *value,
		   GParamSpec   *pspec)
{
	EmpathyIdlePriv *priv;
	EmpathyIdle     *idle;

	priv = GET_PRIV (object);
	idle = EMPATHY_IDLE (object);

	switch (param_id) {
	case PROP_STATE:
		empathy_idle_set_state (idle, g_value_get_enum (value));
		break;
	case PROP_STATUS:
		empathy_idle_set_status (idle, g_value_get_string (value));
		break;
	case PROP_FLASH_STATE:
		empathy_idle_set_flash_state (idle, g_value_get_enum (value));
		break;
	case PROP_AUTO_AWAY:
		empathy_idle_set_auto_away (idle, g_value_get_boolean (value));
		break;
	case PROP_USE_NM:
		empathy_idle_set_use_nm (idle, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
empathy_idle_class_init (EmpathyIdleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = idle_finalize;
	object_class->constructor = idle_constructor;
	object_class->get_property = idle_get_property;
	object_class->set_property = idle_set_property;

	g_object_class_install_property (object_class,
					 PROP_STATE,
					 g_param_spec_uint ("state",
							    "state",
							    "state",
							    0, NUM_TP_CONNECTION_PRESENCE_TYPES,
							    TP_CONNECTION_PRESENCE_TYPE_UNSET,
							    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_STATUS,
					 g_param_spec_string ("status",
							      "status",
							      "status",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_FLASH_STATE,
					 g_param_spec_uint ("flash-state",
							    "flash-state",
							    "flash-state",
							    0, NUM_TP_CONNECTION_PRESENCE_TYPES,
							    TP_CONNECTION_PRESENCE_TYPE_UNSET,
							    G_PARAM_READWRITE));

	 g_object_class_install_property (object_class,
					  PROP_AUTO_AWAY,
					  g_param_spec_boolean ("auto-away",
								"Automatic set presence to away",
								"Should it set presence to away if inactive",
								FALSE,
								G_PARAM_READWRITE));

	 g_object_class_install_property (object_class,
					  PROP_USE_NM,
					  g_param_spec_boolean ("use-nm",
								"Use Network Manager",
								"Set presence according to Network Manager",
								TRUE,
								G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EmpathyIdlePriv));
}

static void
empathy_idle_init (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (idle,
		EMPATHY_TYPE_IDLE, EmpathyIdlePriv);

	idle->priv = priv;
	priv->is_idle = FALSE;

	priv->manager = empathy_account_manager_dup_singleton ();
	priv->state = empathy_account_manager_get_global_presence (priv->manager,
		NULL, &priv->status);


	g_signal_connect (priv->manager, "global-presence-changed",
		G_CALLBACK (idle_presence_changed_cb), idle);

	priv->gs_proxy = dbus_g_proxy_new_for_name (tp_get_bus (),
						    "org.gnome.SessionManager",
						    "/org/gnome/SessionManager/Presence",
						    "org.gnome.SessionManager.Presence");
	if (priv->gs_proxy) {
		dbus_g_proxy_add_signal (priv->gs_proxy, "StatusChanged",
					 G_TYPE_UINT, G_TYPE_INVALID);
		dbus_g_proxy_connect_signal (priv->gs_proxy, "StatusChanged",
					     G_CALLBACK (idle_session_status_changed_cb),
					     idle, NULL);
	} else {
		DEBUG ("Failed to get gs proxy");
	}

#ifdef HAVE_NM
	priv->nm_client = nm_client_new ();
	if (priv->nm_client) {
		g_signal_connect (priv->nm_client, "notify::" NM_CLIENT_STATE,
				  G_CALLBACK (idle_nm_state_change_cb),
				  idle);
	} else {
		DEBUG ("Failed to get nm proxy");
	}
#endif
}

EmpathyIdle *
empathy_idle_dup_singleton (void)
{
	return g_object_new (EMPATHY_TYPE_IDLE, NULL);
}

TpConnectionPresenceType
empathy_idle_get_state (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	return priv->state;
}

void
empathy_idle_set_state (EmpathyIdle *idle,
			TpConnectionPresenceType   state)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	empathy_idle_set_presence (idle, state, priv->status);
}

const gchar *
empathy_idle_get_status (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	if (!priv->status) {
		return empathy_presence_get_default_message (priv->state);
	}

	return priv->status;
}

void
empathy_idle_set_status (EmpathyIdle *idle,
			 const gchar *status)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	empathy_idle_set_presence (idle, priv->state, status);
}

TpConnectionPresenceType
empathy_idle_get_flash_state (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	return priv->flash_state;
}

void
empathy_idle_set_flash_state (EmpathyIdle *idle,
			      TpConnectionPresenceType   state)
{
	EmpathyIdlePriv *priv;

	priv = GET_PRIV (idle);

	priv->flash_state = state;

	if (state == TP_CONNECTION_PRESENCE_TYPE_UNSET) {
	}

	g_object_notify (G_OBJECT (idle), "flash-state");
}

static void
empathy_idle_do_set_presence (EmpathyIdle *idle,
			   TpConnectionPresenceType status_type,
			   const gchar *status_message)
{
	EmpathyIdlePriv *priv = GET_PRIV (idle);
	const gchar *statuses[NUM_TP_CONNECTION_PRESENCE_TYPES] = {
		NULL,
		"offline",
		"available",
		"away",
		"xa",
		"hidden",
		"busy",
		NULL,
		NULL,
	};
	const gchar *status;

	g_assert (status_type > 0 && status_type < NUM_TP_CONNECTION_PRESENCE_TYPES);

	status = statuses[status_type];

	g_return_if_fail (status != NULL);

	empathy_account_manager_request_global_presence (priv->manager,
		status_type, status, status_message);
}

void
empathy_idle_set_presence (EmpathyIdle *idle,
			   TpConnectionPresenceType   state,
			   const gchar *status)
{
	EmpathyIdlePriv *priv;
	const gchar     *default_status;

	priv = GET_PRIV (idle);

	DEBUG ("Changing presence to %s (%d)", status, state);

	/* Do not set translated default messages */
	default_status = empathy_presence_get_default_message (state);
	if (!tp_strdiff (status, default_status)) {
		status = NULL;
	}

	if (!priv->nm_connected) {
		DEBUG ("NM not connected");

		priv->nm_saved_state = state;
		if (tp_strdiff (priv->status, status)) {
			g_free (priv->status);
			priv->status = NULL;
			if (!EMP_STR_EMPTY (status)) {
				priv->status = g_strdup (status);
			}
			g_object_notify (G_OBJECT (idle), "status");
		}

		return;
	}

	empathy_idle_do_set_presence (idle, state, status);
}

gboolean
empathy_idle_get_auto_away (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv = GET_PRIV (idle);

	return priv->auto_away;
}

void
empathy_idle_set_auto_away (EmpathyIdle *idle,
			    gboolean     auto_away)
{
	EmpathyIdlePriv *priv = GET_PRIV (idle);

	priv->auto_away = auto_away;

	g_object_notify (G_OBJECT (idle), "auto-away");
}

gboolean
empathy_idle_get_use_nm (EmpathyIdle *idle)
{
	EmpathyIdlePriv *priv = GET_PRIV (idle);

	return priv->use_nm;
}

void
empathy_idle_set_use_nm (EmpathyIdle *idle,
			 gboolean     use_nm)
{
	EmpathyIdlePriv *priv = GET_PRIV (idle);

#ifdef HAVE_NM
	if (!priv->nm_client || use_nm == priv->use_nm) {
		return;
	}
#endif

	priv->use_nm = use_nm;

#ifdef HAVE_NM
	if (use_nm) {
		idle_nm_state_change_cb (priv->nm_client, NULL, idle);
#else
	if (0) {
#endif
	} else {
		priv->nm_connected = TRUE;
		if (priv->nm_saved_state != TP_CONNECTION_PRESENCE_TYPE_UNSET) {
			empathy_idle_set_state (idle, priv->nm_saved_state);
		}
		priv->nm_saved_state = TP_CONNECTION_PRESENCE_TYPE_UNSET;
	}

	g_object_notify (G_OBJECT (idle), "use-nm");
}

