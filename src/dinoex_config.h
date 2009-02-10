/*
 * by Dirk Meyer (dinoex)
 * Copyright (C) 2004-2009 Dirk Meyer
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

char *print_config_key(const char *key);
void config_startup(void);

void getconfig_set(const char *line);

extern const char *current_config;
extern long current_line;

/* EOF */
