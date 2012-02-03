/** \file mushdb.h
 *
 * \brief Macros for getting information about objects
 */

#include "config.h"
#include "copyrite.h"

#ifndef __DB_H
#define __DB_H

/* Power macros */
#include <stdio.h>
#include "flags.h"

#define Builder(x)       (command_check_byname(x, "@dig", NULL))
#define Guest(x)         has_power_by_name(x,"GUEST",NOTYPE)
#define Tel_Anywhere(x)  (Hasprivs(x) || has_power_by_name(x,"TPORT_ANYWHERE",NOTYPE))
#define Tel_Anything(x)  (Hasprivs(x) || has_power_by_name(x,"TPORT_ANYTHING",NOTYPE))
#define See_All(x)       (Hasprivs(x) || has_power_by_name(x,"SEE_ALL",NOTYPE))
#define Priv_Who(x)      (Hasprivs(x) || has_power_by_name(x,"SEE_ALL",NOTYPE))
#define Can_Hide(x)      (Hasprivs(x) || has_power_by_name(x,"HIDE",NOTYPE))
#define Can_Login(x)     (Hasprivs(x) || has_power_by_name(x,"LOGIN",NOTYPE))
#define Can_Idle(x)      (Hasprivs(x) || has_power_by_name(x,"IDLE",NOTYPE))
#define Long_Fingers(x)  (Hasprivs(x) || has_power_by_name(x,"LONG_FINGERS",NOTYPE))
#define Open_Anywhere(x) (Hasprivs(x) || has_power_by_name(x,"OPEN_ANYWHERE",NOTYPE))
#define Link_Anywhere(x)  (Hasprivs(x) || has_power_by_name(x,"LINK_ANYWHERE",NOTYPE))
#define Can_Boot(x)      (Hasprivs(x) || has_power_by_name(x,"BOOT",NOTYPE))
#define Can_Nspemit(x)   (Wizard(x) || has_power_by_name(x,"CAN_SPOOF",NOTYPE))
#define Do_Quotas(x)     (Wizard(x) || has_power_by_name(x,"QUOTAS",NOTYPE))
#define Change_Poll(x)   (Wizard(x) || has_power_by_name(x,"POLL",NOTYPE))
#define HugeQueue(x)     (Wizard(x) || has_power_by_name(x,"QUEUE",NOTYPE))
#define LookQueue(x)     (Hasprivs(x) || has_power_by_name(x,"SEE_QUEUE",NOTYPE))
#define HaltAny(x)       (Wizard(x) || has_power_by_name(x,"HALT",NOTYPE))
#define NoPay(x)         (God(x) || has_power_by_name(x,"NO_PAY",NOTYPE) || \
                                (!Mistrust(x) && \
                                  ((has_power_by_name(Owner(x),"NO_PAY",NOTYPE)) || \
                                   God(Owner(x)))))
#define Moneybags(x)    (NoPay(x) || Hasprivs(x))
#define NoQuota(x)       (Hasprivs(x) || Hasprivs(Owner(x)) || \
                                has_power_by_name(x,"NO_QUOTA",NOTYPE) || \
                                ((!Mistrust(x) && has_power_by_name(Owner(x), "NO_QUOTA", NOTYPE))))
#define NoKill(x)        (Hasprivs(x) || Hasprivs(Owner(x)) || \
                                has_power_by_name(x,"UNKILLABLE",NOTYPE) || \
                                ((!Mistrust(x) && has_power_by_name(Owner(x),"UNKILLABLE",NOTYPE))))
#define Search_All(x)    (Hasprivs(x) || has_power_by_name(x,"SEARCH",NOTYPE))
#define Global_Funcs(x)  (Hasprivs(x) || has_power_by_name(x,"FUNCTIONS",NOTYPE))
#define Create_Player(x) (Wizard(x) || has_power_by_name(x,"PLAYER_CREATE",NOTYPE))
#define Can_Announce(x)  (Wizard(x) || has_power_by_name(x,"ANNOUNCE",NOTYPE))
#define Can_Cemit(x)     (command_check_byname(x, "@cemit", NULL))

#define Pemit_All(x)    (Wizard(x) || has_power_by_name(x,"PEMIT_ALL",NOTYPE))
#define Sql_Ok(x)       (Wizard(x) || has_power_by_name(x, "SQL_OK", NOTYPE))
#define Can_Debit(x)    (Wizard(x) || has_power_by_name(x, "DEBIT", NOTYPE))
#define Many_Attribs(x)    (has_power_by_name(x, "MANY_ATTRIBS", NOTYPE))
#define Can_Pueblo_Send(x)       (Wizard(x) || has_power_by_name(x, "PUEBLO_SEND", NOTYPE))

/* Permission macros */
#define Can_See_Flag(p,t,f) ((!(f->perms & (F_DARK | F_MDARK | F_ODARK | F_DISABLED)) || \
                               ((!Mistrust(p) && (Owner(p) == Owner(t))) && \
                                !(f->perms & (F_DARK | F_MDARK | F_DISABLED))) || \
                             (See_All(p) && !(f->perms & (F_DARK | F_DISABLED))) || \
                             God(p)))

/* Can p locate x? */
bool unfindable(dbref);
#define Can_Locate(p,x) \
    (controls(p,x) || nearby(p,x) || See_All(p) \
  || (command_check_byname(p, "@whereis", NULL) && (IsPlayer(x) && !Unfind(x) \
                     && !unfindable(Location(x)))))


#define Can_Examine(p,x)    ((p == x) || controls(p,x) || See_All(p) || \
        (Visual(x) && eval_lock(p,x,Examine_Lock)))
#define can_link(p,x)  (!Guest(p) && (controls(p,x) || \
                        (IsExit(x) && (Location(x) == NOTHING))))

/* Can p link an exit to x? */
#define can_link_to(p,x,pe_info) \
     (GoodObject(x) \
   && (controls(p,x) || Link_Anywhere(p) || \
       (!Guest(p) && LinkOk(x) && eval_lock_with(p,x,Link_Lock,pe_info))) \
   && (!NO_LINK_TO_OBJECT || IsRoom(x)))

/* can p open an exit in r? */
#define can_open_from(p,r,pe_info) \
     (GoodObject(r) && IsRoom(r) && !Guest(p) \
   && (controls(p,r) || Open_Anywhere(p) || \
       (OpenOk(r) && eval_lock_with(p,r,Open_Lock,pe_info))))

/* can p access attribute a on object x? */
#define Can_Read_Attr(p,x,a)   \
   (!AF_Internal(a) && \
    (See_All(p) || can_read_attr_internal((p),(x),(a))))

/** can p look at object x? */
#define can_look_at(p, x) \
      (Long_Fingers(p) || nearby(p, x) || \
            (nearby(p, Location(x)) && \
                   (!Opaque(Location(x)) || controls(p, Location(x)))) || \
            (nearby(Location(p), x) && \
                   (!Opaque(Location(p)) || controls(p, Location(p)))))


#define Is_Visible_Attr(x,a)   \
   (!AF_Internal(a) && can_read_attr_internal(NOTHING,(x),(a)))

/* can p write attribute a on object x, assuming p may modify x?
 * Must be (1) God, or (2) a non-internal, non-safe flag and
 * (2a) a Wizard or (2b) a non-wizard attrib and (2b1) you own
 * the attrib or (2b2) it's not atrlocked.
 */
#define Can_Write_Attr(p,x,a)  \
   (can_write_attr_internal((p),(x),(a),1))
#define Can_Write_Attr_Ignore_Safe(p,x,a)  \
   (can_write_attr_internal((p),(x),(a),0))


/* Can p forward a message to x (via @forwardlist)? */
#define Can_Forward(p,x)  \
    (controls(p,x) || Pemit_All(p) || \
        ((getlock(x, Forward_Lock) != TRUE_BOOLEXP) && \
         eval_lock(p, x, Forward_Lock)))

/* Can p forward a mail message to x (via @mailforwardlist)? */
#define Can_MailForward(p,x)  \
    (IsPlayer(x) && (controls(p,x) || \
        ((getlock(x, MailForward_Lock) != TRUE_BOOLEXP) && \
         eval_lock(p, x, MailForward_Lock))))

/* Can from pass to's @lock/interact? */
#define Pass_Interact_Lock(from,to, pe_info) \
  (Loud(from) || eval_lock_with(from, to, Interact_Lock, pe_info))

/* How many pennies can you have? */
#define Max_Pennies(p) (Guest(p) ? MAX_GUEST_PENNIES : MAX_PENNIES)
#define Paycheck(p) (Guest(p) ? GUEST_PAY_CHECK : PAY_CHECK)

/* DB flag macros - these should be defined whether or not the
 * corresponding system option is defined
 * They are successive binary numbers
 */
#define DBF_NO_CHAT_SYSTEM      0x01
#define DBF_WARNINGS            0x02
#define DBF_CREATION_TIMES      0x04
#define DBF_NO_POWERS           0x08
#define DBF_NEW_LOCKS           0x10
#define DBF_NEW_STRINGS         0x20
#define DBF_TYPE_GARBAGE        0x40
#define DBF_SPLIT_IMMORTAL      0x80
#define DBF_NO_TEMPLE           0x100
#define DBF_LESS_GARBAGE        0x200
#define DBF_AF_VISUAL           0x400
#define DBF_VALUE_IS_COST       0x800
#define DBF_LINK_ANYWHERE       0x1000
#define DBF_NO_STARTUP_FLAG     0x2000
#define DBF_PANIC               0x4000
#define DBF_AF_NODUMP           0x8000
#define DBF_SPIFFY_LOCKS        0x10000
#define DBF_NEW_FLAGS           0x20000
#define DBF_NEW_POWERS          0x40000
#define DBF_POWERS_LOGGED       0x80000
#define DBF_LABELS              0x100000
#define DBF_SPIFFY_AF_ANSI      0x200000
#define DBF_HEAR_CONNECT        0x400000

/* Reboot DB flag macros - these should be defined whether or not the
 * corresponding system option is defined
 * They are successive binary numbers
 */
#define RDBF_SCREENSIZE         0x01
#define RDBF_TTYPE              0x02
#define RDBF_PUEBLO_CHECKSUM    0x04
#define RDBF_LOCAL_SOCKET       0x08
#define RDBF_SSL_SLAVE          0x10
#define RDBF_SOCKET_SRC         0x20
#define RDBF_NO_DOING           0x40

#endif                          /* __DB_H */
