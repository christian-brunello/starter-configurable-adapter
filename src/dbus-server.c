/*
 * starter-configurable-adapter - dbus-server.c
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "dbus-server.h"

#include <string.h>

#define LOGD(msg...) g_log("dbus", G_LOG_LEVEL_DEBUG, msg)
#define LOGI(msg...) g_log("dbus", G_LOG_LEVEL_INFO, msg)
#define LOGW(msg...) g_log("dbus", G_LOG_LEVEL_WARNING, msg)

#define STCA_DBUS_OBJECT_PATH "/org/starter/adapter"
#define STCA_DBUS_INTERFACE "org.starter.Adapter"

typedef struct {
  GDBusServer *server;
  GDBusNodeInfo *introspection;
  GPtrArray *connections; /* GDBusConnection* */
} STCADbusServer;

static const gchar introspection_xml[] =
  "<node name='/org/starter/adapter'>"
  "  <interface name='org.starter.Adapter'>"
  "    <method name='GetData'>"
  "      <arg direction='out' type='a{s(ds)}' name='data'/>"
  "    </method>"
  "    <method name='SetData'>"
  "      <arg direction='in' type='a{sd}' name='values'/>"
  "      <arg direction='out' type='b' name='ok'/>"
  "    </method>"
  "    <signal name='DataChanged'>"
  "      <arg name='changed' type='a{s(ds)}'/>"
  "    </signal>"
  "  </interface>"
  "</node>";

static GVariant *
values_to_variant(GHashTable *values)
{
  GVariantBuilder builder;
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_variant_builder_init(&builder, G_VARIANT_TYPE("a{s(ds)}"));

  g_hash_table_iter_init(&iter, values);
  while (g_hash_table_iter_next(&iter, &key, &value))
    {
      const gchar *k = key;
      const STCAValue *v = value;

      if (k == NULL || v == NULL)
	continue;

      g_variant_builder_add(&builder,
			    "{s(ds)}",
			    k,
			    v->value,
			    stca_access_to_string(v->access));
    }

  return g_variant_builder_end(&builder);
}

static gboolean
ensure_all_writable(STCAContext *context, GHashTable *numbers, GError **error)
{
  GHashTableIter iter;
  gpointer key;

  g_hash_table_iter_init(&iter, numbers);
  while (g_hash_table_iter_next(&iter, &key, NULL))
    {
      if (!stca_context_key_is_writable(context, (const gchar *) key, error))
	return FALSE;
    }

  return TRUE;
}

static void
handle_method_call(GDBusConnection *connection,
		   const gchar *sender,
		   const gchar *object_path,
		   const gchar *interface_name,
		   const gchar *method_name,
		   GVariant *parameters,
		   GDBusMethodInvocation *invocation,
		   gpointer user_data)
{
  STCAContext *context = user_data;

  (void) connection;
  (void) sender;
  (void) object_path;
  (void) interface_name;

  LOGD("method %s", method_name);

  if (g_strcmp0(method_name, "GetData") == 0)
    {
      GVariant *map = values_to_variant(context->values);
      g_dbus_method_invocation_return_value(invocation,
					    g_variant_new_tuple(&map, 1));
      return;
    }

  if (g_strcmp0(method_name, "SetData") == 0)
    {
      GVariant *dict = NULL;
      GVariantIter dict_iter;
      const gchar *key = NULL;
      gdouble value = 0.0;
      GHashTable *table;
      GError *error = NULL;
      GHashTable *changed;

      table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
      g_variant_get(parameters, "(@a{sd})", &dict);
      if (dict != NULL)
	{
	  g_variant_iter_init(&dict_iter, dict);
	  while (g_variant_iter_next(&dict_iter, "{sd}", &key, &value))
	    {
	      gdouble *boxed = g_new(gdouble, 1);
	      *boxed = value;
	      g_hash_table_insert(table, g_strdup(key), boxed);
	    }
	  g_variant_unref(dict);
	}

      if (g_hash_table_size(table) == 0)
	{
	  g_hash_table_unref(table);
	  g_dbus_method_invocation_return_value(invocation,
						g_variant_new("(b)", FALSE));
	  return;
	}

      if (!ensure_all_writable(context, table, &error)
	  || !stca_plugin_write_values(context->plugin, table, &error))
	{
	  LOGW("SetData failed: %s", error ? error->message : "unknown");
	  g_clear_error(&error);
	  g_hash_table_unref(table);
	  g_dbus_method_invocation_return_value(invocation,
						g_variant_new("(b)", FALSE));
	  return;
	}

      changed = stca_context_apply_writes(context, table);
      g_hash_table_unref(table);
      if (changed != NULL)
	{
	  stca_dbus_server_emit_data_changed(context, changed);
	  g_hash_table_unref(changed);
	}

      g_dbus_method_invocation_return_value(invocation,
					    g_variant_new("(b)", TRUE));
      return;
    }

  g_dbus_method_invocation_return_error(invocation,
					G_DBUS_ERROR,
					G_DBUS_ERROR_UNKNOWN_METHOD,
					"Unknown method %s", method_name);
}

static const GDBusInterfaceVTable interface_vtable = {
  handle_method_call,
  NULL,
  NULL,
  { 0 }
};

static void
connection_closed(GDBusConnection *connection,
		  gboolean remote_peer_vanished,
		  GError *error,
		  gpointer user_data)
{
  STCADbusServer *dbus = user_data;

  (void) remote_peer_vanished;
  (void) error;

  LOGD("client disconnected");
  g_ptr_array_remove(dbus->connections, connection);
}

static gboolean
on_new_connection(GDBusServer *server,
		  GDBusConnection *connection,
		  gpointer user_data)
{
  STCAContext *context = user_data;
  STCADbusServer *dbus = context->dbus_server;
  guint registration_id;

  (void) server;

  LOGD("new connection");

  g_object_ref(connection);
  g_ptr_array_add(dbus->connections, connection);
  g_signal_connect(connection, "closed",
		   G_CALLBACK(connection_closed), dbus);

  registration_id =
    g_dbus_connection_register_object(connection,
				      STCA_DBUS_OBJECT_PATH,
				      dbus->introspection->interfaces[0],
				      &interface_vtable,
				      context,
				      NULL,
				      NULL);
  if (registration_id == 0)
    LOGW("failed to register D-Bus object on new connection");

  return TRUE;
}

void
stca_dbus_server_emit_data_changed(STCAContext *context, GHashTable *changed)
{
  STCADbusServer *dbus;
  GVariant *map;
  GVariant *tuple;
  guint i;

  if (context == NULL || context->dbus_server == NULL || changed == NULL)
    return;
  if (g_hash_table_size(changed) == 0)
    return;

  dbus = context->dbus_server;

  for (i = 0; i < dbus->connections->len; i++)
    {
      GDBusConnection *conn = dbus->connections->pdata[i];
      GError *local = NULL;

      map = values_to_variant(changed);
      tuple = g_variant_new_tuple(&map, 1);

      if (!g_dbus_connection_emit_signal(conn,
					 NULL,
					 STCA_DBUS_OBJECT_PATH,
					 STCA_DBUS_INTERFACE,
					 "DataChanged",
					 tuple,
					 &local))
	{
	  LOGW("DataChanged emit failed: %s",
	       local ? local->message : "unknown");
	  g_clear_error(&local);
	}
    }
  LOGD("DataChanged emitted to %u client(s), %u key(s)",
       dbus->connections->len, g_hash_table_size(changed));
}

gboolean
stca_dbus_server_start(STCAContext *context, GError **error)
{
  STCADbusServer *dbus;
  gchar *guid;
  gchar *address;
  GError *local = NULL;

  g_return_val_if_fail(context != NULL, FALSE);
  g_return_val_if_fail(context->dbus_server == NULL, FALSE);

  if (context->options.listen_address == NULL
      || *context->options.listen_address == '\0')
    {
      g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
		  "listen-address is required for D-Bus");
      return FALSE;
    }

  if (context->options.listen_port == 0
      || context->options.listen_port > 65535)
    {
      g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
		  "invalid listen-port %u", context->options.listen_port);
      return FALSE;
    }

  dbus = g_new0(STCADbusServer, 1);
  dbus->introspection = g_dbus_node_info_new_for_xml(introspection_xml, &local);
  if (dbus->introspection == NULL)
    {
      g_propagate_error(error, local);
      g_free(dbus);
      return FALSE;
    }

  dbus->connections = g_ptr_array_new_with_free_func(g_object_unref);

  guid = g_dbus_generate_guid();
  address = g_strdup_printf("tcp:host=%s,port=%u",
			    context->options.listen_address,
			    context->options.listen_port);

  LOGD("creating D-Bus server at %s", address);

  dbus->server = g_dbus_server_new_sync(address,
					G_DBUS_SERVER_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS,
					guid,
					NULL,
					NULL,
					&local);
  g_free(guid);
  g_free(address);

  if (dbus->server == NULL)
    {
      g_propagate_error(error, local);
      g_dbus_node_info_unref(dbus->introspection);
      g_ptr_array_unref(dbus->connections);
      g_free(dbus);
      return FALSE;
    }

  g_signal_connect(dbus->server, "new-connection",
		   G_CALLBACK(on_new_connection), context);
  g_dbus_server_start(dbus->server);

  context->dbus_server = dbus;

  LOGI("D-Bus server listening on %s:%u (%s)",
       context->options.listen_address,
       context->options.listen_port,
       STCA_DBUS_INTERFACE);
  return TRUE;
}

void
stca_dbus_server_stop(STCAContext *context)
{
  STCADbusServer *dbus;

  if (context == NULL || context->dbus_server == NULL)
    return;

  dbus = context->dbus_server;
  context->dbus_server = NULL;

  if (dbus->server != NULL)
    {
      g_dbus_server_stop(dbus->server);
      g_clear_object(&dbus->server);
    }

  if (dbus->connections != NULL)
    g_ptr_array_unref(dbus->connections);

  if (dbus->introspection != NULL)
    g_dbus_node_info_unref(dbus->introspection);

  g_free(dbus);
}
