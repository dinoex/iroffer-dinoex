/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2010 Dirk Meyer
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is
 * available in the README file.
 *
 * If you received this file without documentation, it can be
 * downloaded from http://iroffer.dinoex.net/
 *
 * $Id$
 *
 * See also:
 * http://www.ruby-doc.org/docs/ProgrammingRuby/html/ext_ruby.html
 *
 */

/* include the headers */
#include "iroffer_config.h"

#ifdef USE_RUBY
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "dinoex_utilities.h"
#include "dinoex_admin.h"
#include "dinoex_jobs.h"
#include "dinoex_irc.h"
#include "dinoex_misc.h"
#include "dinoex_config.h"
#include "dinoex_ruby.h"

#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#pragma GCC diagnostic ignored "-Wredundant-decls"
#include "ruby.h"

typedef struct protect_call_arg {
  VALUE recvobj;
  ID mid;
  int argc;
  VALUE *argv;
} protect_call_arg_t;

int myruby_status;
int myruby_loaded;
time_t myruby_time;

char *cLine;
char *cFile;
unsigned int cPack;
VALUE cIrofferConfig;
VALUE cIrofferEvent;
VALUE oIrofferEvent = Qnil;

#if USE_RUBYVERSION < 19
static VALUE rb_errinfo(void)
{
  return ruby_errinfo;
}
#endif

static void iroffer_ruby_errro(int error)
{
  VALUE lasterr;
  VALUE inclass;
  VALUE message;
  VALUE ary;
  long c;

  if (error == 0)
    return;

  lasterr = rb_gv_get("$!");
  inclass = rb_class_path(CLASS_OF(lasterr));
  message = rb_obj_as_string(lasterr);
  outerror(OUTERROR_TYPE_WARN_LOUD,
           "error ruby_script: class=%s, message=%s",
           RSTRING_PTR(inclass), RSTRING_PTR(message));

  if (!NIL_P(rb_errinfo())) {
    ary = rb_funcall(rb_errinfo(), rb_intern("backtrace"), 0);
    for (c=0; c<RARRAY_LEN(ary); ++c) {
      outerror(OUTERROR_TYPE_WARN_LOUD,
               "backtrace from %s",
               RSTRING_PTR(RARRAY_PTR(ary)[c]));
    }
  }
}

static VALUE protect_funcall0(VALUE arg)
{
  return rb_funcall2(((protect_call_arg_t *) arg)->recvobj,
                     ((protect_call_arg_t *) arg)->mid,
                     ((protect_call_arg_t *) arg)->argc,
                     ((protect_call_arg_t *) arg)->argv);
}

static VALUE rb_funcall_protected(VALUE recvobj, ID mid, int argc, ...)
{
  va_list ap;
  VALUE *argv;
  VALUE rval;
  int state = 0;
  int i;
  protect_call_arg_t arg;

  if (argc > 0) {
    argv = ALLOCA_N(VALUE, argc);
    va_start(ap, argc);
    for (i = 0; i < argc; ++i) {
      argv[i] = va_arg(ap, VALUE);
    }
    va_end(ap);
  } else {
    argv = 0;
  }
  arg.recvobj = recvobj;
  arg.mid = mid;
  arg.argc = argc;
  arg.argv = argv;
  rval = rb_protect(protect_funcall0, (VALUE) &arg, &state);
  if (state != 0) {
    iroffer_ruby_errro(state);
    return Qnil;
  }
  return rval;
}

static char *rb_obj_as_string_protected(VALUE objval)
{
  VALUE rval;
  int state = 0;

  rval = rb_protect( (VALUE (*)(VALUE))rb_obj_as_string, objval, &state);
  if (state != 0) {
    iroffer_ruby_errro(state);
    return NULL;
  }

  return STR2CSTR(rval);
}

static void ci_free(void * UNUSED(p))
{
}

static VALUE cie_null(VALUE UNUSED(self))
{
  return Qnil;
}

static VALUE cie_new(VALUE rclass)
{
  VALUE tdata = Data_Wrap_Struct(rclass, 0, ci_free, &gdata);
  rb_obj_call_init(tdata, 0, 0);
  return tdata;
}

static VALUE cie_network(void)
{
  return rb_str_new(gnetwork->name, strlen(gnetwork->name));
}

static VALUE cie_inputline(void)
{
  return rb_str_new(cLine, strlen(cLine));
}

static VALUE cie_added_file(void)
{
  return rb_str_new(cFile, strlen(cFile));
}

static VALUE cie_added_pack(void)
{
  return INT2NUM(cPack);
}

static VALUE cie_hostmask(void)
{
  VALUE copy;
  char *val;

  val = getpart(cLine, 1);
  if (val == NULL)
    return Qnil;

  copy = rb_str_new(val + 1, strlen(val + 1));
  mydelete(val);
  return copy;
}

static VALUE cie_nick(void)
{
  VALUE copy;
  char *val;
  char *nick;
  char *end;

  val = getpart(cLine, 1);
  if (val == NULL)
    return Qnil;

  nick = val + 1;
  end = strchr(nick, '!');
  if (end != NULL)
    *end = 0;
  copy = rb_str_new(nick, strlen(nick));
  mydelete(val);
  return copy;
}

static VALUE cie_channel(void)
{
  VALUE copy;
  char *val;

  val = getpart(cLine, 3);
  if (val == NULL)
    return Qnil;

  copy = rb_str_new(val, strlen(val));
  mydelete(val);
  return copy;
}

static VALUE cie_message(void)
{
  VALUE copy;
  char *val;

  val = getpart_eol(cLine, 4);
  if (val == NULL)
    return Qnil;

  copy = rb_str_new(val + 1, strlen(val + 1));
  mydelete(val);
  return copy;
}

static VALUE cie_config(VALUE UNUSED(module), VALUE rkey)
{
  char *key;
  char *val;
  VALUE copy;

  if (NIL_P(rkey))
    return Qnil;

  key = rb_obj_as_string_protected(rkey);
  if (!key)
    return Qnil;

  val = print_config_key(key);
  if (val != NULL) {
    copy = rb_str_new(val, strlen(val));
    mydelete(val);
    return copy;
  }

  return Qnil;
}

static VALUE cie_info_pack(VALUE UNUSED(module), VALUE rnr, VALUE rkey)
{
  xdcc *xd;
  char *key;
  char *val;
  char *tempstr;
  unsigned int nr;
  unsigned int ival;
  off_t oval;
  size_t len;
  VALUE rval;

  if (NIL_P(rnr) || NIL_P(rkey))
    return Qnil;

  nr = FIX2UINT(rnr);
  if (nr == 0)
    return Qnil;

  if (nr > irlist_size(&gdata.xdccs))
    return Qnil;

  xd = get_xdcc_pack(nr);
  if (xd == NULL)
    return Qnil;

  key = rb_obj_as_string_protected(rkey);
  if (!key)
    return Qnil;

  for (;;) {
    if (strcmp(key, "file") == 0) {
      val = xd->file;
      break;
    }
    if (strcmp(key, "desc") == 0) {
      val = xd->desc;
      break;
    }
    if (strcmp(key, "note") == 0) {
      val = xd->note;
      break;
    }
    if (strcmp(key, "group") == 0) {
      val = xd->group;
      break;
    }
    if (strcmp(key, "trigger") == 0) {
      val = xd->trigger;
      break;
    }
    if (strcmp(key, "lock") == 0) {
      val = xd->lock;
      break;
    }
    if (strcmp(key, "gets") == 0) {
      ival = xd->gets;
      return UINT2NUM(ival);
    }
    if (strcmp(key, "color") == 0) {
      ival = xd->color;
      return UINT2NUM(ival);
    }
    if (strcmp(key, "dlimit_max") == 0) {
      ival = xd->dlimit_max;
      return UINT2NUM(ival);
    }
    if (strcmp(key, "dlimit_used") == 0) {
      ival = xd->dlimit_used;
      return UINT2NUM(ival);
    }
    if (strcmp(key, "has_md5sum") == 0) {
      ival = xd->has_md5sum;
      return UINT2NUM(ival);
    }
    if (strcmp(key, "has_crc32") == 0) {
      ival = xd->has_crc32;
      return UINT2NUM(ival);
    }
    if (strcmp(key, "size") == 0) {
      oval = xd->st_size;
      return OFFT2NUM(oval);
    }
    if (strcmp(key, "mtime") == 0) {
      return rb_time_new(xd->mtime, 0);
    }
    if (strcmp(key, "xtime") == 0) {
      return rb_time_new(xd->xtime, 0);
    }
    if (strcmp(key, "crc32") == 0) {
      tempstr = mycalloc(maxtextlengthshort);
      len = snprintf(tempstr, maxtextlengthshort, "%.8lX", xd->crc32);
      rval = rb_str_new(tempstr, len);
      mydelete(tempstr);
      return rval;
    }
    if (strcmp(key, "md5sum") == 0) {
      tempstr = mycalloc(maxtextlengthshort);
      len = snprintf(tempstr, maxtextlengthshort, MD5_PRINT_FMT, MD5_PRINT_DATA(xd->md5sum));
      rval = rb_str_new(tempstr, len);
      mydelete(tempstr);
      return rval;
    }
    /* minspeed, maxspeed */
    return Qnil;
  }
  if (val == NULL)
    return Qnil;
  return rb_str_new(val, strlen(val));
}

static VALUE cie_privmsg(VALUE UNUSED(module), VALUE rname, VALUE rmsg)
{
  char *name;
  char *msg;
  channel_t *ch;

  if (NIL_P(rname) || NIL_P(rmsg))
    return Qnil;

  name = rb_obj_as_string_protected(rname);
  msg = rb_obj_as_string_protected(rmsg);
  if (!name || !msg)
    return Qnil;

  if (gnetwork->serverstatus != SERVERSTATUS_CONNECTED)
    return Qfalse;

  if (gnetwork->botstatus != BOTSTATUS_JOINED)
    return Qfalse;

  if (name[0] != '#') {
    privmsg_slow(name, "%s", msg);
    return Qtrue;
  }

  for (ch = irlist_get_head(&(gnetwork->channels));
       ch;
       ch = irlist_get_next(ch)) {
    if (strcasecmp(ch->name, name) == 0) {
      privmsg_chan(ch, "%s", msg);
      return Qtrue;
    }
  }
  return Qfalse;
}

static VALUE cie_warning(VALUE UNUSED(module), VALUE rmsg)
{
  char *msg;

  if (NIL_P(rmsg))
    return Qnil;

  msg = rb_obj_as_string_protected(rmsg);
  if (!msg)
    return Qnil;

  outerror(OUTERROR_TYPE_WARN_LOUD, "ruby: %s", msg );
  return Qtrue;
}

static VALUE cie_mode(VALUE UNUSED(module), VALUE rname, VALUE rmsg)
{
  char *name;
  char *msg;
  channel_t *ch;

  if (NIL_P(rname) || NIL_P(rmsg))
    return Qnil;

  name = rb_obj_as_string_protected(rname);
  msg = rb_obj_as_string_protected(rmsg);
  if (!name || !msg)
    return Qnil;

  if (gnetwork->serverstatus != SERVERSTATUS_CONNECTED)
    return Qfalse;

  if (gnetwork->botstatus != BOTSTATUS_JOINED)
    return Qfalse;

  if (name[0] != '#')
    return Qfalse;

  for (ch = irlist_get_head(&(gnetwork->channels));
       ch;
       ch = irlist_get_next(ch)) {
    if (strcasecmp(ch->name, name) == 0) {
      writeserver(WRITESERVER_NORMAL, "MODE %s %s", name, msg);
      return Qtrue;
    }
  }
  return Qfalse;
}

static VALUE cie_command(VALUE UNUSED(module), VALUE rmsg)
{
  char *msg;
  userinput *uxdl;

  if (NIL_P(rmsg))
    return Qnil;

  msg = rb_obj_as_string_protected(rmsg);
  if (!msg)
    return Qnil;

  uxdl = mycalloc(sizeof(userinput));

  a_fillwith_msg2(uxdl, NULL, msg);
  uxdl->method = method_console;
  uxdl->net = 0;
  uxdl->level = ADMIN_LEVEL_CONSOLE;
  u_parseit(uxdl);

  mydelete(uxdl);
  return Qtrue;
}

static void Init_IrofferEvent(void) {
  cIrofferEvent = rb_define_class("IrofferEvent", rb_cObject);
  rb_define_singleton_method(cIrofferEvent, "new", cie_new, 0);
  /* action */
  rb_define_method(cIrofferEvent, "privmsg", cie_privmsg, 2);
  rb_define_method(cIrofferEvent, "warning", cie_warning, 1);
  rb_define_method(cIrofferEvent, "mode", cie_mode, 2);
  rb_define_method(cIrofferEvent, "command", cie_command, 1);
  /* hooks */
  rb_define_method(cIrofferEvent, "on_server", cie_null, 0);
  rb_define_method(cIrofferEvent, "on_notice", cie_null, 0);
  rb_define_method(cIrofferEvent, "on_privmsg", cie_null, 0);
  /* accessors */
  rb_define_method(cIrofferEvent, "network", cie_network, 0);
  rb_define_method(cIrofferEvent, "inputline", cie_inputline, 0);
  rb_define_method(cIrofferEvent, "hostmask", cie_hostmask, 0);
  rb_define_method(cIrofferEvent, "nick", cie_nick, 0);
  rb_define_method(cIrofferEvent, "channel", cie_channel, 0);
  rb_define_method(cIrofferEvent, "message", cie_message, 0);
  rb_define_method(cIrofferEvent, "config", cie_config, 1);
  rb_define_method(cIrofferEvent, "added_file", cie_added_file, 0);
  rb_define_method(cIrofferEvent, "added_pack", cie_added_pack, 0);
  rb_define_method(cIrofferEvent, "info_pack", cie_info_pack, 2);
  rb_gc_register_address(&cIrofferEvent);
}

static void load_script(const char *name)
{
  struct stat st;

  updatecontext();

  if (!name)
    return;

  if (stat(name, &st) < 0) {
    myruby_loaded = -1;
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "access ruby_script '%s' failed: %s",
             name, strerror(errno));
    return;
  }
  myruby_time = st.st_mtime;
  ruby_init_loadpath();
  ruby_script(name);
#if USE_RUBYVERSION < 19
  rb_load_file(name);
  myruby_status = ruby_exec();
#else
  rb_load(rb_str_new(name, strlen(name)), 0);
#endif
  if (myruby_status != 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "ruby_exec failed with %d: %s",
             myruby_status, strerror(errno));
    myruby_loaded = 1;
    return;
  }

  myruby_loaded = 1;
  oIrofferEvent = rb_class_new_instance(0, NULL, cIrofferEvent);
  rb_define_variable("objIrofferEvent", &oIrofferEvent);
}

static void check_script(const char *name)
{
  struct stat st;

  updatecontext();

  if (!name)
    return;

  if (stat(name, &st) < 0) {
    myruby_loaded = -1;
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "access ruby_script '%s' failed: %s",
             name, strerror(errno));
    return;
  }
  if (myruby_time == st.st_mtime)
    return;

  load_script(name);
}

static unsigned int do_on_event(VALUE userobject, const char *event)
{
  ID method;
  VALUE skip;

  if (NIL_P(userobject))
    return 0;

  method = rb_intern(event);
  if (! rb_respond_to(userobject, method))
    return 0;

  skip = rb_funcall_protected(userobject, method, 0, NULL);
  return (skip != Qtrue) ? 0 : 1;
}

/* push a server line to the ruby class */
int do_myruby_server(char *line)
{
  if (myruby_loaded == 0)
    return 0;

  cLine = line;
  return do_on_event(oIrofferEvent, "on_server");
}

/* push a notice to the ruby class */
int do_myruby_notice(char *line)
{
  if (myruby_loaded == 0)
    return 0;

  cLine = line;
  return do_on_event(oIrofferEvent, "on_notice");
}

/* push a privmsg to the ruby class */
int do_myruby_privmsg(char *line)
{
  if (myruby_loaded == 0)
    return 0;

  cLine = line;
  return do_on_event(oIrofferEvent, "on_privmsg");
}

/* call the ruby class after adding a pack */
int do_myruby_added(char *filename, unsigned int pack)
{
  if (myruby_loaded == 0)
    return 0;

  cFile = filename;
  cPack = pack;
  return do_on_event(oIrofferEvent, "on_added");
}

/* reload the scrip on rehash */
void rehash_myruby(int check)
{
  if (check) {
    check_script(gdata.ruby_script);
    return;
  }
  load_script(gdata.ruby_script);
}

/* load the interpreter and the script */
void startup_myruby(void)
{
  ruby_init();
  // dumps the version info to stdout
  ruby_show_version();

  myruby_status = 0;
  myruby_loaded = 0;

  //define that callback below
  rb_define_global_function("iroffer_input", cie_inputline, 0);
  rb_define_global_function("iroffer_privmsg", cie_privmsg, 2);
  Init_IrofferEvent();
  load_script(gdata.ruby_script);
}

/* cleanup interpreter */
void shutdown_myruby(void)
{
  myruby_status = ruby_cleanup(myruby_status);
#if USE_RUBYVERSION < 19
  ruby_finalize();
#else
  /* segfaults with ruby19 */
#endif
}

#ifndef WITHOUT_HTTP
/* call a ruby script as CGI via HTTP */
unsigned int http_ruby_script(const char *name, const char *output)
{
  struct stat st;
  char *tempstr;
  int rc;

  updatecontext();

  if (!name)
    return 1;

  if (stat(name, &st) < 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "access ruby_script '%s' failed: %s",
             name, strerror(errno));
    return 1;
  }
  ruby_init_loadpath();
  ruby_script(name);
  tempstr = mycalloc(maxtextlength);
  snprintf(tempstr, maxtextlength, "$stdout = File.new(\"%s\", \"w+\")", output);
  rb_eval_string_protect(tempstr, &rc);
  mydelete(tempstr);
#if USE_RUBYVERSION < 19
  rb_load_file(name);
  rc = ruby_exec();
#else
  rb_load(rb_str_new(name, strlen(name)), 0);
#endif
  if (rc != 0) {
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "ruby_exec failed with %d: %s",
             rc, strerror(errno));
    iroffer_ruby_errro(rc);
    return 1;
  }
  rb_eval_string_protect("$stdout.close", &rc);
  return 0;
}
#endif /* WITHOUT_HTTP */

#endif /* USE_RUBY */

/* EOF */
