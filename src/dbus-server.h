/*
 * starter-configurable-adapter - dbus-server.h
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

#ifndef STCA_DBUS_SERVER_H_INCLUDED
#define STCA_DBUS_SERVER_H_INCLUDED

#include "internals.h"

gboolean stca_dbus_server_start(STCAContext *context, GError **error);
void stca_dbus_server_stop(STCAContext *context);

/*
 * Emit one DataChanged to all connected peers with @changed
 * (gchar* → STCAValue*). No-op if @changed is NULL or empty.
 */
void stca_dbus_server_emit_data_changed(STCAContext *context, GHashTable *changed);

#endif /* STCA_DBUS_SERVER_H_INCLUDED */
