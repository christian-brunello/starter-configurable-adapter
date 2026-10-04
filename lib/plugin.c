/*
 * libstarter-configurable-adapter - plugin.c
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "starter/configurable-adapter/plugin.h"

#include <gio/gio.h>

enum {
  SIGNAL_DATA_READY,
  N_SIGNALS
};

static guint plugin_signals[N_SIGNALS];

STCAValue *
stca_value_new(gdouble value, guint access)
{
  STCAValue *v = g_new(STCAValue, 1);

  v->value = value;
  v->access = access;
  return v;
}

STCAValue *
stca_value_copy(const STCAValue *src)
{
  g_return_val_if_fail(src != NULL, NULL);
  return stca_value_new(src->value, src->access);
}

void
stca_value_free(STCAValue *value)
{
  g_free(value);
}

const gchar *
stca_access_to_string(guint access)
{
  gboolean r = (access & STCA_ACCESS_READ) != 0;
  gboolean w = (access & STCA_ACCESS_WRITE) != 0;

  if (r && w)
    return "rw";
  if (r)
    return "r";
  if (w)
    return "w";
  return "";
}

G_DEFINE_ABSTRACT_TYPE(STCAPlugin, stca_plugin, G_TYPE_OBJECT)

static gboolean
stca_plugin_real_configure(STCAPlugin *self, xmlNodePtr node, GError **error)
{
  (void) self;
  (void) node;

  g_set_error(error,
	      G_IO_ERROR,
	      G_IO_ERROR_NOT_SUPPORTED,
	      "configure() not implemented for this plugin");
  return FALSE;
}

static gboolean
stca_plugin_real_start(STCAPlugin *self, GError **error)
{
  (void) self;
  (void) error;
  return TRUE;
}

static gboolean
stca_plugin_real_stop(STCAPlugin *self, GError **error)
{
  (void) self;
  (void) error;
  return TRUE;
}

static gboolean
stca_plugin_real_write_value(STCAPlugin *self, const gchar *key, gdouble value, GError **error)
{
  (void) self;
  (void) key;
  (void) value;

  g_set_error(error,
	      G_IO_ERROR,
	      G_IO_ERROR_NOT_SUPPORTED,
	      "write_value() not implemented for this plugin");
  return FALSE;
}

static gboolean
stca_plugin_real_write_values(STCAPlugin *self, GHashTable *values, GError **error)
{
  STCAPluginClass *klass;
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_return_val_if_fail(STCA_IS_PLUGIN(self), FALSE);
  g_return_val_if_fail(values != NULL, FALSE);

  klass = STCA_PLUGIN_GET_CLASS(self);
  g_return_val_if_fail(klass->write_value != NULL, FALSE);

  g_hash_table_iter_init(&iter, values);
  while (g_hash_table_iter_next(&iter, &key, &value))
    {
      const gchar *k = key;
      const gdouble *v = value;

      if (k == NULL || v == NULL)
	{
	  g_set_error(error,
		      G_IO_ERROR,
		      G_IO_ERROR_INVALID_DATA,
		      "write_values entry missing key or value");
	  return FALSE;
	}

      if (!klass->write_value(self, k, *v, error))
	return FALSE;
    }

  return TRUE;
}

static void
stca_plugin_class_init(STCAPluginClass *klass)
{
  klass->configure = stca_plugin_real_configure;
  klass->start = stca_plugin_real_start;
  klass->stop = stca_plugin_real_stop;
  klass->write_value = stca_plugin_real_write_value;
  klass->write_values = stca_plugin_real_write_values;

  /**
   * STCAPlugin::data-ready:
   * @plugin: the plugin
   * @values: (transfer none) (element-type utf8 STCAValue): key → STCAValue* table
   *
   * Emitted when the plugin has a fresh batch of values from the source.
   * The adapter copies @values into its own store during the handler.
   */
  plugin_signals[SIGNAL_DATA_READY] =
    g_signal_new("data-ready",
		 G_TYPE_FROM_CLASS(klass),
		 G_SIGNAL_RUN_LAST,
		 0,
		 NULL, NULL, NULL,
		 G_TYPE_NONE, 1,
		 G_TYPE_HASH_TABLE);
}

static void
stca_plugin_init(STCAPlugin *self)
{
  (void) self;
}

gboolean
stca_plugin_configure(STCAPlugin *plugin, xmlNodePtr node, GError **error)
{
  STCAPluginClass *klass;

  g_return_val_if_fail(STCA_IS_PLUGIN(plugin), FALSE);

  klass = STCA_PLUGIN_GET_CLASS(plugin);
  g_return_val_if_fail(klass->configure != NULL, FALSE);
  return klass->configure(plugin, node, error);
}

gboolean
stca_plugin_start(STCAPlugin *plugin, GError **error)
{
  STCAPluginClass *klass;

  g_return_val_if_fail(STCA_IS_PLUGIN(plugin), FALSE);

  klass = STCA_PLUGIN_GET_CLASS(plugin);
  g_return_val_if_fail(klass->start != NULL, FALSE);
  return klass->start(plugin, error);
}

gboolean
stca_plugin_stop(STCAPlugin *plugin, GError **error)
{
  STCAPluginClass *klass;

  g_return_val_if_fail(STCA_IS_PLUGIN(plugin), FALSE);

  klass = STCA_PLUGIN_GET_CLASS(plugin);
  g_return_val_if_fail(klass->stop != NULL, FALSE);
  return klass->stop(plugin, error);
}

gboolean
stca_plugin_write_value(STCAPlugin *plugin, const gchar *key, gdouble value, GError **error)
{
  STCAPluginClass *klass;

  g_return_val_if_fail(STCA_IS_PLUGIN(plugin), FALSE);
  g_return_val_if_fail(key != NULL, FALSE);

  klass = STCA_PLUGIN_GET_CLASS(plugin);
  g_return_val_if_fail(klass->write_value != NULL, FALSE);
  return klass->write_value(plugin, key, value, error);
}

gboolean
stca_plugin_write_values(STCAPlugin *plugin, GHashTable *values, GError **error)
{
  STCAPluginClass *klass;

  g_return_val_if_fail(STCA_IS_PLUGIN(plugin), FALSE);
  g_return_val_if_fail(values != NULL, FALSE);

  klass = STCA_PLUGIN_GET_CLASS(plugin);
  g_return_val_if_fail(klass->write_values != NULL, FALSE);
  return klass->write_values(plugin, values, error);
}

void
stca_plugin_emit_data_ready(STCAPlugin *plugin, GHashTable *values)
{
  g_return_if_fail(STCA_IS_PLUGIN(plugin));
  g_return_if_fail(values != NULL);

  g_signal_emit(plugin, plugin_signals[SIGNAL_DATA_READY], 0, values);
}
