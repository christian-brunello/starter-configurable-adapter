/*
 * starter-configurable-adapter - http-server.h
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

#ifndef STCA_HTTP_SERVER_H_INCLUDED
#define STCA_HTTP_SERVER_H_INCLUDED

#include "internals.h"

gboolean stca_http_server_start(STCAContext *context, GError **error);
void stca_http_server_stop(STCAContext *context);

#endif /* STCA_HTTP_SERVER_H_INCLUDED */
