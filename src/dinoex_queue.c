/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2013 Dirk Meyer
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
#include "dinoex_misc.h"
#include "dinoex_transfer.h"
#include "dinoex_queue.h"

/* change a nickname in a queue */
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

/* check queue for users that have left and drop them */
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
    if (!get_restrictsend()) {
      pq = irlist_get_next(pq);
      continue;
    }
    if (strcmp(pq->hostname, "man") == 0) { /* NOTRANSLATE */
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
    if (gdata.curtime < (signed)(pq->restrictsend_bad + gdata.restrictsend_timeout)) {
      pq = irlist_get_next(pq);
      continue;
    }

    notice(pq->nick, "** Removed From Queue: You are no longer on a known channel");
    ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
            "Removed From Queue: %s on %s not in known Channel.",
            pq->nick, gdata.networks[ pq->net ].name);
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
  }
  gnetwork = backup;
}

/* remove a pack from the queue because the pack has now reached its download limit */
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
    notice_slow(pq->nick, "%s", xd->dlimit_desc); /* NOTRANSLATE */
    gnetwork = backup;
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
  }
}

/* remove all packs a user has */
static void queue_punish_user(irlist_t *list, const char *msg, unsigned int network, const char *nick)
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
    notice(nick, "** Removed From Queue: %s", msg );
    gnetwork = backup;
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
  }
}

/* remove all packs a user has because he has abused the bot */
void queue_punish_abuse(const char *msg, unsigned int network, const char *nick)
{
  queue_punish_user(&gdata.mainqueue, msg, network, nick);
  queue_punish_user(&gdata.idlequeue, msg, network, nick);
}

/* remove all packs a user has because he send us XDCC REMOVE */
unsigned int queue_xdcc_remove(irlist_t *list, unsigned int network, const char *nick, unsigned int number)
{
  gnetwork_t *backup;
  ir_pqueue *pq;
  unsigned int changed = 0;

  for (pq = irlist_get_head(list); pq;) {
    if (pq->net != network) {
      pq = irlist_get_next(pq);
      continue;
    }
    if (strcasecmp(pq->nick, nick) != 0) {
      pq = irlist_get_next(pq);
      continue;
    }

    if (number > 0) {
      /* remove only the matching pack */
      if (number_of_pack(pq->xpack) != number) {
        pq = irlist_get_next(pq);
        continue;
      }
    }

    backup = gnetwork;
    gnetwork = &(gdata.networks[pq->net]);
    notice(nick,
           "Removed you from the queue for \"%s\", you waited %li %s.",
           pq->xpack->desc,
           (long)(gdata.curtime-pq->queuedtime)/60,
           ((gdata.curtime-pq->queuedtime)/60) != 1 ? "minutes" : "minute");
    gnetwork = backup;
    mydelete(pq->nick);
    mydelete(pq->hostname);
    pq = irlist_delete(list, pq);
    ++changed;
  }
  return changed;
}

/* remove the pack in the queue because admin has removed it */
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

/* remove all entries from this queue */
void queue_all_remove(irlist_t *list, const char *message)
{
  gnetwork_t *backup;
  ir_pqueue *pq;

  backup = gnetwork;
  for (pq = irlist_get_head(list);
       pq;
       pq = irlist_delete(list, pq)) {
    gnetwork = &(gdata.networks[pq->net]);
    notice_slow(pq->nick, "%s", message); /* NOTRANSLATE */
    mydelete(pq->nick);
    mydelete(pq->hostname);
  }
  gnetwork = backup;
}

/* count number of entries in a queue for one user */
unsigned int queue_count_host(irlist_t *list, unsigned int *inq, unsigned int man, const char* nick, const char *hostname, xdcc *xd)
{
  ir_pqueue *pq;
  unsigned int alreadytrans = 0;

  *inq = 0;
  for (pq = irlist_get_head(list);
       pq;
       pq = irlist_get_next(pq)) {
    if (pq->net != gnetwork->net)
      continue;

    if (strcmp(pq->hostname, hostname))
      continue;

    if (man && strcasecmp(pq->nick, nick))
      continue;

    ++(*inq);
    if (pq->xpack == xd)
      ++alreadytrans;
  }
  return alreadytrans;
}

/* add a request to the idle queue */
unsigned int addtoidlequeue(const char **msg, char *tempstr, const char* nick, const char* hostname, xdcc *xd, unsigned int pack, unsigned int inq)
{
  ir_pqueue *tempq;
  const char *hostname2;

  updatecontext();
  *msg = NULL;
  if (gdata.idlequeuesize == 0) {
    *tempstr = 0;
    return 0;
  }

  if (hostname != NULL) {
    if (inq >= gdata.maxidlequeuedperperson) {
      snprintf(tempstr, maxtextlength,
               "Denied, You already have %u items queued, Try Again Later",
               inq);
      *msg = "Denied (user/idle)";
      return 1;
    }

    if (irlist_size(&gdata.idlequeue) >= gdata.idlequeuesize) {
      snprintf(tempstr, maxtextlength,
               "Idle queue of size %u is Full, Try Again Later",
               gdata.idlequeuesize);
      *msg = "Denied (slot/idle)";
      return 1;
    }
    hostname2 = hostname;
  } else {
    hostname2 = "man"; /* NOTRANSLATE */
  }

  tempq = irlist_add(&gdata.idlequeue, sizeof(ir_pqueue));
  tempq->queuedtime = gdata.curtime;
  tempq->nick = mystrdup(nick);
  tempq->hostname = mystrdup(hostname2);
  tempq->xpack = xd;
  tempq->net = gnetwork->net;

  snprintf(tempstr, maxtextlength,
           "Added you to the idle queue for pack %u (\"%s\") in position %u. To Remove yourself at "
           "a later time type \"/MSG %s XDCC REMOVE %u\".",
           pack, xd->desc,
           irlist_size(&gdata.idlequeue),
           get_user_nick(), pack);
  *msg = "Queued (idle slot)";
  return 0;
}

/* add a request to the main queue */
unsigned int addtomainqueue(const char **msg, char *tempstr, const char *nick, const char *hostname, unsigned int pack)
{
  const char *hostname2;
  ir_pqueue *tempq;
  xdcc *tempx;
  unsigned int alreadytrans;
  unsigned int inq;
  unsigned int inq2;
  unsigned int man;
  unsigned int mainslot;
  unsigned int fatal;

  updatecontext();

  tempx = get_xdcc_pack(pack);

  man = 0;
  if (hostname != NULL) {
    hostname2 = hostname;
  } else {
    hostname2 = "man"; /* NOTRANSLATE */
    ++man;
  }

  inq = 0;
  inq2 = 0;
  alreadytrans = queue_count_host(&gdata.mainqueue, &inq, man, nick, hostname2, tempx);
  alreadytrans += queue_count_host(&gdata.idlequeue, &inq2, man, nick, hostname2, tempx);

  if (alreadytrans) {
    snprintf(tempstr, maxtextlength,
             "Denied, You already have that item queued.");
    *msg = "Denied (queue/dup)";
    return 1;
  }

  if (hostname != NULL) {
    if ((inq >= gdata.maxqueueditemsperperson) || (inq2 > 0 )) {
      fatal = addtoidlequeue(msg, tempstr, nick, hostname, tempx, pack, inq2);
      if (*msg != NULL)
        return fatal;

      snprintf(tempstr, maxtextlength,
               "Denied, You already have %u items queued, Try Again Later",
               inq);
      *msg = "Denied (user/queue)";
      return 1;
    }

    mainslot = (irlist_size(&gdata.mainqueue) >= gdata.queuesize);
    if (gdata.holdqueue || (gnetwork->botstatus != BOTSTATUS_JOINED) || mainslot) {
      fatal = addtoidlequeue(msg, tempstr, nick, hostname, tempx, pack, inq2);
      if (*msg != NULL)
        return fatal;
    }
    if (mainslot) {
      snprintf(tempstr, maxtextlength,
               "Main queue of size %u is Full, Try Again Later",
               gdata.queuesize);
      *msg = "Denied (slot/queue)";
      return 1;
    }
  }

  tempq = irlist_add(&gdata.mainqueue, sizeof(ir_pqueue));
  tempq->queuedtime = gdata.curtime;
  tempq->nick = mystrdup(nick);
  tempq->hostname = mystrdup(hostname2);
  tempq->xpack = tempx;
  tempq->net = gnetwork->net;

  snprintf(tempstr, maxtextlength,
           "Added you to the main queue for pack %u (\"%s\") in position %u. To Remove yourself at "
           "a later time type \"/MSG %s XDCC REMOVE %u\".",
           pack, tempx->desc,
           irlist_size(&gdata.mainqueue),
           save_nick(gnetwork->user_nick), pack);

  *msg = "Queued (slot)";
  return 0;
}

static const char *send_queue_msg[] = { "", " (low bandwidth)", " (manual)" };

/* send the next item form the queue */
void send_from_queue(unsigned int type, unsigned int pos, char *lastnick)
{
  ir_pqueue *pq;
  transfer *tr;
  gnetwork_t *backup;
  unsigned int usertrans;
  unsigned int pack;

  updatecontext();

  if (gdata.holdqueue)
    return;

  if (!gdata.balanced_queue)
    lastnick = NULL;

  if (irlist_size(&gdata.mainqueue) == 0)
    return;

  /*
   * first determine what the first queue position is that is eligable
   * for a transfer find the first person who has not already maxed out
   * his sends if noone, do nothing and let execution continue to pack
   * queue check
   */

  if (pos > 0) {
    /* get specific entry */
    pq = irlist_get_nth(&gdata.mainqueue, pos - 1);
  } else {
    for (pq = irlist_get_head(&gdata.mainqueue); pq; pq = irlist_get_next(pq)) {
      if (gdata.networks[pq->net].serverstatus != SERVERSTATUS_CONNECTED)
        continue;

      if (gdata.networks[pq->net].botstatus != BOTSTATUS_JOINED)
        continue;

      /* timeout for restart must be less then Transfer Timeout 180s */
      if (gdata.curtime - gdata.networks[pq->net].lastservercontact > 150)
        continue;

      usertrans = 0;
      for (tr = irlist_get_head(&gdata.trans); tr; tr = irlist_get_next(tr)) {
        if (!strcasecmp(tr->nick, pq->nick)) {
          ++usertrans; /* same nick */
          continue;
        }
        if (!strcmp(tr->hostname, "man")) /* NOTRANSLATE */
          continue; /* do not check host when man transfers */

        if (!strcmp(tr->hostname, pq->hostname)) {
          ++usertrans; /* same host */
          continue;
        }
      }

     /* usertrans is the number of transfers a user has in progress */
      if (usertrans >= gdata.maxtransfersperperson)
        continue;

      /* skip last trasfering user */
      if (lastnick != NULL) {
        if (!strcasecmp(pq->nick, lastnick))
          continue;
      }

      /* found the person that will get the send */
      break;
    }
  }

  if (!pq) {
    if (lastnick != NULL) {
      /* try again */
      send_from_queue(type, pos, NULL);
    }
    return;
  }

  if (type > 2) type = 0;
  pack = number_of_pack(pq->xpack);
  ioutput(OUT_S|OUT_L|OUT_D, COLOR_YELLOW,
          "QUEUED SEND%s: %s (%s on %s), Pack #%u",
          send_queue_msg[ type ],
          pq->nick, pq->hostname, gdata.networks[ pq->net ].name, pack);

  if (pack == XDCC_SEND_LIST)
    init_xdcc_file(pq->xpack, gdata.xdcclistfile);
  look_for_file_changes(pq->xpack);

  backup = gnetwork;
  gnetwork = &(gdata.networks[pq->net]);

  tr = create_transfer(pq->xpack, pq->nick, pq->hostname);
  mydelete(pq->nick);
  mydelete(pq->hostname);
  irlist_delete(&gdata.mainqueue, pq);

  t_notice_transfer(tr, NULL, pack, 1);
  t_unlmited(tr, NULL);
  t_setup_dcc(tr);

  gnetwork = backup;
  check_idle_queue(0);
}

/* start sending one queued pack */
void start_one_send(void)
{
  if (!gdata.exiting &&
      irlist_size(&gdata.mainqueue) &&
      (irlist_size(&gdata.trans) < gdata.slotsmax)) {
    send_from_queue(0, 0, NULL);
  }
}

/* start sending the queued packs */
void start_sends(void)
{
  unsigned int i;

  for (i=0; i<gdata.slotsmax; ++i) {
    start_one_send();
  }
}

static ir_pqueue *find_in_idle_queue(void)
{
  ir_pqueue *pq;
  ir_pqueue *mq;
  transfer *tr;
  unsigned int usertrans;
  unsigned int pass;

  pq = NULL;
  for (pass = 0; pass < 2; ++pass) {
    for (pq = irlist_get_head(&gdata.idlequeue);
         pq;
         pq = irlist_get_next(pq)) {
      if (gdata.networks[pq->net].serverstatus != SERVERSTATUS_CONNECTED)
        continue;

      if (gdata.networks[pq->net].botstatus != BOTSTATUS_JOINED)
        continue;

      /* timeout for restart must be less then Transfer Timeout 180s */
      if (gdata.curtime - gdata.networks[pq->net].lastservercontact > 150)
        continue;

      usertrans=0;
      for (mq = irlist_get_head(&gdata.mainqueue);
           mq;
           mq = irlist_get_next(mq)) {
        if ((!strcmp(mq->hostname, pq->hostname)) || (!strcasecmp(mq->nick, pq->nick))) {
          ++usertrans;
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
          ++usertrans;
        }
      }
      if (usertrans >= gdata.maxtransfersperperson)
        continue;

      break; /* found the person that will get the send */
    }
  }
  return pq;
}

/* check idle queue and move one entry into the main queue */
void check_idle_queue(unsigned int pos)
{
  ir_pqueue *pq;
  ir_pqueue *tempq;

  updatecontext();
  if (gdata.exiting)
    return;

  if (gdata.holdqueue)
    return;

  if (irlist_size(&gdata.idlequeue) == 0)
    return;

  if (irlist_size(&gdata.mainqueue) >= gdata.queuesize)
    return;

  if (pos == 0) {
    pq = find_in_idle_queue();
  } else {
    pq = irlist_get_nth(&gdata.idlequeue, pos - 1);
  }
  if (pq == NULL)
    return;

  tempq = irlist_add(&gdata.mainqueue, sizeof(ir_pqueue));
  *tempq = *pq;
  irlist_delete(&gdata.idlequeue, pq);

  start_one_send();
}

/* check idle queue and move one entry into the main queue */
int check_main_queue(unsigned int max)
{
  if (gdata.exiting)
    return 0;

  /* update mainqueue from idlequeue */
  check_idle_queue(0);

  if (irlist_size(&gdata.mainqueue) == 0)
    return 0;

  if (irlist_size(&gdata.trans) >= max)
    return 0;

  return 1;
}

/* fill mainqueue with data from idle queue on start */
void start_main_queue(void)
{
  unsigned int i;

  for (i=0; i<gdata.queuesize; ++i) {
    check_idle_queue(0);
  }
}

/* put transfer back into queue */
ir_pqueue *requeue(transfer *tr, ir_pqueue *old)
{
  ir_pqueue *tempq;

  tempq = irlist_add(&gdata.mainqueue, sizeof(ir_pqueue));
  tempq->queuedtime = tr->connecttimems / 1000;
  tempq->nick = mystrdup(tr->nick);
  tempq->hostname = mystrdup(tr->hostname);
  tempq->xpack = tr->xpack;
  tempq->net = tr->net;
  irlist_remove(&gdata.mainqueue, tempq);

  if (old == NULL) {
    irlist_insert_head(&gdata.mainqueue, tempq);
  } else {
    irlist_insert_after(&gdata.mainqueue, tempq, old);
  }
  return tempq;
}

/* End of File */
