/*
 * starter-configurable-adapter - main.c
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
#include <libxml/parser.h>

#include "internals.h"
#include "http-server.h"

#define LOGD(msg...) g_log("main", G_LOG_LEVEL_DEBUG, msg)
#define LOGI(msg...) g_log("main", G_LOG_LEVEL_INFO, msg)
#define LOGE(msg...) g_log("main", G_LOG_LEVEL_CRITICAL, msg)

#define STCA_DEFAULT_PLUGIN_DIR "/usr/local/lib/starter/configurable-adapter/plugins"

static gboolean
parse_cli(STCAContext *context, int *argc, char ***argv, GError **error)
{
  GOptionEntry cli_entries[] = {
    { "config", 'c', 0, G_OPTION_ARG_FILENAME, &context->options.config_file,
      "Path to the adapter configuration file", "FILE" },
    { "config-preprocessor", 'P', 0, G_OPTION_ARG_STRING, &context->options.config_preprocessor,
      "Command to preprocess config file ({F} = path); empty disables", "CMD" },
    { "plugin-dir", 'p', 0, G_OPTION_ARG_FILENAME, &context->options.plugin_dir,
      "Path to the plugins directory", "DIR" },
    { "version", 'V', 0, G_OPTION_ARG_NONE, &context->options.version,
      "Show software version and exit", NULL },
    { NULL }
  };

  GOptionContext *opt_context;
  gboolean r;

  opt_context = g_option_context_new("- Starter Configurable Adapter");
  g_option_context_add_main_entries(opt_context, cli_entries, NULL);
  r = g_option_context_parse(opt_context, argc, argv, error);
  g_option_context_free(opt_context);
  return r;
}

static void
load_environment(STCAContext *context)
{
  if (context->options.plugin_dir == NULL)
    {
      const gchar *env_plugin = g_getenv("STCA_PLUGIN_PATH");

      if (env_plugin && *env_plugin != '\0')
	context->options.plugin_dir = g_strdup(env_plugin);
      else
	context->options.plugin_dir = g_strdup(STCA_DEFAULT_PLUGIN_DIR);
    }

  if (context->options.config_preprocessor == NULL)
    {
      const gchar *env_prep = g_getenv("STCA_CONFIG_PREPROCESSOR");

      if (env_prep != NULL)
	context->options.config_preprocessor = g_strdup(env_prep);
    }
}

static void
on_data_ready(STCAPlugin *plugin, GHashTable *values, gpointer user_data)
{
  STCAContext *context = user_data;

  (void) plugin;
  stca_context_merge_values(context, values);
  LOGD("merged %u values from data-ready", g_hash_table_size(values));
}

static void
adapter_shutdown(STCAContext *context)
{
  GError *local = NULL;

  if (context->plugin != NULL)
    {
      if (!stca_plugin_stop(context->plugin, &local))
	{
	  LOGE("plugin stop failed: %s", local ? local->message : "unknown");
	  g_clear_error(&local);
	}
    }

  stca_http_server_stop(context);

  if (context->plugin != NULL && context->data_ready_handler_id != 0)
    {
      g_signal_handler_disconnect(context->plugin, context->data_ready_handler_id);
      context->data_ready_handler_id = 0;
    }
}

int
main(int argc, char *argv[])
{
  GError *error = NULL;
  STCAContext context;
  GMainLoop *loop;

  LOGD("Booting new instance of starter-configurable-adapter...");

  stca_internals_context_initialize(&context);
  stca_internals_log_init(argv[0]);

  if (!parse_cli(&context, &argc, &argv, &error))
    {
      LOGE("error parsing command line options: %s", error->message);
      g_clear_error(&error);
      stca_internals_context_finalize(&context);
      return EXIT_FAILURE;
    }

  if (context.options.version)
    {
      g_print("Starter Configurable Adapter v1.0.0\n");
      stca_internals_context_finalize(&context);
      return EXIT_SUCCESS;
    }

  load_environment(&context);

  LOGD("configuration file: %s", context.options.config_file);
  LOGD("plugins directory: %s", context.options.plugin_dir);

  if (context.options.config_file == NULL || *context.options.config_file == '\0')
    {
      LOGE("configuration file is required (--config)");
      stca_internals_context_finalize(&context);
      return EXIT_FAILURE;
    }

  LIBXML_TEST_VERSION

  LOGI("scanning plugins");
  if (!stca_internals_scan_plugins(&context, &error))
    {
      LOGE("plugin scan failed: %s", error ? error->message : "unknown");
      g_clear_error(&error);
      stca_internals_context_finalize(&context);
      return EXIT_FAILURE;
    }

  LOGI("parsing configuration %s", context.options.config_file);
  if (!stca_internals_config_parse(&context, context.options.config_file, &error))
    {
      LOGE("config parse failed: %s", error ? error->message : "unknown");
      g_clear_error(&error);
      stca_internals_context_finalize(&context);
      return EXIT_FAILURE;
    }

  context.data_ready_handler_id =
    g_signal_connect(context.plugin, "data-ready",
		     G_CALLBACK(on_data_ready), &context);

  if (!stca_http_server_start(&context, &error))
    {
      LOGE("HTTP server start failed: %s", error ? error->message : "unknown");
      g_clear_error(&error);
      adapter_shutdown(&context);
      stca_internals_context_finalize(&context);
      return EXIT_FAILURE;
    }

  if (!stca_plugin_start(context.plugin, &error))
    {
      LOGE("plugin start failed: %s", error ? error->message : "unknown");
      g_clear_error(&error);
      adapter_shutdown(&context);
      stca_internals_context_finalize(&context);
      return EXIT_FAILURE;
    }

  LOGI("adapter online (plugin=%s)", context.plugin_name);

  loop = g_main_loop_new(NULL, FALSE);
  g_main_loop_run(loop);

  adapter_shutdown(&context);
  xmlCleanupParser();
  g_main_loop_unref(loop);
  stca_internals_context_finalize(&context);
  return 0;
}
