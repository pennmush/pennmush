/**
 * \file plugin.h
 *
 * \brief Routines for Penn's plugin system.
 */

#ifndef __PENNPLUGIN_H
#define __PENNPLUGIN_H

//extern void **plugins;
extern int plugin_count;

typedef struct plugin_info {
  char *name;
  char *author;
  char *app_version;
  int version_id;
  int author_id;
} PLUGIN_INFO;

typedef struct penn_plugin {
  void* handle;
  char *name;
  PLUGIN_INFO *info;
} PENN_PLUGIN;

extern PENN_PLUGIN **plugins;

extern void load_plugins();
extern void unload_plugins();

#endif /* __PENN_PLUGIN_H */
