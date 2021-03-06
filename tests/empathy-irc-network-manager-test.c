#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib/gstdio.h>
#include <tp-account-widgets/tpaw-irc-network-manager.h>

#include "test-helper.h"
#include "test-irc-helper.h"

#define GLOBAL_SAMPLE "default-irc-networks-sample.xml"
#define USER_SAMPLE "user-irc-networks-sample.xml"
#define USER_FILE "user-irc-networks.xml"

static void
test_tpaw_irc_network_manager_add (void)
{
  TpawIrcNetworkManager *mgr;
  TpawIrcNetwork *network;
  GSList *networks;
  gchar *name;

  mgr = tpaw_irc_network_manager_new (NULL, NULL);
  g_assert (mgr != NULL);

  networks = tpaw_irc_network_manager_get_networks (mgr);
  g_assert (networks == NULL);

  /* add a network */
  network = tpaw_irc_network_new ("My Network");
  g_assert (network != NULL);
  tpaw_irc_network_manager_add (mgr, network);
  g_object_unref (network);

  networks = tpaw_irc_network_manager_get_networks (mgr);
  g_assert_cmpuint (g_slist_length (networks), ==, 1);
  g_object_get (networks->data, "name", &name, NULL);
  g_assert_cmpstr (name, ==, "My Network");
  g_free (name);
  g_slist_foreach (networks, (GFunc) g_object_unref, NULL);
  g_slist_free (networks);

  /* add another network having the same name */
  network = tpaw_irc_network_new ("My Network");
  g_assert (network != NULL);
  tpaw_irc_network_manager_add (mgr, network);
  g_object_unref (network);

  networks = tpaw_irc_network_manager_get_networks (mgr);
  g_assert_cmpuint (g_slist_length (networks), ==, 2);
  g_object_get (networks->data, "name", &name, NULL);
  g_assert_cmpstr (name, ==, "My Network");
  g_free (name);
  g_object_get (g_slist_next (networks)->data, "name", &name, NULL);
  g_assert_cmpstr (name, ==, "My Network");
  g_free (name);
  g_slist_foreach (networks, (GFunc) g_object_unref, NULL);
  g_slist_free (networks);

  g_object_unref (mgr);
}

static void
test_load_global_file (void)
{
  TpawIrcNetworkManager *mgr;
  gchar *global_file, *user_file;
  GSList *networks, *l;
  struct server_t freenode_servers[] = {
    { "irc.freenode.net", 6667, FALSE },
    { "irc.eu.freenode.net", 6667, FALSE }};
  struct server_t gimpnet_servers[] = {
    { "irc.gimp.org", 6667, FALSE },
    { "irc.us.gimp.org", 6667, FALSE }};
  struct server_t test_servers[] = {
    { "irc.test.org", 6669, TRUE }};
  struct server_t undernet_servers[] = {
    { "eu.undernet.org", 6667, FALSE }};
  gboolean network_checked[4];
  gchar *global_file_orig;

  global_file_orig = get_xml_file (GLOBAL_SAMPLE);
  mgr = tpaw_irc_network_manager_new (global_file_orig, NULL);

  g_object_get (mgr,
      "global-file", &global_file,
      "user-file", &user_file,
      NULL);
  g_assert_cmpstr (global_file, ==, global_file_orig);
  g_free (global_file);
  g_free (global_file_orig);
  g_free (user_file);

  networks = tpaw_irc_network_manager_get_networks (mgr);
  g_assert_cmpuint (g_slist_length (networks), ==, 4);

  network_checked[0] = network_checked[1] = network_checked[2] =
    network_checked[3] = FALSE;
  /* check networks and servers */
  for (l = networks; l != NULL; l = g_slist_next (l))
    {
      gchar *name;

      g_object_get (l->data, "name", &name, NULL);
      g_assert (name != NULL);

      if (strcmp (name, "Freenode") == 0)
        {
          check_network (l->data, "Freenode", "UTF-8", freenode_servers, 2);
          network_checked[0] = TRUE;
        }
      else if (strcmp (name, "GIMPNet") == 0)
        {
          check_network (l->data, "GIMPNet", "UTF-8", gimpnet_servers, 2);
          network_checked[1] = TRUE;
        }
      else if (strcmp (name, "Test Server") == 0)
        {
          check_network (l->data, "Test Server", "ISO-8859-1", test_servers, 1);
          network_checked[2] = TRUE;
        }
      else if (strcmp (name, "Undernet") == 0)
        {
          check_network (l->data, "Undernet", "UTF-8", undernet_servers, 1);
          network_checked[3] = TRUE;
        }
      else
        {
          g_assert_not_reached ();
        }

      g_free (name);
    }
  g_assert (network_checked[0] && network_checked[1] && network_checked[2] &&
      network_checked[3]);

  g_slist_foreach (networks, (GFunc) g_object_unref, NULL);
  g_slist_free (networks);
  g_object_unref (mgr);
}

static gboolean
remove_network_named (TpawIrcNetworkManager *mgr,
                      const gchar *network_name)
{
  GSList *networks, *l;
  gboolean removed = FALSE;

  networks = tpaw_irc_network_manager_get_networks (mgr);

  /* check networks and servers */
  for (l = networks; l != NULL && !removed; l = g_slist_next (l))
    {
      TpawIrcNetwork *network = l->data;
      gchar *name;

      g_object_get (network, "name", &name, NULL);
      g_assert (name != NULL);

      if (strcmp (name, network_name) == 0)
        {
          tpaw_irc_network_manager_remove (mgr, network);
          removed = TRUE;
        }

      g_free (name);
    }

  g_slist_foreach (networks, (GFunc) g_object_unref, NULL);
  g_slist_free (networks);

  return removed;
}

static void
test_tpaw_irc_network_manager_remove (void)
{
  TpawIrcNetworkManager *mgr;
  GSList *networks, *l;
  struct server_t freenode_servers[] = {
    { "irc.freenode.net", 6667, FALSE },
    { "irc.eu.freenode.net", 6667, FALSE }};
  struct server_t test_servers[] = {
    { "irc.test.org", 6669, TRUE }};
  struct server_t undernet_servers[] = {
    { "eu.undernet.org", 6667, FALSE }};
  gboolean network_checked[3];
  gboolean result;
  gchar *global_file_orig;

  global_file_orig = get_xml_file (GLOBAL_SAMPLE);
  mgr = tpaw_irc_network_manager_new (global_file_orig, NULL);
  g_free (global_file_orig);

  result = remove_network_named (mgr, "GIMPNet");
  g_assert (result);

  networks = tpaw_irc_network_manager_get_networks (mgr);
  g_assert_cmpuint (g_slist_length (networks), ==, 3);

  network_checked[0] = network_checked[1] = network_checked[2] = FALSE;
  /* check networks and servers */
  for (l = networks; l != NULL; l = g_slist_next (l))
    {
      gchar *name;

      g_object_get (l->data, "name", &name, NULL);
      g_assert (name != NULL);

      if (strcmp (name, "Freenode") == 0)
        {
          check_network (l->data, "Freenode", "UTF-8", freenode_servers, 2);
          network_checked[0] = TRUE;
        }
      else if (strcmp (name, "Test Server") == 0)
        {
          check_network (l->data, "Test Server", "ISO-8859-1", test_servers, 1);
          network_checked[1] = TRUE;
        }
      else if (strcmp (name, "Undernet") == 0)
        {
          check_network (l->data, "Undernet", "UTF-8", undernet_servers, 1);
          network_checked[2] = TRUE;
        }
      else
        {
          g_assert_not_reached ();
        }

      g_free (name);
    }
  g_assert (network_checked[0] && network_checked[1] && network_checked[2]);

  g_slist_foreach (networks, (GFunc) g_object_unref, NULL);
  g_slist_free (networks);
  g_object_unref (mgr);
}

static void
test_load_user_file (void)
{
  TpawIrcNetworkManager *mgr;
  gchar *global_file, *user_file;
  GSList *networks, *l;
  struct server_t gimpnet_servers[] = {
    { "irc.gimp.org", 6667, FALSE },
    { "irc.us.gimp.org", 6667, FALSE },
    { "irc.au.gimp.org", 6667, FALSE }};
  struct server_t my_server[] = {
    { "irc.mysrv.net", 7495, TRUE }};
  struct server_t another_server[] = {
    { "irc.anothersrv.be", 6660, FALSE }};
  gboolean network_checked[3];
  gchar *user_file_orig;

  copy_xml_file (USER_SAMPLE, USER_FILE);
  user_file_orig = get_user_xml_file (USER_FILE);
  mgr = tpaw_irc_network_manager_new (NULL, user_file_orig);

  g_object_get (mgr,
      "global-file", &global_file,
      "user-file", &user_file,
      NULL);
  g_assert (global_file == NULL);
  g_assert_cmpstr (user_file, ==, user_file_orig);
  g_free (global_file);
  g_free (user_file);
  g_free (user_file_orig);

  networks = tpaw_irc_network_manager_get_networks (mgr);
  g_assert_cmpuint (g_slist_length (networks), ==, 3);

  network_checked[0] = network_checked[1] = network_checked[2] = FALSE;
  /* check networks and servers */
  for (l = networks; l != NULL; l = g_slist_next (l))
    {
      gchar *name;

      g_object_get (l->data, "name", &name, NULL);
      g_assert (name != NULL);

      if (strcmp (name, "GIMPNet") == 0)
        {
          check_network (l->data, "GIMPNet", "UTF-8", gimpnet_servers, 3);
          network_checked[0] = TRUE;
        }
      else if (strcmp (name, "My Server") == 0)
        {
          check_network (l->data, "My Server", "UTF-8", my_server, 1);
          network_checked[1] = TRUE;
        }
      else if (strcmp (name, "Another Server") == 0)
        {
          check_network (l->data, "Another Server", "UTF-8", another_server, 1);
          network_checked[2] = TRUE;
        }
      else
        {
          g_assert_not_reached ();
        }

      g_free (name);
    }
  g_assert (network_checked[0] && network_checked[1] && network_checked[2]);

  g_slist_foreach (networks, (GFunc) g_object_unref, NULL);
  g_slist_free (networks);
  g_object_unref (mgr);
}

static void
test_load_both_files (void)
{
  TpawIrcNetworkManager *mgr;
  gchar *global_file, *user_file;
  GSList *networks, *l;
  struct server_t freenode_servers[] = {
    { "irc.freenode.net", 6667, FALSE },
    { "irc.eu.freenode.net", 6667, FALSE }};
  struct server_t gimpnet_servers[] = {
    { "irc.gimp.org", 6667, FALSE },
    { "irc.us.gimp.org", 6667, FALSE },
    { "irc.au.gimp.org", 6667, FALSE }};
  struct server_t my_server[] = {
    { "irc.mysrv.net", 7495, TRUE }};
  struct server_t another_server[] = {
    { "irc.anothersrv.be", 6660, FALSE }};
  struct server_t undernet_servers[] = {
    { "eu.undernet.org", 6667, FALSE }};
  gboolean network_checked[5];
  gchar *global_file_orig, *user_file_orig;

  global_file_orig = get_xml_file (GLOBAL_SAMPLE);
  user_file_orig = get_user_xml_file (USER_FILE);
  mgr = tpaw_irc_network_manager_new (global_file_orig, user_file_orig);

  g_object_get (mgr,
      "global-file", &global_file,
      "user-file", &user_file,
      NULL);
  g_assert_cmpstr (global_file, ==, global_file_orig);
  g_assert_cmpstr (user_file, ==, user_file_orig);
  g_free (global_file);
  g_free (global_file_orig);
  g_free (user_file);
  g_free (user_file_orig);

  networks = tpaw_irc_network_manager_get_networks (mgr);
  g_assert_cmpuint (g_slist_length (networks), ==, 5);

  network_checked[0] = network_checked[1] = network_checked[2] =
    network_checked[3] = network_checked[4] = FALSE;
  /* check networks and servers */
  for (l = networks; l != NULL; l = g_slist_next (l))
    {
      gchar *name;

      g_object_get (l->data, "name", &name, NULL);
      g_assert (name != NULL);

      if (strcmp (name, "Freenode") == 0)
        {
          check_network (l->data, "Freenode", "UTF-8", freenode_servers, 2);
          network_checked[0] = TRUE;
        }
      else if (strcmp (name, "GIMPNet") == 0)
        {
          check_network (l->data, "GIMPNet", "UTF-8", gimpnet_servers, 3);
          network_checked[1] = TRUE;
        }
      else if (strcmp (name, "My Server") == 0)
        {
          check_network (l->data, "My Server", "UTF-8", my_server, 1);
          network_checked[2] = TRUE;
        }
      else if (strcmp (name, "Another Server") == 0)
        {
          check_network (l->data, "Another Server", "UTF-8", another_server, 1);
          network_checked[3] = TRUE;
        }
      else if (strcmp (name, "Undernet") == 0)
        {
          check_network (l->data, "Undernet", "UTF-8", undernet_servers, 1);
          network_checked[4] = TRUE;
        }
      else
        {
          g_assert_not_reached ();
        }

      g_free (name);
    }
  g_assert (network_checked[0] && network_checked[1] && network_checked[2] &&
      network_checked[3] && network_checked[4]);

  g_slist_foreach (networks, (GFunc) g_object_unref, NULL);
  g_slist_free (networks);
  g_object_unref (mgr);
}

static void
test_modify_user_file (void)
{
  TpawIrcNetworkManager *mgr;
  TpawIrcNetwork *network;
  TpawIrcServer *server;
  gchar *global_file, *user_file;
  GSList *networks, *l;
  struct server_t gimpnet_servers[] = {
    { "irc.gimp.org", 6667, TRUE },
    { "irc.us.gimp.org", 6668, FALSE }};
  struct server_t great_server[] = {
    { "irc.greatserver.com", 7873, TRUE }};
  struct server_t another_server[] = {
    { "irc.anothersrv.be", 6660, FALSE }};
  gboolean network_modified[2];
  gboolean network_checked[3];
  gchar *user_file_orig;

  copy_xml_file (USER_SAMPLE, USER_FILE);
  user_file_orig = get_user_xml_file (USER_FILE);
  mgr = tpaw_irc_network_manager_new (NULL, user_file_orig);

  g_object_get (mgr,
      "global-file", &global_file,
      "user-file", &user_file,
      NULL);
  g_assert (global_file == NULL);
  g_assert_cmpstr (user_file, ==, user_file_orig);
  g_free (global_file);
  g_free (user_file);

  networks = tpaw_irc_network_manager_get_networks (mgr);
  g_assert_cmpuint (g_slist_length (networks), ==, 3);

  network_modified[0] = network_modified[1] = FALSE;
  /* check networks and servers */
  for (l = networks; l != NULL; l = g_slist_next (l))
    {
      gchar *name;

      network = l->data;
      g_object_get (network, "name", &name, NULL);
      g_assert (name != NULL);

      if (strcmp (name, "GIMPNet") == 0)
        {
          GSList *servers, *ll;

          /* change charset */
          g_object_set (network, "charset", "ISO-8859-1", NULL);

          servers = tpaw_irc_network_get_servers (network);
          for (ll = servers; ll != NULL; ll = g_slist_next (ll))
            {
              gchar *address;

              server = ll->data;
              g_object_get (server, "address", &address, NULL);
              if (strcmp (address, "irc.gimp.org") == 0)
                {
                  /* change SSL */
                  g_object_set (server, "ssl", TRUE, NULL);
                }
              else if (strcmp (address, "irc.us.gimp.org") == 0)
                {
                  /* change port */
                  g_object_set (server, "port", 6668, NULL);
                }
              else if (strcmp (address, "irc.au.gimp.org") == 0)
                {
                  /* remove this server */
                  tpaw_irc_network_remove_server (network, server);
                }
              else
                {
                  g_assert_not_reached ();
                }

              g_free (address);
            }

          network_modified[0] = TRUE;

          g_slist_foreach (servers, (GFunc) g_object_unref, NULL);
          g_slist_free (servers);
        }
      else if (strcmp (name, "My Server") == 0)
        {
          /* remove this network */
          tpaw_irc_network_manager_remove (mgr, network);
          network_modified[1] = TRUE;
        }
      else if (strcmp (name, "Another Server") == 0)
        {
          /* Don't change this one */
        }
      else
        {
          g_assert_not_reached ();
        }

      g_free (name);
    }
  g_assert (network_modified[0] && network_modified[1]);

  /* Add a new network */
  network = tpaw_irc_network_new ("Great Server");
  server = tpaw_irc_server_new ("irc.greatserver.com", 7873, TRUE);
  tpaw_irc_network_append_server (network, server);
  tpaw_irc_network_manager_add (mgr, network);
  g_object_unref (server);
  g_object_unref (network);

  g_slist_foreach (networks, (GFunc) g_object_unref, NULL);
  g_slist_free (networks);
  g_object_unref (mgr);


  /* Now let's reload the file and check its contain */
  mgr = tpaw_irc_network_manager_new (NULL, user_file_orig);
  g_free (user_file_orig);

  networks = tpaw_irc_network_manager_get_networks (mgr);
  g_assert_cmpuint (g_slist_length (networks), ==, 3);

  network_checked[0] = network_checked[1] = network_checked[2] = FALSE;
  /* check networks and servers */
  for (l = networks; l != NULL; l = g_slist_next (l))
    {
      gchar *name;

      g_object_get (l->data, "name", &name, NULL);
      g_assert (name != NULL);

      if (strcmp (name, "GIMPNet") == 0)
        {
          check_network (l->data, "GIMPNet", "ISO-8859-1", gimpnet_servers, 2);
          network_checked[0] = TRUE;
        }
      else if (strcmp (name, "Great Server") == 0)
        {
          check_network (l->data, "Great Server", "UTF-8", great_server, 1);
          network_checked[1] = TRUE;
        }
      else if (strcmp (name, "Another Server") == 0)
        {
          check_network (l->data, "Another Server", "UTF-8", another_server, 1);
          network_checked[2] = TRUE;
        }
      else
        {
          g_assert_not_reached ();
        }

      g_free (name);
    }
  g_assert (network_checked[0] && network_checked[1] && network_checked[2]);

  g_slist_foreach (networks, (GFunc) g_object_unref, NULL);
  g_slist_free (networks);
  g_object_unref (mgr);
}

static void
test_modify_both_files (void)
{
  TpawIrcNetworkManager *mgr;
  TpawIrcNetwork *network;
  TpawIrcServer *server;
  gchar *global_file, *user_file;
  GSList *networks, *l;
  struct server_t gimpnet_servers[] = {
    { "irc.gimp.org", 6667, TRUE },
    { "irc.us.gimp.org", 6668, FALSE }};
  struct server_t great_server[] = {
    { "irc.greatserver.com", 7873, TRUE }};
  struct server_t another_server[] = {
    { "irc.anothersrv.be", 6660, FALSE }};
  struct server_t undernet_servers[] = {
    { "eu.undernet.org", 6667, FALSE },
    { "us.undernet.org", 6667, FALSE }};
  gboolean network_modified[4];
  gboolean network_checked[4];
  gchar *global_file_orig, *user_file_orig;

  copy_xml_file (USER_SAMPLE, USER_FILE);
  global_file_orig = get_xml_file (GLOBAL_SAMPLE);
  user_file_orig = get_user_xml_file (USER_FILE);
  mgr = tpaw_irc_network_manager_new (global_file_orig, user_file_orig);

  g_object_get (mgr,
      "global-file", &global_file,
      "user-file", &user_file,
      NULL);
  g_assert_cmpstr (global_file, ==, global_file_orig);
  g_assert_cmpstr (user_file, ==, user_file_orig);
  g_free (global_file);
  g_free (global_file_orig);
  g_free (user_file);
  g_free (user_file_orig);

  networks = tpaw_irc_network_manager_get_networks (mgr);
  g_assert_cmpuint (g_slist_length (networks), ==, 5);

  network_modified[0] = network_modified[1] = network_modified[2] =
    network_modified[3] = FALSE;
  /* check networks and servers */
  for (l = networks; l != NULL; l = g_slist_next (l))
    {
      gchar *name;

      network = l->data;
      g_object_get (network, "name", &name, NULL);
      g_assert (name != NULL);

      if (strcmp (name, "GIMPNet") == 0)
        {
          /* Modify user network */
          GSList *servers, *ll;

          servers = tpaw_irc_network_get_servers (network);
          for (ll = servers; ll != NULL; ll = g_slist_next (ll))
            {
              gchar *address;

              server = ll->data;
              g_object_get (server, "address", &address, NULL);
              if (strcmp (address, "irc.gimp.org") == 0)
                {
                  /* change SSL */
                  g_object_set (server, "ssl", TRUE, NULL);
                }
              else if (strcmp (address, "irc.us.gimp.org") == 0)
                {
                  /* change port */
                  g_object_set (server, "port", 6668, NULL);
                }
              else if (strcmp (address, "irc.au.gimp.org") == 0)
                {
                  /* remove this server */
                  tpaw_irc_network_remove_server (network, server);
                }
              else
                {
                  g_assert_not_reached ();
                }

              g_free (address);
            }

          network_modified[0] = TRUE;

          g_slist_foreach (servers, (GFunc) g_object_unref, NULL);
          g_slist_free (servers);
        }
      else if (strcmp (name, "My Server") == 0)
        {
          /* remove user network */
          tpaw_irc_network_manager_remove (mgr, network);
          network_modified[1] = TRUE;
        }
      else if (strcmp (name, "Freenode") == 0)
        {
          /* remove global network */
          tpaw_irc_network_manager_remove (mgr, network);
          network_modified[2] = TRUE;
        }
      else if (strcmp (name, "Undernet") == 0)
        {
          /* modify global network */
          server = tpaw_irc_server_new ("us.undernet.org", 6667, FALSE);
          tpaw_irc_network_append_server (network, server);
          g_object_unref (server);

          network_modified[3] = TRUE;
        }
      else if (strcmp (name, "Another Server") == 0)
        {
          /* Don't change this one */
        }
      else
        {
          g_assert_not_reached ();
        }

      g_free (name);
    }
  g_assert (network_modified[0] && network_modified[1] && network_modified[2]
      && network_modified[3]);

  /* Add a new network */
  network = tpaw_irc_network_new ("Great Server");
  server = tpaw_irc_server_new ("irc.greatserver.com", 7873, TRUE);
  tpaw_irc_network_append_server (network, server);
  tpaw_irc_network_manager_add (mgr, network);
  g_object_unref (server);
  g_object_unref (network);

  g_slist_foreach (networks, (GFunc) g_object_unref, NULL);
  g_slist_free (networks);
  g_object_unref (mgr);


  /* Now let's reload the file and check its contain */
  global_file_orig = get_xml_file (GLOBAL_SAMPLE);
  user_file_orig = get_user_xml_file (USER_FILE);
  mgr = tpaw_irc_network_manager_new (global_file_orig, user_file_orig);
  g_free (global_file_orig);
  g_free (user_file_orig);

  networks = tpaw_irc_network_manager_get_networks (mgr);
  g_assert_cmpuint (g_slist_length (networks), ==, 4);

  network_checked[0] = network_checked[1] = network_checked[2] =
    network_checked[3] = FALSE;
  /* check networks and servers */
  for (l = networks; l != NULL; l = g_slist_next (l))
    {
      gchar *name;

      g_object_get (l->data, "name", &name, NULL);
      g_assert (name != NULL);

      if (strcmp (name, "GIMPNet") == 0)
        {
          check_network (l->data, "GIMPNet", "UTF-8", gimpnet_servers, 2);
          network_checked[0] = TRUE;
        }
      else if (strcmp (name, "Great Server") == 0)
        {
          check_network (l->data, "Great Server", "UTF-8", great_server, 1);
          network_checked[1] = TRUE;
        }
      else if (strcmp (name, "Another Server") == 0)
        {
          check_network (l->data, "Another Server", "UTF-8", another_server, 1);
          network_checked[2] = TRUE;
        }
      else if (strcmp (name, "Undernet") == 0)
        {
          check_network (l->data, "Undernet", "UTF-8", undernet_servers, 2);
          network_checked[3] = TRUE;
        }
      else
        {
          g_assert_not_reached ();
        }

      g_free (name);
    }
  g_assert (network_checked[0] && network_checked[1] && network_checked[2] &&
      network_checked[3]);

  g_slist_foreach (networks, (GFunc) g_object_unref, NULL);
  g_slist_free (networks);
  g_object_unref (mgr);
}

static void
test_tpaw_irc_network_manager_find_network_by_address (void)
{
  TpawIrcNetworkManager *mgr;
  TpawIrcNetwork *network;
  struct server_t freenode_servers[] = {
    { "irc.freenode.net", 6667, FALSE },
    { "irc.eu.freenode.net", 6667, FALSE }};
  gchar *global_file_orig;

  global_file_orig = get_xml_file (GLOBAL_SAMPLE);
  mgr = tpaw_irc_network_manager_new (global_file_orig, NULL);
  g_free (global_file_orig);

  network = tpaw_irc_network_manager_find_network_by_address (mgr,
      "irc.freenode.net");
  g_assert (network != NULL);
  check_network (network, "Freenode", "UTF-8", freenode_servers, 2);

  network = tpaw_irc_network_manager_find_network_by_address (mgr,
      "irc.eu.freenode.net");
  g_assert (network != NULL);
  check_network (network, "Freenode", "UTF-8", freenode_servers, 2);

  network = tpaw_irc_network_manager_find_network_by_address (mgr,
      "unknown");
  g_assert (network == NULL);

  g_object_unref (mgr);
}

static void
test_no_modify_with_empty_user_file (void)
{
  TpawIrcNetworkManager *mgr;
  GSList *networks;
  gchar *global_file_orig;
  gchar *user_file_orig;

  /* user don't have a networks file yet */
  user_file_orig = get_user_xml_file (USER_FILE);
  g_unlink (user_file_orig);

  global_file_orig = get_xml_file (GLOBAL_SAMPLE);
  mgr = tpaw_irc_network_manager_new (global_file_orig, user_file_orig);
  g_free (global_file_orig);
  g_object_unref (mgr);

  /* We didn't modify anything so USER_FILE should be empty */
  mgr = tpaw_irc_network_manager_new (NULL, user_file_orig);
  g_free (user_file_orig);

  networks = tpaw_irc_network_manager_get_networks (mgr);
  g_assert_cmpuint (g_slist_length (networks), ==, 0);

  g_slist_foreach (networks, (GFunc) g_object_unref, NULL);
  g_slist_free (networks);
  g_object_unref (mgr);
}

int
main (int argc,
    char **argv)
{
  int result;

  test_init (argc, argv);

  g_test_add_func ("/irc-network-manager/add",
      test_tpaw_irc_network_manager_add);
  g_test_add_func ("/irc-network-manager/load-global-file",
      test_load_global_file);
  g_test_add_func ("/irc-network-manager/remove",
      test_tpaw_irc_network_manager_remove);
  g_test_add_func ("/irc-network-manager/load-user-file", test_load_user_file);
  g_test_add_func ("/irc-network-manager/load-both-files",
      test_load_both_files);
  g_test_add_func ("/irc-network-manager/modify-user-file",
      test_modify_user_file);
  g_test_add_func ("/irc-network-manager/modify-both-files",
      test_modify_both_files);
  g_test_add_func ("/irc-network-manager/find-network-by-address",
      test_tpaw_irc_network_manager_find_network_by_address);
  g_test_add_func ("/irc-network-manager/no-modify-with-empty-user-file",
      test_no_modify_with_empty_user_file);

  result = g_test_run ();
  test_deinit ();
  return result;
}
