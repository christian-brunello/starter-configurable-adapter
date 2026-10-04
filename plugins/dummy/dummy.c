/*
 * starter-configurable-adapter - dummy plugin
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

#include <stdlib.h>
#include <string.h>
#include <gmodule.h>
#include <gio/gio.h>

#include "starter/configurable-adapter/plugin.h"

#define LOGI(msg...) g_log("dummy", G_LOG_LEVEL_INFO, msg)

#define STCA_TYPE_DUMMY (stca_dummy_get_type())
G_DECLARE_FINAL_TYPE(STCADummy, stca_dummy, STCA, DUMMY, STCAPlugin)

struct _STCADummy {
  STCAPlugin parent_instance;
  guint interval_ms;
  guint timer_id;
  gdouble temperature;
  gdouble humidity;
  gdouble online;
  gdouble setpoint;
};

G_DEFINE_TYPE(STCADummy, stca_dummy, STCA_TYPE_PLUGIN)

static void
dummy_insert(GHashTable *table, const gchar *key, gdouble value, guint access)
{
  g_hash_table_insert(table, g_strdup(key), stca_value_new(value, access));
}

static gboolean
dummy_tick(gpointer user_data)
{
  STCADummy *self = user_data;
  GHashTable *batch;

  self->temperature += 0.1;
  if (self->temperature > 30.0)
    self->temperature = 20.0;

  self->humidity += 0.25;
  if (self->humidity > 80.0)
    self->humidity = 40.0;

  self->online = 1.0;

  batch = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
				(GDestroyNotify) stca_value_free);
  dummy_insert(batch, "temperature", self->temperature, STCA_ACCESS_READ);
  dummy_insert(batch, "humidity", self->humidity, STCA_ACCESS_READ);
  dummy_insert(batch, "online", self->online, STCA_ACCESS_READ);
  dummy_insert(batch, "setpoint", self->setpoint,
	       STCA_ACCESS_READ | STCA_ACCESS_WRITE);

  stca_plugin_emit_data_ready(STCA_PLUGIN(self), batch);
  g_hash_table_unref(batch);
  return G_SOURCE_CONTINUE;
}

static gboolean
stca_dummy_configure(STCAPlugin *plugin, xmlNodePtr node, GError **error)
{
  STCADummy *self = STCA_DUMMY(plugin);
  xmlNodePtr child;

  (void) error;

  for (child = node->children; child != NULL; child = child->next)
    {
      if (child->type != XML_ELEMENT_NODE)
	continue;

      if (xmlStrcmp(child->name, (const xmlChar *) "interval-ms") == 0)
	{
	  xmlChar *content = xmlNodeGetContent(child);
	  long v;

	  if (content == NULL)
	    continue;

	  v = strtol((const char *) content, NULL, 10);
	  xmlFree(content);
	  if (v > 0)
	    self->interval_ms = (guint) v;
	}
    }

  LOGI("configured interval-ms=%u", self->interval_ms);
  return TRUE;
}

static gboolean
stca_dummy_start(STCAPlugin *plugin, GError **error)
{
  STCADummy *self = STCA_DUMMY(plugin);

  (void) error;

  if (self->timer_id == 0)
    self->timer_id = g_timeout_add(self->interval_ms, dummy_tick, self);

  dummy_tick(self);
  LOGI("dummy plugin started");
  return TRUE;
}

static gboolean
stca_dummy_stop(STCAPlugin *plugin, GError **error)
{
  STCADummy *self = STCA_DUMMY(plugin);

  (void) error;

  if (self->timer_id != 0)
    {
      g_source_remove(self->timer_id);
      self->timer_id = 0;
    }

  LOGI("dummy plugin stopped");
  return TRUE;
}

static gboolean
stca_dummy_write_value(STCAPlugin *plugin, const gchar *key, gdouble value, GError **error)
{
  STCADummy *self = STCA_DUMMY(plugin);

  if (g_strcmp0(key, "setpoint") != 0)
    {
      g_set_error(error,
		  G_IO_ERROR,
		  G_IO_ERROR_PERMISSION_DENIED,
		  "key `%s' is not writable", key);
      return FALSE;
    }

  self->setpoint = value;
  LOGI("write %s=%.3f accepted", key, value);
  return TRUE;
}

static void
stca_dummy_class_init(STCADummyClass *klass)
{
  STCAPluginClass *plugin_class = STCA_PLUGIN_CLASS(klass);

  plugin_class->configure = stca_dummy_configure;
  plugin_class->start = stca_dummy_start;
  plugin_class->stop = stca_dummy_stop;
  plugin_class->write_value = stca_dummy_write_value;
}

static void
stca_dummy_init(STCADummy *self)
{
  self->interval_ms = 5000;
  self->timer_id = 0;
  self->temperature = 21.5;
  self->humidity = 55.0;
  self->online = 1.0;
  self->setpoint = 22.0;
}

G_MODULE_EXPORT STCAPluginInfo *
stca_plugin_get_info(void)
{
  return stca_plugin_info_new("dummy", "1.0.0");
}

G_MODULE_EXPORT GObject *
stca_plugin_create(void)
{
  return g_object_new(STCA_TYPE_DUMMY, NULL);
}
