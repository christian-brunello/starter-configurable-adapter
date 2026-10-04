/*
 * libstarter-configurable-adapter - plugin-info.c
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

struct _STCAPluginInfo {
  GObject parent_instance;
  gchar *name;
  gchar *version;
};

G_DEFINE_TYPE(STCAPluginInfo, stca_plugin_info, G_TYPE_OBJECT)

static void
stca_plugin_info_finalize(GObject *object)
{
  STCAPluginInfo *self = STCA_PLUGIN_INFO(object);

  g_free(self->name);
  g_free(self->version);

  G_OBJECT_CLASS(stca_plugin_info_parent_class)->finalize(object);
}

static void
stca_plugin_info_class_init(STCAPluginInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->finalize = stca_plugin_info_finalize;
}

static void
stca_plugin_info_init(STCAPluginInfo *self)
{
  self->name = NULL;
  self->version = NULL;
}

STCAPluginInfo *
stca_plugin_info_new(const gchar *name, const gchar *version)
{
  STCAPluginInfo *self;

  self = g_object_new(STCA_TYPE_PLUGIN_INFO, NULL);
  self->name = g_strdup(name);
  self->version = g_strdup(version);
  return self;
}

const gchar *
stca_plugin_info_get_name(STCAPluginInfo *self)
{
  g_return_val_if_fail(STCA_IS_PLUGIN_INFO(self), NULL);
  return self->name;
}

const gchar *
stca_plugin_info_get_version(STCAPluginInfo *self)
{
  g_return_val_if_fail(STCA_IS_PLUGIN_INFO(self), NULL);
  return self->version;
}
