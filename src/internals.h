/*
 * starter-configurable-adapter - internals.h
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

#ifndef STCA_INTERNALS_H_INCLUDED
#define STCA_INTERNALS_H_INCLUDED

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "starter/configurable-adapter/plugin.h"

typedef struct _STCAContext STCAContext;

struct _STCAContext {
  struct {
    gchar *config_file;
    gchar *config_preprocessor;
    gchar *plugin_dir;
    gchar *listen_address;
    guint listen_port;
    gchar *adapter_name;
    gboolean version;
  } options;
  GHashTable *plugins; /* name → STCAPluginEntry* */
  gchar *plugin_name; /* selected plugin name from config */
  STCAPlugin *plugin; /* live instance */
  gulong data_ready_handler_id;
  /* Adapter-owned refreshed values: gchar* → STCAValue* */
  GHashTable *values;
  gpointer http_server; /* SoupServer*, opaque here */
};

void stca_internals_context_initialize(STCAContext *context);
void stca_internals_context_finalize(STCAContext *context);

void stca_internals_log_init(const char *instance_tag);
gboolean stca_internals_scan_plugins(STCAContext *context, GError **error);
gboolean stca_internals_config_parse(STCAContext *context, const gchar *path, GError **error);

/* Merge data-ready payload (gchar* → STCAValue*) into the adapter store. */
void stca_context_merge_values(STCAContext *context, GHashTable *values);

/* After a successful write: update numeric values only (gchar* → gdouble*). */
void stca_context_apply_writes(STCAContext *context, GHashTable *numbers);

/* TRUE if key exists and has STCA_ACCESS_WRITE. */
gboolean stca_context_key_is_writable(STCAContext *context, const gchar *key, GError **error);

#endif /* STCA_INTERNALS_H_INCLUDED */
