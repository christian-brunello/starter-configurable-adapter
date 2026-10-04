/*
 * starter-configurable-adapter - log.c
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

#include <string.h>

#include "internals.h"

static char *instance_tag = NULL;

static void
stca_log_handler(const gchar *log_domain,
		 GLogLevelFlags log_level,
		 const gchar *message,
		 gpointer user_data)
{
  GDateTime *now = g_date_time_new_now_local();
  gchar *timestamp = g_date_time_format(now, "%Y-%m-%d %H:%M:%S");
  const gchar *level_str = "UNKNOWN";
  const gchar *color_start = "";
  const gchar *color_end = "\033[0m";

  (void) user_data;

  if (log_level & G_LOG_LEVEL_ERROR)
    {
      level_str = "ERROR";
      color_start = "\033[1;31m";
    }
  else if (log_level & G_LOG_LEVEL_CRITICAL)
    {
      level_str = "CRITICAL";
      color_start = "\033[1;35m";
    }
  else if (log_level & G_LOG_LEVEL_WARNING)
    {
      level_str = "WARN";
      color_start = "\033[1;33m";
    }
  else if (log_level & G_LOG_LEVEL_MESSAGE)
    {
      level_str = "MESSAGE";
      color_start = "\033[1;32m";
    }
  else if (log_level & G_LOG_LEVEL_INFO)
    {
      level_str = "INFO";
      color_start = "\033[1;36m";
    }
  else if (log_level & G_LOG_LEVEL_DEBUG)
    {
      level_str = "DEBUG";
      color_start = "\033[0;37m";
    }

  g_printerr("[%s] %s[%-8s]%s [%s:%s] %s\n",
	     timestamp,
	     color_start, level_str, color_end,
	     instance_tag,
	     log_domain,
	     message);

  g_free(timestamp);
  g_date_time_unref(now);
}

void
stca_internals_log_init(const char *argv0)
{
  const char *base;

  base = strrchr(argv0, '/');
  instance_tag = g_strdup(base ? base + 1 : argv0);

  g_log_set_default_handler(stca_log_handler, NULL);
  g_setenv("G_MESSAGES_DEBUG", "all", FALSE);
}
