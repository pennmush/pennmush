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
#include "cmds.h"
#include "command.h"
#include "conf.h"
#include "log.h"
#include "mymalloc.h"
#include "notify.h"
#include "plugin.h"

#include <dlfcn.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

HASHTAB plugins;

/**
 * Free the memory being used by a plugin when removing
 * it from the hashtab
 * 
 * \param ptr pointer to the struct to free
 */
void free_plugin(void *ptr) {
    PENN_PLUGIN *p = (PENN_PLUGIN *) ptr;
    mush_free(p, "penn_plugin");
}

/** 
 * Loop through all the .so files found in the
 * plugins directory, and for each one found
 * attempt to open it, check for the plugin information
 * and run the plugin.
 * 
 * The first step is to try and open a handle to the plugin,
 * if a valid handle can't be created then we ignore this plugin.
 * 
 * Second step is to get a handle to the plugin_info() function
 * found in the plugin, if the function can't be found then we
 * ignore this plugin, as it doesn't meet the requirements.
 * 
 * Third step is to get a handle to the plugin_init() function,
 * if we can't find the function then we ignore the plugin.
 * 
 * Fourth step is to keep track of the plugin we just opened so that
 * we can close it later on (or run further functions on it).
 * 
 * Final step is to actually run the plugin_init() function on the
 * plugin which will allow the function to set up anything it needs.
 *
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

  hash_init(&plugins, 1, free_plugin);

  if (NULL != (pluginsDir = opendir(options.plugins_dir))) {
    while ((in_file = readdir(pluginsDir))) {
      if (!strcmp(in_file->d_name, ".")) continue;
      if (!strcmp(in_file->d_name, "..")) continue;
      if (!strstr(in_file->d_name, ".so")) continue;

      memset(plugin_file, 0, strlen(plugin_file));
      plugin_name_return = snprintf(plugin_file, sizeof(plugin_file), "%s/%s", options.plugins_dir, in_file->d_name);
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
      plugin->file = in_file->d_name;

      do_rawlog(LT_ERR, "Plugin: %s by %s version %s", plugin->info->name, plugin->info->author, plugin->info->app_version);

      hash_add(&plugins, plugin->name, plugin);

      init_plugin();
    }
  }
}

/**
 * Loop through all currently loaded plugins and close
 * their respective handles.
 * 
 * Once we have closed all the plugin handles then we free
 * the structure that was used for keeping track of them and
 * reset the plugin_count back to 0.
 * 
 * If this is a full shutdown then none of this really matters,
 * but if it is an @shutdown/reboot then we need to make sure
 * everything is clean for when load_plugins() runs again.
 */
void unload_plugins()
{
  PENN_PLUGIN *plugin;

  for (plugin = hash_firstentry(&plugins); plugin; plugin = hash_nextentry(&plugins)) {
    if (plugin->handle) {
      dlclose(plugin->handle);
      hash_delete(&plugins, plugin->name);
    }
  }
}

/**
 * In-game command for dealing with plugins.
 * 
 * Command will deal with the following things:
 *  - active - List currently active plugins
 *  - info - Display information about an active plugin
 *  - list - List all plugins in the plugins directory
 *  - load - Load a plugin
 *  - reload - Reload an active plugin (combines unload and load together)
 *  - unload - Unload an active plugin
 * 
 * Arguments for commands:
 *  - active requires no arguments
 *  - info requires the plugin name (as found in 'list' or 'active')
 *  - list requires no arguments
 *  - load requires the plugin name (as found in 'list')
 *  - reload requires the plugin name (as found in 'list')
 *  - unload requires the plugin name (as found in 'list')
 */
COMMAND(cmd_plugin) {
    PENN_PLUGIN *plugin;
    if (SW_ISSET(sw, SWITCH_ACTIVE)) {

    } else if (SW_ISSET(sw, SWITCH_INFO)) {

    } else if (SW_ISSET(sw, SWITCH_LIST)) {
      notify_format(executor, "ID  Plugin Name                   Active? Description                         ");

      for (plugin = hash_firstentry(&plugins); plugin; plugin = hash_nextentry(&plugins)) {
        if (plugin->handle) {
          notify_format(executor, "%3d %-29s %-7s %-36s", 0, plugin->name, "YES", plugin->info->shortdesc);
        }
      }
    } else if (SW_ISSET(sw, SWITCH_LOAD)) {

    } else if (SW_ISSET(sw, SWITCH_RELOAD)) {

    } else if (SW_ISSET(sw, SWITCH_UNLOAD)) {

    } else {
        /* Probably do the same as SWITCH_INFO? */
    }
}