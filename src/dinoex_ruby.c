/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2009 Dirk Meyer
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
VALUE cIrofferConfig;
VALUE cIrofferEvent;
VALUE oIrofferEvent = Qnil;

static void iroffer_ruby_errro(int error)
{
  VALUE lasterr;
  VALUE inclass;
  VALUE message;
  VALUE ary;
  int c;

  if (error == 0)
    return;

  lasterr = rb_gv_get("$!");
  inclass = rb_class_path(CLASS_OF(lasterr));
  message = rb_obj_as_string(lasterr);
  outerror(OUTERROR_TYPE_WARN_LOUD,
           "error ruby_script: class=%s, message=%s",
           RSTRING(inclass)->ptr, RSTRING(message)->ptr);

  if (!NIL_P(ruby_errinfo)) {
    ary = rb_funcall(ruby_errinfo, rb_intern("backtrace"), 0);
    for (c=0; c<RARRAY(ary)->len; c++) {
      outerror(OUTERROR_TYPE_WARN_LOUD,
               "backtrace from %s",
               RSTRING(RARRAY(ary)->ptr[c])->ptr);
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
    for (i = 0; i < argc; i++) {
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
  oIrofferEvent = tdata;
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

  if (has_joined_channels(0) < 1)
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

  if (has_joined_channels(0) < 1)
    return Qfalse;

  if (name[0] != '#')
    return Qfalse;

  for (ch = irlist_get_head(&(gnetwork->channels));
       ch;
       ch = irlist_get_next(ch)) {
    if (strcasecmp(ch->name, name) == 0) {
      writeserver(WRITESERVER_SLOW, "MODE %s %s", name, msg);
      return Qtrue;
    }
  }
  return Qfalse;
}

static void Init_IrofferEvent(void) {
  cIrofferEvent = rb_define_class("IrofferEvent", rb_cObject);
  rb_define_singleton_method(cIrofferEvent, "new", cie_new, 0);
  rb_define_method(cIrofferEvent, "privmsg", cie_privmsg, 2);
  rb_define_method(cIrofferEvent, "warning", cie_warning, 1);
  rb_define_method(cIrofferEvent, "mode", cie_mode, 2);
  rb_define_method(cIrofferEvent, "on_server", cie_null, 0);
  rb_define_method(cIrofferEvent, "on_notice", cie_null, 0);
  rb_define_method(cIrofferEvent, "on_privmsg", cie_null, 0);
  rb_define_method(cIrofferEvent, "network", cie_network, 0);
  rb_define_method(cIrofferEvent, "inputline", cie_inputline, 0);
  rb_define_method(cIrofferEvent, "hostmask", cie_hostmask, 0);
  rb_define_method(cIrofferEvent, "nick", cie_nick, 0);
  rb_define_method(cIrofferEvent, "channel", cie_channel, 0);
  rb_define_method(cIrofferEvent, "message", cie_message, 0);
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
  ruby_script("embedded");
  rb_load_file(name);
  myruby_loaded = 1;

  myruby_status = ruby_exec();
  if (myruby_status == 0)
    return;

  outerror(OUTERROR_TYPE_WARN_LOUD,
           "ruby_exec failed with %d: %s",
           myruby_status, strerror(errno));
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

static VALUE
do_on_event(VALUE userobject, const char *event)
{
  ID method;

  if (NIL_P(userobject))
    return Qnil;

  method = rb_intern(event);
  if (! rb_respond_to(userobject, method))
    return Qnil;

  rb_funcall_protected(userobject, method, 0, NULL);
  return Qtrue;
}

void do_myruby_server(char *line)
{
  if (myruby_loaded == 0)
    return;

  cLine = line;
  do_on_event(oIrofferEvent, "on_server");
}

void do_myruby_notice(char *line)
{
  if (myruby_loaded == 0)
    return;

  cLine = line;
  do_on_event(oIrofferEvent, "on_notice");
}

void do_myruby_privmsg(char *line)
{
  if (myruby_loaded == 0)
    return;

  cLine = line;
  do_on_event(oIrofferEvent, "on_privmsg");
}

void rehash_myruby(int check)
{
  if (check) {
    check_script(gdata.ruby_script);
    return;
  }
  load_script(gdata.ruby_script);
}

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

void shutdown_myruby(void)
{
  myruby_status = ruby_cleanup(myruby_status);
  ruby_finalize();
}

#endif /* USE_RUBY */

/* EOF */
