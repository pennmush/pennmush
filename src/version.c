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

#include "copyrite.h"
#include "version.h"

#include "conf.h"
#include "notify.h"
#include "strutil.h"
#include "gitinfo.h"

#ifndef WIN32
#include "buildinf.h"
#endif

void do_version(dbref player);

/** The version command.
 * \param player the enactor.
 */
void
do_version(dbref player)
{
  notify_format(player, T("You are connected to %s"), MUDNAME);
  if (MUDURL && *MUDURL)
    notify_format(player, T("Address: %s"), MUDURL);
  notify_format(player, T("Last restarted: %s"),
                show_time(globals.start_time, 0));
  notify_format(player, T("PennMUSH version %s patchlevel %s %s"), VERSION,
                PATCHLEVEL, PATCHDATE);
#ifdef GIT_REVISION
  notify_format(player, T("Git revision: %s"), GIT_REVISION);
#endif
#ifdef WIN32
  notify_format(player, T("Build date: %s"), __DATE__);
#else
  notify_format(player, T("Build date: %s"), BUILDDATE);
  notify_format(player, T("Compiler: %s"), COMPILER);
  notify_format(player, T("Compilation flags: %s"), CCFLAGS);
#endif

}
