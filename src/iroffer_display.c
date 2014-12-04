/*      
 * iroffer by David Johnson (PMG) 
 * Copyright (C) 1998-2005 David Johnson 
 * 
 * By using this file, you agree to the terms and conditions set
 * forth in the GNU General Public License.  More information is    
 * available in the LICENSE file.
 * 
 * If you received this file without documentation, it can be
 * downloaded from http://iroffer.org/
 * 
 * @(#) iroffer_display.c 1.48@(#)
 * pmg@wellington.i202.centerclick.org|src/iroffer_display.c|20050116225130|34559
 * 
 */

/* include the headers */
#include "iroffer_config.h"
#include "iroffer_defines.h"
#include "iroffer_headers.h"
#include "iroffer_globals.h"
#include "dinoex_utilities.h"


void initscreen(int startup, int clear) {
   struct winsize win;
   char *tempstr;
   struct termios tio;
   
   updatecontext();
   
   if (gdata.background == 2) return;
   
   /* clear */
   gdata.attop = 0;

   if ( gdata.noscreen )
      return;
   
   if (clear)
     {
        tostdout(IRVT_CURSOR_HOME0 IRVT_ERASE_DOWN IRVT_SCROLL_ALL);
     }

   gdata.termlines = 80;
   gdata.termcols = 24;
   
   if (isatty(fileno(stdin)) == 0)
     {
       outerror(OUTERROR_TYPE_CRASH,
                "cannot use foreground mode on a non-terminal");
     }
   
   if ((ioctl(0, TIOCGWINSZ, &win) == 0) && win.ws_col && win.ws_row) {
      gdata.termcols = win.ws_col;
      gdata.termlines = win.ws_row;
      }
   
   if (tcgetattr(fileno(stdin), &tio) < 0)
     {
       outerror(OUTERROR_TYPE_CRASH,
                "can't get terminal settings: %s", strerror(errno));
     }
   
   if (startup)
     {
       gdata.startup_tio = tio;
     }
   
   tio.c_lflag &= ~(ECHO | ICANON);
   tio.c_cc[VMIN] = 0;
   tio.c_cc[VTIME] = 4;
   
   if (tcsetattr(fileno(stdin), TCSANOW, &tio) < 0)
     {
       outerror(OUTERROR_TYPE_CRASH,
                "can't set terminal settings: %s", strerror(errno));
     }
   
   /* last 2 lines */
   tempstr = mymalloc(maxtextlength);
   getstatusline(tempstr,maxtextlength);
   tempstr[between(0,gdata.termcols-4,maxtextlength-1)] = '\0';
   tostdout(IRVT_CURSOR_HOME1 "[ %-*s ]",
            gdata.termlines - 1,
            gdata.termcols - 4, tempstr);
   mydelete(tempstr);

   drawbot();

   /* scrolling */
   tostdout(IRVT_SCROLL_LINES1, gdata.termlines - 2);
   
}

void uninitscreen(void)
{
  if (gdata.background == 2)
    return;
  
  if ( gdata.noscreen )
    return;
  
  printf(IRVT_SCROLL_ALL IRVT_CURSOR_HOME1 "\n", gdata.termlines);
  
  tcsetattr(fileno(stdin), TCSANOW, &gdata.startup_tio);
   
  return;
}

void checktermsize(void) {
   int notok;
   struct winsize win;
   
   updatecontext();
   
   notok = 0;
   if (gdata.background == 2) return;
   
   if ((ioctl(0, TIOCGWINSZ, &win) == 0) && win.ws_col && win.ws_row) {
      if (gdata.termcols != win.ws_col) notok++;
      if (gdata.termlines != win.ws_row) notok++;
      if (notok) {
         gdata.termcols = win.ws_col;
         gdata.termlines = win.ws_row;
         initscreen(0, 0);
         if (!gdata.attop) gototop();
         tostdout("** Window Size Changed To: %ux%u\n", gdata.termcols, gdata.termlines);
         gotobot();
         }
      }
   }

void gototop (void) {
   gdata.attop = 1;
   if ( gdata.noscreen )
      return;
   if (gdata.background == 2)
      return;
   
   tostdout(IRVT_CURSOR_HOME1, gdata.termlines - 2);
   }

static const char *get_console_nick(void)
{
  const char *mynick;

  mynick = gdata.networks[0].user_nick;
  if (mynick == NULL)
    mynick = gdata.networks[0].config_nick;
  if (mynick == NULL)
    mynick = gdata.config_nick;
  if (mynick == NULL)
    mynick = "";
  return mynick;
}

void drawbot(void)
{
  const char *mynick;
  char tchar = 0;
  unsigned int len;
  unsigned int maxlen;
  unsigned int nicklen;
  
  if ((gdata.background == 2) || gdata.noscreen)
    {
      return;
    }
  
  len = strlen(gdata.console_input_line);
  
  mynick = get_console_nick();
  nicklen = 16 + strlen(mynick);
  
  if (gdata.termcols - 1 < nicklen)
    {
      maxlen = 0;
    }
  else
    {
      maxlen = gdata.termcols - 1 - nicklen;
    }
  
  if ((len > maxlen))
    {
      tchar = gdata.console_input_line[maxlen];
      gdata.console_input_line[maxlen] = '\0';
    }
  
  /* draw bottom line */
  tostdout(IRVT_CURSOR_HOME1 "[ iroffer (%s) > %-*s]",
           gdata.termlines,
           mynick,
           gdata.termcols - nicklen,
           gdata.console_input_line);
  
  /* move cursor */
  tostdout(IRVT_CURSOR_HOME2,
           gdata.termlines,
           ((gdata.curcol > maxlen) ? maxlen : gdata.curcol) + nicklen);
  
  if ((len > maxlen))
    {
      gdata.console_input_line[maxlen] = tchar;
    }
  
  return;
}

void gotobot (void)
{
  const char *mynick;
  unsigned int maxlen;
  unsigned int nicklen;
  
  gdata.attop = 0;
  if ( gdata.noscreen )
    return;
  if (gdata.background == 2)
    return;
  
  mynick = get_console_nick();
  nicklen = 16 + strlen(mynick);
  maxlen = gdata.termcols - 1 - nicklen;
  
  tostdout(IRVT_CURSOR_HOME2, gdata.termlines,
           ((gdata.curcol > maxlen) ? maxlen : gdata.curcol) + nicklen);
    }


void tostdout_disable_buffering(void)
{
  if (!gdata.stdout_buffer_init)
    {
      return;
    }
  
  set_socket_nonblocking(fileno(stdout),0);
  
  tostdout_write();
  
  ir_boutput_delete(&gdata.stdout_buffer);
  gdata.stdout_buffer_init = 0;
  
  return;
}

void tostdout_write(void)
{
  if (!gdata.stdout_buffer_init)
    {
      fflush(stdout);
      return;
    }
  
  ir_boutput_attempt_flush(&gdata.stdout_buffer);
  
  return;
}

static void
#ifdef __GNUC__
__attribute__ ((format(printf, 1, 0)))
#endif
vtostdout(const char *format, va_list ap)
{
  int len;
  char tempstr[maxtextlength+1];
  
  if (!gdata.stdout_buffer_init)
    {
      vprintf(format,ap);
      fflush(stdout);
    }
  else
    {
      len = vsnprintf(tempstr,maxtextlength,format,ap);
      
      if ((len < 0) || (len >= (int)maxtextlength))
        {
          len = 0;
        }
      
      ir_boutput_write(&gdata.stdout_buffer, tempstr, len);
      
    }
  
  return;
}

void tostdout(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vtostdout(format, args);
  va_end(args);
}


void parseconsole(void)
{
  static char console_escape_seq[maxtextlengthshort];
  
  unsigned char tempbuffa[INPUT_BUFFER_LENGTH];
  int length;
  unsigned int linelength;
  userinput ui;
  unsigned int i, j;
  
  updatecontext();
  
  bzero(console_escape_seq, sizeof(console_escape_seq));
  if (is_fd_readable(fileno(stdin)))
    {
      memset(tempbuffa, 0, INPUT_BUFFER_LENGTH);
      length = read (fileno(stdin), &tempbuffa, INPUT_BUFFER_LENGTH);
      
      if (length < 1)
        {
          outerror(OUTERROR_TYPE_CRASH,"read from stdin failed: %s",(length<0) ? strerror(errno) : "EOF!");
        }
      
      linelength = strlen(gdata.console_input_line);
      for (i=0; i<(unsigned int)length; i++)
        {
          if (console_escape_seq[0] != '\0')
            {
              size_t esc_len = strlen(console_escape_seq);
              
              if (esc_len < (maxtextlengthshort-2))
                {
                  console_escape_seq[esc_len]=tempbuffa[i];
                  esc_len++;
                  if (((tempbuffa[i] >= 'a') && (tempbuffa[i] <= 'z')) ||
                      ((tempbuffa[i] >= 'A') && (tempbuffa[i] <= 'Z')))
                    {
                      /* process sequence */
                      if (console_escape_seq[1] == '[')
                        {
                          if (console_escape_seq[esc_len-1] == 'A')
                            {
                              /* up */
                              int count;
                              if (esc_len > 3)
                                {
                                  count=atoi(&console_escape_seq[2]);
                                }
                              else
                                {
                                  count=1;
                                }
                              
                              while(count--)
                                {
                                  if (gdata.console_history_offset)
                                    {
                                      gdata.console_history_offset--;
                                      strncpy(gdata.console_input_line,
                                              irlist_get_nth(&gdata.console_history,
                                                             gdata.console_history_offset),
                                              INPUT_BUFFER_LENGTH-1);
                                      linelength = strlen(gdata.console_input_line);
                                      gdata.curcol = linelength;
                                    }
                                }
                              
                            }
                          else if (console_escape_seq[esc_len-1] == 'B')
                            {
                              /* down */
                              int count;
                              if (esc_len > 3)
                                {
                                  count=atoi(&console_escape_seq[2]);
                                }
                              else
                                {
                                  count=1;
                                }
                              
                              while(count--)
                                {
                                  gdata.console_history_offset++;
                                  if (gdata.console_history_offset < irlist_size(&gdata.console_history))
                                    {
                                      strncpy(gdata.console_input_line,
                                              irlist_get_nth(&gdata.console_history,
                                                             gdata.console_history_offset),
                                              INPUT_BUFFER_LENGTH-1);
                                      linelength = strlen(gdata.console_input_line);
                                      gdata.curcol = linelength;
                                    }
                                  else
                                    {
                                      memset(gdata.console_input_line, 0,
                                             INPUT_BUFFER_LENGTH);
                                      linelength = 0;
                                      gdata.curcol = 0;
                                    }
                                  
                                  gdata.console_history_offset = min2(gdata.console_history_offset,irlist_size(&gdata.console_history));
                                }
                            }
                          else if (console_escape_seq[esc_len-1] == 'C')
                            {
                              /* right */
                              int count;
                              if (esc_len > 3)
                                {
                                  count=atoi(&console_escape_seq[2]);
                                }
                              else
                                {
                                  count=1;
                                }
                              gdata.curcol += count;
                              if (gdata.curcol > linelength)
                                gdata.curcol = linelength;
                            }
                          else if (console_escape_seq[esc_len-1] == 'D')
                            {
                              /* left */
                              unsigned int count;
                              if (esc_len > 3)
                                {
                                  count=atoi(&console_escape_seq[2]);
                                }
                              else
                                {
                                  count=1;
                                }
                              if (count <= gdata.curcol)
                                gdata.curcol -= count;
                            }
                          /* else ignore */
                        }
                      /* else ignore */
                      
                      memset(console_escape_seq, 0, maxtextlengthshort);
                    }
                }
              else
                {
                  /* sequence is too long, ignore */
                  memset(console_escape_seq, 0, maxtextlengthshort);
                }
            }
          else if (tempbuffa[i] == '\x1b')
            {
              console_escape_seq[0]=tempbuffa[i];
            }
          else if ((tempbuffa[i] == '\t') && gdata.console_input_line[0])
            {
              if (!gdata.attop) gototop();
              j = u_expand_command();
              gdata.curcol += j;
              linelength += j;
            }
          else if (tempbuffa[i] == '\n')
            {
              char *hist;
              
              hist = irlist_get_tail(&gdata.console_history);
              
              if ((!hist || strcmp(hist,gdata.console_input_line)) && gdata.console_input_line[0])
                {
                  hist = irlist_add(&gdata.console_history, linelength+1);
                  strncpy(hist, gdata.console_input_line, linelength);
                }
              
              if (irlist_size(&gdata.console_history) > MAX_HISTORY_SIZE)
                {
                  irlist_delete(&gdata.console_history, irlist_get_head(&gdata.console_history));
                }
              
              gdata.console_history_offset = irlist_size(&gdata.console_history);
              
              if (!gdata.attop) gototop();
              
              u_fillwith_console(&ui,gdata.console_input_line);
              u_parseit(&ui);
              
              gdata.curcol=0;
              memset(gdata.console_input_line, 0, INPUT_BUFFER_LENGTH);
            }
          else if (isprintable(tempbuffa[i]))
            {
              if (linelength < (INPUT_BUFFER_LENGTH-2))
                {
                  for (j=linelength; j>gdata.curcol; j--)
                    {
                      gdata.console_input_line[j] = gdata.console_input_line[j-1];
                    }
                  gdata.console_input_line[gdata.curcol]=tempbuffa[i];
                  gdata.curcol++;
                  linelength++;
                }
            }
          else if (tempbuffa[i] == gdata.startup_tio.c_cc[VINTR])
            {
            }
          else if (tempbuffa[i] == gdata.startup_tio.c_cc[VQUIT])
            {
            }
          else if (tempbuffa[i] == gdata.startup_tio.c_cc[VERASE])
            {
              if (gdata.curcol)
                {
                  for (j=gdata.curcol-1; j<linelength; j++)
                    {
                      gdata.console_input_line[j] = gdata.console_input_line[j+1];
                    }
                  linelength--;
                  gdata.console_input_line[linelength] = '\0';
                  gdata.curcol--;
                }
            }
          else if (tempbuffa[i] == gdata.startup_tio.c_cc[VWERASE])
            {
              while (gdata.curcol && (gdata.console_input_line[gdata.curcol-1] == ' '))
                {
                  for (j=gdata.curcol-1; j<linelength; j++)
                    {
                      gdata.console_input_line[j] = gdata.console_input_line[j+1];
                    }
                  linelength--;
                  gdata.console_input_line[linelength] = '\0';
                  gdata.curcol--;
                }
              while (gdata.curcol && (gdata.console_input_line[gdata.curcol-1] != ' '))
                {
                  for (j=gdata.curcol-1; j<linelength; j++)
                    {
                      gdata.console_input_line[j] = gdata.console_input_line[j+1];
                    }
                  linelength--;
                  gdata.console_input_line[linelength] = '\0';
                  gdata.curcol--;
                }
              while (gdata.curcol && (gdata.console_input_line[gdata.curcol-1] == ' '))
                {
                  for (j=gdata.curcol-1; j<linelength; j++)
                    {
                      gdata.console_input_line[j] = gdata.console_input_line[j+1];
                    }
                  linelength--;
                  gdata.console_input_line[linelength] = '\0';
                  gdata.curcol--;
                }
            }
          else if (tempbuffa[i] == gdata.startup_tio.c_cc[VKILL])
            {
            }
          else if (tempbuffa[i] == gdata.startup_tio.c_cc[VEOF])
            {
            }
          else if (tempbuffa[i] == gdata.startup_tio.c_cc[VSTART])
            {
            }
          else if (tempbuffa[i] == gdata.startup_tio.c_cc[VSTOP])
            {
            }
          else if (tempbuffa[i] == gdata.startup_tio.c_cc[VSUSP])
            {
            }
          else if (tempbuffa[i] == gdata.startup_tio.c_cc[VLNEXT])
            {
            }
          else if (tempbuffa[i] == gdata.startup_tio.c_cc[VREPRINT])
            {
            }
          else if (tempbuffa[i] == gdata.startup_tio.c_cc[VDISCARD])
            {
            }
          else if (tempbuffa[i] == '\x01') /* Ctrl-A */
            {
              gdata.curcol = 0;
            }
          else if (tempbuffa[i] == '\x05') /* Ctrl-E */
            {
              gdata.curcol = linelength;
            }
        }
      
      drawbot();
    }
}

/* End of File */

