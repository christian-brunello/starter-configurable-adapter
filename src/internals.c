/*
 * starter-configurable-adapter - internals.c
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

#include <string.h>
#include <gmodule.h>

#define LOGD(msg...) g_log("plugin", G_LOG_LEVEL_DEBUG, msg)
#define LOGI(msg...) g_log("plugin", G_LOG_LEVEL_INFO, msg)
#define LOGE(msg...) g_log("plugin", G_LOG_LEVEL_CRITICAL, msg)

void
stca_context_merge_values(STCAContext *context, GHashTable *values)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_return_if_fail(context != NULL);
  g_return_if_fail(context->values != NULL);
  g_return_if_fail(values != NULL);

  g_hash_table_iter_init(&iter, values);
  while (g_hash_table_iter_next(&iter, &key, &value))
    {
      const gchar *k = key;
      const STCAValue *v = value;

      if (k == NULL || v == NULL)
	continue;

      g_hash_table_replace(context->values, g_strdup(k), stca_value_copy(v));
    }
}

void
stca_context_apply_writes(STCAContext *context, GHashTable *numbers)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_return_if_fail(context != NULL);
  g_return_if_fail(context->values != NULL);
  g_return_if_fail(numbers != NULL);

  g_hash_table_iter_init(&iter, numbers);
  while (g_hash_table_iter_next(&iter, &key, &value))
    {
      const gchar *k = key;
      const gdouble *n = value;
      STCAValue *stored;

      if (k == NULL || n == NULL)
	continue;

      stored = g_hash_table_lookup(context->values, k);
      if (stored != NULL)
	stored->value = *n;
    }
}

gboolean
stca_context_key_is_writable(STCAContext *context, const gchar *key, GError **error)
{
  STCAValue *stored;

  g_return_val_if_fail(context != NULL, FALSE);
  g_return_val_if_fail(key != NULL, FALSE);

  stored = g_hash_table_lookup(context->values, key);
  if (stored == NULL)
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_NOT_FOUND,
		  "unknown key `%s'", key);
      return FALSE;
    }

  if ((stored->access & STCA_ACCESS_WRITE) == 0)
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_PERMISSION_DENIED,
		  "key `%s' is not writable", key);
      return FALSE;
    }

  return TRUE;
}

void
stca_internals_context_initialize(STCAContext *context)
{
  memset(context, 0x00, sizeof(STCAContext));

  context->options.listen_port = 8080;
  context->plugins = g_hash_table_new_full(g_str_hash,
					   g_str_equal,
					   g_free,
					   g_object_unref);
  context->values = g_hash_table_new_full(g_str_hash,
					  g_str_equal,
					  g_free,
					  (GDestroyNotify) stca_value_free);
}

void
stca_internals_context_finalize(STCAContext *context)
{
  if (context->plugin != NULL && context->data_ready_handler_id != 0)
    {
      g_signal_handler_disconnect(context->plugin, context->data_ready_handler_id);
      context->data_ready_handler_id = 0;
    }

  g_clear_object(&context->plugin);
  g_free(context->plugin_name);

  g_free(context->options.config_file);
  g_free(context->options.config_preprocessor);
  g_free(context->options.plugin_dir);
  g_free(context->options.listen_address);
  g_free(context->options.adapter_name);

  if (context->values != NULL)
    g_hash_table_unref(context->values);

  if (context->plugins != NULL)
    g_hash_table_unref(context->plugins);
}

gboolean
stca_internals_scan_plugins(STCAContext *context, GError **error)
{
  gboolean r = FALSE;
  GDir *dir = NULL;
  const char *filename = NULL;
  GModule *module = NULL;

  LOGD("scan plugins in directory %s", context->options.plugin_dir);

  if ((dir = g_dir_open(context->options.plugin_dir, 0, error)) == NULL)
    goto out;

  while ((filename = g_dir_read_name(dir)) != NULL)
    {
      STCAPluginGetInfoFunc func = NULL;
      gchar *full_path;

      if (!g_str_has_suffix(filename, "." G_MODULE_SUFFIX))
	{
	  LOGD("skipping `%s'", filename);
	  continue;
	}

      LOGD("managing `%s'", filename);

      full_path = g_build_filename(context->options.plugin_dir, filename, NULL);

      if ((module = g_module_open(full_path, G_MODULE_BIND_LOCAL)) == NULL)
	{
	  g_set_error(error, G_IO_ERROR,
		      G_IO_ERROR_FAILED,
		      "unable to open module %s: %s",
		      full_path, g_module_error());
	  g_free(full_path);
	  goto out;
	}

      if (g_module_symbol(module, "stca_plugin_get_info", (gpointer *) &func) && func != NULL)
	{
	  STCAPluginInfo *info = func();

	  if (info != NULL)
	    {
	      const gchar *name = stca_plugin_info_get_name(info);
	      STCAPluginEntry *entry;

	      if (name == NULL || *name == '\0')
		{
		  g_set_error(error,
			      G_IO_ERROR,
			      G_IO_ERROR_INVALID_DATA,
			      "plugin at %s has empty name", full_path);
		  g_object_unref(info);
		  g_module_close(module);
		  g_free(full_path);
		  goto out;
		}

	      if (stca_plugin_entry_lookup(context->plugins, name) != NULL)
		{
		  g_set_error(error,
			      G_IO_ERROR,
			      G_IO_ERROR_EXISTS,
			      "duplicate plugin name `%s' (module %s)",
			      name, full_path);
		  g_object_unref(info);
		  g_module_close(module);
		  g_free(full_path);
		  goto out;
		}

	      entry = stca_plugin_entry_new(full_path, info);
	      g_object_unref(info);

	      LOGI("found plugin %s: Name: %s, Version: %s",
		   stca_plugin_entry_get_path(entry),
		   stca_plugin_info_get_name(stca_plugin_entry_get_info(entry)),
		   stca_plugin_info_get_version(stca_plugin_entry_get_info(entry)));

	      g_hash_table_insert(context->plugins, g_strdup(name), entry);
	    }
	}
      else
	{
	  g_set_error(error,
		      G_IO_ERROR,
		      G_IO_ERROR_FAILED,
		      "unable to lookup symbol `stca_plugin_get_info' from module at %s",
		      full_path);
	  g_module_close(module);
	  g_free(full_path);
	  goto out;
	}

      g_module_close(module);
      g_free(full_path);
    }

  r = TRUE;

  LOGD("done scanning plugins in directory %s. Registered: %u",
       context->options.plugin_dir,
       g_hash_table_size(context->plugins));

out:
  if (dir)
    g_dir_close(dir);

  return r;
}
