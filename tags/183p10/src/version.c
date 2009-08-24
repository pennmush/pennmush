/**
 * \file version.c
 *
 * \brief Version.
 *
 * \verbatim
 * This file defines the @version command. It's all by itself because
 * we want to rebuild this file at every compilation, so that the
 * BUILDDATE is correct
 * \endverbatim
 */
#include "config.h"
#include "copyrite.h"
#include "conf.h"
#include "externs.h"
#include "version.h"
#include "patches.h"
#ifndef WIN32
#include "buildinf.h"
#endif
#include "confmagic.h"

void do_version(dbref player);

/** The version command.
 * \param player the enactor.
 */
void
do_version(dbref player)
{

  notify_format(player, T("You are connected to %s"), MUDNAME);
  notify_format(player, T("Last restarted: %s"),
                show_time(globals.start_time, 0));
  notify_format(player, "PennMUSH version %s patchlevel %s %s", VERSION,
                PATCHLEVEL, PATCHDATE);
#ifdef PATCHES
  notify_format(player, "Patches: %s", PATCHES);
#endif
#ifdef WIN32
  notify_format(player, T("Build date: %s"), __DATE__);
#else
  notify_format(player, T("Build date: %s"), BUILDDATE);
  notify_format(player, T("Compiler: %s"), COMPILER);
  notify_format(player, T("Compilation flags: %s"), CCFLAGS);
#endif
}
