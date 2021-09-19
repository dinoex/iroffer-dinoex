/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2007-2021 Dirk Meyer
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is
 * available in the LICENSE file.
 *
 * If you received this file without documentation, it can be
 * downloaded from https://iroffer.net/
 *
 * SPDX-FileCopyrightText: 2007-2021 Dirk Meyer
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * $Id$
 *
 */

#ifdef USE_RUBY
unsigned int do_myruby_server(char *line);
unsigned int do_myruby_notice(char *line);
unsigned int do_myruby_privmsg(char *line);
unsigned int do_myruby_added(char *filename, unsigned int pack);
unsigned int do_myruby_upload_done(char *filename);
unsigned int do_myruby_packlist(void);
unsigned int do_myruby_ruby(const userinput * const u);
void rehash_myruby(int check);
void startup_myruby(void);
void shutdown_myruby(void);
#ifndef WITHOUT_HTTP
unsigned int http_ruby_script(const char *name, const char *output);
#endif /* WITHOUT_HTTP */
#endif /* USE_RUBY */

/* End of File */
