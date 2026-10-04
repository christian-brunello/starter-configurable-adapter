/*
 * starter-configurable-adapter - plugin-entry.h
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

#ifndef STCA_PLUGIN_ENTRY_H_INCLUDED
#define STCA_PLUGIN_ENTRY_H_INCLUDED

#include <glib.h>
#include <glib-object.h>

#include "starter/configurable-adapter/plugin.h"

G_BEGIN_DECLS

#define STCA_TYPE_PLUGIN_ENTRY (stca_plugin_entry_get_type())
G_DECLARE_FINAL_TYPE(STCAPluginEntry, stca_plugin_entry, STCA, PLUGIN_ENTRY, GObject)

STCAPluginEntry *stca_plugin_entry_new(const gchar *path, STCAPluginInfo *info);
STCAPluginEntry *stca_plugin_entry_lookup(GHashTable *plugins, const gchar *name);

const gchar *stca_plugin_entry_get_path(STCAPluginEntry *self);
STCAPluginInfo *stca_plugin_entry_get_info(STCAPluginEntry *self);

GObject *stca_plugin_entry_create(STCAPluginEntry *self, GError **error);

G_END_DECLS

#endif /* STCA_PLUGIN_ENTRY_H_INCLUDED */
