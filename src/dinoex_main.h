/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2011 Dirk Meyer
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

void command_options(int argc, char *const *argv);

int add_password(const char *hash);

void check_osname(const char *sysname);

void
#ifdef __GNUC__
__attribute__ ((noreturn))
#endif
exit_iroffer(unsigned int gotsignal);

/* End of File */
