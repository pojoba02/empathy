/*
 * Copyright (C) 2009 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors: Cosimo Cecchi <cosimo.cecchi@collabora.co.uk>
 */

#ifndef __EMPATHY_ACCOUNT_WIDGET_PRIVATE_H__
#define __EMPATHY_ACCOUNT_WIDGET_PRIVATE_H__

#include <libempathy-gtk/empathy-account-widget.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

struct _EmpathyAccountWidgetUIDetails {
  GtkBuilder *gui;

  char *default_focus;
};


void empathy_account_widget_handle_params (EmpathyAccountWidget *self,
    const gchar *first_widget,
    ...);

void empathy_account_widget_setup_widget (EmpathyAccountWidget *self,
    GtkWidget *widget,
    const gchar *param_name);

G_END_DECLS

#endif /* __EMPATHY_ACCOUNT_WIDGET_PRIVATE_H__ */