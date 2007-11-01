/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2007 Dirk Meyer
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is
 * available in the LICENSE file.
 *
 * If you received this file without documentation, it can be
 * downloaded from http://iroffer.dinoex.net/
 *
 * $Id$
 *
 */

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "dinoex_utilities.h"
#include "dinoex_irc.h"
#include "dinoex_queue.h"

void queue_update_nick(irlist_t *list, const char *oldnick, const char *newnick)
{
  ir_pqueue *pq;

  for (pq = irlist_get_head(list);
       pq;
       pq = irlist_get_next(pq)) {
    if (!strcasecmp(pq->nick, oldnick)) {
      mydelete(pq->nick);
      pq->nick = mystrdup(newnick);
    }
  }
}

void queue_reverify_restrictsend(irlist_t *list)
{
  gnetwork_t *backup;
  ir_pqueue *pq;

  backup = gnetwork;
  for (pq = irlist_get_head(list); pq;) {
    gnetwork = &(gdata.networks[pq->net]);
    if (gnetwork->serverstatus != SERVERSTATUS_CONNECTED) {
      pq = irlist_get_next(pq);
      continue;
    }
    if (strcmp(pq->hostname, "man") == 0) {
      pq = irlist_get_next(pq);
      continue;
    }
    if (isinmemberlist(pq->nick)) {
      pq->restrictsend_bad = 0;
      pq = irlist_get_next(pq);
      continue;
    }
    if (!pq->restrictsend_bad) {
      pq->restrictsend_bad = gdata.curtime;
      if (gdata.restrictsend_warning) {
        notice(pq->nick, "You are no longer on a known channel");
      }
      pq = irlist_get_next(pq);
      continue;
    }
    if ((gdata.curtime - pq->restrictsend_bad) < gdata.restrictsend_timeout) {
      pq = irlist_get_next(pq);
      continue;
    }

    notice(pq->nick, "** Removed From Queue: You are no longer on a known channel");
    ioutput(CALLTYPE_NORMAL, OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "Removed From Queue: %s on %s not in known Channel.",
            pq->nick, gdata.networks[ pq->net ].name);
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
  }
  gnetwork = backup;
}

void queue_pack_limit(irlist_t *list, xdcc *xd)
{
  gnetwork_t *backup;
  ir_pqueue *pq;

  for (pq = irlist_get_head(list); pq;) {
    if (pq->xpack != xd) {
      pq = irlist_get_next(pq);
      continue;
    }

    backup = gnetwork;
    gnetwork = &(gdata.networks[pq->net]);
    notice_slow(pq->nick, "** Sorry, This Pack is over download limit for today.  Try again tomorrow.");
    notice_slow(pq->nick, xd->dlimit_desc);
    gnetwork = backup;
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
  }
}

void queue_punishslowusers(irlist_t *list, int network, const char *nick)
{
  gnetwork_t *backup;
  ir_pqueue *pq;

  for (pq = irlist_get_head(list); pq;) {
    if (pq->net != network) {
      pq = irlist_get_next(pq);
      continue;
    }
    if (strcasecmp(pq->nick, nick) != 0) {
      pq = irlist_get_next(pq);
      continue;
    }

    backup = gnetwork;
    gnetwork = &(gdata.networks[pq->net]);
    notice(nick, "** Removed From Queue: You are being punished for your slowness");
    gnetwork = backup;
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
  }
}

int queue_xdcc_remove(irlist_t *list, int network, const char *nick)
{
  gnetwork_t *backup;
  ir_pqueue *pq;
  int changed = 0;

  for (pq = irlist_get_head(list); pq;) {
    if (pq->net != network) {
      pq = irlist_get_next(pq);
      continue;
    }
    if (strcasecmp(pq->nick, nick) != 0) {
      pq = irlist_get_next(pq);
      continue;
    }

    backup = gnetwork;
    gnetwork = &(gdata.networks[pq->net]);
    notice(nick,
           "Removed you from the queue for \"%s\", you waited %li minute%s.",
           pq->xpack->desc,
           (long)(gdata.curtime-pq->queuedtime)/60,
           ((gdata.curtime-pq->queuedtime)/60) != 1 ? "s" : "");
    gnetwork = backup;
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
    changed ++;
  }
  return changed;
}

void queue_pack_remove(irlist_t *list, xdcc *xd)
{
  gnetwork_t *backup;
  ir_pqueue *pq;

  for (pq = irlist_get_head(list); pq;) {
    if (pq->xpack != xd) {
      pq = irlist_get_next(pq);
      continue;
    }

    backup = gnetwork;
    gnetwork = &(gdata.networks[pq->net]);
    notice(pq->nick, "** Removed From Queue: Pack removed");
    gnetwork = backup;
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
  }
}

void queue_all_remove(irlist_t *list, const char *message)
{
  gnetwork_t *backup;
  ir_pqueue *pq;

  backup = gnetwork;
  for (pq = irlist_get_head(list);
       pq;
       pq = irlist_delete(list, pq)) {
    gnetwork = &(gdata.networks[pq->net]);
    notice_slow(pq->nick, message);
    mydelete(pq->nick);
    mydelete(pq->hostname);
  }
  gnetwork = backup;
}

char* addtoidlequeue(char *tempstr, const char* nick, const char* hostname, xdcc *xd, int pack, int inq)
{
  ir_pqueue *tempq;

  updatecontext();
  if (gdata.idlequeuesize == 0)
    return NULL;

  if (inq >= gdata.maxidlequeuedperperson) {
    ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " Denied (user/idle): ");
    snprintf(tempstr, maxtextlength,
             "Denied, You already have %i items queued, Try Again Later",
             inq);
    return tempstr;
  }

  if (irlist_size(&gdata.idlequeue) >= gdata.idlequeuesize) {
    ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " Denied (slot/idle): ");
    snprintf(tempstr, maxtextlength,
             "Idle queue of size %d is Full, Try Again Later",
             gdata.idlequeuesize);
    return tempstr;
  }

  ioutput(CALLTYPE_MULTI_MIDDLE, OUT_S|OUT_L|OUT_D, COLOR_YELLOW, " Queued (idle slot): ");
  tempq = irlist_add(&gdata.idlequeue, sizeof(ir_pqueue));
  tempq->queuedtime = gdata.curtime;
  tempq->nick = mystrdup(nick);
  tempq->hostname = mystrdup(hostname);
  tempq->xpack = xd;
  tempq->net = gnetwork->net;

  snprintf(tempstr, maxtextlength,
           "Added you to the idle queue for pack %d (\"%s\") in position %d. To Remove yourself at "
           "a later time type \"/MSG %s XDCC REMOVE\".",
           pack, xd->desc,
           irlist_size(&gdata.idlequeue),
           save_nick(gnetwork->user_nick));
  return tempstr;
}

void check_idle_queue(void)
{
  ir_pqueue *pq;
  ir_pqueue *mq;
  ir_pqueue *tempq;
  transfer *tr;
  int usertrans;
  int pass;

  updatecontext();
  if (gdata.exiting)
    return;

  if (gdata.holdqueue)
    return;

  if (gdata.restrictlist && (has_joined_channels(0) == 0))
    return;

  if (irlist_size(&gdata.idlequeue) == 0)
    return;

  if (irlist_size(&gdata.mainqueue) >= gdata.queuesize)
    return;

  for (pass = 0; pass < 2; pass++) {
    for (pq = irlist_get_head(&gdata.idlequeue);
         pq;
         pq = irlist_get_next(pq)) {
      if (gdata.networks[pq->net].serverstatus != SERVERSTATUS_CONNECTED)
        continue;

      /* timeout for restart must be less then Transfer Timeout 180s */
      if (gdata.curtime - gdata.networks[pq->net].lastservercontact > 150)
        continue;

      usertrans=0;
      for (mq = irlist_get_head(&gdata.mainqueue);
           mq;
           mq = irlist_get_next(mq)) {
        if ((!strcmp(mq->hostname, pq->hostname)) || (!strcasecmp(mq->nick, pq->nick))) {
          usertrans++;
        }
      }

      if (usertrans >= gdata.maxqueueditemsperperson)
        continue;

      if (pass == 0)
        continue;

      usertrans=0;
      for (tr = irlist_get_head(&gdata.trans);
           tr;
           tr = irlist_get_next(tr)) {
        if ((!strcmp(tr->hostname, pq->hostname)) || (!strcasecmp(tr->nick, pq->nick))) {
          usertrans++;
        }
      }
      if (usertrans >= gdata.maxtransfersperperson)
        continue;

      break; /* found the person that will get the send */
    }
  }
  if (pq == NULL)
    return;

  tempq = irlist_add(&gdata.mainqueue, sizeof(ir_pqueue));
  *tempq = *pq;
  irlist_delete(&gdata.idlequeue, pq);

  if (irlist_size(&gdata.mainqueue) &&
      (irlist_size(&gdata.trans) < min2(MAXTRANS, gdata.slotsmax))) {
    sendaqueue(0, 0, NULL);
  }
}

/* End of File */
