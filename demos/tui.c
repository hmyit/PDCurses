/*
***************************************************************************
* This file comprises part of PDCurses. PDCurses is Public Domain software.
* You may use this code for whatever purposes you desire. This software
* is provided AS IS with NO WARRANTY whatsoever.
* Should this software be used in another application, an acknowledgement
* that PDCurses code is used would be appreciated, but is not mandatory.
*
* Any changes which you make to this software which may improve or enhance
* it, should be forwarded to the current maintainer for the benefit of 
* other users.
*
* The only restriction placed on this code is that no distribution of
* modified PDCurses code be made under the PDCurses name, by anyone
* other than the current maintainer.
* 
* See the file maintain.er for details of the current maintainer.
***************************************************************************
*/
/********************************* tui.c ************************************/
/*
 * File   : tui.c      'textual user interface'
 * Author : P.J. Kunst  (kunst@prl.philips.nl)
 * Date   : 25-02-93
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <ctype.h>
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tui.h"

void statusmsg Args((char *));
int waitforkey Args((void));
void rmerror Args((void));

#ifdef PDCDEBUG
char *rcsid_tui  = "$Id: tui.c,v 1.12 2006/01/11 23:19:53 wmcbrine Exp $";
#endif

#if defined(__unix) && !defined(__DJGPP__)
#include <unistd.h>
#endif

#ifdef A_COLOR
# define TITLECOLOR         1                   /* color pair indices */
# define MAINMENUCOLOR      (2 | A_BOLD)
# define MAINMENUREVCOLOR   (3 | A_BOLD | A_REVERSE)
# define SUBMENUCOLOR       (4 | A_BOLD)
# define SUBMENUREVCOLOR    (5 | A_BOLD | A_REVERSE)
# define BODYCOLOR          6
# define STATUSCOLOR        (7 | A_BOLD)
# define INPUTBOXCOLOR      8
# define EDITBOXCOLOR       (9 | A_BOLD | A_REVERSE)
#else
# define TITLECOLOR         0                   /* color pair indices */
# define MAINMENUCOLOR      (A_BOLD)
# define MAINMENUREVCOLOR   (A_BOLD | A_REVERSE)
# define SUBMENUCOLOR       (A_BOLD)
# define SUBMENUREVCOLOR    (A_BOLD | A_REVERSE)
# define BODYCOLOR          0
# define STATUSCOLOR        (A_BOLD)
# define INPUTBOXCOLOR      0
# define EDITBOXCOLOR       (A_BOLD | A_REVERSE)
#endif


#define  th   1                   /* title window height */
#define  mh   1                   /* main menu height */
#define  sh   2                   /* status window height */
#define  bh   (LINES-th-mh-sh-1)  /* body window height */
#define  bw   COLS                /* body window width */


/******************************* STATIC ************************************/

static WINDOW *wtitl, *wmain, *wbody, *wstat; /* title, menu, body, status win*/
static int nexty, nextx;
static int key = ERR, ch = ERR;
static bool quit = FALSE;
static bool incurses = FALSE;

#ifndef PDCURSES
static char wordchar Args((void))
{
	return 0x17;	/* ^W */ 
}
#endif

#if __STDC__
static char *padstr(char *s, int length)
#else
static char *padstr(s, length)
char *s;
int length;
#endif
{
	static char buf[MAXSTRLEN];
	char fmt[10];

	sprintf(fmt, (int)strlen(s) > length ? "%%.%ds" : "%%-%ds", length);
	sprintf(buf, fmt, s);

	return buf;
}

#if __STDC__
static char *prepad(char *s, int length)
#else
static char *prepad(s, length)
char *s;
int length;
#endif
{
	int i;
	char *p = s;

	if (length > 0)
	{
#if __STDC__
		memmove((void *)(s + length), (const void *)s, strlen(s) + 1);
#else
		memmove(s + length, s, strlen(s) + 1);
#endif
		for (i = 0; i < length; i++)
			*p++ = ' ';
	}

	return s;
}

#if __STDC__
static void rmline(WINDOW *win, int nr)   /* keeps box lines intact */
#else
static void rmline(win, nr)
WINDOW *win;
int nr;
#endif
{
	mvwaddstr(win, nr, 1, padstr(" ", bw - 2));
	wrefresh(win);
}

static void initcolor Args((void))
{
#ifdef A_COLOR
	if (has_colors())
		start_color();

	/* foreground, background */

	init_pair(TITLECOLOR       & ~A_ATTR, COLOR_BLACK, COLOR_CYAN);      
	init_pair(MAINMENUCOLOR    & ~A_ATTR, COLOR_WHITE, COLOR_CYAN);    
	init_pair(MAINMENUREVCOLOR & ~A_ATTR, COLOR_WHITE, COLOR_BLACK);
	init_pair(SUBMENUCOLOR     & ~A_ATTR, COLOR_WHITE, COLOR_CYAN);    
	init_pair(SUBMENUREVCOLOR  & ~A_ATTR, COLOR_WHITE, COLOR_BLACK);   
	init_pair(BODYCOLOR        & ~A_ATTR, COLOR_WHITE, COLOR_BLUE);      
	init_pair(STATUSCOLOR      & ~A_ATTR, COLOR_WHITE, COLOR_CYAN);   
	init_pair(INPUTBOXCOLOR    & ~A_ATTR, COLOR_BLACK, COLOR_CYAN);
	init_pair(EDITBOXCOLOR     & ~A_ATTR, COLOR_WHITE, COLOR_BLACK);
#endif
}

#if __STDC__
static void setcolor(WINDOW *win, chtype color)
#else
static void setcolor(win, color)
WINDOW *win;
chtype color;
#endif
{
	chtype attr = color & A_ATTR;  /* extract Bold, Reverse, Blink bits */

#ifdef A_COLOR
	attr &= ~A_REVERSE;  /* ignore reverse, use colors instead! */
	wattrset(win, COLOR_PAIR(color & A_CHARTEXT) | attr);
#else
	attr &= ~A_BOLD;     /* ignore bold, gives messy display on HP-UX */
	wattrset(win, attr);
#endif
}

#if __STDC__
static void colorbox(WINDOW *win, chtype color, int hasbox)
#else
static void colorbox(win, color, hasbox)
WINDOW *win;
chtype color;
int hasbox;
#endif
{
	int maxy, maxx;
	chtype attr = color & A_ATTR;  /* extract Bold, Reverse, Blink bits */

	setcolor(win, color);

#ifdef A_COLOR
	if (has_colors())
		wbkgd(win,
			COLOR_PAIR(color & A_CHARTEXT) | (attr & ~A_REVERSE));
	else
#endif
		wbkgd(win, color);

	werase(win); 

	getmaxyx(win, maxy, maxx);

	if (hasbox && (maxy > 2))
		box(win, 0, 0);

	touchwin(win);
	wrefresh(win);
}

static void idle Args((void))
{
	char buf[MAXSTRLEN];
	time_t t;
	struct tm *tp;

	if (time (&t) == -1)
		return;  /* time not available */

	tp = localtime(&t);
	sprintf(buf, " %.2d-%.2d-%.4d  %.2d:%.2d:%.2d",
		tp->tm_mday, tp->tm_mon + 1, tp->tm_year + 1900,
		tp->tm_hour, tp->tm_min, tp->tm_sec);

	mvwaddstr(wtitl, 0, bw - strlen(buf) - 2, buf);
	wrefresh(wtitl); 
}

#if __STDC__
static void menudim(menu *mp, int *lines, int *columns)
#else
static void menudim(mp, lines, columns)
menu *mp;
int *lines;
int *columns;
#endif
{
	int n, l, mmax = 0;

	for (n=0; mp->func; n++, mp++)
		if ((l = strlen(mp->name)) > mmax) mmax = l;

	*lines = n;
	*columns = mmax + 2;
}

#if __STDC__
static void setmenupos(int y, int x)
#else
static void setmenupos(y, x)
int y, x;
#endif
{
	nexty = y;
	nextx = x;
}

#if __STDC__
static void getmenupos(int *y, int *x)
#else
static void getmenupos(y, x)
int *y, *x;
#endif
{
	*y = nexty;
	*x = nextx;
}

#if __STDC__
static int hotkey(char *s)
#else
static int hotkey(s)
char *s;
#endif
{
	int c0 = *s;	/* if no upper case found, return first char */

	for (; *s; s++)
		if (isupper(*s))
			break;

	return *s ? *s : c0;
}

#if __STDC__
static void repaintmenu(WINDOW *wmenu, menu *mp)
#else
static void repaintmenu(wmenu, mp)
WINDOW *wmenu;
menu *mp;
#endif
{
	int i;
	menu *p = mp;

	for (i = 0; p->func; i++, p++)
		mvwaddstr(wmenu, i + 1, 2, p->name);

	touchwin(wmenu);
	wrefresh(wmenu);
}

#if __STDC__
static void repaintmainmenu(int width, menu *mp)
#else
static void repaintmainmenu(width, mp)
int width;
menu *mp;
#endif
{
	int i;
	menu *p = mp;

	for (i = 0; p->func; i++, p++)
		mvwaddstr(wmain, 0, i * width,
			prepad(padstr(p->name, width - 1), 1));

	touchwin (wmain); wrefresh (wmain);
}

static void mainhelp Args((void))
{
#ifdef ALT_X
	statusmsg("Use arrow keys and Enter to select (Alt-X to quit)");
#else
	statusmsg("Use arrow keys and Enter to select");
#endif
}

static void hidecursor Args((void))
{
#if defined(PDCURSES) || defined(SYSV)
	curs_set(0);
#endif
}

static void normalcursor Args((void))
{
#if defined(PDCURSES) || defined(SYSV)
	curs_set(1);
#endif
}

static void insertcursor Args((void))
{
#if defined(PDCURSES) || defined(SYSV)
	curs_set(2);
#endif
}

#if __STDC__
static void mainmenu(menu *mp)
#else
static void mainmenu(mp)
menu *mp;
#endif
{
	int nitems, barlen, old = -1, cur = 0, c, cur0;

	menudim(mp, &nitems, &barlen);
	repaintmainmenu(barlen, mp);

	while (!quit)
	{
		if (cur != old)
		{
			if (old != -1)
			{
				mvwaddstr (wmain, 0, old * barlen, 
					prepad(padstr(mp[old].name,
						barlen - 1), 1));

				statusmsg(mp[cur].desc);
			}
			else
				mainhelp();

			setcolor (wmain, MAINMENUREVCOLOR);

			mvwaddstr(wmain, 0, cur * barlen, 
				prepad(padstr(mp[cur].name, barlen - 1), 1));

			setcolor(wmain, MAINMENUCOLOR);
			old = cur;
			wrefresh(wmain);
		}

		switch (c = (key != ERR ? key : waitforkey()))
		{
		case KEY_DOWN:
		case '\n':			/* menu item selected */
			touchwin(wbody);
			wrefresh(wbody);
			rmerror();
			setmenupos(th + mh, cur * barlen);
			normalcursor();
			(mp[cur].func)();	/* perform function */
			hidecursor();

			switch (key)
			{
			case KEY_LEFT:
				cur = (cur + nitems - 1) % nitems;
				key = '\n';
				break;

			case KEY_RIGHT:
				cur = (cur + 1) % nitems;
				key = '\n';
				break;

			default:
				key = ERR;
			}

			repaintmainmenu(barlen, mp);
			old = -1;
			break;

		case KEY_LEFT:
			cur = (cur + nitems - 1) % nitems;
			break;

		case KEY_RIGHT:
			cur = (cur + 1) % nitems;
			break;

		case KEY_ESC:
			mainhelp();
			break;

		default:
			cur0 = cur;

			do
			{
				cur = (cur + 1) % nitems;

			} while ((cur != cur0) &&
				(hotkey(mp[cur].name) != toupper(c)));

			if (hotkey(mp[cur].name) == toupper(c))
				key = '\n';
		}

	}

	rmerror();
	touchwin(wbody);
	wrefresh(wbody);
}

static void cleanup Args((void))	/* cleanup curses settings */
{
	if (incurses)
	{
		delwin(wtitl);
		delwin(wmain);
		delwin(wbody);
		delwin(wstat);
		normalcursor();
		endwin();
#ifdef XCURSES
		XCursesExit();
#endif
		incurses = FALSE;
	}
}


/******************************* EXTERNAL **********************************/

void clsbody Args((void))
{
	werase(wbody);
	wmove(wbody, 0, 0);
}

int bodylen Args((void))
{
	int maxy, maxx;

	getmaxyx(wbody, maxy, maxx);
	return maxy;
}

WINDOW *bodywin Args((void))
{
	return wbody;
}

void rmerror Args((void))
{
	rmline(wstat, 0);
}

void rmstatus Args((void))
{
	rmline(wstat, 1);
}

#if __STDC__
void titlemsg(char *msg)
#else
void titlemsg(msg)
char *msg;
#endif
{
	mvwaddstr(wtitl, 0, 2, padstr(msg, bw - 3));
	wrefresh(wtitl);
}

#if __STDC__
void bodymsg(char *msg)
#else
void bodymsg(msg)
char *msg;
#endif
{
	waddstr(wbody, msg);
	wrefresh(wbody);
}

#if __STDC__
void errormsg(char *msg)
#else
void errormsg(msg)
char *msg;
#endif
{
	beep();
	mvwaddstr(wstat, 0, 2, padstr(msg, bw - 3));
	wrefresh(wstat);
}

#if __STDC__
void statusmsg(char *msg)
#else
void statusmsg(msg)
char *msg;
#endif
{
	mvwaddstr(wstat, 1, 2, padstr(msg, bw - 3));
	wrefresh(wstat);
}

bool keypressed Args((void))
{
	ch = wgetch(wbody);

	return ch != ERR;
}

int getkey Args((void))
{
	int c = ch;

	ch = ERR;
#ifdef ALT_X
	quit = (c == ALT_X);	/* PC only ! */
#endif
	return c;
}

int waitforkey Args((void))
{
	do idle(); while (!keypressed());
	return getkey();
}

void DoExit Args((void))	/* terminate program */
{
	quit = TRUE;
}

#if __STDC__
void domenu(menu *mp)
#else
void domenu(mp)
menu *mp;
#endif
{
	int y, x, nitems, barlen, mheight, mw, old = -1, cur = 0, cur0;
	bool stop = FALSE;
	WINDOW *wmenu;

	hidecursor();
	getmenupos(&y, &x);
	menudim(mp, &nitems, &barlen);
	mheight = nitems + 2;
	mw = barlen + 2;
	wmenu = newwin(mheight, mw, y, x);
	colorbox(wmenu, SUBMENUCOLOR, 1);
	repaintmenu(wmenu, mp);

	key = ERR;

	while (!stop && !quit)
	{
		if (cur != old)
		{
			if (old != -1)
				mvwaddstr(wmenu, old + 1, 1, 
					prepad(padstr(mp[old].name,
						barlen - 1), 1));

			setcolor(wmenu, SUBMENUREVCOLOR);
			mvwaddstr(wmenu, cur + 1, 1,
				prepad(padstr(mp[cur].name, barlen - 1), 1));

			setcolor(wmenu, SUBMENUCOLOR);
			statusmsg(mp[cur].desc);

			old = cur;
			wrefresh(wmenu);
		}

		switch (key = ((key != ERR) ? key : waitforkey()))
		{
		case '\n':			/* menu item selected */
			touchwin(wbody);
			wrefresh(wbody);
			setmenupos(y + 1, x + 1);
			rmerror();

			key = ERR;
			normalcursor();
			(mp[cur].func)();	/* perform function */
			hidecursor();

			repaintmenu(wmenu, mp);

			old = -1;
			break;

		case KEY_UP:
			cur = (cur + nitems - 1) % nitems;
			key = ERR;
			break;

		case KEY_DOWN:
			cur = (cur + 1) % nitems;
			key = ERR;
			break;

		case KEY_ESC:
		case KEY_LEFT:
		case KEY_RIGHT:
			if (key == KEY_ESC)
				key = ERR;	/* return to prev submenu */

			stop = TRUE;
			break;

		default:
			cur0 = cur;

			do
			{
				cur = (cur + 1) % nitems;

			} while ((cur != cur0) &&
				(hotkey(mp[cur].name) != toupper((int)key)));

			key = (hotkey(mp[cur].name) == toupper((int)key)) ?
				'\n' : ERR;
		}

	}

	rmerror();
	delwin(wmenu);
	touchwin(wbody);
	wrefresh(wbody);
}

#if __STDC__
void startmenu(menu *mp, char *mtitle)
#else
void startmenu(mp, mtitle)
menu *mp;
char *mtitle;
#endif
{
	initscr();
	incurses = TRUE;
	initcolor();

	wtitl = subwin(stdscr, th, bw, 0, 0);
	wmain = subwin(stdscr, mh, bw, th, 0);
	wbody = subwin(stdscr, bh, bw, th + mh, 0);
	wstat = subwin(stdscr, sh, bw, th + mh + bh, 0);

	colorbox(wtitl, TITLECOLOR, 0);
	colorbox(wmain, MAINMENUCOLOR, 0);
	colorbox(wbody, BODYCOLOR, 0);
	colorbox(wstat, STATUSCOLOR, 0);

	if (mtitle)
		titlemsg(mtitle);

	cbreak();		/* direct input (no newline required)... */
	noecho();		/* ... without echoing */
	hidecursor();		/* hide cursor (if possible) */
	nodelay(wbody, TRUE);	/* don't wait for input... */
	halfdelay(10);		/* ...well, no more than a second, anyway */
	keypad(wbody, TRUE);	/* enable cursor keys */
	scrollok(wbody, TRUE);	/* enable scrolling in main window */

	leaveok(stdscr, TRUE);
	leaveok(wtitl, TRUE);
	leaveok(wmain, TRUE);
	leaveok(wstat, TRUE);

	mainmenu(mp);

	cleanup();
}


/*man-start*********************************************************************

  weditstr()     - edit string

  PDCurses Description:
	The initial value of 'str' with a maximum length of 'field'-1,
	which is supplied by the calling routine, is editted. The user's 
	erase (^H), kill (^U) and delete word (^W) chars are interpreted. 
	The PC insert or Tab keys toggle between insert and edit mode.
	Escape aborts the edit session, leaving 'str' unchanged.
	Enter, Up or Down Arrow are used to accept the changes to 'str'.
	NOTE: editstr(), mveditstr(), and mvweditstr() are macros.

  PDCurses Return Value:
	These functions return the input terminating character on 
	success (Escape, Enter, Up or Down Arrow) and ERR on error.

  PDCurses Errors:
	It is an error to call this function with a NULL window pointer.
	The length of the initial 'str' must not exceed 'field'-1.

  Portability:
        
	PDCurses	int weditstr(WINDOW *win, char *str, int field);
	X/Open Dec '88 
	BSD Curses    
	SYS V Curses 

**man-end**********************************************************************/


#if __STDC__
static void repainteditbox(WINDOW *win, int x, char *buf)
#else
static void repainteditbox(win, x, buf)
WINDOW *win;
int x;
char *buf;
#endif
{
	int maxy, maxx;

	getmaxyx(win, maxy, maxx);
	werase(win);
	mvwprintw(win, 0, 0, "%s", padstr(buf, maxx));
	wmove(win, 0, x);
	wrefresh(win); 
}

#if __STDC__
int weditstr(WINDOW *win, char *buf, int field)
#else
int weditstr(win, buf, field)
WINDOW *win;
char *buf;
int field;
#endif
{
	char org[MAXSTRLEN], *tp, *bp = buf;
	bool defdisp = TRUE, stop = FALSE, insert = FALSE;
	int cury, curx, begy, begx, oldattr;
	WINDOW *wedit;
	int c = 0;

	if ((field >= MAXSTRLEN) || (buf == NULL) ||
	    ((int)strlen(buf) > field - 1))
		return ERR;

	strcpy(org, buf);	/* save original */

	wrefresh(win);
	getyx(win, cury, curx);
	getbegyx(win, begy, begx);

	wedit = subwin(win, 1, field, begy + cury, begx + curx);
	oldattr = wedit->_attrs;
	colorbox(wedit, EDITBOXCOLOR, 0);

	normalcursor();

	while (!stop)
	{
		repainteditbox(wedit, bp - buf, buf);

		switch (c = waitforkey())
		{
		case KEY_ESC:
			strcpy(buf, org);	/* restore original */
			stop = TRUE;
			break;

		case '\n':
		case KEY_UP:
		case KEY_DOWN:
			stop = TRUE;
			break;

		case KEY_LEFT:
			if (bp > buf)
				bp--;
			break;

		case KEY_RIGHT:
			defdisp = FALSE;
			if (bp - buf < (int)strlen(buf))
				bp++;
			break;

		case '\t':			/* TAB -- because insert
						   is broken on HPUX */
		case KEY_IC:			/* enter insert mode */
		case KEY_EIC:			/* exit insert mode */
			defdisp = FALSE;
			insert = !insert;

			if (insert)
				insertcursor();
			else
				normalcursor();
			break;

		default:
			if (c == erasechar())		/* backspace, ^H */
			{
				if (bp > buf)
				{
#if __STDC__
					memmove((void *)(bp - 1),
						(const void *)bp,
						strlen(bp) + 1);
#else
					memmove(bp-1, bp, strlen(bp) + 1);
#endif
					bp--;
				}
			}
			else if (c == killchar())	/* ^U */
			{
				bp = buf;
				*bp = '\0';
			}
			else if (c == wordchar())	/* ^W */
			{
				tp = bp;

				while ((bp > buf) && (*(bp - 1) == ' ')) 
					bp--;
				while ((bp > buf) && (*(bp - 1) != ' ')) 
					bp--;
#if __STDC__
				memmove((void *)bp, (const void *)tp, 
					strlen(tp) + 1);
#else
				memmove(bp, tp, strlen(tp) + 1);
#endif
			}
			else if (isprint(c))
			{
				if (defdisp)
				{
					bp = buf;
					*bp = '\0';
					defdisp = FALSE;
				}

				if (insert)
				{
					if ((int)strlen(buf) < field - 1)
					{
#if __STDC__
						memmove((void *)(bp + 1), 
							(const void *)bp,
							strlen(bp) + 1);
#else
						memmove(bp + 1, bp, 
							strlen(bp) + 1);
#endif
						*bp++ = c;
					}
				}
				else if (bp - buf < field - 1)
				{
					/* append new string terminator */

					if (!*bp)
						bp[1] = '\0';
            
					*bp++ = c;
				}
			}
		}
	}

	wattrset(wedit, oldattr);
	repainteditbox(wedit, bp - buf, buf);
	delwin(wedit);

	return c;
}

#if __STDC__
WINDOW *winputbox(WINDOW *win, int nlines, int ncols)
#else
WINDOW *winputbox(win, nlines, ncols)
WINDOW *win;
int nlines, ncols;
#endif
{
	WINDOW *winp;
	int cury, curx, begy, begx;

	getyx(win, cury, curx);
	getbegyx(win, begy, begx);

	winp = newwin(nlines, ncols, begy + cury, begx + curx);
	colorbox(winp, INPUTBOXCOLOR, 1);

	return winp;
}

#if __STDC__
int getstrings(char *desc[], char *buf[], int field)
#else
int getstrings(desc, buf, field)
char *desc[];
char *buf[];
int field;
#endif
{
	WINDOW *winput;
	int oldy, oldx, maxy, maxx, nlines, ncols, i, n, l, mmax = 0;
	int c = 0;
	bool stop = FALSE;

	for (n = 0; desc[n]; n++)
		if ((l = strlen(desc[n])) > mmax)
			mmax = l;

	nlines = n + 2; ncols = mmax + field + 4;
	getyx(wbody, oldy, oldx);
	getmaxyx(wbody, maxy, maxx);

	winput = mvwinputbox(wbody, (maxy - nlines) / 2, (maxx - ncols) / 2, 
		nlines, ncols);

	for (i = 0; i < n; i++)
		mvwprintw(winput, i + 1, 2, "%s", desc[i]);

	i = 0;

	while (!stop)
	{
		switch (c = mvweditstr(winput, i+1, mmax+3, buf[i], field))
		{
		case KEY_ESC:
			stop = TRUE;
			break;

		case KEY_UP:
			i = (i + n - 1) % n;
			break;

		case '\n':
		case '\t':
		case KEY_DOWN:
			if (++i == n)
				stop = TRUE;	/* all passed? */
		}
	}

	delwin(winput);
	touchwin(wbody);
	wmove(wbody, oldy, oldx);
	wrefresh(wbody);

	return c;
}
