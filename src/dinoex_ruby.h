/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2007-2011 Dirk Meyer
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

#ifdef USE_RUBY
int do_myruby_server(char *line);
int do_myruby_notice(char *line);
int do_myruby_privmsg(char *line);
int do_myruby_added(char *filename, unsigned int pack);
int do_myruby_packlist(void);
int do_myruby_ruby(const userinput * const u);
void rehash_myruby(int check);
void startup_myruby(void);
void shutdown_myruby(void);
#ifndef WITHOUT_HTTP
unsigned int http_ruby_script(const char *name, const char *output);
#endif /* WITHOUT_HTTP */
#endif /* USE_RUBY */

/* End of File */
