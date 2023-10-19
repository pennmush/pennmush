/**
 * \file plugin.h
 *
 * \brief Routines for Penn's plugin system.
 */

#ifndef __PENNPLUGIN_H
#define __PENNPLUGIN_H

typedef struct plugin_info {
  char *name;
  char *author;
  char *app_version;
  int version_id;
  char shortdesc[30];
  char description[BUFFER_LEN];
} PLUGIN_INFO;

typedef struct penn_plugin {
  void* handle;
  char *name;
  char file[256];
  int id;
  PLUGIN_INFO *info;
} PENN_PLUGIN;

extern HASHTAB plugins;

extern void load_plugins();
extern void unload_plugins();

#endif /* __PENN_PLUGIN_H */
