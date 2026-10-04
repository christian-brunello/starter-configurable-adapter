/*
 * starter-configurable-adapter - plugin-entry.c
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

#include <gmodule.h>
#include <gio/gio.h>

#include "plugin-entry.h"

struct _STCAPluginEntry {
  GObject parent_instance;
  gchar *path;
  STCAPluginInfo *info;
  GModule *module;
  STCAPluginCreateFunc create;
};

G_DEFINE_TYPE(STCAPluginEntry, stca_plugin_entry, G_TYPE_OBJECT)

static void
stca_plugin_entry_dispose(GObject *object)
{
  STCAPluginEntry *self = STCA_PLUGIN_ENTRY(object);

  self->create = NULL;

  if (self->module != NULL)
    {
      g_module_close(self->module);
      self->module = NULL;
    }

  g_clear_object(&self->info);

  G_OBJECT_CLASS(stca_plugin_entry_parent_class)->dispose(object);
}

static void
stca_plugin_entry_finalize(GObject *object)
{
  STCAPluginEntry *self = STCA_PLUGIN_ENTRY(object);

  g_free(self->path);

  G_OBJECT_CLASS(stca_plugin_entry_parent_class)->finalize(object);
}

static void
stca_plugin_entry_class_init(STCAPluginEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = stca_plugin_entry_dispose;
  object_class->finalize = stca_plugin_entry_finalize;
}

static void
stca_plugin_entry_init(STCAPluginEntry *self)
{
  self->path = NULL;
  self->info = NULL;
  self->module = NULL;
  self->create = NULL;
}

STCAPluginEntry *
stca_plugin_entry_new(const gchar *path, STCAPluginInfo *info)
{
  STCAPluginEntry *self;

  g_return_val_if_fail(path != NULL, NULL);
  g_return_val_if_fail(STCA_IS_PLUGIN_INFO(info), NULL);

  self = g_object_new(STCA_TYPE_PLUGIN_ENTRY, NULL);
  self->path = g_strdup(path);
  self->info = g_object_ref(info);
  return self;
}

STCAPluginEntry *
stca_plugin_entry_lookup(GHashTable *plugins, const gchar *name)
{
  gpointer entry;

  g_return_val_if_fail(plugins != NULL, NULL);
  g_return_val_if_fail(name != NULL, NULL);

  entry = g_hash_table_lookup(plugins, name);
  if (entry == NULL)
    return NULL;

  return STCA_PLUGIN_ENTRY(entry);
}

const gchar *
stca_plugin_entry_get_path(STCAPluginEntry *self)
{
  g_return_val_if_fail(STCA_IS_PLUGIN_ENTRY(self), NULL);
  return self->path;
}

STCAPluginInfo *
stca_plugin_entry_get_info(STCAPluginEntry *self)
{
  g_return_val_if_fail(STCA_IS_PLUGIN_ENTRY(self), NULL);
  return self->info;
}

static gboolean
stca_plugin_entry_ensure_loaded(STCAPluginEntry *self, GError **error)
{
  if (self->create != NULL)
    return TRUE;

  if (self->module == NULL)
    {
      self->module = g_module_open(self->path, G_MODULE_BIND_LOCAL);
      if (self->module == NULL)
	{
	  g_set_error(error,
		      G_IO_ERROR,
		      G_IO_ERROR_FAILED,
		      "unable to open module %s: %s",
		      self->path,
		      g_module_error());
	  return FALSE;
	}
    }

  if (!g_module_symbol(self->module, "stca_plugin_create", (gpointer *) &self->create)
      || self->create == NULL)
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_FAILED,
		  "unable to lookup symbol `stca_plugin_create' from module at %s",
		  self->path);
      self->create = NULL;
      return FALSE;
    }

  return TRUE;
}

GObject *
stca_plugin_entry_create(STCAPluginEntry *self, GError **error)
{
  GObject *object;

  g_return_val_if_fail(STCA_IS_PLUGIN_ENTRY(self), NULL);

  if (!stca_plugin_entry_ensure_loaded(self, error))
    return NULL;

  object = self->create();
  if (object == NULL)
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_FAILED,
		  "stca_plugin_create() returned NULL for module %s",
		  self->path);
      return NULL;
    }

  return object;
}
