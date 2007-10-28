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

int set_config_bool(const char *key, const char *text);
int set_config_int(const char *key, const char *text);
int set_config_string(const char *key, char *text);
int set_config_list(const char *key, char *text);

void set_default_network_name(void);

int set_config_func(const char *key, char *text);

void config_startup(void);

/* EOF */
