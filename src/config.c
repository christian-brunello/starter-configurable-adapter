/*
 * starter-configurable-adapter - config.c
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

#include "internals.h"
#include "plugin-entry.h"

#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "autoconf.h"

#define LOGD(msg...) g_log("config", G_LOG_LEVEL_DEBUG, msg)
#define LOGI(msg...) g_log("config", G_LOG_LEVEL_INFO, msg)

#ifndef HAVE_G_STRING_REPLACE
static void
stca_string_replace(GString *string, const gchar *find, const gchar *replace)
{
  const gchar *p;
  gsize find_len;
  gssize pos = 0;

  g_return_if_fail(string != NULL);
  g_return_if_fail(find != NULL && *find != '\0');
  g_return_if_fail(replace != NULL);

  find_len = strlen(find);
  while ((p = strstr(string->str + pos, find)) != NULL)
    {
      gsize offset = (gsize) (p - string->str);
      g_string_erase(string, (gssize) offset, (gssize) find_len);
      g_string_insert(string, (gssize) offset, replace);
      pos = (gssize) (offset + strlen(replace));
    }
}
#endif

static gchar *
stca_config_run_preprocessor(const gchar *preprocessor,
			     const gchar *path,
			     GError **error)
{
  GString *cmd;
  gint spawn_argc = 0;
  gchar **spawn_argv = NULL;
  gchar *stdout_buf = NULL;
  gchar *stderr_buf = NULL;
  gint exit_status = 0;
  GError *local_error = NULL;

  cmd = g_string_new(preprocessor);
#ifdef HAVE_G_STRING_REPLACE
  g_string_replace(cmd, "{F}", path, 0);
#else
  stca_string_replace(cmd, "{F}", path);
#endif

  LOGD("preprocessor command: `%s'", cmd->str);

  if (!g_shell_parse_argv(cmd->str, &spawn_argc, &spawn_argv, &local_error))
    {
      g_propagate_error(error, local_error);
      g_string_free(cmd, TRUE);
      return NULL;
    }

  g_string_free(cmd, TRUE);

  if (!g_spawn_sync(NULL, spawn_argv, NULL,
		    G_SPAWN_SEARCH_PATH,
		    NULL, NULL,
		    &stdout_buf, &stderr_buf, &exit_status, &local_error))
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_FAILED,
		  "failed to run config preprocessor: %s",
		  local_error ? local_error->message : "unknown error");
      g_clear_error(&local_error);
      g_strfreev(spawn_argv);
      return NULL;
    }

  g_strfreev(spawn_argv);

  if (exit_status != 0)
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_FAILED,
		  "config preprocessor exited with status %d%s%s",
		  exit_status,
		  (stderr_buf && *stderr_buf) ? ": " : "",
		  (stderr_buf && *stderr_buf) ? stderr_buf : "");
      g_free(stdout_buf);
      g_free(stderr_buf);
      return NULL;
    }

  g_free(stderr_buf);

  if (stdout_buf == NULL || *stdout_buf == '\0')
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_FAILED,
		  "config preprocessor produced empty output");
      g_free(stdout_buf);
      return NULL;
    }

  return stdout_buf;
}

static gchar *
xml_child_content(xmlNodePtr parent, const char *name)
{
  xmlNodePtr child;

  for (child = parent->children; child != NULL; child = child->next)
    {
      if (child->type != XML_ELEMENT_NODE)
	continue;
      if (xmlStrcmp(child->name, (const xmlChar *) name) != 0)
	continue;
      return (gchar *) xmlNodeGetContent(child);
    }

  return NULL;
}

static gboolean
parse_global(STCAContext *context, xmlNodePtr node, GError **error)
{
  gchar *tmp;

  tmp = xml_child_content(node, "listen-address");
  if (tmp != NULL)
    {
      g_free(context->options.listen_address);
      context->options.listen_address = tmp;
    }

  tmp = xml_child_content(node, "listen-port");
  if (tmp != NULL)
    {
      char *end = NULL;
      long port = strtol(tmp, &end, 10);

      if (end == tmp || *end != '\0' || port <= 0 || port > 65535)
	{
	  g_set_error(error,
		      G_IO_ERROR,
		      G_IO_ERROR_INVALID_DATA,
		      "invalid listen-port `%s'", tmp);
	  g_free(tmp);
	  return FALSE;
	}

      context->options.listen_port = (guint) port;
      g_free(tmp);
    }

  tmp = xml_child_content(node, "adapter-name");
  if (tmp != NULL)
    {
      g_free(context->options.adapter_name);
      context->options.adapter_name = tmp;
    }

  return TRUE;
}

static gboolean
parse_plugin(STCAContext *context, xmlNodePtr node, GError **error)
{
  xmlChar *name_attr;
  STCAPluginEntry *entry;
  GObject *object;

  name_attr = xmlGetProp(node, (const xmlChar *) "name");
  if (name_attr == NULL || *name_attr == '\0')
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_INVALID_DATA,
		  "<plugin> requires name attribute");
      if (name_attr)
	xmlFree(name_attr);
      return FALSE;
    }

  if (context->plugin != NULL)
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_EXISTS,
		  "only one <plugin> element is allowed");
      xmlFree(name_attr);
      return FALSE;
    }

  entry = stca_plugin_entry_lookup(context->plugins, (const gchar *) name_attr);
  if (entry == NULL)
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_NOT_FOUND,
		  "plugin `%s' not found", (const gchar *) name_attr);
      xmlFree(name_attr);
      return FALSE;
    }

  object = stca_plugin_entry_create(entry, error);
  if (object == NULL)
    {
      xmlFree(name_attr);
      return FALSE;
    }

  if (!STCA_IS_PLUGIN(object))
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_INVALID_DATA,
		  "plugin `%s' did not create an STCAPlugin",
		  (const gchar *) name_attr);
      g_object_unref(object);
      xmlFree(name_attr);
      return FALSE;
    }

  if (!stca_plugin_configure(STCA_PLUGIN(object), node, error))
    {
      g_object_unref(object);
      xmlFree(name_attr);
      return FALSE;
    }

  context->plugin_name = g_strdup((const gchar *) name_attr);
  context->plugin = STCA_PLUGIN(object);
  xmlFree(name_attr);

  LOGI("configured plugin `%s'", context->plugin_name);
  return TRUE;
}

gboolean
stca_internals_config_parse(STCAContext *context, const gchar *path, GError **error)
{
  const gchar *preprocessor;
  gchar *preprocessed = NULL;
  xmlDocPtr doc = NULL;
  xmlNodePtr root;
  xmlNodePtr child;
  gboolean ok = FALSE;

  g_return_val_if_fail(context != NULL, FALSE);
  g_return_val_if_fail(path != NULL, FALSE);

  preprocessor = context->options.config_preprocessor;
  LOGD("parsing config file: %s (preprocessor: %s)",
       path,
       (preprocessor && *preprocessor) ? preprocessor : "(none)");

  if (preprocessor != NULL && *preprocessor != '\0')
    {
      preprocessed = stca_config_run_preprocessor(preprocessor, path, error);
      if (preprocessed == NULL)
	goto out;

      doc = xmlReadMemory(preprocessed, (int) strlen(preprocessed), path, NULL, 0);
      if (doc == NULL)
	{
	  g_set_error(error,
		      G_IO_ERROR,
		      G_IO_ERROR_FAILED,
		      "failed to parse preprocessed XML from %s", path);
	  goto out;
	}
    }
  else
    {
      doc = xmlReadFile(path, NULL, 0);
      if (doc == NULL)
	{
	  g_set_error(error,
		      G_IO_ERROR,
		      G_IO_ERROR_FAILED,
		      "failed to parse XML file %s", path);
	  goto out;
	}
    }

  root = xmlDocGetRootElement(doc);
  if (root == NULL || xmlStrcmp(root->name, (const xmlChar *) "stca") != 0)
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_INVALID_DATA,
		  "root element must be <stca>");
      goto out;
    }

  for (child = root->children; child != NULL; child = child->next)
    {
      if (child->type != XML_ELEMENT_NODE)
	continue;

      if (xmlStrcmp(child->name, (const xmlChar *) "global") == 0)
	{
	  if (!parse_global(context, child, error))
	    goto out;
	}
      else if (xmlStrcmp(child->name, (const xmlChar *) "plugin") == 0)
	{
	  if (!parse_plugin(context, child, error))
	    goto out;
	}
      else
	{
	  g_set_error(error,
		      G_IO_ERROR,
		      G_IO_ERROR_INVALID_DATA,
		      "unknown <stca> child <%s>", child->name);
	  goto out;
	}
    }

  if (context->plugin == NULL)
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_INVALID_DATA,
		  "configuration must contain a <plugin> element");
      goto out;
    }

  if (context->options.listen_address == NULL)
    context->options.listen_address = g_strdup("127.0.0.1");

  if (context->options.adapter_name == NULL)
    context->options.adapter_name = g_strdup("STCA");

  ok = TRUE;

out:
  g_free(preprocessed);
  if (doc != NULL)
    xmlFreeDoc(doc);
  return ok;
}
