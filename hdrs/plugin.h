/**
 * \file plugin.h
 *
 * \brief Routines for Penn's plugin system.
 */

#ifndef __PENNPLUGIN_H
#define __PENNPLUGIN_H

//extern void **plugins;
int plugin_count = 0;

typedef struct penn_plugin {
  void* handle;
  char *name;
} PENN_PLUGIN;

PENN_PLUGIN **plugins = NULL;

#endif /* __PENN_PLUGIN_H */
