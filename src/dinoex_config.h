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

void
#ifdef __GNUC__
__attribute__ ((format(printf, 1, 2)))
#endif
dump_line(const char *format, ...);

char *print_config_key(const char *key);

void getconfig_set(const char *line);

void config_dump(void);
void config_reset(void);
size_t config_expand(char *buffer, size_t max, int print);
void config_startup(void);

extern const char *current_config;
extern unsigned long current_line;
extern unsigned int current_bracket;
extern unsigned int current_network;

/* EOF */
