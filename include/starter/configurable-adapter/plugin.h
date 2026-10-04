/*
 * libstarter-configurable-adapter - plugin.h
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

#ifndef STCA_PLUGIN_H_INCLUDED
#define STCA_PLUGIN_H_INCLUDED

#include <glib-object.h>
#include <libxml/tree.h>

G_BEGIN_DECLS

/* ---- Value + access (plugin ↔ adapter) ---- */

typedef enum {
  STCA_ACCESS_READ  = 1 << 0,
  STCA_ACCESS_WRITE = 1 << 1
} STCAAccessFlags;

typedef struct _STCAValue {
  gdouble value;
  guint access; /* bitmask of STCAAccessFlags */
} STCAValue;

STCAValue *stca_value_new(gdouble value, guint access);
STCAValue *stca_value_copy(const STCAValue *src);
void stca_value_free(STCAValue *value);

/* Returns a static string: "", "r", "w", or "rw". */
const gchar *stca_access_to_string(guint access);

/* ---- Plugin module metadata ---- */

#define STCA_TYPE_PLUGIN_INFO (stca_plugin_info_get_type())
G_DECLARE_FINAL_TYPE(STCAPluginInfo, stca_plugin_info, STCA, PLUGIN_INFO, GObject)

STCAPluginInfo *stca_plugin_info_new(const gchar *name, const gchar *version);
const gchar *stca_plugin_info_get_name(STCAPluginInfo *self);
const gchar *stca_plugin_info_get_version(STCAPluginInfo *self);

typedef STCAPluginInfo *(*STCAPluginGetInfoFunc)(void);
typedef GObject *(*STCAPluginCreateFunc)(void);

/* ---- Plugin instance base class ---- */

#define STCA_TYPE_PLUGIN (stca_plugin_get_type())
G_DECLARE_DERIVABLE_TYPE(STCAPlugin, stca_plugin, STCA, PLUGIN, GObject)

struct _STCAPluginClass {
  GObjectClass parent_class;

  gboolean (*configure)(STCAPlugin *self, xmlNodePtr node, GError **error);
  gboolean (*start)(STCAPlugin *self, GError **error);
  gboolean (*stop)(STCAPlugin *self, GError **error);
  gboolean (*write_value)(STCAPlugin *self, const gchar *key, gdouble value, GError **error);
  gboolean (*write_values)(STCAPlugin *self, GHashTable *values, GError **error);

  gpointer padding[8];
};

gboolean stca_plugin_configure(STCAPlugin *plugin, xmlNodePtr node, GError **error);
gboolean stca_plugin_start(STCAPlugin *plugin, GError **error);
gboolean stca_plugin_stop(STCAPlugin *plugin, GError **error);

/*
 * Synchronous write toward the external source.
 * @values keys are gchar*, values are gdouble*.
 */
gboolean stca_plugin_write_value(STCAPlugin *plugin, const gchar *key, gdouble value, GError **error);
gboolean stca_plugin_write_values(STCAPlugin *plugin, GHashTable *values, GError **error);

/*
 * Emit signal "data-ready" with a batch of key → STCAValue*.
 * @values is transfer-none for the duration of handlers; the adapter copies it.
 */
void stca_plugin_emit_data_ready(STCAPlugin *plugin, GHashTable *values);

G_END_DECLS

#endif /* STCA_PLUGIN_H_INCLUDED */
