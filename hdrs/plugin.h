/**
 * \file plugin.h
 *
 * \brief Routines for Penn's plugin system.
 */

#ifndef __PENNPLUGIN_H
#define __PENNPLUGIN_H

extern void **plugins;
extern int plugin_count;

struct penn_plugins {
  void* handle;
  char *name;
  struct penn_plugins* next;
  struct penn_plugins* prev;
};

struct penn_plugins *plugin_head;
struct penn_plugins *plugin_last;
struct penn_plugins *plugin_curr;

#endif /* __PENN_PLUGIN_H */
