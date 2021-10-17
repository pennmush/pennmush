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
  int i = 1;

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

        plugin->name = mush_malloc(sizeof(char *), "plugin_name");
      plugin->name = plugin->info->name;
      plugin->file = in_file->d_name;
      plugin->id = i;

        i++;

      do_rawlog(LT_ERR, "Plugin: %s by %s version %s", plugin->info->name, plugin->info->author, plugin->info->app_version);

      hash_add(&plugins, in_file->d_name, plugin);

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
    int plugin_id;

    if (SW_ISSET(sw, SWITCH_ACTIVE)) {

    } else if (SW_ISSET(sw, SWITCH_INFO)) {
        if (sscanf(arg_left, "%d", &plugin_id) != 1) {
            notify(executor, T("Invalid plugin id!"));
        } else {
            show_plugin_info(executor, plugin_id);
        }
    } else if (SW_ISSET(sw, SWITCH_LIST)) {
      do_list_plugins(executor, sw);
    } else if (SW_ISSET(sw, SWITCH_LOAD)) {

    } else if (SW_ISSET(sw, SWITCH_RELOAD)) {
        if (sscanf(arg_left, "%d", &plugin_id) != 1) {
            notify(executor, T("Invalid plugin id!"));
        } else {
            do_reload_plugin(executor, plugin_id);
        }
    } else if (SW_ISSET(sw, SWITCH_UNLOAD)) {

    } else {
        /* Probably do the same as SWITCH_INFO? */
    }
}

/**
 * Get the plugin by its id.
 *
 * \param id The id of the plugin
 * \return plugin The plugin found or null if it can't be found
 */
PENN_PLUGIN* get_plugin_by_id(int id) {
    PENN_PLUGIN *plugin = NULL;

    if (id == 0) return NULL; /* ID of 0 means the plugin hasn't been loaded into penn */

    for (plugin = hash_firstentry(&plugins); plugin; plugin = hash_nextentry(&plugins)) {
        if ( plugin->id == id ) {
            break;
        }
    }

    return plugin;
}

/**
 * Free the memory being used by a plugin when removing
 * it from the hashtab
 * 
 * \param executor Who ran the @plugin command
 * \param sw The switch that was used with @plugin
 */
void do_list_plugins(dbref executor, switch_mask sw) {
    PENN_PLUGIN *plugin;
    DIR *pluginsDir;
    struct dirent *in_file;

    notify_format(executor, "ID Plugin Name                   Active? Description                          ");

    if (NULL != (pluginsDir = opendir(options.plugins_dir))) {
        while ((in_file = readdir(pluginsDir))) {
            if (!strcmp(in_file->d_name, ".")) continue;
            if (!strcmp(in_file->d_name, "..")) continue;
            if (!strstr(in_file->d_name, ".so")) continue;

            plugin = hashfind(in_file->d_name, &plugins);
            if (plugin && plugin->handle) {
                notify_format(executor, "%2d %-29s %-7s %-37s", plugin->id, plugin->name, "YES", plugin->info->shortdesc);
            } else {
                notify_format(executor, "%2d %-29s %-7s %-37s", 0, in_file->d_name, "NO", "");
            }
        }
    }
}

/**
 * Display the information for a particular plugin by id
 *
 * \param executor Who ran the @plugin command
 * \param id The id of the plugin as found in @plugin/list
 */
void show_plugin_info(dbref executor, int id) {
    PENN_PLUGIN *plugin = get_plugin_by_id(id);

    if (!plugin) { notify(executor, T("No plugin found!")); return; }
    if (!plugin->info) { notify(executor, T("Plugin has no information associated with it!")); return; }

    if (plugin && plugin->info) {
        notify_format(executor, "%13s %-65s", "Name:", plugin->info->name);
        notify_format(executor, "%13s %-65s", "Version:", plugin->info->app_version);
        notify_format(executor, "%13s %-65s", "Author:", plugin->info->author);
        notify_format(executor, "%13s %-65s", "Description:", plugin->info->description);
        notify_format(executor, "%13s %-65s", "File:", plugin->file);
    }
}

/**
 * Reload an already loaded plugin. Can be because of changes
 * made to the plugin itself, or because the plugin was unloaded
 * in order to disable it.
 * 
 * \param executor Who ran the @plugin command
 * \param id The id of the plugin as found in @plugin/list
 */
void do_reload_plugin(dbref executor, int id) {
    PENN_PLUGIN *plugin = get_plugin_by_id(id);

    if (!plugin) { notify(executor, T("No plugin found!")); return; }
    if (!plugin->info) { notify(executor, T("Plugin has no information associated with it!")); return; }

    /* Close the handle to the plugin and delete it from the hashtab */
    dlclose(plugin->handle);
    hash_delete(&plugins, plugin->name);

    /* Open a new handle to the plugin and add it to the hashtab */
    
}