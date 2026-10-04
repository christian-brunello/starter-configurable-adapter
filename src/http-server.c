/*
 * starter-configurable-adapter - http-server.c
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

#include "http-server.h"
#include "plugin-entry.h"
#include "autoconf.h"

#include <string.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>

#define LOGI(msg...) g_log("http", G_LOG_LEVEL_INFO, msg)
#define LOGW(msg...) g_log("http", G_LOG_LEVEL_WARNING, msg)

static void
json_builder_add_stca_value(JsonBuilder *builder, const STCAValue *v)
{
  json_builder_begin_object(builder);
  json_builder_set_member_name(builder, "value");
  json_builder_add_double_value(builder, v->value);
  json_builder_set_member_name(builder, "access");
  json_builder_add_string_value(builder, stca_access_to_string(v->access));
  json_builder_end_object(builder);
}

static gchar *
values_to_json(GHashTable *values)
{
  JsonBuilder *builder;
  JsonGenerator *gen;
  JsonNode *root;
  GHashTableIter iter;
  gpointer key;
  gpointer value;
  gchar *text;

  builder = json_builder_new();
  json_builder_begin_object(builder);

  g_hash_table_iter_init(&iter, values);
  while (g_hash_table_iter_next(&iter, &key, &value))
    {
      const gchar *k = key;
      const STCAValue *v = value;

      if (k == NULL || v == NULL)
	continue;

      json_builder_set_member_name(builder, k);
      json_builder_add_stca_value(builder, v);
    }

  json_builder_end_object(builder);
  root = json_builder_get_root(builder);
  gen = json_generator_new();
  json_generator_set_root(gen, root);
  text = json_generator_to_data(gen, NULL);

  json_node_unref(root);
  g_object_unref(gen);
  g_object_unref(builder);
  return text;
}

static gchar *
value_to_json(const STCAValue *v)
{
  JsonBuilder *builder;
  JsonGenerator *gen;
  JsonNode *root;
  gchar *text;

  builder = json_builder_new();
  json_builder_add_stca_value(builder, v);
  root = json_builder_get_root(builder);
  gen = json_generator_new();
  json_generator_set_root(gen, root);
  text = json_generator_to_data(gen, NULL);

  json_node_unref(root);
  g_object_unref(gen);
  g_object_unref(builder);
  return text;
}

/* Validate every key in @numbers (gchar* → gdouble*) is writable. */
static gboolean
ensure_all_writable(STCAContext *context, GHashTable *numbers, GError **error)
{
  GHashTableIter iter;
  gpointer key;

  g_hash_table_iter_init(&iter, numbers);
  while (g_hash_table_iter_next(&iter, &key, NULL))
    {
      if (!stca_context_key_is_writable(context, key, error))
	return FALSE;
    }

  return TRUE;
}

static gboolean
parse_json_number(const gchar *body, gsize len, gdouble *out, GError **error)
{
  JsonParser *parser;
  JsonNode *root;
  gboolean ok = FALSE;

  parser = json_parser_new();
  if (!json_parser_load_from_data(parser, body, (gssize) len, error))
    {
      g_object_unref(parser);
      return FALSE;
    }

  root = json_parser_get_root(parser);
  if (root != NULL && JSON_NODE_HOLDS_VALUE(root))
    {
      *out = json_node_get_double(root);
      ok = TRUE;
    }
  else
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_INVALID_DATA,
		  "body must be a JSON number");
    }

  g_object_unref(parser);
  return ok;
}

static GHashTable *
parse_json_object_numbers(const gchar *body, gsize len, GError **error)
{
  JsonParser *parser;
  JsonNode *root;
  JsonObject *obj;
  GHashTable *table;
  GList *members;
  GList *l;

  parser = json_parser_new();
  if (!json_parser_load_from_data(parser, body, (gssize) len, error))
    {
      g_object_unref(parser);
      return NULL;
    }

  root = json_parser_get_root(parser);
  if (root == NULL || !JSON_NODE_HOLDS_OBJECT(root))
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_INVALID_DATA,
		  "body must be a JSON object of numbers");
      g_object_unref(parser);
      return NULL;
    }

  obj = json_node_get_object(root);
  table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  members = json_object_get_members(obj);

  for (l = members; l != NULL; l = l->next)
    {
      const gchar *name = l->data;
      JsonNode *node = json_object_get_member(obj, name);
      gdouble *boxed;

      if (node == NULL || !JSON_NODE_HOLDS_VALUE(node))
	{
	  g_set_error(error,
		      G_IO_ERROR,
		      G_IO_ERROR_INVALID_DATA,
		      "member `%s' is not a JSON number", name);
	  g_list_free(members);
	  g_hash_table_unref(table);
	  g_object_unref(parser);
	  return NULL;
	}

      boxed = g_new(gdouble, 1);
      *boxed = json_node_get_double(node);
      g_hash_table_insert(table, g_strdup(name), boxed);
    }

  g_list_free(members);
  g_object_unref(parser);
  return table;
}

#ifdef HAVE_LIBSOUP_3

static void
set_json_response(SoupServerMessage *msg, guint status, const gchar *json)
{
  SoupMessageHeaders *headers = soup_server_message_get_response_headers(msg);

  soup_server_message_set_status(msg, status, NULL);
  soup_message_headers_replace(headers, "Content-Type", "application/json");
  soup_server_message_set_response(msg, "application/json",
				   SOUP_MEMORY_COPY,
				   json ? json : "{}",
				   json ? strlen(json) : 2);
}

static void
handle_request(SoupServer *server,
	       SoupServerMessage *msg,
	       const char *path,
	       GHashTable *query,
	       gpointer user_data)
{
  STCAContext *context = user_data;
  const char *method = soup_server_message_get_method(msg);
  SoupMessageBody *req_body;
  const gchar *body_data = NULL;
  gsize body_len = 0;

  (void) server;
  (void) query;

  req_body = soup_server_message_get_request_body(msg);
  if (req_body != NULL)
    {
      soup_message_body_flatten(req_body);
      body_data = req_body->data;
      body_len = req_body->length;
    }

  if (g_strcmp0(path, "/health") == 0 && g_strcmp0(method, "GET") == 0)
    {
      set_json_response(msg, 200, "{\"status\":\"ok\"}");
      return;
    }

  if (g_strcmp0(path, "/info") == 0 && g_strcmp0(method, "GET") == 0)
    {
      JsonBuilder *builder;
      JsonGenerator *gen;
      JsonNode *root;
      gchar *text;
      STCAPluginEntry *entry = NULL;
      const gchar *plugin_version = NULL;

      if (context->plugin_name != NULL)
	entry = stca_plugin_entry_lookup(context->plugins, context->plugin_name);
      if (entry != NULL)
	plugin_version = stca_plugin_info_get_version(stca_plugin_entry_get_info(entry));

      builder = json_builder_new();
      json_builder_begin_object(builder);
      json_builder_set_member_name(builder, "adapter-name");
      json_builder_add_string_value(builder,
				    context->options.adapter_name
				    ? context->options.adapter_name : "STCA");
      json_builder_set_member_name(builder, "adapter-version");
      json_builder_add_string_value(builder, PACKAGE_VERSION);
      json_builder_set_member_name(builder, "plugin");
      json_builder_add_string_value(builder,
				    context->plugin_name ? context->plugin_name : "");
      json_builder_set_member_name(builder, "plugin-version");
      json_builder_add_string_value(builder, plugin_version ? plugin_version : "");
      json_builder_end_object(builder);

      root = json_builder_get_root(builder);
      gen = json_generator_new();
      json_generator_set_root(gen, root);
      text = json_generator_to_data(gen, NULL);
      set_json_response(msg, 200, text);
      g_free(text);
      json_node_unref(root);
      g_object_unref(gen);
      g_object_unref(builder);
      return;
    }

  if (g_strcmp0(path, "/data") == 0 && g_strcmp0(method, "GET") == 0)
    {
      gchar *json = values_to_json(context->values);
      set_json_response(msg, 200, json);
      g_free(json);
      return;
    }

  if (g_str_has_prefix(path, "/data/") && g_strcmp0(method, "GET") == 0)
    {
      const gchar *key = path + strlen("/data/");
      STCAValue *v;

      if (*key == '\0')
	{
	  set_json_response(msg, 404, "{\"error\":\"missing key\"}");
	  return;
	}

      v = g_hash_table_lookup(context->values, key);
      if (v == NULL)
	{
	  set_json_response(msg, 404, "{\"error\":\"not found\"}");
	  return;
	}

      {
	gchar *json = value_to_json(v);
	set_json_response(msg, 200, json);
	g_free(json);
      }
      return;
    }

  if (g_strcmp0(path, "/data") == 0
      && (g_strcmp0(method, "PUT") == 0 || g_strcmp0(method, "POST") == 0))
    {
      GError *error = NULL;
      GHashTable *table;

      table = parse_json_object_numbers(body_data ? body_data : "", body_len, &error);
      if (table == NULL)
	{
	  set_json_response(msg, 400, "{\"error\":\"invalid JSON object\"}");
	  g_clear_error(&error);
	  return;
	}

      if (!ensure_all_writable(context, table, &error))
	{
	  guint status = (error && error->code == G_IO_ERROR_NOT_FOUND) ? 404 : 403;
	  set_json_response(msg, status,
			    status == 404
			    ? "{\"error\":\"not found\"}"
			    : "{\"error\":\"not writable\"}");
	  g_clear_error(&error);
	  g_hash_table_unref(table);
	  return;
	}

      if (!stca_plugin_write_values(context->plugin, table, &error))
	{
	  set_json_response(msg, 500, "{\"error\":\"write failed\"}");
	  g_clear_error(&error);
	  g_hash_table_unref(table);
	  return;
	}

      stca_context_apply_writes(context, table);
      g_hash_table_unref(table);
      soup_server_message_set_status(msg, 204, NULL);
      return;
    }

  if (g_str_has_prefix(path, "/data/")
      && (g_strcmp0(method, "PUT") == 0 || g_strcmp0(method, "POST") == 0))
    {
      const gchar *key = path + strlen("/data/");
      GError *error = NULL;
      gdouble value = 0.0;
      GHashTable *table;
      gdouble *boxed;

      if (*key == '\0')
	{
	  set_json_response(msg, 400, "{\"error\":\"missing key\"}");
	  return;
	}

      if (!parse_json_number(body_data ? body_data : "", body_len, &value, &error))
	{
	  set_json_response(msg, 400, "{\"error\":\"invalid JSON number\"}");
	  g_clear_error(&error);
	  return;
	}

      if (!stca_context_key_is_writable(context, key, &error))
	{
	  guint status = (error && error->code == G_IO_ERROR_NOT_FOUND) ? 404 : 403;
	  set_json_response(msg, status,
			    status == 404
			    ? "{\"error\":\"not found\"}"
			    : "{\"error\":\"not writable\"}");
	  g_clear_error(&error);
	  return;
	}

      if (!stca_plugin_write_value(context->plugin, key, value, &error))
	{
	  set_json_response(msg, 500, "{\"error\":\"write failed\"}");
	  g_clear_error(&error);
	  return;
	}

      table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
      boxed = g_new(gdouble, 1);
      *boxed = value;
      g_hash_table_insert(table, g_strdup(key), boxed);
      stca_context_apply_writes(context, table);
      g_hash_table_unref(table);
      soup_server_message_set_status(msg, 204, NULL);
      return;
    }

  set_json_response(msg, 404, "{\"error\":\"not found\"}");
}

gboolean
stca_http_server_start(STCAContext *context, GError **error)
{
  SoupServer *server;
  GSocketAddress *addr;
  GError *local_error = NULL;

  g_return_val_if_fail(context != NULL, FALSE);

  server = soup_server_new("server-header", "starter-configurable-adapter", NULL);

  addr = g_inet_socket_address_new_from_string(context->options.listen_address,
					       context->options.listen_port);
  if (addr == NULL)
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_INVALID_DATA,
		  "invalid listen-address `%s'",
		  context->options.listen_address);
      g_object_unref(server);
      return FALSE;
    }

  if (!soup_server_listen(server, addr, 0, &local_error))
    {
      g_propagate_error(error, local_error);
      g_object_unref(addr);
      g_object_unref(server);
      return FALSE;
    }

  g_object_unref(addr);

  soup_server_add_handler(server, NULL, handle_request, context, NULL);
  context->http_server = server;

  LOGI("HTTP server listening on %s:%u",
       context->options.listen_address,
       context->options.listen_port);
  return TRUE;
}

void
stca_http_server_stop(STCAContext *context)
{
  if (context == NULL || context->http_server == NULL)
    return;

  soup_server_disconnect(SOUP_SERVER(context->http_server));
  g_clear_object((GObject **) &context->http_server);
  LOGI("HTTP server stopped");
}

#else /* libsoup-2.4 */

static void
set_json_response2(SoupMessage *msg, guint status, const gchar *json)
{
  soup_message_set_status(msg, status);
  soup_message_set_response(msg, "application/json",
			    SOUP_MEMORY_COPY,
			    json ? json : "{}",
			    json ? strlen(json) : 2);
}

static void
handle_request2(SoupServer *server,
		SoupMessage *msg,
		const char *path,
		GHashTable *query,
		SoupClientContext *client,
		gpointer user_data)
{
  STCAContext *context = user_data;
  const char *method = msg->method;

  (void) server;
  (void) query;
  (void) client;

  if (g_strcmp0(path, "/health") == 0 && msg->method == SOUP_METHOD_GET)
    {
      set_json_response2(msg, SOUP_STATUS_OK, "{\"status\":\"ok\"}");
      return;
    }

  if (g_strcmp0(path, "/info") == 0 && msg->method == SOUP_METHOD_GET)
    {
      set_json_response2(msg, SOUP_STATUS_OK,
			 "{\"adapter-name\":\"STCA\",\"adapter-version\":\"" PACKAGE_VERSION "\"}");
      return;
    }

  if (g_strcmp0(path, "/data") == 0 && msg->method == SOUP_METHOD_GET)
    {
      gchar *json = values_to_json(context->values);
      set_json_response2(msg, SOUP_STATUS_OK, json);
      g_free(json);
      return;
    }

  if (g_str_has_prefix(path, "/data/") && msg->method == SOUP_METHOD_GET)
    {
      const gchar *key = path + strlen("/data/");
      STCAValue *v = g_hash_table_lookup(context->values, key);

      if (v == NULL)
	{
	  set_json_response2(msg, SOUP_STATUS_NOT_FOUND, "{\"error\":\"not found\"}");
	  return;
	}

      {
	gchar *json = value_to_json(v);
	set_json_response2(msg, SOUP_STATUS_OK, json);
	g_free(json);
      }
      return;
    }

  if (g_strcmp0(path, "/data") == 0
      && (msg->method == SOUP_METHOD_PUT || msg->method == SOUP_METHOD_POST))
    {
      GError *error = NULL;
      GHashTable *table;

      table = parse_json_object_numbers(msg->request_body->data,
					msg->request_body->length, &error);
      if (table == NULL)
	{
	  set_json_response2(msg, SOUP_STATUS_BAD_REQUEST, "{\"error\":\"invalid JSON object\"}");
	  g_clear_error(&error);
	  return;
	}

      if (!ensure_all_writable(context, table, &error))
	{
	  guint status = (error && error->code == G_IO_ERROR_NOT_FOUND)
	    ? SOUP_STATUS_NOT_FOUND : SOUP_STATUS_FORBIDDEN;
	  set_json_response2(msg, status,
			     status == SOUP_STATUS_NOT_FOUND
			     ? "{\"error\":\"not found\"}"
			     : "{\"error\":\"not writable\"}");
	  g_clear_error(&error);
	  g_hash_table_unref(table);
	  return;
	}

      if (!stca_plugin_write_values(context->plugin, table, &error))
	{
	  set_json_response2(msg, SOUP_STATUS_INTERNAL_SERVER_ERROR, "{\"error\":\"write failed\"}");
	  g_clear_error(&error);
	  g_hash_table_unref(table);
	  return;
	}

      stca_context_apply_writes(context, table);
      g_hash_table_unref(table);
      soup_message_set_status(msg, SOUP_STATUS_NO_CONTENT);
      return;
    }

  if (g_str_has_prefix(path, "/data/")
      && (msg->method == SOUP_METHOD_PUT || msg->method == SOUP_METHOD_POST))
    {
      const gchar *key = path + strlen("/data/");
      GError *error = NULL;
      gdouble value = 0.0;
      GHashTable *table;
      gdouble *boxed;

      if (!parse_json_number(msg->request_body->data, msg->request_body->length, &value, &error))
	{
	  set_json_response2(msg, SOUP_STATUS_BAD_REQUEST, "{\"error\":\"invalid JSON number\"}");
	  g_clear_error(&error);
	  return;
	}

      if (!stca_context_key_is_writable(context, key, &error))
	{
	  guint status = (error && error->code == G_IO_ERROR_NOT_FOUND)
	    ? SOUP_STATUS_NOT_FOUND : SOUP_STATUS_FORBIDDEN;
	  set_json_response2(msg, status,
			     status == SOUP_STATUS_NOT_FOUND
			     ? "{\"error\":\"not found\"}"
			     : "{\"error\":\"not writable\"}");
	  g_clear_error(&error);
	  return;
	}

      if (!stca_plugin_write_value(context->plugin, key, value, &error))
	{
	  set_json_response2(msg, SOUP_STATUS_INTERNAL_SERVER_ERROR, "{\"error\":\"write failed\"}");
	  g_clear_error(&error);
	  return;
	}

      table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
      boxed = g_new(gdouble, 1);
      *boxed = value;
      g_hash_table_insert(table, g_strdup(key), boxed);
      stca_context_apply_writes(context, table);
      g_hash_table_unref(table);
      soup_message_set_status(msg, SOUP_STATUS_NO_CONTENT);
      return;
    }

  (void) method;
  set_json_response2(msg, SOUP_STATUS_NOT_FOUND, "{\"error\":\"not found\"}");
}

gboolean
stca_http_server_start(STCAContext *context, GError **error)
{
  SoupServer *server;
  SoupAddress *addr;

  g_return_val_if_fail(context != NULL, FALSE);

  addr = soup_address_new(context->options.listen_address, context->options.listen_port);
  if (addr == NULL || soup_address_resolve_sync(addr, NULL) != SOUP_STATUS_OK)
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_INVALID_DATA,
		  "invalid listen-address `%s'",
		  context->options.listen_address);
      if (addr)
	g_object_unref(addr);
      return FALSE;
    }

  server = soup_server_new(SOUP_SERVER_INTERFACE, addr, NULL);
  g_object_unref(addr);

  if (server == NULL)
    {
      g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "soup_server_new failed");
      return FALSE;
    }

  soup_server_add_handler(server, NULL, handle_request2, context, NULL);
  soup_server_run_async(server);
  context->http_server = server;

  LOGI("HTTP server listening on %s:%u",
       context->options.listen_address,
       context->options.listen_port);
  return TRUE;
}

void
stca_http_server_stop(STCAContext *context)
{
  if (context == NULL || context->http_server == NULL)
    return;

  soup_server_quit(SOUP_SERVER(context->http_server));
  g_clear_object((GObject **) &context->http_server);
  LOGI("HTTP server stopped");
}

#endif
