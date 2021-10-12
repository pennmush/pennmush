/**
 * \file plugin.c
 *
 * \brief Plugin support for PennMUSH.
 *
 * Provides plugin support with all associated commands
 * and functions necessary for the server and in-game
 *
 *
 */

#include "copyrite.h"
#include "conf.h"
#include "log.h"
#include "mymalloc.h"
#include "plugin.h"

#include <dlfcn.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

PENN_PLUGIN **plugins = NULL;
int plugin_count = 0;

/**
 * Plugin support
 */
void load_plugins() {
  typedef int plugin_init();
  typedef void *get_plugin();

  DIR *pluginsDir;
  struct dirent *in_file;
  char plugin_file[256];

  void *handle;
  plugin_init *init_plugin;
  get_plugin *info_plugin;

  PENN_PLUGIN *plugin;

  int plugin_name_return = 0;

  if (NULL != (pluginsDir = opendir("../plugins"))) {
    while ((in_file = readdir(pluginsDir))) {
      if (!strcmp(in_file->d_name, ".")) continue;
      if (!strcmp(in_file->d_name, "..")) continue;
      if (!strstr(in_file->d_name, ".so")) continue;

      memset(plugin_file, 0, strlen(plugin_file));
      plugin_name_return = snprintf(plugin_file, sizeof(plugin_file), "../plugins/%s", in_file->d_name);
      if (plugin_name_return < 0) continue;

      do_rawlog(LT_ERR, "Found plugin: %s ", plugin_file);

      handle = dlopen(plugin_file, RTLD_LAZY);
      if (handle == NULL) continue;

      do_rawlog(LT_ERR, "Opened plugin: %s", plugin_file);

      info_plugin = dlsym(handle, "get_plugin");
      if (info_plugin == NULL) {
        do_rawlog(LT_ERR, "Missing get_plugin: %s", plugin_file);
        dlclose(handle);
        continue;
      }

      init_plugin = dlsym(handle, "plugin_init");
      if (init_plugin == NULL) {
        do_rawlog(LT_ERR, "Missing plugin_init: %s", plugin_file);
        dlclose(handle);
        continue;
      }

      plugin = mush_malloc(sizeof(PENN_PLUGIN), "penn_plugin");
      plugin->handle = &handle;

      plugin->info = mush_malloc(sizeof(PLUGIN_INFO), "plugin_info");
      plugin->info = info_plugin();

      plugin->name = plugin->info->name;

      do_rawlog(LT_ERR, "Plugin: %s by %s version %s", plugin->info->name, plugin->info->author, plugin->info->app_version);

      plugin_count++;

      plugins = mush_realloc(plugins, sizeof(plugin) + sizeof(PENN_PLUGIN), "plugins");

      plugins[plugin_count] = plugin;

      init_plugin();
    }
  }
}

void unload_plugins()
{
  for (int i = 0; i < plugin_count; i++) {
    if (plugins[i]->handle) {
      dlclose(plugins[i]->handle);
    }
  }
  if (plugins != NULL) {
    mush_free(plugins, "plugins");
  }

  plugin_count = 0;
}