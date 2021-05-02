/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <xcb/res.h>
#ifdef __OpenBSD__
#include <sys/sysctl.h>
#include <kvm.h>
#endif /* __OpenBSD */

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                               * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define ISVISIBLEONTAG(C, T)    ((C->tags & T))
#define ISVISIBLE(C)            ISVISIBLEONTAG(C, C->mon->tagset[C->mon->seltags])
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)
#define XRDB_LOAD_COLOR(R,V)    if (XrmGetResource(xrdb, R, NULL, &type, &value) == True) { \
                                  if (value.addr != NULL && strnlen(value.addr, 8) == 7 && value.addr[0] == '#') { \
                                    int i = 1; \
                                    for (; i <= 6; i++) { \
                                      if (value.addr[i] < 48) break; \
                                      if (value.addr[i] > 57 && value.addr[i] < 65) break; \
                                      if (value.addr[i] > 70 && value.addr[i] < 97) break; \
                                      if (value.addr[i] > 102) break; \
                                    } \
                                    if (i == 7) { \
                                      strncpy(V, value.addr, 7); \
                                      V[7] = '\0'; \
                                    } \
                                  } \
                                }

#define OPAQUE                  1.0

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define _NET_SYSTEM_TRAY_ORIENTATION_HORZ 0

/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY      0
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_FOCUS_IN             4
#define XEMBED_MODALITY_ON         10

#define XEMBED_MAPPED              (1 << 0)
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_WINDOW_DEACTIVATE    2

#define VERSION_MAJOR               0
#define VERSION_MINOR               0
#define XEMBED_EMBEDDED_VERSION (VERSION_MAJOR << 16) | VERSION_MINOR

/* enums */
enum { Manager, Xembed, XembedInfo, XLast }; /* Xembed atoms */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeBar, SchemeTag, SchemeBorder, SchemeFocus, SchemeUnfocus }; /* color schemes */
enum { NetSupported, NetSystemTray, NetSystemTrayOP, NetSystemTrayOrientation, NetSystemTrayVisual,
	   NetWMName, NetWMState, NetWMFullscreen, NetActiveWindow, NetWMWindowType, NetWMWindowTypeDock,
	   NetSystemTrayOrientationHorz, NetWMWindowTypeDialog, NetClientList, NetWMCheck, NetLast }; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText,
       ClkClientWin, ClkRootWin, ClkLast }; /* clicks */

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
	char name[256];
	float mina, maxa;
	float cfact;
	int x, y, w, h;
	int sfx, sfy, sfw, sfh; /* stored float geometry, used on mode revert */
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int bw, oldbw;
	unsigned int tags;
	unsigned int switchtotag;
	int floatx, floaty, floatw, floath;
	int floatborderpx;
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen, isactfullscreen, isterminal, noswallow;
	int ignorecfgreqpos, ignorecfgreqsize;
	char scratchkey;
	pid_t pid;
	Client *next;
	Client *snext;
	Client *swallowing;
	Monitor *mon;
	Window win;
};

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	unsigned int signum;
	void (*func)(const Arg *);
	const Arg arg;
} Signal;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

typedef struct Pertag Pertag;
struct Monitor {
	char ltsymbol[16];
	int ltaxis[3];
	float mfact;
	int nmaster;
	int num;
	int by;               /* bar geometry */
	int eby;              /* extrabar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	int gappx;            /* gaps between windows */
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	int showbar;
    int showebar;
	int topbar;
	Client *clients;
	Client *sel;
	Client *stack;
	Monitor *next;
	Window barwin;
    Window ebarwin;
	const Layout *lt[2];
	Pertag *pertag;
};

typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	unsigned int tags;
	unsigned int switchtotag;
	int isfloating;
	int isterminal;
	int noswallow;
	int monitor;
	const char scratchkey;
	int floatx, floaty, floatw, floath;
	int floatborderpx;
} Rule;

typedef struct Systray Systray;
struct Systray {
	Window win;
	Client *icons;
};

typedef struct TabGroup TabGroup;
struct TabGroup {
	int x;
	int n;
	int i;
	int active;
	int start;
	int end;
	struct TabGroup * next;
};

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int *bw, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachabove(Client *c);
static void attachaside(Client *c);
static void attachbelow(Client *c);
static void attachbottom(Client *c);
static void attachtop(Client *c);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static void copyvalidchars(char *text, char *rawtext);
static Monitor *createmon(void);
static void demoncleaner(const Arg *arg);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawebar(char *text, Monitor *m, int xpos);
static void drawbar(Monitor *m);
static void drawbars(void);
static void drawbartabgroups(Monitor *m, int x, int stw, int passx);
static void drawbartab(Monitor *m, Client *c, int x, int w, int tabgroup_active);
static void drawbartaboptionals(Monitor *m, Client *c, int x, int w, int tabgroup_active);
static void drawtaggrid(Monitor *m, int *x_pos, unsigned int occ);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static Client *findbefore(Client *c);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static Atom getatomprop(Client *c, Atom prop);
static int getdwmblockspid();
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static unsigned int getsystraywidth();
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void incnmaster(const Arg *arg);
static void inplacerotate(const Arg *arg);
static void keypress(XEvent *e);
static int fake_signal(void);
static void killclient(const Arg *arg);
static void loadxrdb(void);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void mirrorlayout(const Arg *arg);
static void monocle(Monitor *m);
static void motionnotify(XEvent *e);
static void moveresize(const Arg *arg);
static void moveresizeedge(const Arg *arg);
static void movemouse(const Arg *arg);
static Client *nexttagged(Client *c);
static Client *nexttiled(Client *c);
static void pop(Client *) __attribute__((unused));
static Client *prevc(Client *c, float f);
static Client *nextc(Client *c, float f);
static void propertynotify(XEvent *e);
static void pushdown(const Arg *arg);
static void pushup(const Arg *arg);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void replaceclient(Client *old, Client *new);
static void resize(Client *c, int x, int y, int w, int h, int bw, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h, int bw);
static void resizemouse(const Arg *arg);
static void removesystrayicon(Client *i);
static void resizerequest(XEvent *e);
static void restack(Monitor *m);
static int riodraw(Client *c, const char slopstyle[]);
static void rioposition(Client *c, int x, int y, int w, int h);
static void rioresize(const Arg *arg);
static void riospawn(const Arg *arg);
static void rotatelayoutaxis(const Arg *arg);
static void run(void);
static void runorraise(const Arg *arg);
static void scan(void);
static int sendevent(Window w, Atom proto, int m, long d0, long d1, long d2, long d3, long d4);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setactfullscreen(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setgaps(const Arg *arg);
static void setlayout(const Arg *arg);
static void setcfact(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void shadowfloat(Client *c);
static void seturgent(Client *c, int urg);
static void showhide(Client *c);
static void sigchld(int unused);
static void sigdwmblocks(const Arg *arg);
static void spawn(const Arg *arg);
static pid_t spawncmd(const Arg *arg);
static void spawnscratch(const Arg *arg);
static int status2dtextlength(char *stext);
static void switchcol(const Arg *arg);
static void switchtag(const Arg *arg);
static Monitor *systraytomon(Monitor *m);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *);
static void togglebar(const Arg *arg);
static void toggleebar(const Arg *arg);
static void togglebars(const Arg *arg);
static void togglefloating(const Arg *arg);
static void togglescratch(const Arg *arg);
static void togglefullscr(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void transfer(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatesystray(void);
static void updatesystrayicongeom(Client *i, int w, int h);
static void updatesystrayiconstate(Client *i, XPropertyEvent *ev);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static Client *wintosystrayicon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void xinitvisual();
static void xrdb(const Arg *arg);
static void zoom(const Arg *arg);

static int swallow(Client *p, Client *c);
static void unswallow(Client *c);
static pid_t getparentprocess(pid_t p);
static int isdescprocess(pid_t p, pid_t c);
static Client *swallowingclient(Window w);
static Client *termforwin(const Client *c);
static pid_t winpid(Window w);

/* variables */
static Client *prevzoom = NULL;
static const char broken[] = "broken";
static char stext[1024];
static char rawstext[1024];
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh, blw = 0;      /* bar geometry */
static int lrpad;            /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static int riodimensions[4] = { -1, -1, -1, -1 };
static pid_t riopid = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[ResizeRequest] = resizerequest,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast], xatom[XLast];
static Atom wunshadow;
static int running = 1;
static Cur *cursor[CurLast];
static Clr **scheme;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static Window root, wmcheckwin;
static xcb_connection_t *xcon;

static Systray *systray = NULL;
static unsigned long systrayorientation = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;

static int useargb = 0;
static Visual *visual;
static int depth;
static Colormap cmap;

static int dwmblockssig;
pid_t dwmblockspid = 0;

unsigned int esep = 0, eblock = 0;

/* configuration, allows nested code to access above variables */
#include "config.h"

struct Pertag {
	int ltaxes[LENGTH(tags) + 1][3];
	unsigned int curtag, prevtag; /* current and previous tag */
	int nmasters[LENGTH(tags) + 1]; /* number of windows in master area */
	float mfacts[LENGTH(tags) + 1]; /* mfacts per tag */
	unsigned int sellts[LENGTH(tags) + 1]; /* selected layouts */
	const Layout *ltidxs[LENGTH(tags) + 1][2]; /* matrix of tags and layouts indexes  */
	int showbars[LENGTH(tags) + 1]; /* display bar for the current tag */
	int showebars[LENGTH(tags) + 1]; /* display ebar for the current tag */
};

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* function implementations */
void
applyrules(Client *c)
{
	const char *class, *instance;
	unsigned int i;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->isfloating = 0;
	c->tags = 0;
	c->scratchkey = 0;
	XGetClassHint(dpy, c->win, &ch);
	class    = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name  ? ch.res_name  : broken;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		&& (!r->class || strstr(class, r->class))
		&& (!r->instance || strstr(instance, r->instance)))
		{
			c->isterminal = r->isterminal;
			c->noswallow  = r->noswallow;
			c->isfloating = r->isfloating;
			c->tags |= r->tags;
			c->scratchkey = r->scratchkey;
			c->floatborderpx = r->floatborderpx;
			c->floatx = r->floatx;
			c->floaty = r->floaty;
			c->floatw = r->floatw;
			c->floath = r->floath;
			for (m = mons; m && m->num != r->monitor; m = m->next);
			if (m)
				c->mon = m;
			if (r->switchtotag) {
				Arg a = { .ui = r->tags };
				c->switchtotag = selmon->tagset[selmon->seltags];
				view(&a);
			}
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);

	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int *bw, int interact)
{
	int savew, saveh;
	int baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > sw)
			*x = sw - WIDTH(c);
		if (*y > sh)
			*y = sh - HEIGHT(c);
		if (*x + *w + 2 * *bw < 0)
			*x = 0;
		if (*y + *h + 2 * *bw < 0)
			*y = 0;
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * *bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * *bw <= m->wy)
			*y = m->wy;
	}
	if (*h < bh)
		*h = bh;
	if (*w < bh)
		*w = bh;
	if ((resizehints && m->gappx > resizehintsbm * borderpx && m ->lt[m->sellt]->arrange != monocle) || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		savew = *w;
		saveh = *h;
		if (!baseismin) { /* temporarily remove base dimensions */
			savew -= c->basew;
			saveh -= c->baseh;
		}
		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h)
				savew = saveh * c->maxa + 0.5;
			else if (c->mina < (float)*h / *w)
				saveh = savew * c->mina + 0.5;
		}
		if (baseismin) { /* increment calculation requires this */
			savew -= c->basew;
			saveh -= c->baseh;
		}
		/* adjust for increment value */
		if (c->incw)
			savew -= savew % c->incw;
		if (c->inch)
			saveh -= saveh % c->inch;
		/* restore base dimensions */
		savew = MAX(savew + c->basew, c->minw);
		saveh = MAX(saveh + c->baseh, c->minh);
		if (c->maxw)
			savew = MIN(savew, c->maxw);
		if (c->maxh)
			saveh = MIN(saveh, c->maxh);
		if (saveh < *h)
			*h = saveh;
		if (savew < *w)
			*w = savew;
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h || *bw != c->bw;
}

void
arrange(Monitor *m)
{
	if (m)
		showhide(m->stack);
	else for (m = mons; m; m = m->next)
		showhide(m->stack);
	if (m) {
		arrangemon(m);
		restack(m);
	} else for (m = mons; m; m = m->next)
		arrangemon(m);
}

void
arrangemon(Monitor *m)
{
	Client *c;
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
	else
		/* <>< case; rather than providing an arrange function and upsetting other logic that tests for its presence, simply add borders here */
		for (c = selmon->clients; c; c = c->next)
			if (ISVISIBLE(c) && c->bw == 0)
				resize(c, c->x, c->y, c->w - 2*borderpx, c->h - 2*borderpx, borderpx, 0);
}

void
attach(Client *c)
{
	c->next = c->mon->clients;
	c->mon->clients = c;
}

void
attachabove(Client *c)
{
	if (c->mon->sel == NULL || c->mon->sel == c->mon->clients || c->mon->sel->isfloating) {
		attach(c);
		return;
	}

	Client *at;
	for (at = c->mon->clients; at->next != c->mon->sel; at = at->next);
	c->next = at->next;
	at->next = c;
}

void
attachaside(Client *c) {
	Client *at = nexttagged(c);
	if(!at) {
		attach(c);
		return;
		}
	c->next = at->next;
	at->next = c;
}

void
attachbelow(Client *c)
{
	if(c->mon->sel == NULL || c->mon->sel == c || c->mon->sel->isfloating) {
		attach(c);
		return;
	}
	c->next = c->mon->sel->next;
	c->mon->sel->next = c;
}

void
attachbottom(Client *c)
{
	Client *below = c->mon->clients;
	for (; below && below->next; below = below->next);
	c->next = NULL;
	if (below)
		below->next = c;
	else
		c->mon->clients = c;
}

void
attachtop(Client *c)
{
	int n;
	Monitor *m = selmon;
	Client *below;

	for (n = 1, below = c->mon->clients;
		below && below->next && (below->isfloating || !ISVISIBLEONTAG(below, c->tags) || n != m->nmaster);
		n = below->isfloating || !ISVISIBLEONTAG(below, c->tags) ? n + 0 : n + 1, below = below->next);
	c->next = NULL;
	if (below) {
		c->next = below->next;
		below->next = c;
	}
	else
		c->mon->clients = c;
}

void
attachstack(Client *c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void
buttonpress(XEvent *e)
{
	unsigned int i, x, click, occ = 0;
	unsigned int columns;
	Arg arg = {0};
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;
	int stw = getsystraywidth();

	columns = LENGTH(tags) / tagrows + ((LENGTH(tags) % tagrows > 0) ? 1 : 0);
	click = ClkRootWin;
	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	if (ev->window == selmon->barwin) {
		i = x = 0;
		if (drawtagmask & DRAWCLASSICTAGS) {
			for (c = m->clients; c; c = c->next)
				occ |= c->tags == 255 ? 0 : c->tags;
			do {
				/* do not reserve space for vacant tags */
				if (!(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
					continue;
				x += TEXTW(tags[i]);
			} while (ev->x >= x && ++i < LENGTH(tags));
		}
		if(i < LENGTH(tags) && (drawtagmask & DRAWCLASSICTAGS)) {
			click = ClkTagBar;
			arg.ui = 1 << i;
		} else if(ev->x < x + columns * bh / tagrows && (drawtagmask & DRAWTAGGRID)) {
			click = ClkTagBar;
			i = (ev->x - x) / (bh / tagrows);
			i = i + columns * (ev->y / (bh / tagrows));
			if (i >= LENGTH(tags)) {
				i = LENGTH(tags) - 1;
			}
			arg.ui = 1 << i;
		}
		else if(ev->x < x + blw + ((drawtagmask & DRAWTAGGRID) ? columns * bh / tagrows : 0))
			click = ClkLtSymbol;
		else
			drawbartabgroups(m, x + blw, stw, ev->x);
	} else if (ev->window == selmon->ebarwin) {
			click = ClkStatusText;
			char *text = rawstext;
			int i = -1;
			int x = 0;
			char ch;
			dwmblockssig = -1;
			while (text[++i]) {
				if ((unsigned char)text[i] < ' ') {
					ch = text[i];
					text[i] = '\0';
					x += status2dtextlength(text);
					text[i] = ch;
					text += i+1;
					i = -1;
					if (x >=  ev->x && dwmblockssig != -1)
						break;
					dwmblockssig = ch;
				}
			}
			if (dwmblockssig == -1)
				dwmblockssig = 0;
	} else if ((c = wintoclient(ev->window))) {
		focus(c);
		restack(selmon);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
		&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void
checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void
cleanup(void)
{
	Arg a = {.ui = ~0};
	Layout foo = { "", NULL };
	Monitor *m;
	size_t i;

	view(&a);
	selmon->lt[selmon->sellt] = &foo;
	for (m = mons; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	XUnmapWindow(dpy, m->ebarwin);
	XDestroyWindow(dpy, m->ebarwin);
    while (mons)
		cleanupmon(mons);
	if (showsystray && systray) {
		while (systray->icons)
			removesystrayicon(systray->icons);
		if (systray->win) {
			XUnmapWindow(dpy, systray->win);
			XDestroyWindow(dpy, systray->win);
		}
		free(systray);
	}
	for (i = 0; i < CurLast; i++)
		drw_cur_free(drw, cursor[i]);
	for (i = 0; i < LENGTH(colors) + 1; i++)
		free(scheme[i]);
	XDestroyWindow(dpy, wmcheckwin);
	drw_free(drw);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void
cleanupmon(Monitor *mon)
{
	Monitor *m;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
	XUnmapWindow(dpy, mon->barwin);
	XUnmapWindow(dpy, mon->ebarwin);
	XDestroyWindow(dpy, mon->barwin);
	XDestroyWindow(dpy, mon->ebarwin);
	free(mon->pertag);
	free(mon);
}

void
clientmessage(XEvent *e)
{
	XWindowAttributes wa;
	XSetWindowAttributes swa;
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);

	if (showsystray && cme->window == systray->win && cme->message_type == netatom[NetSystemTrayOP]) {
		/* add systray icons */
		if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
			if (!(c = (Client *)calloc(1, sizeof(Client))))
				die("fatal: could not malloc() %u bytes\n", sizeof(Client));
			if (!(c->win = cme->data.l[2])) {
				free(c);
				return;
			}

			/* Clear status bar to avoid artifacts beneath systray icons */
			drw_rect(drw, 0, 0, selmon->ww, bh, 1, 1);
			drw_map(drw, selmon->barwin, 0, 0, selmon->ww, bh);

			c->mon = selmon;
			c->next = systray->icons;
			systray->icons = c;
			XGetWindowAttributes(dpy, c->win, &wa);
			c->x = c->oldx = c->y = c->oldy = 0;
			c->w = c->oldw = wa.width;
			c->h = c->oldh = wa.height;
			c->oldbw = wa.border_width;
			c->bw = 0;
			c->isfloating = True;
			/* reuse tags field as mapped status */
			c->tags = 1;
			updatesizehints(c);
			updatesystrayicongeom(c, wa.width, wa.height);
			XAddToSaveSet(dpy, c->win);
			XSelectInput(dpy, c->win, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
			XClassHint ch ={"dwm", "dwm"};
			XSetClassHint(dpy, c->win, &ch);
			XReparentWindow(dpy, c->win, systray->win, 0, 0);
			/* use parents background color */
			swa.background_pixel  = scheme[BARBORDERS ? SchemeUnfocus : SchemeBar][ColBg].pixel;
			XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);
			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_EMBEDDED_NOTIFY, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
			XSync(dpy, False);
			setclientstate(c, NormalState);
			drawbar(selmon);
			updatesystray();
		}
		return;
	}

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		|| cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
				|| cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (c != selmon->sel && !c->isurgent)
			seturgent(c, 1);
	}
}

void
configure(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurenotify(XEvent *e)
{
	Monitor *m;
	XConfigureEvent *ev = &e->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == root) {
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if (updategeom() || dirty) {
			drw_resize(drw, sw, bh);
			updatebars();
			for (m = mons; m; m = m->next) {
				XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
				XMoveResizeWindow(dpy, m->ebarwin, m->wx, m->eby, m->ww, bh);
			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void
configurerequest(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = wintoclient(ev->window))) {
		if (ev->value_mask & CWBorderWidth)
			c->bw = ev->border_width;
		else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {

			m = c->mon;
			if (!c->ignorecfgreqpos) {
				if (ev->value_mask & CWX) {
					c->oldx = c->x;
					c->x = m->mx + ev->x;
				}
				if (ev->value_mask & CWY) {
					c->oldy = c->y;
					c->y = m->my + ev->y;
				}
			}
			if (!c->ignorecfgreqsize) {
				if (ev->value_mask & CWWidth) {
					c->oldw = c->w;
					c->w = ev->width;
				}
				if (ev->value_mask & CWHeight) {
					c->oldh = c->h;
					c->h = ev->height;
				}
			}
			if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
			if ((c->y + c->h) > m->my + m->mh && c->isfloating)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if (ISVISIBLE(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
		} else
			configure(c);
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, False);
}

void
copyvalidchars(char *text, char *rawtext)
{
	int i = -1, j = 0;

	while (rawtext[++i]) {
		if ((unsigned char)rawtext[i] >= ' ') {
			text[j++] = rawtext[i];
		}
	}
	text[j] = '\0';
}

Monitor *
createmon(void)
{
	Monitor *m;
	unsigned int i;

	m = ecalloc(1, sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = 1;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->showbar = showbar;
    m->showebar = showebar;
	m->topbar = topbar;
	m->gappx = gappx;
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
	m->ltaxis[0] = layoutaxis[0];
	m->ltaxis[1] = layoutaxis[1];
	m->ltaxis[2] = layoutaxis[2];
	m->pertag = ecalloc(1, sizeof(Pertag));
	m->pertag->curtag = m->pertag->prevtag = 1;
	/* init tags, bars, layouts, axes, nmasters, and mfacts */
	for (i = 0; i <= LENGTH(tags); i++) {
		m->pertag->nmasters[i] = m->nmaster;
		m->pertag->mfacts[i] = m->mfact;
		m->pertag->ltaxes[i][0] = m->ltaxis[0];
		m->pertag->ltaxes[i][1] = m->ltaxis[1];
		m->pertag->ltaxes[i][2] = m->ltaxis[2];
		m->pertag->ltidxs[i][0] = m->lt[0];
		m->pertag->ltidxs[i][1] = m->lt[1];
		m->pertag->sellts[i] = m->sellt;
		m->pertag->showbars[i] = m->showbar;
		m->pertag->showebars[i] = m->showebar;
	}
	return m;
}

void
demoncleaner(const Arg *arg)
{
	/* Clear status bar to avoid artifacts beneath systray icons */
	drw_rect(drw, 0, 0, selmon->ww, bh, 1, 1);
	drw_map(drw, selmon->barwin, 0, 0, selmon->ww, bh);

	toggleebar(0);
	togglebar(0);
	arrange(selmon);
	togglebar(0);
	toggleebar(0);
	arrange(selmon);
}

void
destroynotify(XEvent *e)
{
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((c = wintoclient(ev->window)))
		unmanage(c, 1);
	else if (showsystray && (c = wintosystrayicon(ev->window))) {
		removesystrayicon(c);
		updatesystray();
	}
	else if ((c = swallowingclient(ev->window)))
		unmanage(c->swallowing, 1);
}

void
detach(Client *c)
{
	Client **tc;

	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void
detachstack(Client *c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
		c->mon->sel = t;
	}
}

Monitor *
dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = selmon->next))
			m = mons;
	} else if (selmon == mons)
		for (m = mons; m->next; m = m->next);
	else
		for (m = mons; m->next != selmon; m = m->next);
	return m;
}

void
drawbar(Monitor *m)
{
	int indn;
	int w, x = 0, stw = 0;
	unsigned int i, occ = 0, urg = 0;
	Client *c;

	if (showsystray && m == systraytomon(m))
		stw = getsystraywidth();

	for (c = m->clients; c; c = c->next) {
		occ |= c->tags == 255 ? 0 : c->tags;
		if (c->isurgent)
			urg |= c->tags;
	}
	if (drawtagmask & DRAWCLASSICTAGS)
	for (i = 0; i < LENGTH(tags); i++) {
		/* do not draw vacant tags */
		if (!(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
		continue;

		indn = 0;
		w = TEXTW(tags[i]);
		if (m->tagset[m->seltags] & 1 << i) {
			drw_setscheme(drw, scheme[SchemeFocus]);
			drw_text(drw, x, 0, w, bh - (BARBORDERS == 1 ? 1 : 0), lrpad / 2, tags[i], urg & 1 << i);
			if (BARBORDERS == 1) {
				XSetForeground(drw->dpy, drw->gc,scheme[SchemeFocus][ColFloat].pixel);
				XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, 0, w, 1);
				XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, 0, 1, bh - 1);
			}
		} else {
			drw_setscheme(drw, scheme[BARBORDERS == 1 ? SchemeUnfocus : SchemeBar]);
			drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
		}
		for (c = m->clients; c; c = c->next) {
			if ((c->tags & (1 << i)) && (indn * 3 + 2 < bh)) {
				drw_rect(drw, x + 1, indn * 3 + 1, selmon->sel == c ? 5 : 2, 2, 1, urg & 1 << i);
				indn++;
			}
		}
		x += w;
	}
	if (drawtagmask & DRAWTAGGRID) {
		drawtaggrid(m,&x,occ);
	}
	w = blw = TEXTW(m->ltsymbol);
	drw_setscheme(drw, scheme[BARBORDERS ? SchemeUnfocus : SchemeBar]);
	x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);
	drawbartabgroups(m, x, stw, 0);
	drw_map(drw, m->barwin, 0, 0, m->ww - stw, bh);
	if (BARBORDERS == 1) {
		XSetForeground(drw->dpy, drw->gc,scheme[SchemeUnfocus][ColBg].pixel);
		XFillRectangle(drw->dpy, drw->drawable, drw->gc, 0, bh - 1, m->ww, 1);
	}

	if (showsystray) {
		/* Clear status bar to avoid artifacts beneath systray icons */
		drw_setscheme(drw, scheme[LENGTH(colors)]);
		drw->scheme[ColBg] = scheme[SchemeBar][bartheme ? ColFloat : ColBg];
		drw_rect(drw, m->ww - stw, 0, m->ww, bh, 1, 1);
		drw_map(drw, selmon->barwin, m->ww - stw, 0, m->ww, bh);

		updatesystray();
	}
}

void
drawbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbar(m);
}

void
drawebar(char* stext, Monitor *m, int xpos)
{
	int w, len;
	short isCode = 0;
	char *text;
	char *p;

	int x = 0, sep = 0, block = 0;
	int k = -1, i = -1;
	char ch, blocktext[1024];

	drw_rect(drw, 0, 0, selmon->ww, bh, 1, 1);
	len = strlen(stext);
	if (!(text = (char*) malloc(sizeof(char)*(len + 1))))
		die("malloc");

	p = text;

	while (stext[++k]) {
		blocktext[++i] = stext[k];
		if ((unsigned char)stext[k] < ' ') {
			ch = stext[k];
			stext[k] = blocktext[i] = '\0';
			while (blocktext[++i])
				blocktext[i] = '\0';
			block = status2dtextlength(stext);
			if (xpos && xpos >= sep && xpos <= sep + block) {
				esep = sep;
				eblock = block;
			}
			drw_setscheme(drw, scheme[LENGTH(colors)]);
			if (sep == esep && block == eblock && block) {
				drw->scheme[ColFg] = scheme[SchemeFocus][ColFg];
				drw->scheme[ColBg] = scheme[SchemeFocus][ColBg];
			} else {
				drw->scheme[ColFg] = scheme[(BARBORDERS ? SchemeUnfocus : SchemeBar)][ColFg];
				drw->scheme[ColBg] = scheme[(BARBORDERS ? SchemeUnfocus : SchemeBar)][ColBg];
			}

			/* process status text-element */

			copyvalidchars(text, blocktext);
			text[len] = '\0';
			i = -1;
			while (text[++i]) {
				if (text[i] == '^' && !isCode) {
					isCode = 1;
					text[i] = '\0';
					w = TEXTW(text) - lrpad;
					drw_text(drw, x, BARBORDERS == 1 ? -1 : 0, w, bh, 0, text, 0);
					x += w;
					/* process code */
					while (text[++i] != '^') {
						if (text[i] == 'c') {
							char buf[8];
							if (i + 7 >= len) {
								i += 7;
								len = 0;
								break;
							}
							memcpy(buf, (char*)text+i+1, 7);
							buf[7] = '\0';
							if (sep != esep || block != eblock)
								drw_clr_create(drw, &drw->scheme[ColFg], buf, alphas[(BARBORDERS ? SchemeUnfocus : SchemeBar)][ColFg]);
							i += 7;
						} else if (text[i] == 'b') {
							char buf[8];
							if (i + 7 >= len) {
								i += 7;
								len = 0;
								break;
							}
							memcpy(buf, (char*)text+i+1, 7);
							buf[7] = '\0';
							if (sep != esep || block != eblock)
								drw_clr_create(drw, &drw->scheme[ColBg], buf, alphas[(BARBORDERS ? SchemeUnfocus : SchemeBar)][ColBg]);
							i += 7;
						} else if (text[i] == 'd') {
							if (sep == esep && block == eblock && block) {
								drw->scheme[ColFg] = scheme[SchemeFocus][ColFg];
								drw->scheme[ColBg] = scheme[SchemeFocus][ColBg];
							} else {
								drw->scheme[ColFg] = scheme[(BARBORDERS ? SchemeUnfocus : SchemeBar)][ColFg];
								drw->scheme[ColBg] = scheme[(BARBORDERS ? SchemeUnfocus : SchemeBar)][ColBg];
							}
						} else if (text[i] == 'r') {
							int rx = atoi(text + ++i);
							while (text[++i] != ',');
							int ry = atoi(text + ++i);
							while (text[++i] != ',');
							int rw = atoi(text + ++i);
							while (text[++i] != ',');
							int rh = atoi(text + ++i);
							if (ry < 0)
								ry = 0;
							if (rx < 0)
								rx = 0;
							drw_rect(drw, rx + x, ry, rw, rh, 1, 0);
						} else if (text[i] == 'f') {
							x += atoi(text + ++i);
						}
					}
					text = text + i + 1;
					len -= i + 1;
					i = -1;
					isCode = 0;
					if (len <= 0)
						break;
				}
			}
			if (!isCode && len > 0) {
				w = TEXTW(text) - lrpad;
				drw_text(drw, x, BARBORDERS == 1 ? -1 : 0, w, bh, 0, text, 0);
				x += w;
			}
			i = -1;

			if (BARBORDERS == 1 && block > 0) {
				XSetForeground(drw->dpy, drw->gc,scheme[SchemeUnfocus][ColFloat].pixel);
				XFillRectangle(drw->dpy, drw->drawable, drw->gc, sep, 0, 1, bh - 1);
				XFillRectangle(drw->dpy, drw->drawable, drw->gc, sep, 0, block - 1, 1);
				XSetForeground(drw->dpy, drw->gc,scheme[SchemeBar][ColBorder].pixel);
				XFillRectangle(drw->dpy, drw->drawable, drw->gc, sep + block - 1, 0, 1, bh - 1);
				XFillRectangle(drw->dpy, drw->drawable, drw->gc, sep + block - 2, bh - 2, 1, 1);
				XFillRectangle(drw->dpy, drw->drawable, drw->gc, sep, bh - 1, block, 1);
			}
			sep += block;
			stext[k] = ch;
			stext += k+1;
			k = -1;
		}
	}

	copyvalidchars(text, stext);
	drw_setscheme(drw, scheme[LENGTH(colors)]);
	drw->scheme[ColFg] = scheme[(BARBORDERS ? SchemeUnfocus : SchemeBar)][ColFg];
	drw->scheme[ColBg] = scheme[(BARBORDERS ? SchemeUnfocus : SchemeBar)][ColBg];
	w = TEXTW(text) - lrpad;
	drw_text(drw, x, BARBORDERS == 1 ? -1 : 0, w, bh, 0, text, 0);
	x += w;

	if (BARBORDERS == 1 && statuslastblock == 1 && x - sep > 0) {
		XSetForeground(drw->dpy, drw->gc,scheme[SchemeUnfocus][ColFloat].pixel);
		XFillRectangle(drw->dpy, drw->drawable, drw->gc, sep, 0, 1, bh - 1);
		XFillRectangle(drw->dpy, drw->drawable, drw->gc, sep, 0, x - sep + lrpad/2 - 1, 1);
		XSetForeground(drw->dpy, drw->gc,scheme[SchemeBar][ColBorder].pixel);
		XFillRectangle(drw->dpy, drw->drawable, drw->gc, sep, bh - 1, x - sep + lrpad/2 - 1, 1);
		XFillRectangle(drw->dpy, drw->drawable, drw->gc, x + lrpad/2 - 1, 1, 1, bh - 2);
		XFillRectangle(drw->dpy, drw->drawable, drw->gc, x + lrpad/2 - 2, bh - 2, 1, 1);
	}
	drw_rect(drw, x, 0, selmon->ww - x, bh, 1, 1);

	free(p);

	drw_map(drw, m->ebarwin, 0, 0, m->ww, bh);
}

void
drawbartabgroups(Monitor *m, int x, int stw, int passx) {
	Client *c;
	TabGroup *tg_head = NULL, *tg, *tg2;
	int tabwidth, tabx, tabgroupwidth, bw;

	if (notileborder == 1 && m->gappx > borderpx)
		bw = 0;
	else
		bw = borderpx;

	// Calculate
	if (NULL != m->lt[m->sellt]->arrange) {
		for (c = m->clients; c; c = c->next) {
			if (ISVISIBLE(c) && !c->isfloating && abs(m->ltaxis[0]) != 2 && m->lt[m->sellt]->arrange != monocle) {
				for (tg = tg_head; tg && tg->x != c->x - m->mx && tg->next; tg = tg->next);
				if (!tg || (tg && tg->x != c->x - m->mx)) {
					tg2 = calloc(1, sizeof(TabGroup));
					tg2->start = tg2->end = tg2->x = c->x - m->mx;
					tg2->end += c->w + 2 * bw;
					if (tg) { tg->next = tg2; } else { tg_head = tg2; }
				}
			}
		}
	}
	if (!tg_head) {
		tg_head = calloc(1, sizeof(TabGroup));
		tg_head->end = m->ww;
	}
	for (c = m->clients; c; c = c->next) {
		if (!ISVISIBLE(c) || (c->isfloating)) continue;
		for (tg = tg_head; tg && tg->x != c->x - m->mx && tg->next; tg = tg->next);
		if (m->sel == c) { tg->active = True; }
		tg->n++;
	}
	for (tg = tg_head; tg; tg = tg->next) {
		if ((m->mx + m->ww) - tg->end < BARTABGROUPS_FUZZPX) {
			tg->end = m->mx + m->ww;
		} else {
			for (tg2 = tg_head; tg2; tg2 = tg2->next) {
				if (tg != tg2 && abs(tg->end - tg2->start) < BARTABGROUPS_FUZZPX) {
				  tg->end = (tg->end + tg2->start) / 2.0;
				  tg2->start = tg->end;
				}
			}
		}
	}

	// Draw
	if (BARBORDERS == 1) {
		int n;
		for(n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
		if (n == 0) {
			char bartext[256];
			strcpy(bartext, "dwm-"VERSION);
			drw_setscheme(drw, scheme[SchemeUnfocus]);
			drw_text(drw, x, 0, m->ww - stw - x, bh - 1, lrpad / 2, bartext, 0);
			XSetForeground(drw->dpy, drw->gc,scheme[SchemeUnfocus][ColFloat].pixel);
			XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, 0, 1, bh - 1);
			XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, 0, m->mw - x - stw - 1, 1);
			XSetForeground(drw->dpy, drw->gc,scheme[SchemeBar][ColBorder].pixel);
			XFillRectangle(drw->dpy, drw->drawable, drw->gc, m->mw - stw - 1, 1, 1, bh - 2);
			XFillRectangle(drw->dpy, drw->drawable, drw->gc, m->mw - stw - 2, bh - 2, 1, 1);
			XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, bh - 1, m->mw - x - stw - 1, 1);
		} else {
			drw_setscheme(drw, scheme[SchemeUnfocus]);
			drw_rect(drw, x, 0, m->ww - stw - x, bh - 1, 1, 1);
		}
	} else {
		drw_setscheme(drw, scheme[SchemeBar]);
		drw_rect(drw, x, 0, m->ww - stw - x, bh, 1, 1);
	}
	for (c = m->clients; c; c = c->next) {
		if (!ISVISIBLE(c) || (c->isfloating)) continue;
		for (tg = tg_head; tg && tg->x != c->x - m->mx && tg->next; tg = tg->next);
		tabgroupwidth = (MIN(tg->end, m->ww - stw) - MAX(x, tg->start));
		tabwidth = (tabgroupwidth / tg->n);
		tabx = MAX(x, tg->start) + (tabwidth * tg->i);
		tabwidth += (tg->n == tg->i + 1 ?  tabgroupwidth % tg->n : 0);
		drawbartab(m, c, tabx, tabwidth, tg->active);
		drawbartaboptionals(m, c, tabx, tabwidth, tg->active);
		if (m ->lt[m->sellt]->arrange == tile && abs(m->ltaxis[0]) != 2) {
			if (passx > 0 && passx > tabx && passx < tabx + tabwidth) {
				focus(c);
				restack(selmon);
			}
		} else {
			if (passx > 0
					&& passx > x + (m->ww - x - stw) / tg->n * tg->i
					&& passx < x + (m->ww - x - stw) / tg->n * (tg->i + 1))
			{
				focus(c);
				restack(selmon);
			}
		}
		tg->i++;
	}
	while (tg_head != NULL) { tg = tg_head; tg_head = tg_head->next; free(tg); }
}

void drawbartab(Monitor *m, Client *c, int x, int w, int tabgroup_active) {
	if (!c) return;
	if (oneclientdimmer == 1) {
		Client *s;
		int n;
		for(n = 0, s = nexttiled(m->clients); s; s = nexttiled(s->next), n++);
		drw_setscheme(drw, scheme[(m->sel == c && n != 1) ? SchemeFocus : SchemeUnfocus]);
	} else
		drw_setscheme(drw, scheme[m->sel == c ? SchemeFocus : SchemeUnfocus]);
	drw_text(drw, x, 0, w, bh - (BARBORDERS ? 1 : 0), lrpad / 2, c->name, 0);
}

void drawbartaboptionals(Monitor *m, Client *c, int x, int w, int tabgroup_active) {
	int i, draw_grid, nclienttags, nviewtags;

	if (!c) return;

	// Tag Grid indicators
	draw_grid = BARTABGROUPS_TAGSINDICATOR;
	if (BARTABGROUPS_TAGSINDICATOR == 1) {
		nclienttags = 0;
		nviewtags = 0;
		for (i = 0; i < LENGTH(tags); i++) {
			if ((m->tagset[m->seltags] >> i) & 1) { nviewtags++; }
			if ((c->tags >> i) & 1) { nclienttags++; }
		}
		draw_grid = nclienttags > 1 || nviewtags > 1;
	}
	if (draw_grid) {
		for (i = 0; i < LENGTH(tags); i++) {
			drw_rect(drw, (
					x + w
					- BARTABGROUPS_INDICATORSPADPX
					- ((LENGTH(tags) / BARTABGROUPS_TAGSROWS) * BARTABGROUPS_TAGSPX)
					- (i % (LENGTH(tags)/BARTABGROUPS_TAGSROWS))
					+ ((i % (LENGTH(tags) / BARTABGROUPS_TAGSROWS)) * BARTABGROUPS_TAGSPX)
				),
				(
					BARTABGROUPS_INDICATORSPADPX
					+ ((i / (LENGTH(tags)/BARTABGROUPS_TAGSROWS)) * BARTABGROUPS_TAGSPX)
					- ((i / (LENGTH(tags)/BARTABGROUPS_TAGSROWS)))
				),
				BARTABGROUPS_TAGSPX, BARTABGROUPS_TAGSPX, (c->tags >> i) & 1, 0
			);
		}
	}

	// 3D-Tabs
	int n = 0;
	if (oneclientdimmer == 1 && !c->isfloating) {
		Client *s;
		for(n = 0, s = nexttiled(m->clients); s; s = nexttiled(s->next), n++);
	}
	if (BARBORDERS == 1) {
		if ((oneclientdimmer == 1 && n == 1) || m->sel != c) { 
			XSetForeground(drw->dpy, drw->gc, scheme[SchemeUnfocus][ColFloat].pixel);
			XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, 0, w - 1, 1);
			XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, 0, 1, bh - 1);
			XSetForeground(drw->dpy, drw->gc, scheme[SchemeBar][ColBorder].pixel);
			XFillRectangle(drw->dpy, drw->drawable, drw->gc, x + w - 1, 1, 1, bh - 1);
			XFillRectangle(drw->dpy, drw->drawable, drw->gc, x + w - 2, bh - 2, 1, 1);
			XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, bh - 1, w - 1, 1);
		} else {
			XSetForeground(drw->dpy, drw->gc, scheme[SchemeFocus][ColFloat].pixel);
			XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, 0, w, 1);
			XFillRectangle(drw->dpy, drw->drawable, drw->gc, x - 1, 0, 1, bh - 1);
		}
	} else {
		XSetForeground(drw->dpy, drw->gc, scheme[SchemeBar][ColBg].pixel);
		XFillRectangle(drw->dpy, drw->drawable, drw->gc, x - (m->sel == c ? 1 : 0), 0, 1, bh);
	}
}

void drawtaggrid(Monitor *m, int *x_pos, unsigned int occ)
{
    unsigned int x, y, h, max_x, columns;
    int invert, i,j, k;

    h = bh / tagrows;
    x = max_x = *x_pos;
    y = 0;
    columns = LENGTH(tags) / tagrows + ((LENGTH(tags) % tagrows > 0) ? 1 : 0);

    /* Firstly we will fill the borders of squares */

	XSetForeground(drw->dpy, drw->gc, scheme[SchemeTag][ColBorder].pixel);
    XFillRectangle(dpy, drw->drawable, drw->gc, x, y, h*columns + 1, bh);

    /* We will draw LENGTH(tags) squares in tagraws raws. */
	for(j = 0,  i= 0; j < tagrows; j++) {
        x = *x_pos;
        for (k = 0; k < columns && i < LENGTH(tags); k++, i++) {
		    invert = m->tagset[m->seltags] & 1 << i ? 0 : 1;

            /* Select active color for current square */
            XSetForeground(drw->dpy, drw->gc, !invert ? scheme[SchemeTag][ColFg].pixel :
                                scheme[SchemeTag][ColBg].pixel);
            XFillRectangle(dpy, drw->drawable, drw->gc, x+1, y+1, h-1, h-1);

            /* Mark square if tag has client */
            if (occ & 1 << i) {
                XSetForeground(drw->dpy, drw->gc, scheme[SchemeTag][ColFloat].pixel);
                XFillRectangle(dpy, drw->drawable, drw->gc, x + 1, y + 1,
                               h / 2, h / 2);
            }
		    x += h;
            if (x > max_x) {
                max_x = x;
            }
        }
        y += h;
	}
    *x_pos = max_x + 1;
}

void
enternotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	if (m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
	} else if (!c || c == selmon->sel)
		return;
	focus(c);
}

void
expose(XEvent *e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (m = wintomon(ev->window))) {
		drawbar(m);
		drawebar(rawstext, m, 0);
		if (showsystray && m == selmon)
			updatesystray();
	}
}

Client *
findbefore(Client *c)
{
	Client *tmp;
	if (c == selmon->clients)
		return NULL;
	for (tmp = selmon->clients; tmp && tmp->next != c; tmp = tmp->next);
	return tmp;
}

void
focus(Client *c)
{
	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);
	if (selmon->sel && selmon->sel != c)
		unfocus(selmon->sel, 0);
	if (c) {
		if (c->mon != selmon)
			selmon = c->mon;
		if (c->isurgent)
			seturgent(c, 0);
		detachstack(c);
		attachstack(c);
		grabbuttons(c, 1);
		XSetWindowBorder(dpy, c->win, scheme[SchemeBorder][ColFg].pixel);
		shadowfloat(c);
		setfocus(c);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	selmon->sel = c;
	if (selmon->lt[selmon->sellt]->arrange == tile)
		arrangemon(selmon);
	if (selmon->lt[selmon->sellt]->arrange == monocle)
		arrangemon(selmon);
	drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

void
focusmon(const Arg *arg)
{
	Monitor *m;

	if (!mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, 0);
	selmon = m;
	focus(NULL);
}

void
focusstack(const Arg *arg)
{
	Client *c = NULL, *i;

	if (!selmon->sel)
		return;
	if (arg->i > 0) {
		for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next);
		if (!c)
			for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
	} else {
		for (i = selmon->clients; i != selmon->sel; i = i->next)
			if (ISVISIBLE(i))
				c = i;
		if (!c)
			for (; i; i = i->next)
				if (ISVISIBLE(i))
					c = i;
	}
	if (c) {
		focus(c);
		restack(selmon);
	}
}

Atom
getatomprop(Client *c, Atom prop)
{
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	/* FIXME getatomprop should return the number of items and a pointer to
	 * the stored data instead of this workaround */
	Atom req = XA_ATOM;
	if (prop == xatom[XembedInfo])
		req = xatom[XembedInfo];

	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, req,
		&da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		if (da == xatom[XembedInfo] && dl == 2)
			atom = ((Atom *)p)[1];
		XFree(p);
	}
	return atom;
}

int
getdwmblockspid()
{
	char buf[16];
	FILE *fp = popen("pidof -s dwmblocks", "r");
	if (fgets(buf, sizeof(buf), fp));
	pid_t pid = strtoul(buf, NULL, 10);
	pclose(fp);
	dwmblockspid = pid;
	return pid != 0 ? 0 : -1;
}

int
getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

unsigned int
getsystraywidth()
{
	unsigned int w = 0;
	Client *i;
	if (showsystray)
		for (i = systray->icons; i; w += i->w + systrayspacing, i = i->next);
	return w ? w + systrayspacing : 0;
}

int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else {
		if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void
grabbuttons(Client *c, int focused)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		if (!focused)
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
				BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClkClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button,
						buttons[i].mask | modifiers[j],
						c->win, False, BUTTONMASK,
						GrabModeAsync, GrabModeSync, None, None);
	}
}

void
grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		KeyCode code;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		for (i = 0; i < LENGTH(keys); i++)
			if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
						True, GrabModeAsync, GrabModeAsync);
	}
}

void
incnmaster(const Arg *arg)
{
	unsigned int n;
	Client *c;

	for(n = 0, c = nexttiled(selmon->clients); c; c = nexttiled(c->next), n++);
	if(!arg || !selmon->lt[selmon->sellt]->arrange || selmon->nmaster + arg->i < 1 || selmon->nmaster + arg->i > n)
		return;
	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void
keypress(XEvent *e)
{
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].func)
			keys[i].func(&(keys[i].arg));
}

int
fake_signal(void)
{
	char fsignal[256];
	char indicator[9] = "fsignal:";
	char str_signum[16];
	int i, v, signum;
	size_t len_fsignal, len_indicator = strlen(indicator);

	// Get root name property
	if (gettextprop(root, XA_WM_NAME, fsignal, sizeof(fsignal))) {
		len_fsignal = strlen(fsignal);

		// Check if this is indeed a fake signal
		if (len_indicator > len_fsignal ? 0 : strncmp(indicator, fsignal, len_indicator) == 0) {
			memcpy(str_signum, &fsignal[len_indicator], len_fsignal - len_indicator);
			str_signum[len_fsignal - len_indicator] = '\0';

			// Convert string value into managable integer
			for (i = signum = 0; i < strlen(str_signum); i++) {
				v = str_signum[i] - '0';
				if (v >= 0 && v <= 9) {
					signum = signum * 10 + v;
				}
			}

			// Check if a signal was found, and if so handle it
			if (signum)
				for (i = 0; i < LENGTH(signals); i++)
					if (signum == signals[i].signum && signals[i].func)
						signals[i].func(&(signals[i].arg));

			// A fake signal was sent
			return 1;
		}
	}

	// No fake signal was sent, so proceed with update
	return 0;
}

void
killclient(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (!sendevent(selmon->sel->win, wmatom[WMDelete], NoEventMask, wmatom[WMDelete], CurrentTime, 0, 0, 0)) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, selmon->sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

void
loadxrdb()
{
  Display *display;
  char * resm;
  XrmDatabase xrdb;
  char *type;
  XrmValue value;

  display = XOpenDisplay(NULL);

  if (display != NULL) {
    resm = XResourceManagerString(display);

    if (resm != NULL) {
      xrdb = XrmGetStringDatabase(resm);

      if (xrdb != NULL) {
        XRDB_LOAD_COLOR("dwm.bar_fg",  bar_fg);
        XRDB_LOAD_COLOR("dwm.bar_bg",  bar_bg);
        XRDB_LOAD_COLOR("dwm.bar_brd", bar_brd);
        XRDB_LOAD_COLOR("dwm.bar_flo", bar_flo);
        XRDB_LOAD_COLOR("dwm.tag_fg",  tag_fg);
        XRDB_LOAD_COLOR("dwm.tag_bg",  tag_bg);
        XRDB_LOAD_COLOR("dwm.tag_brd", tag_brd);
        XRDB_LOAD_COLOR("dwm.tag_flo", tag_flo);
        XRDB_LOAD_COLOR("dwm.brd_fg",  brd_fg);
        XRDB_LOAD_COLOR("dwm.brd_bg",  brd_bg);
        XRDB_LOAD_COLOR("dwm.brd_brd", brd_brd);
        XRDB_LOAD_COLOR("dwm.brd_flo", brd_flo);
        XRDB_LOAD_COLOR("dwm.foc_fg",  foc_fg);
        XRDB_LOAD_COLOR("dwm.foc_bg",  foc_bg);
        XRDB_LOAD_COLOR("dwm.foc_brd", foc_brd);
        XRDB_LOAD_COLOR("dwm.foc_flo", foc_flo);
        XRDB_LOAD_COLOR("dwm.unf_fg",  unf_fg);
        XRDB_LOAD_COLOR("dwm.unf_bg",  unf_bg);
        XRDB_LOAD_COLOR("dwm.unf_brd", unf_brd);
        XRDB_LOAD_COLOR("dwm.unf_flo", unf_flo);
		XrmDestroyDatabase(xrdb);
      }
    }
  }

  XCloseDisplay(display);
}

void
manage(Window w, XWindowAttributes *wa)
{
	Client *c, *t = NULL, *term = NULL;
	Window trans = None;
	XWindowChanges wc;
	int focusclient = 1;

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	c->pid = winpid(w);
	/* geometry */
	c->floatborderpx = -1;
	c->floatx = c->floaty = c->floatw = c->floath = -11;
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;
	c->cfact = 1.0;

	updatetitle(c);
	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->tags = t->tags;
	} else {
		c->mon = selmon;
		applyrules(c);
		term = termforwin(c);
	}

	if (c->x + WIDTH(c) + 2 * borderpx > c->mon->wx + c->mon->ww)
		c->x = c->mon->wx + c->mon->ww - WIDTH(c) - 2 * borderpx;
	if (c->y + HEIGHT(c) + 2 * borderpx > c->mon->wy + c->mon->wh)
		c->y = c->mon->wy + c->mon->wh - HEIGHT(c) - 2 * borderpx;
	c->x = MAX(c->x, c->mon->wx);
	c->y = MAX(c->y, c->mon->wy);
	if (c->h > c->mon->wh - 2 * borderpx)
		c->h = c->mon->wh - 2 * borderpx;
	if (c->w > c->mon->ww - 2 * borderpx)
		c->w = c->mon->ww - 2 * borderpx;
	if (c->y < c->mon->wy)
		c->y = c->mon->wy;
	c->bw = borderpx;

	/* float-dimensions setup */
	if (c->floatw > bh)
		c->w = c->floatw;
	else if (c->floatw < 0 && c->floatw > -10)
		c->w = (-1 * c->floatw - 1) * c->mon->ww / 8;

	if (c->floath > bh)
		c->h = c->floath;
	else if (c->floath < 0 && c->floath > -10)
		c->h = (-1 * c->floath - 1) * (c->mon->wh) / 8;

	if (c->floatx >= 0)
		c->x = c->mon->mx + c->floatx;
	else if (c->floatx < 0 && c->floatx > -10)
		c->x = c->mon->wx + (-1 * c->floatx - 1) * c->mon->ww / 8;
	else if (c->floatx == -10)
		c->x = c->mon->wx + (c->mon->mw - c->w) / 2;

	if (c->floaty >= 0)
		c->y = c->mon->my + c->floaty;
	else if (c->floaty < 0 && c->floaty > -10)
		c->y = c->mon->wy + (-1 * c->floaty - 1) * c->mon->wh / 8;
	else if (c->floaty == -10)
		c->y = c->mon->wy + (c->mon->wh - c->h) / 2;

	if (c->isfloating && c->floatborderpx >= 0)
		wc.border_width = c->floatborderpx;
	else
		wc.border_width = c->bw;

	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	if(c->isfloating)
		XSetWindowBorder(dpy, w, scheme[SchemeBorder][ColFloat].pixel);
	else {
		if ( selmon->gappx > 2 * borderpx)
			XSetWindowBorder(dpy, w, scheme[SchemeBorder][ColBg].pixel);
		else
			XSetWindowBorder(dpy, w, scheme[SchemeBorder][ColBorder].pixel);
	}
	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);
	c->sfx = c->x;
	c->sfy = c->y;
	c->sfw = c->w;
	c->sfh = c->h;
	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c, 0);
	if (!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed;
	if (c->isfloating)
		XRaiseWindow(dpy, c->win);
	if (c->isfloating)
		XSetWindowBorder(dpy, w, scheme[SchemeBorder][ColFloat].pixel);
	shadowfloat(c);
	/* Do not attach client if it is being swallowed */
	if (term && swallow(term, c)) {
		/* Do not let swallowed client steal focus unless the terminal has focus */
		focusclient = (term == selmon->sel);
	} else {
		switch(attachdirection){
			case 1:
				attachabove(c);
				break;
			case 2:
				attachaside(c);
				break;
			case 3:
				attachbelow(c);
				break;
			case 4:
				attachbottom(c);
				break;
			case 5:
				attachtop(c);
				break;
			default:
				attach(c);
		}
		if (focusclient || !c->mon->sel || !c->mon->stack)
			attachstack(c);
		else {
			c->snext = c->mon->sel->snext;
			c->mon->sel->snext = c;
		}
	}

	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		(unsigned char *) &(c->win), 1);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
	setclientstate(c, NormalState);
	if (focusclient) {
		if (c->mon == selmon)
			unfocus(selmon->sel, 0);
		c->mon->sel = c;
	}

	if (!c->swallowing) {
		if (riopid && (!riodraw_matchpid || isdescprocess(riopid, c->pid))) {
			if (riodimensions[3] != -1)
				rioposition(c, riodimensions[0], riodimensions[1], riodimensions[2], riodimensions[3]);
			else {
				killclient(&((Arg) { .v = c }));
				return;
			}
		}
	}

	arrange(c->mon);
	XMapWindow(dpy, c->win);
	if (focusclient)
		focus(NULL);
}

void
mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *e)
{
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	Client *i;
	if (showsystray && (i = wintosystrayicon(ev->window))) {
		sendevent(i->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
		updatesystray();
	}

	if (!XGetWindowAttributes(dpy, ev->window, &wa))
		return;
	if (wa.override_redirect)
		return;
	if (!wintoclient(ev->window))
		manage(ev->window, &wa);
}

void
mirrorlayout(const Arg *arg) {
	if(!selmon->lt[selmon->sellt]->arrange)
		return;
	selmon->ltaxis[0] *= -1;
	selmon->pertag->ltaxes[selmon->pertag->curtag][0] = selmon->ltaxis[0];
	arrange(selmon);
}

void
monocle(Monitor *m)
{
	unsigned int n = 0;
	Client *c;

	for (c = m->clients; c; c = c->next)
		if (ISVISIBLE(c))
			n++;
	if (n > 0) /* override layout symbol */
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
	for (c = m->stack; c && (!ISVISIBLE(c) || c->isfloating); c = c->snext);
	if (c && !c->isfloating) {
		XMoveWindow(dpy, c->win, m->wx - c->bw, m->wy);
		if (abs(m->showbar) + abs(m->showebar) == 0)
			resize(c, m->wx - c->bw, m->wy - c->bw, m->ww, m->wh, 0, 0);
		else
			resize(c, m->wx - c->bw, m->wy, m->ww, m->wh - c->bw, 0, 0);
		c = c->snext;
	}
	for (; c; c = c->snext)
		if (!c->isfloating && ISVISIBLE(c))
			XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
}

void
motionnotify(XEvent *e)
{
	static Monitor *mon = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if (ev->window != root)
		return;
	if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	mon = m;
	if (abs(selmon->showebar) == 1 && ((topbar && ev->y < bh) || (!topbar && ev->y > m->mh - bh))) {
		Client *c;
		c = selmon->sel;
		if(c && c->isactfullscreen)
			return;
		if ((!eblock && !esep) || ev->x < esep || ev->x > esep + eblock) {
			eblock = esep = 0;
			drawebar(rawstext, m, ev->x);
		} else
			return;
	} else if (eblock) {
		eblock = esep = 0;
		drawebar(rawstext, m, 0);
	}
}

void
movemouse(const Arg *arg)
{
	int x, y, ocx, ocy, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (abs(selmon->wx - nx) < snap)
				nx = selmon->wx;
			else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
				nx = selmon->wx + selmon->ww - WIDTH(c);
			if (abs(selmon->wy - ny) < snap)
				ny = selmon->wy;
			else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
				ny = selmon->wy + selmon->wh - HEIGHT(c);
			if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
			&& (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
				togglefloating(NULL);
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, nx, ny, c->w, c->h, c->bw, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

void
moveresize(const Arg *arg) {
	/* only floating windows can be moved */
	Client *c;
	c = selmon->sel;
	int x, y, w, h, nx, ny, nw, nh, ox, oy, ow, oh;
	char xAbs, yAbs, wAbs, hAbs;
	int msx, msy, dx, dy, nmx, nmy;
	unsigned int dui;
	Window dummy;

	if (!c || !arg)
		return;
	if (selmon->lt[selmon->sellt]->arrange && !c->isfloating)
		return;
	if (sscanf((char *)arg->v, "%d%c %d%c %d%c %d%c", &x, &xAbs, &y, &yAbs, &w, &wAbs, &h, &hAbs) != 8)
		return;

	/* compute new window position; prevent window from be positioned outside the current monitor */
	nw = c->w + w;
	if (wAbs == 'W')
		nw = w < selmon->mw - 2 * c->bw ? w : selmon->mw - 2 * c->bw;

	nh = c->h + h;
	if (hAbs == 'H')
		nh = h < selmon->mh - 2 * c->bw ? h : selmon->mh - 2 * c->bw;

	nx = c->x + x;
	if (xAbs == 'X') {
		if (x < selmon->mx)
			nx = selmon->mx;
		else if (x > selmon->mx + selmon->mw)
			nx = selmon->mx + selmon->mw - nw - 2 * c->bw;
		else
			nx = x;
	}

	ny = c->y + y;
	if (yAbs == 'Y') {
		if (y < selmon->my)
			ny = selmon->my;
		else if (y > selmon->my + selmon->mh)
			ny = selmon->my + selmon->mh - nh - 2 * c->bw;
		else
			ny = y;
	}

	ox = c->x;
	oy = c->y;
	ow = c->w;
	oh = c->h;

	XRaiseWindow(dpy, c->win);
	Bool xqp = XQueryPointer(dpy, root, &dummy, &dummy, &msx, &msy, &dx, &dy, &dui);
	resize(c, nx, ny, nw, nh, c->bw, True);

	/* move cursor along with the window to avoid problems caused by the sloppy focus */
	if (xqp && ox <= msx && (ox + ow) >= msx && oy <= msy && (oy + oh) >= msy)
	{
		nmx = c->x - ox + c->w - ow;
		nmy = c->y - oy + c->h - oh;
		XWarpPointer(dpy, None, None, 0, 0, 0, 0, nmx, nmy);
	}
}

void
moveresizeedge(const Arg *arg) {
	/* move or resize floating window to edge of screen */
	Client *c;
	c = selmon->sel;
	char e;
	int nx, ny, nw, nh, ox, oy, ow, oh, bp;
	int msx, msy, dx, dy, nmx, nmy;
	int starty;
	unsigned int dui;
	Window dummy;

	nx = c->x;
	ny = c->y;
	nw = c->w;
	nh = c->h;

	if(abs(selmon->showbar) + abs(selmon->showebar) == 2) {
		starty = topbar ? 2 * bh : 0;
		bp = !topbar ? 2 * bh : 0;
	} else if(abs(selmon->showbar) + abs(selmon->showebar) == 1) {
		starty = topbar ? bh : 0;
		bp = !topbar ? bh : 0;
	} else {
		starty = 0;
		bp = 0;
	}

	if (!c || !arg)
		return;
	if (selmon->lt[selmon->sellt]->arrange && !c->isfloating)
		return;
	if(sscanf((char *)arg->v, "%c", &e) != 1)
		return;

	if(e == 't')
		ny = starty;

	if(e == 'b')
		ny = c->h > selmon->mh - 2 * c->bw ? c->h - bp : selmon->mh - c->h - 2 * c->bw - bp;

	if(e == 'l')
		nx = selmon->mx;

	if(e == 'r')
		nx = c->w > selmon->mw - 2 * c->bw ? selmon->mx + c->w : selmon->mx + selmon->mw - c->w - 2 * c->bw;

	if(e == 'T') {
		/* if you click to resize again, it will return to old size/position */
		if(c->h + starty == c->oldh + c->oldy) {
			nh = c->oldh;
			ny = c->oldy;
		} else {
			nh = c->h + c->y - starty;
			ny = starty;
		}
	}

	if(e == 'B')
		nh = c->h + c->y + 2 * c->bw + bp == selmon->mh ? c->oldh : selmon->mh - c->y - 2 * c->bw - bp;

	if(e == 'L') {
		if(selmon->mx + c->w == c->oldw + c->oldx) {
			nw = c->oldw;
			nx = c->oldx;
		} else {
			nw = c->w + c->x - selmon->mx;
			nx = selmon->mx;
		}
	}

	if(e == 'R')
		nw = c->w + c->x + 2 * c->bw == selmon->mx + selmon->mw ? c->oldw : selmon->mx + selmon->mw - c->x - 2 * c->bw;

	ox = c->x;
	oy = c->y;
	ow = c->w;
	oh = c->h;

	XRaiseWindow(dpy, c->win);
	Bool xqp = XQueryPointer(dpy, root, &dummy, &dummy, &msx, &msy, &dx, &dy, &dui);
	resize(c, nx, ny, nw, nh, c->bw, True);

	/* move cursor along with the window to avoid problems caused by the sloppy focus */
	if (xqp && ox <= msx && (ox + ow) >= msx && oy <= msy && (oy + oh) >= msy) {
		nmx = c->x - ox + c->w - ow;
		nmy = c->y - oy + c->h - oh;
		XWarpPointer(dpy, None, None, 0, 0, 0, 0, nmx, nmy);
	}
}

Client *
nexttagged(Client *c) {
	Client *walked = c->mon->clients;
	for(;
		walked && (walked->isfloating || !ISVISIBLEONTAG(walked, c->tags));
		walked = walked->next
	);
	return walked;
}

Client *
nexttiled(Client *c)
{
	for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
	return c;
}

void
pop(Client *c)
{
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

void
propertynotify(XEvent *e)
{
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if (showsystray && (c = wintosystrayicon(ev->window))) {
		if (ev->atom == XA_WM_NORMAL_HINTS) {
			updatesizehints(c);
			updatesystrayicongeom(c, c->w, c->h);
		}
		else
			updatesystrayiconstate(c, ev);
		updatesystray();
	}

	if ((ev->window == root) && (ev->atom == XA_WM_NAME)) {
		if (!fake_signal())
			updatestatus();
	} else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = wintoclient(ev->window))) {
		switch(ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
				(c->isfloating = (wintoclient(trans)) != NULL))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			updatesizehints(c);
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			drawbars();
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			updatetitle(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

Client *
nextc(Client *c, float f) {
	if(!f)
		return nexttiled(c);

	for(; c && !ISVISIBLE(c); c = c->next);
	return c;
}

static Client *
prevc(Client *c, float f) {
	Client *p, *r;

	for(p = selmon->clients, r = NULL; c && p && p != c; p = p->next)
		if((f || !p->isfloating) && ISVISIBLE(p))
			r = p;
	return r;
}

static void
pushup(const Arg *arg) {
	Client *sel = selmon->sel;
	Client *c;

	if(!sel || (sel->isfloating && !arg->f))
		return;
	if((c = prevc(sel, arg->f))) {
		/* attach before c */
		detach(sel);
		sel->next = c;
		if(selmon->clients == c)
			selmon->clients = sel;
		else {
			for(c = selmon->clients; c->next != sel->next; c = c->next);
			c->next = sel;
		}
	} else {
		/* move to the end */
		for(c = sel; c->next; c = c->next);
		detach(sel);
		sel->next = NULL;
		c->next = sel;
	}
	focus(sel);
	arrange(selmon);
}

static void
pushdown(const Arg *arg) {
	Client *sel = selmon->sel;
	Client *c;

	if(!sel || (sel->isfloating && !arg->f))
		return;
	if((c = nextc(sel->next, arg->f))) {
		/* attach after c */
		detach(sel);
		sel->next = c->next;
		c->next = sel;
	} else {
		/* move to the front */
		detach(sel);
		attach(sel);
	}
	focus(sel);
	arrange(selmon);
}

void
quit(const Arg *arg)
{
	running = 0;
}

Monitor *
recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void
removesystrayicon(Client *i)
{
	Client **ii;

	if (!showsystray || !i)
		return;
	for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next);
	if (ii)
		*ii = i->next;
	free(i);
}

void
replaceclient(Client *old, Client *new)
{
	Client *c = NULL;
	Monitor *mon = old->mon;

	new->mon = mon;
	new->tags = old->tags;
	new->isfloating = old->isfloating;

	new->next = old->next;
	new->snext = old->snext;

	if (old == mon->clients)
		mon->clients = new;
	else {
		for (c = mon->clients; c && c->next != old; c = c->next);
		c->next = new;
	}

	if (old == mon->stack)
		mon->stack = new;
	else {
		for (c = mon->stack; c && c->snext != old; c = c->snext);
		c->snext = new;
	}

	old->next = NULL;
	old->snext = NULL;

	XMoveWindow(dpy, old->win, WIDTH(old) * -2, old->y);

	if (ISVISIBLE(new)) {
		if (new->isfloating)
			resize(new, old->x, old->y, new->w - 2*new->bw, new->h - 2*new->bw, 0, 0);
		else
			resize(new, old->x, old->y, old->w - 2*new->bw, old->h - 2*new->bw, 0, 0);
	}
}

void
resize(Client *c, int x, int y, int w, int h, int bw, int interact)
{
	if (applysizehints(c, &x, &y, &w, &h, &bw, interact))
		resizeclient(c, x, y, w, h, bw);
}

void
resizeclient(Client *c, int x, int y, int w, int h, int bw)
{
	XWindowChanges wc;

	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;
	if (c->isfloating && c->floatborderpx >= 0)
		wc.border_width = c->floatborderpx;
	else {
		c->oldbw = c->bw; c->bw = wc.border_width = bw;
	}
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	XSync(dpy, False);
}

void
resizemouse(const Arg *arg)
{
	int ocx, ocy, nw, nh;
	int ocx2, ocy2, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	int horizcorner, vertcorner;
	int di;
	unsigned int dui;
	Window dummy;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	ocx2 = c->x + c->w;
	ocy2 = c->y + c->h;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!XQueryPointer (dpy, c->win, &dummy, &dummy, &di, &di, &nx, &ny, &dui))
	       return;
	horizcorner = nx < c->w / 2;
	vertcorner = ny < c->h / 2;
	XWarpPointer (dpy, None, c->win, 0, 0, 0, 0,
		      horizcorner ? (-c->bw) : (c->w + c->bw - 1),
		      vertcorner ? (-c->bw) : (c->h + c->bw - 1));

	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
			nx = horizcorner ? ev.xmotion.x : c->x;
			ny = vertcorner ? ev.xmotion.y : c->y;
			nw = MAX(horizcorner ? (ocx2 - nx) : (ev.xmotion.x - ocx - 2 * c->bw + 1), 1);
			nh = MAX(vertcorner ? (ocy2 - ny) : (ev.xmotion.y - ocy - 2 * c->bw + 1), 1);

			if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
			&& c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
			{
				if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
				&& (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
					togglefloating(NULL);
			}
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, nx, ny, nw, nh, c->bw, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0,
		      horizcorner ? (-c->bw) : (c->w + c->bw - 1),
		      vertcorner ? (-c->bw) : (c->h + c->bw - 1));
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

void
resizerequest(XEvent *e)
{
	XResizeRequestEvent *ev = &e->xresizerequest;
	Client *i;

	if ((i = wintosystrayicon(ev->window))) {
		updatesystrayicongeom(i, ev->width, ev->height);
		updatesystray();
	}
}

void
restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	drawbar(m);
	if (!m->sel)
		return;
	if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
		XRaiseWindow(dpy, m->sel->win);
	if (m->lt[m->sellt]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for (c = m->stack; c; c = c->snext)
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}
	for (c = m->stack; c; c = c->snext)
		shadowfloat(c);
    XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

int
riodraw(Client *c, const char slopstyle[])
{
	int i;
	char str[100] = {0};
	char strout[100] = {0};
	char tmpstring[30] = {0};
	char slopcmd[100] = "slop -f x%xx%yx%wx%hx ";
	int firstchar = 0;
	int counter = 0;

	strcat(slopcmd, slopstyle);
	FILE *fp = popen(slopcmd, "r");

	while (fgets(str, 100, fp) != NULL)
		strcat(strout, str);

	pclose(fp);

	if (strlen(strout) < 6)
		return 0;

	for (i = 0; i < strlen(strout); i++){
		if (!firstchar) {
			if (strout[i] == 'x')
				firstchar = 1;
			continue;
		}

		if (strout[i] != 'x')
			tmpstring[strlen(tmpstring)] = strout[i];
		else {
			riodimensions[counter] = atoi(tmpstring);
			counter++;
			memset(tmpstring,0,strlen(tmpstring));
		}
	}

	if (riodimensions[0] <= -40 || riodimensions[1] <= -40 || riodimensions[2] <= 50 || riodimensions[3] <= 50) {
		riodimensions[3] = -1;
		return 0;
	}

	if (c) {
		rioposition(c, riodimensions[0], riodimensions[1], riodimensions[2], riodimensions[3]);
		return 0;
	}

	return 1;
}

void
rioposition(Client *c, int x, int y, int w, int h)
{
	Monitor *m;
	if ((m = recttomon(x, y, w, h)) && m != c->mon) {
		detach(c);
		detachstack(c);
		c->mon = m;
		c->tags = m->tagset[m->seltags];
		attach(c);
		attachstack(c);
		selmon = m;
		focus(c);
	}

	c->isfloating = 1;
	if (riodraw_borders)
		resizeclient(c, x, y, w - (c->bw * 2), h - (c->bw * 2), c->bw);
	else
		resizeclient(c, x - c->bw, y - c->bw, w, h, c->bw);
	drawbar(c->mon);
	arrange(c->mon);

	riodimensions[3] = -1;
	riopid = 0;
}

/* drag out an area using slop and resize the selected window to it */
void
rioresize(const Arg *arg)
{
	Client *c = (arg && arg->v ? (Client*)arg->v : selmon->sel);
	if (c)
		riodraw(c, slopresizestyle);
}

/* spawn a new window and drag out an area using slop to postiion it */
void
riospawn(const Arg *arg)
{
	if (riodraw_spawnasync) {
		riopid = spawncmd(arg);
		riodraw(NULL, slopspawnstyle);
	} else if (riodraw(NULL, slopspawnstyle))
		riopid = spawncmd(arg);
}

void
rotatelayoutaxis(const Arg *arg) {
	if(!selmon->lt[selmon->sellt]->arrange)
		return;
	if(arg->i == 0) {
		if(selmon->ltaxis[0] > 0)
			selmon->ltaxis[0] = selmon->ltaxis[0] + 1 > 2 ? 1 : selmon->ltaxis[0] + 1;
		else
			selmon->ltaxis[0] = selmon->ltaxis[0] - 1 < -2 ? -1 : selmon->ltaxis[0] - 1;
		if(selmon->ltaxis[1] == abs(selmon->ltaxis[0]))
			selmon->ltaxis[1] = abs(selmon->ltaxis[0]) + 1 > 2 ? 1 : abs(selmon->ltaxis[0]) + 1;
		if(selmon->ltaxis[2] == abs(selmon->ltaxis[0]))
			selmon->ltaxis[2] = abs(selmon->ltaxis[0]) + 1 > 2 ? 1 : abs(selmon->ltaxis[0]) + 1;
	}
	else
		selmon->ltaxis[arg->i] = selmon->ltaxis[arg->i] + 1 > 3 ? 1 : selmon->ltaxis[arg->i] + 1;
	selmon->pertag->ltaxes[selmon->pertag->curtag][arg->i] = selmon->ltaxis[arg->i];
	arrange(selmon);
}

void
run(void)
{
	XEvent ev;
	/* main event loop */
	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev))
		if (handler[ev.type])
			handler[ev.type](&ev); /* call handler */
}

void
runorraise(const Arg *arg) {
    char *app = ((char **)arg->v)[0];
    Arg a = { .ui = ~0 };
    Monitor *mon;
    Client *c;
    XClassHint hint = { NULL, NULL };
    /* Tries to find the client */
    for (mon = mons; mon; mon = mon->next) {
        for (c = mon->clients; c; c = c->next) {
            XGetClassHint(dpy, c->win, &hint);
            if (hint.res_class && strcmp(app, hint.res_class) == 0) {
                a.ui = c->tags;
                view(&a);
                focus(c);
                XRaiseWindow(dpy, c->win);
                return;
            }
        }
    }
    /* Client not found: spawn it */
    spawn(arg);
}

void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

void
sendmon(Client *c, Monitor *m)
{
	if (c->mon == m)
		return;
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	c->mon = m;
	c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
	switch(attachdirection){
		case 1:
			attachabove(c);
			break;
		case 2:
			attachaside(c);
			break;
		case 3:
			attachbelow(c);
			break;
		case 4:
			attachbottom(c);
			break;
		case 5:
			attachtop(c);
			break;
		default:
			attach(c);
	}
	attachstack(c);
	focus(NULL);
	arrange(NULL);
}

void
setclientstate(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char *)data, 2);
}

int
sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4)
{
	int n;
	Atom *protocols, mt;
	int exists = 0;
	XEvent ev;

	if (proto == wmatom[WMTakeFocus] || proto == wmatom[WMDelete]) {
		mt = wmatom[WMProtocols];
		if (XGetWMProtocols(dpy, w, &protocols, &n)) {
			while (!exists && n--)
				exists = protocols[n] == proto;
			XFree(protocols);
		}
	} else {
		exists = True;
		mt = proto;
	}
	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = w;
		ev.xclient.message_type = mt;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = d0;
		ev.xclient.data.l[1] = d1;
		ev.xclient.data.l[2] = d2;
		ev.xclient.data.l[3] = d3;
		ev.xclient.data.l[4] = d4;
		XSendEvent(dpy, w, False, mask, &ev);
	}
	return exists;
}

void
setfocus(Client *c)
{
	if (!c->neverfocus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow],
			XA_WINDOW, 32, PropModeReplace,
			(unsigned char *) &(c->win), 1);
	}
	sendevent(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus], CurrentTime, 0, 0, 0);
}

void
setactfullscreen(Client *c)
{
	if (!c->isactfullscreen) {
		c->isactfullscreen = 1;
		c->oldstate = c->isfloating;
		c->isfloating = 1;
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh, 0);
		XRaiseWindow(dpy, c->win);
	} else if (c->isactfullscreen){
		c->isactfullscreen = 0;
		c->isfloating = c->oldstate;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		c->bw = c->oldbw;
		resizeclient(c, c->x, c->y, c->w, c->h, c->bw);
		arrange(c->mon);
	}
}

void
setfullscreen(Client *c, int fullscreen)
{
	if (fullscreen && !c->isfullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->isfullscreen = 1;
	} else if (!fullscreen && c->isfullscreen){
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)0, 0);
		c->isfullscreen = 0;
	}
    demoncleaner(0);
}

void
setgaps(const Arg *arg)
{
	Client *c;
	if ((arg->i == 0) || (selmon->gappx + arg->i < 0))
		selmon->gappx = 0;
	else {
		if (selmon->gappx + arg->i < 50)
		selmon->gappx += arg->i;
		}
	if ((selmon->gappx == 1 * borderpx && arg->i < 0)) {
		for (c = nexttiled(selmon->clients); c; c = nexttiled(c->next))
			XSetWindowBorder(dpy, c->win, scheme[SchemeBorder][ColBorder].pixel);
		focus(c);
	}
	if ((selmon->gappx == 2 * borderpx && arg->i > 0)) {
		for (c = nexttiled(selmon->clients); c; c = nexttiled(c->next))
			XSetWindowBorder(dpy, c->win, scheme[SchemeBorder][ColBg].pixel);
		focus(c);
	}
	arrange(selmon);
}

void
setlayout(const Arg *arg)
{
	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag] ^= 1;
	if (arg && arg->v)
		selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt] = (Layout *)arg->v;
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
	arrangemon(selmon);
}

void setcfact(const Arg *arg) {
	float f;
	Client *c;

	c = selmon->sel;

	if(!arg || !c || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f + c->cfact;
	if(arg->f == 0.0)
		f = 1.0;
	else if(f < 0.25 || f > 4.0)
		return;
	c->cfact = f;
	arrange(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void
setmfact(const Arg *arg)
{
	float f;

	if (!arg || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
	if (f < 0.05 || f > 0.95)
		return;
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = f;
	arrange(selmon);
}

void
setup(void)
{
	int i;
	XSetWindowAttributes wa;
	Atom utf8string;

	/* clean up any zombies immediately */
	sigchld(0);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	xinitvisual();
	drw = drw_create(dpy, screen, root, sw, sh, visual, depth, cmap);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h;
	bh = drw->fonts->h + 2;
	updategeom();
	/* init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
    netatom[NetSystemTray] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
	netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	netatom[NetSystemTrayOrientation] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
	netatom[NetSystemTrayOrientationHorz] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION_HORZ", False);
	netatom[NetSystemTrayVisual] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_VISUAL", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDock] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
	xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);
	xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
    wunshadow = XInternAtom(dpy, "_COMPTON_SHADOW", False);
    /* init cursors */
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);
	/* init appearance */
	scheme = ecalloc(LENGTH(colors) + 1, sizeof(Clr *));
	scheme[LENGTH(colors)] = drw_scm_create(drw, colors[0], alphas[0], 4);
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], alphas[i], 4);
	/* init system tray */
	if (showsystray)
		updatesystray();
	/* init bars */
	updatebars();
	updatestatus();
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char *) "dwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *) netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select events */
	wa.cursor = cursor[CurNormal]->cursor;
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	grabkeys();
	focus(NULL);
}


void
seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

 void
shadowfloat(Client *c)
{
	if (!c->isfloating && selmon->lt[selmon->sellt]->arrange) {
		unsigned long shadow[] = { 0x00000000 };
		XChangeProperty(dpy, c->win, wunshadow, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)shadow, 1);
	} else {
		XDeleteProperty(dpy, c->win, wunshadow);
	}
}

void
showhide(Client *c)
{
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if (!c->mon->lt[c->mon->sellt]->arrange || c->isfloating)
			resize(c, c->x, c->y, c->w, c->h, c->bw, 0);
//			resize(c, c->x, c->y, c->w, c->h, borderpx, 0);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}
void switchtag(const Arg *arg)
{
    unsigned int columns;
    unsigned int new_tagset = 0;
    unsigned int pos, i;
    int col, row;
    Arg new_arg;

    columns = (drawtagmask & DRAWCLASSICTAGS) ? LENGTH(tags) : LENGTH(tags) / tagrows + ((LENGTH(tags) % tagrows > 0) ? 1 : 0);

    for (i = 0; i < LENGTH(tags); ++i) {
        if (!(selmon->tagset[selmon->seltags] & 1 << i)) {
            continue;
        }
        pos = i;
        row = pos / columns;
        col = pos % columns;
        if (arg->ui & SWITCHTAG_UP) {     /* UP */
            row --;
            if (row < 0) {
                row = tagrows - 1;
            }
            do {
                pos = row * columns + col;
                row --;
            } while (pos >= LENGTH(tags));
        }
        if (arg->ui & SWITCHTAG_DOWN) {     /* DOWN */
            row ++;
            if (row >= tagrows) {
                row = 0;
            }
            pos = row * columns + col;
            if (pos >= LENGTH(tags)) {
                row = 0;
            }
            pos = row * columns + col;
        }
        if (arg->ui & SWITCHTAG_LEFT) {     /* LEFT */
            col --;
            if (col < 0) {
                col = columns - 1;
            }
            do {
                pos = row * columns + col;
                col --;
            } while (pos >= LENGTH(tags));
        }
        if (arg->ui & SWITCHTAG_RIGHT) {     /* RIGHT */
            col ++;
            if (col >= columns) {
                col = 0;
            }
            pos = row * columns + col;
            if (pos >= LENGTH(tags)) {
                col = 0;
                pos = row * columns + col;
            }
        }
        new_tagset |= 1 << pos;
    }
    new_arg.ui = new_tagset;
    if (arg->ui & SWITCHTAG_TOGGLETAG) {
        toggletag(&new_arg);
    }
    if (arg->ui & SWITCHTAG_TAG) {
        tag(&new_arg);
    }
    if (arg->ui & SWITCHTAG_VIEW) {
        view (&new_arg);
    }
    if (arg->ui & SWITCHTAG_TOGGLEVIEW) {
        toggleview (&new_arg);
    }
}

void
sigchld(int unused)
{
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		die("can't install SIGCHLD handler:");
	while (0 < waitpid(-1, NULL, WNOHANG));
}

void
sigdwmblocks(const Arg *arg)
{
	union sigval sv;
	sv.sival_int = (dwmblockssig << 8) | arg->i;
	if (!dwmblockspid)
		if (getdwmblockspid() == -1)
			return;

	if (sigqueue(dwmblockspid, SIGUSR1, sv) == -1) {
		if (errno == ESRCH) {
			if (!getdwmblockspid())
				sigqueue(dwmblockspid, SIGUSR1, sv);
		}
	}
}

void
spawn(const Arg *arg)
{
	spawncmd(arg);
}

pid_t
spawncmd(const Arg *arg)
{
	pid_t pid;
	if (arg->v == dmenucmd)
		dmenumon[0] = '0' + selmon->num;
	if ((pid = fork()) == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[0]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
	return pid;
}

int
swallow(Client *t, Client *c)
{
	if (c->noswallow || c->isterminal)
		return 0;
	if (!swallowfloating && c->isfloating)
		return 0;

	replaceclient(t, c);
	c->ignorecfgreqpos = 1;
	c->swallowing = t;

	return 1;
}

void
unswallow(Client *c)
{
	replaceclient(c, c->swallowing);
	c->swallowing = NULL;
}

int
status2dtextlength(char* stext)
{
	int i, w, len;
	short isCode = 0;
	char *text;
	char *p;

	len = strlen(stext) + 1;
	if (!(text = (char*) malloc(sizeof(char)*len)))
		die("malloc");
	p = text;
	copyvalidchars(text, stext);

	/* compute width of the status text */
	w = 0;
	i = -1;
	while (text[++i]) {
		if (text[i] == '^') {
			if (!isCode) {
				isCode = 1;
				text[i] = '\0';
				w += TEXTW(text) - lrpad;
				text[i] = '^';
				if (text[++i] == 'f')
					w += atoi(text + ++i);
			} else {
				isCode = 0;
				text = text + i + 1;
				i = -1;
			}
		}
	}
	if (!isCode)
		w += TEXTW(text) - lrpad;
	free(p);
	return w;
}

Monitor *
systraytomon(Monitor *m) {
	Monitor *t;
	int i, n;
	if(!systraypinning) {
		if(!m)
			return selmon;
		return m == selmon ? m : NULL;
	}
	for(n = 1, t = mons; t && t->next; n++, t = t->next) ;
	for(i = 1, t = mons; t && t->next && i < systraypinning; i++, t = t->next) ;
	if(systraypinningfailfirst && n < systraypinning)
		return mons;
	return t;
}

void
switchcol(const Arg *arg)
{
	Client *c, *t;
	int col = 0;
	int i;

	if (!selmon->sel)
		return;
	for (i = 0, c = nexttiled(selmon->clients); c ;
	     c = nexttiled(c->next), i++) {
		if (c == selmon->sel)
			col = (i + 1) > selmon->nmaster;
	}
	if (i <= selmon->nmaster)
		return;
	for (c = selmon->stack; c; c = c->snext) {
		if (!ISVISIBLE(c))
			continue;
		for (i = 0, t = nexttiled(selmon->clients); t && t != c;
		     t = nexttiled(t->next), i++);
		if (t && (i + 1 > selmon->nmaster) != col) {
			focus(c);
			restack(selmon);
			break;
		}
	}
}

void spawnscratch(const Arg *arg)
{
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[1], ((char **)arg->v)+1);
		fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[1]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
}

void
tag(const Arg *arg)
{
	if (selmon->sel && arg->ui & TAGMASK) {
		selmon->sel->tags = arg->ui & TAGMASK;
		focus(NULL);
		arrange(selmon);
	}
}

void
tagmon(const Arg *arg)
{
	if (!selmon->sel || !mons->next)
		return;
	sendmon(selmon->sel, dirtomon(arg->i));
}

void
tile(Monitor *m)
{
	char sym1 = 61, sym2 = 93, sym3 = 61, sym;
	int x1 = m->wx + m->gappx, y1 = m->wy + m->gappx, h1 = m->wh - m->gappx, w1 = m->ww - m->gappx, X1 = x1 + w1, Y1 = y1 + h1;
	int x2 = m->wx + m->gappx, y2 = m->wy + m->gappx, h2 = m->wh - m->gappx, w2 = m->ww - m->gappx, X2 = x2 + w2, Y2 = y2 + h2;
	unsigned int i, n, n1, n2, bw;
	Client *c, *s, *d, *t, *o;
	float mfacts = 0, sfacts = 0;

	if (notileborder == 1 && m->gappx > borderpx)
		bw = 0;
	else
		bw = borderpx;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) {
		if (n < m->nmaster)
			mfacts += c->cfact;
		else
			sfacts += c->cfact;
	}
	if(m->nmaster > n)
		m->nmaster = (n == 0) ? 1 : n;
	/* layout symbol */
	if(abs(m->ltaxis[0]) == m->ltaxis[1])    /* explicitly: ((abs(m->ltaxis[0]) == 1 && m->ltaxis[1] == 1) || (abs(m->ltaxis[0]) == 2 && m->ltaxis[1] == 2)) */
		sym1 = 124;
	if(abs(m->ltaxis[0]) == m->ltaxis[2])
		sym3 = 124;
	if(m->ltaxis[1] == 3)
		sym1 = (n == 0) ? 0 : m->nmaster;
	if(m->ltaxis[2] == 3)
		sym3 = (n == 0) ? 0 : n - m->nmaster;
	if(m->ltaxis[0] < 0) {
		sym = sym1;
		sym1 = sym3;
		sym2 = 91;
		sym3 = sym;
	}
	if(m->nmaster == 1) {
		if(m->ltaxis[0] > 0)
			sym1 = 91;
		else
			sym3 = 93;
	}
	if(m->nmaster > 1 && m->ltaxis[1] == 3 && m->ltaxis[2] == 3)
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "%d%c%d", sym1, sym2, sym3);
	else if((m->nmaster > 1 && m->ltaxis[1] == 3 && m->ltaxis[0] > 0) || (m->ltaxis[2] == 3 && m->ltaxis[0] < 0))
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "%d%c%c", sym1, sym2, sym3);
	else if((m->ltaxis[2] == 3 && m->ltaxis[0] > 0) || (m->nmaster > 1 && m->ltaxis[1] == 3 && m->ltaxis[0] < 0))
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "%c%c%d", sym1, sym2, sym3);
	else
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "%c%c%c", sym1, sym2, sym3);
	if (n == 0)
		return;

	/* master and stack area */
	if(abs(m->ltaxis[0]) == 1 && n > m->nmaster) {
		w1 *= m->mfact;
		w2 -= w1;
		x1 += (m->ltaxis[0] < 0) ? w2 : 0;
		x2 += (m->ltaxis[0] < 0) ? 0 : w1;
	} else if(abs(m->ltaxis[0]) == 2 && n > m->nmaster) {
		h1 *= m->mfact;
		h2 -= h1;
		y1 += (m->ltaxis[0] < 0) ? h2 : 0;
		y2 += (m->ltaxis[0] < 0) ? 0 : h1;
	}

	if(m->gappx == 0) {
		if(abs(m->showbar) + abs(m->showebar) == 0) {
			y1 -= topbar ? borderpx : 0;	h1 += borderpx;
			y2 -= topbar ? borderpx : 0;	h2 += borderpx;
		}
		if(abs(m->ltaxis[0]) == 1 && n > m->nmaster) {
			h1 += borderpx;			h2 += borderpx;
			w1 += borderpx;			w2 += borderpx;
			if(m->ltaxis[0] < 0)	x2 -= borderpx;
			else					x1 -= borderpx;
			if(m->topbar == 0) {	y1 -= borderpx;	y2 -= borderpx; }
		}
		if(abs(m->ltaxis[0]) == 2 && n > m->nmaster) {
			w1 += 2 * borderpx;		w2 += 2 * borderpx;
			x1 -= borderpx;			x2 -= borderpx;
			if(m->topbar == 0) {	h1 += borderpx;	y1 -= borderpx; }
			else					h2 += borderpx;
		}
		if(n == 1) {
			h1 += borderpx;			h2 += borderpx;
			w1 += 2 * borderpx;		w2 += 2 * borderpx;
			x2 -= borderpx;			x1 -= borderpx;
			if(m->topbar == 0) {	y1 -= borderpx;	y2 -= borderpx; }
		}
	}

	X1 = x1 + w1; X2 = x2 + w2; Y1 = y1 + h1; Y2 = y2 + h2;

	/* master */
	n1 = (m->ltaxis[1] != 1 || w1 < (bh + m->gappx + 2 * borderpx) * (m->nmaster + 1)) ? 1 : m->nmaster;
	n2 = (m->ltaxis[1] != 2 || h1 < (bh + m->gappx + 2 * borderpx) * (m->nmaster + 1)) ? 1 : m->nmaster;
	for(i = 0, o = c = nexttiled(m->clients); i < m->nmaster; o = c = nexttiled(c->next), i++) {
		resize(c, x1, y1,
			(m->ltaxis[1] == 1 && i + 1 == m->nmaster) ? X1 - x1 - 2 * bw - m->gappx : w1 * (n1 > 1 ? (c->cfact / mfacts) : 1) - 2 * bw - m->gappx,
			(m->ltaxis[1] == 2 && i + 1 == m->nmaster) ? Y1 - y1 - 2 * bw - m->gappx : h1 * (n2 > 1 ? (c->cfact / mfacts) : 1) - 2 * bw - m->gappx,
			bw, 0);
		if(n1 > 1)
			x1 = c->x + WIDTH(c) + m->gappx;
		if(n2 > 1)
			y1 = c->y + HEIGHT(c) + m->gappx;
	}
	if(m->ltaxis[1] == 3) {
		for(i = 0, o = d = nexttiled(m->clients); i < m->nmaster; o = d = nexttiled(d->next), i++) {
			XMoveWindow(dpy, d->win, WIDTH(d) * -2, d->y);
		}
		for (t = m->stack; t; t = t->snext) {
			if (!ISVISIBLE(t) || t->isfloating)
				continue;
			for (i = 0, d = nexttiled(m->clients); d && d != t; d = nexttiled(d->next), i++);
			if (i >= m->nmaster)
				continue;
			XMoveWindow(dpy, t->win, x1, y1);
//			resize(t, x1, y1, w1 - 2 * bw - m->gappx, h1 - 2 * bw - m->gappx, bw, 0);
			break;
		}
	}

	/* stack */
	if(n > m->nmaster) {
		n1 = (m->ltaxis[2] != 1 || w2 < (bh + m->gappx + 2 * borderpx) * (n - m->nmaster + 1)) ? 1 : n - m->nmaster;
		n2 = (m->ltaxis[2] != 2 || h2 < (bh + m->gappx + 2 * borderpx) * (n - m->nmaster + 1)) ? 1 : n - m->nmaster;
			for(i = 0, c = o; c; c = nexttiled(c->next), i++) {
				resize(c, x2, y2, 
					(m->ltaxis[2] == 1 && i + 1 == n - m->nmaster) ? X2 - x2 - 2 * bw - m->gappx : w2 * (n1 > 1 ? (c->cfact / sfacts) : 1) - 2 * bw - m->gappx, 
					(m->ltaxis[2] == 2 && i + 1 == n - m->nmaster) ? Y2 - y2 - 2 * bw - m->gappx : h2 * (n2 > 1 ? (c->cfact / sfacts) : 1) - 2 * bw - m->gappx,
					bw, 0);
				if(n1 > 1)
					x2 = c->x + WIDTH(c) + m->gappx;
				if(n2 > 1)
					y2 = c->y + HEIGHT(c) + m->gappx;
			}
		if(m->ltaxis[2] == 3) {
			for(i = 0, c = o; c; c = nexttiled(c->next), i++)
				XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
			for (s = m->stack; s; s = s->snext) {
				if (!ISVISIBLE(s) || s->isfloating)
					continue;
				for (i = 0, c = nexttiled(m->clients); c && c != s; c = nexttiled(c->next), i++);
				if (i < m->nmaster)
					continue;
				resize(c, x2, y2, w2 - 2 * bw - m->gappx, h2 - 2 * bw - m->gappx, bw, 0);
				XMoveWindow(dpy, c->win, x2, y2);
				break;
			}
		}
	}
}

void
togglebar(const Arg *arg)
{
	selmon->showbar = selmon->pertag->showbars[selmon->pertag->curtag] = !selmon->showbar;
	updatebarpos(selmon);
	if (showsystray) {
		XWindowChanges wc;
		if (!selmon->showbar)
			wc.y = -bh;
		else if (selmon->showbar) {
			wc.y = 0;
			if (!selmon->topbar)
				wc.y = selmon->mh - bh;
		}
		XConfigureWindow(dpy, systray->win, CWY, &wc);
	}
	XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww, bh);
	XMoveResizeWindow(dpy, selmon->ebarwin, selmon->wx, selmon->eby, selmon->ww, bh);
	arrange(selmon);
}

void
toggleebar(const Arg *arg)
{
	selmon->showebar = selmon->pertag->showebars[selmon->pertag->curtag] = !selmon->showebar;
    updatebarpos(selmon);
	if (showsystray) {
		XWindowChanges wc;
		if (!selmon->showbar)
			wc.y = -bh;
		else if (selmon->showbar) {
			wc.y = 0;
			if (!selmon->topbar)
				wc.y = selmon->mh - bh;
		}
		XConfigureWindow(dpy, systray->win, CWY, &wc);
	}
    XMoveResizeWindow(dpy, selmon->ebarwin, selmon->wx, selmon->eby, selmon->ww, bh);
	XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww, bh);
    arrange(selmon);
}

void
togglebars(const Arg *arg)
{
	if((selmon->showbar + selmon->showebar == 2))
		toggleebar(NULL);
	else if((selmon->showbar + selmon->showebar == 1))
		togglebar(NULL);
	else if((selmon->showbar + selmon->showebar == 0)) {
		togglebar(NULL);
		toggleebar(NULL);
	}
}

void
togglefloating(const Arg *arg)
{
	if (!selmon->sel)
		return;
	selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
	if (selmon->sel->isfloating)
		/* restore last known float dimensions */
		resize(selmon->sel, selmon->sel->sfx, selmon->sel->sfy,
				selmon->sel->sfw - 2 * (borderpx - selmon->sel->bw),
				selmon->sel->sfh - 2 * (borderpx - selmon->sel->bw),
				borderpx, 0);
	else {
		/* save last known float dimensions */
		selmon->sel->sfx = selmon->sel->x;
		selmon->sel->sfy = selmon->sel->y;
		selmon->sel->sfw = selmon->sel->w + (selmon->gappx > borderpx ? 2 * borderpx : 0);
		selmon->sel->sfh = selmon->sel->h + (selmon->gappx > borderpx ? 2 * borderpx : 0);
	}
	shadowfloat(selmon->sel);
	arrange(selmon);
	arrangemon(selmon);
}

void
togglefullscr(const Arg *arg)
{
  if(selmon->sel)
    setactfullscreen(selmon->sel);
}

void
togglescratch(const Arg *arg)
{
	Client *c;
	unsigned int found = 0;

	for (c = selmon->clients; c && !(found = c->scratchkey == ((char**)arg->v)[0][0]); c = c->next);
	if (found) {
		c->tags = ISVISIBLE(c) ? 0 : selmon->tagset[selmon->seltags];
		focus(NULL);
		arrange(selmon);

		if (ISVISIBLE(c)) {
			focus(c);
			restack(selmon);
		}

	} else{
		spawnscratch(arg);
	}
}

void
toggletag(const Arg *arg)
{
	unsigned int newtags;
	unsigned int i;

	if (!selmon->sel)
		return;
	newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
		selmon->sel->tags = newtags;
		if(newtags == ~0) {
			selmon->pertag->prevtag = selmon->pertag->curtag;
			selmon->pertag->curtag = 0;
		}
		if(!(newtags & 1 << (selmon->pertag->curtag - 1))) {
			selmon->pertag->prevtag = selmon->pertag->curtag;
			for (i = 0; !(newtags & 1 << i); i++); /* get first new tag */
			selmon->pertag->curtag = i + 1;
		}
		selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
		selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];
		selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
		selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
		selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
		selmon->ltaxis[0] = selmon->pertag->ltaxes[selmon->pertag->curtag][0];
		selmon->ltaxis[1] = selmon->pertag->ltaxes[selmon->pertag->curtag][1];
		selmon->ltaxis[2] = selmon->pertag->ltaxes[selmon->pertag->curtag][2];
		if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
			togglebar(NULL);
		focus(NULL);
		arrange(selmon);
	}
}

void
toggleview(const Arg *arg)
{
	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);
	int i;

	Client *const selected = selmon->sel;
	Client **const masters = calloc(selmon->nmaster, sizeof(Client *));
	if (!masters) {
		die("fatal: could not calloc() %u bytes \n", selmon->nmaster * sizeof(Client *));
	}
	Client *c;
	size_t j;
	for (c = nexttiled(selmon->clients), j = 0; c && j < selmon->nmaster; c = nexttiled(c->next), ++j)
		masters[selmon->nmaster - (j + 1)] = c;
	for (size_t j = 0; j < selmon->nmaster; ++j)
		if (masters[j])
			pop(masters[j]);
	free(masters);
	focus(selected);
	if (newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;

		if (newtagset == ~0) {
			selmon->pertag->prevtag = selmon->pertag->curtag;
			selmon->pertag->curtag = 0;
		}

		/* test if the user did not select the same tag */
		if (!(newtagset & 1 << (selmon->pertag->curtag - 1))) {
			selmon->pertag->prevtag = selmon->pertag->curtag;
			for (i = 0; !(newtagset & 1 << i); i++) ;
			selmon->pertag->curtag = i + 1;
		}

		/* apply settings for this view */
		selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
		selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
		selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
		selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
		selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

		if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
			togglebar(NULL);
		if (selmon->showebar != selmon->pertag->showebars[selmon->pertag->curtag])
			toggleebar(NULL);

		focus(NULL);
		arrange(selmon);
	}
}

void
unfocus(Client *c, int setfocus)
{
	if (!c)
		return;
	grabbuttons(c, 0);
	if(c->isfloating)
		XSetWindowBorder(dpy, c->win, scheme[SchemeBorder][ColFloat].pixel);
	else {
		if ( selmon->gappx > borderpx)
			XSetWindowBorder(dpy, c->win, scheme[SchemeBorder][ColBg].pixel);
		else
			XSetWindowBorder(dpy, c->win, scheme[SchemeBorder][ColBorder].pixel);
	}
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void
unmanage(Client *c, int destroyed)
{
	Client *s;
	Monitor *m = c->mon;
	XWindowChanges wc;

	if (c->swallowing)
		unswallow(c);

	s = swallowingclient(c->win);
	if (s)
		s->swallowing = NULL;

	detach(c);
	detachstack(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	free(c);
	focus(NULL);
	updateclientlist();
	arrange(m);
	if (c->switchtotag) {
		Arg a = { .ui = c->switchtotag };
		view(&a);
	}
}

void
unmapnotify(XEvent *e)
{
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = wintoclient(ev->window))) {
		if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, 0);
	} else if (showsystray && (c = wintosystrayicon(ev->window))) {
		/* KLUDGE! sometimes icons occasionally unmap their windows, but do
		 * _not_ destroy them. We map those windows back */
		XMapRaised(dpy, c->win);
		removesystrayicon(c);
		updatesystray();
	}
}

void
updatebars(void)
{
	unsigned int w;
	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixel = 0,
		.border_pixel = 0,
		.colormap = cmap,
		.event_mask = ButtonPressMask|ExposureMask
	};
	XClassHint ch = {"dwm", "dwm"};
	for (m = mons; m; m = m->next) {
		if (!m->barwin) {
			w = m->ww;
			if (showsystray && m == systraytomon(m))
				w -= getsystraywidth();
			m->barwin = XCreateWindow(dpy, root, m->wx, m->by, w, bh, 0, depth,
			                          InputOutput, visual,
			                          CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &wa);
			XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
			if (showsystray && m == systraytomon(m))
				XMapRaised(dpy, systray->win);
			XMapRaised(dpy, m->barwin);
			XSetClassHint(dpy, m->barwin, &ch);
		}
        if(!m->ebarwin) {
			m->ebarwin = XCreateWindow(dpy, root, m->wx, m->eby, mons->ww, bh, 0, depth,
									InputOutput, visual,
									CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &wa);
			XDefineCursor(dpy, m->ebarwin, cursor[CurNormal]->cursor);
			XMapRaised(dpy, m->ebarwin);
			XSetClassHint(dpy, m->ebarwin, &ch);
		}
	}
}

void
updatebarpos(Monitor *m)
{
	m->wy = m->my;
	m->wh = m->mh;
	if ((m->showbar) && (m->showebar)){
		m->wh -= 2 * bh;
		m->wy = m->topbar ? m->wy + 2 * bh : m->wy;
		m->by = m->topbar ? m->wy - bh : m->wy + m->wh;
		m->eby =  m->topbar ? m->wy - 2 * bh : m->wy + m->wh + bh;
	} else if ((m->showbar) && !(m->showebar)) {
		m->wh -= bh;
		m->wy = m->topbar ? m->wy + bh: m->wy;
		m->by = m->topbar ? m->wy - bh : m->wy + m->wh;
		m->eby = - bh;
	} else if (!(m->showbar) && (m->showebar)) {
		m->wh -= bh;
		m->wy = m->topbar ? m->wy + bh: m->wy;
		m->eby = m->topbar ? m->wy - bh : m->wy + m->wh;
		m->by = - bh;
	} else {
		m->eby = - bh;
		m->by = - bh;
    }
}

void
updateclientlist()
{
	Client *c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty(dpy, root, netatom[NetClientList],
				XA_WINDOW, 32, PropModeAppend,
				(unsigned char *) &(c->win), 1);
}

int
updategeom(void)
{
	int dirty = 0;

#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
		XineramaScreenInfo *unique = NULL;

		for (n = 0, m = mons; m; m = m->next, n++);
		/* only consider unique geometries as separate screens */
		unique = ecalloc(nn, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;
		if (n <= nn) { /* new monitors available */
			for (i = 0; i < (nn - n); i++) {
				for (m = mons; m && m->next; m = m->next);
				if (m)
					m->next = createmon();
				else
					mons = createmon();
			}
			for (i = 0, m = mons; i < nn && m; m = m->next, i++)
				if (i >= n
				|| unique[i].x_org != m->mx || unique[i].y_org != m->my
				|| unique[i].width != m->mw || unique[i].height != m->mh)
				{
					dirty = 1;
					m->num = i;
					m->mx = m->wx = unique[i].x_org;
					m->my = m->wy = unique[i].y_org;
					m->mw = m->ww = unique[i].width;
					m->mh = m->wh = unique[i].height;
					updatebarpos(m);
				}
		} else { /* less monitors available nn < n */
			for (i = nn; i < n; i++) {
				for (m = mons; m && m->next; m = m->next);
				while ((c = m->clients)) {
					dirty = 1;
					m->clients = c->next;
					detachstack(c);
					c->mon = mons;
					switch(attachdirection){
					case 1:
						attachabove(c);
						break;
					case 2:
						attachaside(c);
						break;
					case 3:
						attachbelow(c);
						break;
					case 4:
						attachbottom(c);
						break;
					case 5:
						attachtop(c);
						break;
					default:
						attach(c);
					}
					attachstack(c);
				}
				if (m == selmon)
					selmon = mons;
				cleanupmon(m);
			}
		}
		free(unique);
	} else
#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh) {
			dirty = 1;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
		}
	}
	if (dirty) {
		selmon = mons;
		selmon = wintomon(root);
	}
	return dirty;
}

void
updatenumlockmask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
				== XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
}

void
updatestatus(void)
{
	if (!gettextprop(root, XA_WM_NAME, rawstext, sizeof(rawstext)))
		strcpy(stext, "dwm-"VERSION);
	else
		copyvalidchars(stext, rawstext);
	drawebar(rawstext, selmon, 0);
}

void
updatesystray(void)
{
	XSetWindowAttributes wa;
	XWindowChanges wc;
	Client *i;
	Monitor *m = systraytomon(NULL);
	unsigned int x = m->mx + m->mw;
	unsigned int w = 1, xpad = 0, ypad = 0;
	#if BARPADDING_PATCH
	xpad = sp;
	ypad = vp;
	#endif // BARPADDING_PATCH

	if (!showsystray)
		return;
	if (!systray) {
		/* init systray */
		if (!(systray = (Systray *)calloc(1, sizeof(Systray))))
			die("fatal: could not malloc() %u bytes\n", sizeof(Systray));

		wa.override_redirect = True;
		wa.event_mask = ButtonPressMask|ExposureMask;
		wa.background_pixel = scheme[BARBORDERS ? SchemeUnfocus : SchemeBar][ColBg].pixel;
		wa.border_pixel = 0;
		wa.colormap = cmap;
		systray->win = XCreateWindow(dpy, root, x - xpad, m->by + ypad, w, bh, 0, depth,
						InputOutput, visual,
						CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &wa);
		XSelectInput(dpy, systray->win, SubstructureNotifyMask);
		XChangeProperty(dpy, systray->win, netatom[NetSystemTrayOrientation], XA_CARDINAL, 32,
				PropModeReplace, (unsigned char *)&systrayorientation, 1);
		XChangeProperty(dpy, systray->win, netatom[NetSystemTrayVisual], XA_VISUALID, 32,
				PropModeReplace, (unsigned char *)&visual->visualid, 1);
		XChangeProperty(dpy, systray->win, netatom[NetWMWindowType], XA_ATOM, 32,
				PropModeReplace, (unsigned char *)&netatom[NetWMWindowTypeDock], 1);
		XMapRaised(dpy, systray->win);
		XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
		if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
			sendevent(root, xatom[Manager], StructureNotifyMask, CurrentTime, netatom[NetSystemTray], systray->win, 0, 0);
			XSync(dpy, False);
		}
		else {
			fprintf(stderr, "dwm: unable to obtain system tray.\n");
			free(systray);
			systray = NULL;
			return;
		}
	}

	drw_setscheme(drw, scheme[BARBORDERS ? SchemeUnfocus : SchemeBar]);
	for (w = 0, i = systray->icons; i; i = i->next) {
		/* make sure the background color stays the same */
		wa.background_pixel = scheme[BARBORDERS ? SchemeUnfocus : SchemeBar][ColBg].pixel;
		XChangeWindowAttributes(dpy, i->win, CWBackPixel, &wa);
		XMapRaised(dpy, i->win);
		w += systrayspacing;
		i->x = w;
		XMoveResizeWindow(dpy, i->win, i->x, 0, i->w, i->h);
		w += i->w;
		if (i->mon != m)
			i->mon = m;
	}
	w = w ? w + systrayspacing : 1;
	x -= w;
	XMoveResizeWindow(dpy, systray->win, x - xpad, m->by + ypad, w, bh);
	wc.x = x - xpad;
	wc.y = m->by + ypad;
	wc.width = w;
	wc.height = bh;
	wc.stack_mode = Above; wc.sibling = m->barwin;
	XConfigureWindow(dpy, systray->win, CWX|CWY|CWWidth|CWHeight|CWSibling|CWStackMode, &wc);
	XMapWindow(dpy, systray->win);
	XMapSubwindows(dpy, systray->win);
	/* redraw background */
	XSetForeground(dpy, drw->gc, scheme[BARBORDERS ? SchemeUnfocus : SchemeBar][ColBg].pixel);
	XFillRectangle(dpy, systray->win, drw->gc, 0, 0, w, bh);
	XSync(dpy, False);
}

void
updatesystrayicongeom(Client *i, int w, int h)
{
	if (i) {
		i->h = bh;
		if (w == h)
			i->w = bh;
		else if (h == bh)
			i->w = w;
		else
			i->w = (int) ((float)bh * ((float)w / (float)h));
		applysizehints(i, &(i->x), &(i->y), &(i->w), &(i->h), &(i->bw), False);
		/* force icons into the systray dimensions if they don't want to */
		if (i->h > bh) {
			if (i->w == i->h)
				i->w = bh;
			else
				i->w = (int) ((float)bh * ((float)i->w / (float)i->h));
			i->h = bh;
		}
	}
}

void
updatesystrayiconstate(Client *i, XPropertyEvent *ev)
{
	long flags;
	int code = 0;

	if (!showsystray || !i || ev->atom != xatom[XembedInfo] ||
			!(flags = getatomprop(i, xatom[XembedInfo])))
		return;

	if (flags & XEMBED_MAPPED && !i->tags) {
		i->tags = 1;
		code = XEMBED_WINDOW_ACTIVATE;
		XMapRaised(dpy, i->win);
		setclientstate(i, NormalState);
	}
	else if (!(flags & XEMBED_MAPPED) && i->tags) {
		i->tags = 0;
		code = XEMBED_WINDOW_DEACTIVATE;
		XUnmapWindow(dpy, i->win);
		setclientstate(i, WithdrawnState);
	}
	else
		return;
	sendevent(i->win, xatom[Xembed], StructureNotifyMask, CurrentTime, code, 0,
			systray->win, XEMBED_EMBEDDED_VERSION);
 }

void
updatetitle(Client *c)
{
	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, broken);
}

void
updatewindowtype(Client *c)
{
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (wtype == netatom[NetWMWindowTypeDialog])
		c->isfloating = 1;
}

void
updatewmhints(Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->win))) {
		if (c == selmon->sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		} else
			c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = 0;
		XFree(wmh);
	}
}

void
view(const Arg *arg)
{
	int i;
	unsigned int tmptag;

	if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK) {
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
		selmon->pertag->prevtag = selmon->pertag->curtag;

		if (arg->ui == ~0)
			selmon->pertag->curtag = 0;
		else {
			for (i = 0; !(arg->ui & 1 << i); i++) ;
			selmon->pertag->curtag = i + 1;
		}
	} else {
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}
	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
	selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
	selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
	selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];
	selmon->ltaxis[0] = selmon->pertag->ltaxes[selmon->pertag->curtag][0];
	selmon->ltaxis[1] = selmon->pertag->ltaxes[selmon->pertag->curtag][1];
	selmon->ltaxis[2] = selmon->pertag->ltaxes[selmon->pertag->curtag][2];
	if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
		togglebar(NULL);
	if (selmon->showebar != selmon->pertag->showebars[selmon->pertag->curtag])
		toggleebar(NULL);
	focus(NULL);
	arrange(selmon);
}

pid_t
winpid(Window w)
{
	pid_t result = 0;

#ifdef __linux__
	xcb_res_client_id_spec_t spec = {0};
	spec.client = w;
	spec.mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID;

	xcb_generic_error_t *e = NULL;
	xcb_res_query_client_ids_cookie_t c = xcb_res_query_client_ids(xcon, 1, &spec);
	xcb_res_query_client_ids_reply_t *r = xcb_res_query_client_ids_reply(xcon, c, &e);

	if (!r)
		return (pid_t)0;

	xcb_res_client_id_value_iterator_t i = xcb_res_query_client_ids_ids_iterator(r);
	for (; i.rem; xcb_res_client_id_value_next(&i)) {
		spec = i.data->spec;
		if (spec.mask & XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID) {
			uint32_t *t = xcb_res_client_id_value_value(i.data);
			result = *t;
			break;
		}
	}

	free(r);

	if (result == (pid_t)-1)
		result = 0;
#endif /* __linux__ */
#ifdef __OpenBSD__
	Atom type;
	int format;
	unsigned long len, bytes;
	unsigned char *prop;
	pid_t ret;

	if (XGetWindowProperty(dpy, w, XInternAtom(dpy, "_NET_WM_PID", 0), 0, 1, False, AnyPropertyType, &type, &format, &len, &bytes, &prop) != Success || !prop)
		return 0;

	ret = *(pid_t*)prop;
	XFree(prop);
	result = ret;
#endif /* __OpenBSD__ */

	return result;
}

pid_t
getparentprocess(pid_t p)
{
	unsigned int v = 0;

#ifdef __linux__
	FILE *f;
	char buf[256];
	snprintf(buf, sizeof(buf) - 1, "/proc/%u/stat", (unsigned)p);

	if (!(f = fopen(buf, "r")))
		return 0;

	fscanf(f, "%*u %*s %*c %u", &v);
	fclose(f);
#endif /* __linux__*/

#ifdef __OpenBSD__
	int n;
	kvm_t *kd;
	struct kinfo_proc *kp;

	kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, NULL);
	if (!kd)
		return 0;

	kp = kvm_getprocs(kd, KERN_PROC_PID, p, sizeof(*kp), &n);
	v = kp->p_ppid;
#endif /* __OpenBSD__ */

	return (pid_t)v;
}

int
isdescprocess(pid_t p, pid_t c)
{
	while (p != c && c != 0)
		c = getparentprocess(c);

	return (int)c;
}

Client *
termforwin(const Client *w)
{
	Client *c;
	Monitor *m;

	if (!w->pid || w->isterminal)
		return NULL;

	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			if (c->isterminal && !c->swallowing && c->pid && isdescprocess(c->pid, w->pid))
				return c;
		}
	}

	return NULL;
}

Client *
swallowingclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			if (c->swallowing && c->swallowing->win == w)
				return c;
		}
	}

	return NULL;
}

Client *
wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;
	return NULL;
}

Monitor *
wintomon(Window w)
{
	int x, y;
	Client *c;
	Monitor *m;

	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for (m = mons; m; m = m->next)
		if (w == m->barwin)
			return m;
	if ((c = wintoclient(w)))
		return c->mon;
	return selmon;
}

Client *
wintosystrayicon(Window w) {
	Client *i = NULL;

	if (!showsystray || !w)
		return i;
	for (i = systray->icons; i && i->win != w; i = i->next);
	return i;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("dwm: another window manager is already running");
	return -1;
}

void
xinitvisual()
{
	XVisualInfo *infos;
	XRenderPictFormat *fmt;
	int nitems;
	int i;

	XVisualInfo tpl = {
		.screen = screen,
		.depth = 32,
		.class = TrueColor
	};
	long masks = VisualScreenMask | VisualDepthMask | VisualClassMask;

	infos = XGetVisualInfo(dpy, masks, &tpl, &nitems);
	visual = NULL;
	for(i = 0; i < nitems; i ++) {
		fmt = XRenderFindVisualFormat(dpy, infos[i].visual);
		if (fmt->type == PictTypeDirect && fmt->direct.alphaMask) {
			visual = infos[i].visual;
			depth = infos[i].depth;
			cmap = XCreateColormap(dpy, root, visual, AllocNone);
			useargb = 1;
			break;
		}
	}

	XFree(infos);

	if (! visual) {
		visual = DefaultVisual(dpy, screen);
		depth = DefaultDepth(dpy, screen);
		cmap = DefaultColormap(dpy, screen);
	}
}

void
xrdb(const Arg *arg)
{
  loadxrdb();
  int i;
  for (i = 0; i < LENGTH(colors); i++)
                scheme[i] = drw_scm_create(drw, colors[i], alphas[i], 4);
  focus(NULL);
  arrange(NULL);
}

void
zoom(const Arg *arg)
{
	Client *c = selmon->sel;
	Client *at = NULL, *cold, *cprevious = NULL;

	if (!selmon->lt[selmon->sellt]->arrange
	|| (selmon->sel && selmon->sel->isfloating))
		return;
	if (c == nexttiled(selmon->clients)) {
		at = findbefore(prevzoom);
		if (at)
			cprevious = nexttiled(at->next);
		if (!cprevious || cprevious != prevzoom) {
			prevzoom = NULL;
			if (!c || !(c = nexttiled(c->next)))
				return;
		} else
			c = cprevious;
	}
	cold = nexttiled(selmon->clients);
	if (c != cold && !at)
		at = findbefore(c);
	detach(c);
	attach(c);
	/* swap windows instead of pushing the previous one down */
	if (c != cold && at) {
		prevzoom = cold;
		if (cold && at != cold) {
			detach(cold);
			cold->next = at->next;
			at->next = cold;
		}
	}
	focus(c);
	arrange(c->mon);
}

int
main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp("-v", argv[1]))
		die("dwm-"VERSION);
	else if (argc != 1)
		die("usage: dwm [-v]");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("dwm: cannot open display");
	if (!(xcon = XGetXCBConnection(dpy)))
		die("dwm: cannot get xcb connection\n");
	checkotherwm();
        XrmInitialize();
        loadxrdb();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec ps", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
	scan();
	run();
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}

void
insertclient(Client *item, Client *insertItem, int after) {
	Client *c;
	if (item == NULL || insertItem == NULL || item == insertItem) return;
	detach(insertItem);
	if (!after && selmon->clients == item) {
		attach(insertItem);
		return;
	}
	if (after) {
		c = item;
	} else {
		for (c = selmon->clients; c; c = c->next) { if (c->next == item) break; }
	}
	insertItem->next = c->next;
	c->next = insertItem;
}

void
inplacerotate(const Arg *arg)
{
	if(!selmon->sel || (selmon->sel->isfloating && !arg->f)) return;

	unsigned int selidx = 0, i = 0;
	Client *c = NULL, *stail = NULL, *mhead = NULL, *mtail = NULL, *shead = NULL;

	// Shift client
	for (c = selmon->clients; c; c = c->next) {
		if (ISVISIBLE(c) && !(c->isfloating)) {
		if (selmon->sel == c) { selidx = i; }
		if (i == selmon->nmaster - 1) { mtail = c; }
		if (i == selmon->nmaster) { shead = c; }
		if (mhead == NULL) { mhead = c; }
		stail = c;
		i++;
		}
	}
	if (arg->i < 0 && selidx >= selmon->nmaster) insertclient(stail, shead, 1);
	if (arg->i > 0 && selidx >= selmon->nmaster) insertclient(shead, stail, 0);
	if (arg->i < 0 && selidx < selmon->nmaster)  insertclient(mtail, mhead, 1);
	if (arg->i > 0 && selidx < selmon->nmaster)  insertclient(mhead, mtail, 0);

	// Restore focus position
	i = 0;
	for (c = selmon->clients; c; c = c->next) {
		if (!ISVISIBLE(c) || (c->isfloating)) continue;
		if (i == selidx) { focus(c); break; }
		i++;
	}
	arrange(selmon);
	focus(c);
}

void
transfer(const Arg *arg) {
	Client *c, *mtail = selmon->clients, *stail = NULL, *insertafter;
	int transfertostack = 0, i, nmasterclients;

	for (i = 0, c = selmon->clients; c; c = c->next) {
		if (!ISVISIBLE(c) || c->isfloating) continue;
		if (selmon->sel == c) { transfertostack = i < selmon->nmaster && selmon->nmaster != 0; }
		if (i < selmon->nmaster) { nmasterclients++; mtail = c; }
		stail = c;
		i++;
	}
	if (selmon->sel->isfloating || i == 0) {
		return;
	} else if (transfertostack) {
		selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = MIN(i, selmon->nmaster) - 1;
		insertafter = stail;
	} else {
		selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = selmon->nmaster + 1;
		insertafter = mtail;
	}
	if (insertafter != selmon->sel) {
		detach(selmon->sel);
		if (selmon->nmaster == 1 && !transfertostack) {
		 attach(selmon->sel); // Head prepend case
		} else {
			selmon->sel->next = insertafter->next;
			insertafter->next = selmon->sel;
		}
	}
	arrange(selmon);
}
