/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2008 Dirk Meyer
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

#include "ruby.h"

int myruby_status;
int myruby_loaded;

char *cLine;
VALUE cIrofferConfig;

static void ci_free(void *p)
{
}

static char *rb_obj_as_cstring_protect(VALUE objval)
{
  char *str;
  VALUE rval;
  int state = 0;

  rval = rb_protect( (VALUE (*)(VALUE))rb_obj_as_string, objval, state);
  str = STR2CSTR(rval);
  return str;
}

static VALUE cic_buffer(VALUE self)
{
  return rb_str_new(cLine, strlen(cLine));
}

static VALUE cic_new(VALUE class)
{
  VALUE tdata = Data_Wrap_Struct(class, 0, ci_free, &gdata);
  rb_obj_call_init(tdata, 0, 0);
  return tdata;
}

static void Init_IrofferConfig() {
  cIrofferConfig = rb_define_class("IrofferConfig", rb_cObject);
  rb_define_singleton_method(cIrofferConfig, "new", cic_new, 0);
  rb_define_method(cIrofferConfig, "buffer", cic_buffer, 0);
  rb_gc_register_address(&cIrofferConfig);
}

static VALUE cf_iroffer_input(void) 
{
  return rb_str_new(cLine, strlen(cLine));
}

static VALUE cf_iroffer_privmsg(VALUE module, VALUE rname, VALUE rmsg)
{
  VALUE vname;
  VALUE vmsg;
  char *name;
  char *msg;
  channel_t *ch;

  if (NIL_P(rname) || NIL_P(rmsg))
    return Qnil;

  vname = rb_obj_as_string(rname);
  name = STR2CSTR(vname);
  vmsg = rb_obj_as_string(rmsg);
  msg = STR2CSTR(vmsg);
  if (!name || !msg)
    return Qnil;

  if (gnetwork->serverstatus != SERVERSTATUS_CONNECTED)
    return Qnil;

  if (has_joined_channels(0) < 1)
    return Qnil;

  if (name[0] != '#') {
    privmsg_slow(name, "%s", msg);
    return Qnil;
  }

  for (ch = irlist_get_head(&(gnetwork->channels));
       ch;
       ch = irlist_get_next(ch)) {
    if (strcasecmp(ch->name, name) == 0) {
      privmsg_chan(ch, "%s", msg);
      return Qnil;
    }
  }
  return Qnil;
}

static void load_script(const char *name)
{
  struct stat st;

  updatecontext();

  if (!name)
    return;

  if (stat(name,&st) < 0) {
    myruby_loaded = -1;
    outerror(OUTERROR_TYPE_WARN_LOUD,
             "access ruby_script '%s' failed: %s",
             name, strerror(errno));
    return;
  }
  ruby_init_loadpath();
  ruby_script("embedded");
  rb_load_file(name);
  myruby_loaded = 1;
}

void do_myruby(char *line)
{
  if (myruby_loaded == 0)
    return;

  cLine = line;
#ifndef DEBUG_RUBY
  myruby_status = ruby_exec();
  if (myruby_status == 0)
    return;

  outerror(OUTERROR_TYPE_WARN_LOUD,
           "ruby_exec failed with %d: %s",
           myruby_status, strerror(errno));
#else
  ruby_run();
#endif
}

void rehash_myruby(void)
{
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
  rb_define_global_function("iroffer_input", cf_iroffer_input, 0);
  rb_define_global_function("iroffer_privmsg", cf_iroffer_privmsg, 2);
  Init_IrofferConfig();
  load_script(gdata.ruby_script);
}

void shutdown_myruby(void)
{
  myruby_status = ruby_cleanup(myruby_status);
  ruby_finalize();
}

#endif /* USE_RUBY */

/* EOF */
