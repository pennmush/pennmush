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
#ifndef WIN32
#include "buildinf.h"
#endif
#include "confmagic.h"
#include "svninfo.h"

void do_version(dbref player);

/** The version command.
 * \param player the enactor.
 */
void
do_version(dbref player)
{
#ifdef SVNREVISION
  int svnrev = 0;
  int scan;
#ifdef SVNDATE
  char svndate[75];
#endif                          /* SVNDATE */
#endif                          /* SVNREVISION */
  notify_format(player, T("You are connected to %s"), MUDNAME);
  if (MUDURL && *MUDURL)
    notify_format(player, T("Address: %s"), MUDURL);
  notify_format(player, T("Last restarted: %s"),
                show_time(globals.start_time, 0));
  notify_format(player, T("PennMUSH version %s patchlevel %s %s"), VERSION,
                PATCHLEVEL, PATCHDATE);
#ifdef SVNREVISION
  scan = sscanf(SVNREVISION, "$" "Rev: %d $", &svnrev);
  if (scan == 1) {
#ifdef SVNDATE
    scan = sscanf(SVNDATE, "$" "Date: %s $", svndate);
    if (scan == 1)
      notify_format(player, T("SVN revision: %d [%s]"), svnrev, svndate);
    else
      notify_format(player, T("SVN revision: %d"), svnrev);
#else
    notify_format(player, T("SVN revision: %d"), svnrev);
#endif                          /* SVNDATE */
  }
#endif                          /* SVNREVISION */


#ifdef WIN32
  notify_format(player, T("Build date: %s"), __DATE__);
#else
  notify_format(player, T("Build date: %s"), BUILDDATE);
  notify_format(player, T("Compiler: %s"), COMPILER);
  notify_format(player, T("Compilation flags: %s"), CCFLAGS);
#endif

}
