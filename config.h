//	██████╗ ██╗    ██╗███╗   ███╗
//	██╔══██╗██║    ██║████╗ ████║
//	██║  ██║██║ █╗ ██║██╔████╔██║
//	██║  ██║██║███╗██║██║╚██╔╝██║
//	██████╔╝╚███╔███╔╝██║ ╚═╝ ██║
//	╚═════╝  ╚══╝╚══╝ ╚═╝     ╚═╝


/* See LICENSE file for copyright and license details. */

#define BARTABGROUPS_FUZZPX 5          // # pixels cutoff between bartab groups to merge (e.g. max gaps px)
#define BARTABGROUPS_TAGSINDICATOR 1   // 0 = disable, 1 = enable when >1 client or view tag, 2 = enable always
#define BARTABGROUPS_TAGSPX 5          // # pixels for tag grid boxes
#define BARTABGROUPS_TAGSROWS 2        // # rows in tag grid
#define BARTABGROUPS_INDICATORSPADPX 2 // # pixels from l/r to pad tags indicators

/* appearance */
static const int bartheme           = 1;        /* 0 = off, 1 = on */
static const int statustheme        = 2;        /* 0 = classic, 1 = button-theme, 2 = float-theme */
static const int tabbartheme        = 2;        /* 0 = classic, 1 = button-theme, 2 = float-theme */
static const unsigned int gappx     = 10;       /* gaps between windows */
static const unsigned int borderpx  = 1;        /* border pixel of windows */
static const unsigned int snap      = 10;       /* snap pixel */
static const unsigned int systraypinning = 0;   /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayspacing = 2;   /* systray spacing */
static const int systraypinningfailfirst = 1;   /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
static const int showsystray        = 1;        /* 0 means no systray */
static const int swallowfloating    = 1;        /* 1 means swallow floating windows by default */
static const int showbar            = 1;        /* 0 means no bar */
static const int showebar           = 1;        /* 0 means no extra bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const int statuslastblock    = 1;        /* 1 means last item in statusbar is displayed as block */
static const int notileborder       = 1;        /* 1 disables tiled borders for non-small gaps */
static const int oneclientdimmer    = 1;        /* 1 makes tab for one client in unfocused color... */
static const int istatustimeout     = 5;              /* max timeout before displaying regular status after istatus */
static const char istatusprefix[]   = "msg: ";         /* prefix for important status messages */
static const char istatusclose[]    = "msg:close";    /* prefix for important status messages */
static const char slopspawnstyle[]  = "-t 0 -l -c 0.92,0.85,0.69,0.3 -o"; /* do NOT define -f (format) here */
static const char slopresizestyle[] = "-t 0 -l -c 0.92,0.85,0.69,0.3 -o"; /* do NOT define -f (format) here */
static const int riodraw_borders    = 0;        /* 0 or 1, indicates whether the area drawn using slop includes the window borders */
static const int riodraw_matchpid   = 1;        /* 0 or 1, indicates whether to match the PID of the client that was spawned with riospawn */
static const int riodraw_spawnasync = 0;        /* 0 means that the application is only spawned after a successful selection while
                                                 * 1 means that the application is being initialised in the background while the selection is made */
//static const char dmenufont[]       = "InconsolataLGC Nerd Font:pixelsize=14";
static const char dmenufont[]       = "TerminessTTF Nerd Font:pixelsize=16";
//static const char dmenufont[]       = "FantasqueSansMono Nerd Font:pixelsize=16";
//static const char dmenufont[]       = "TerminessTTF Nerd Font:pixelsize=18";
//static const char dmenufont[]       = "GohuFont Nerd Font:pixelsize=16";
static const char *fonts[]          = { dmenufont };
static char bar_fg[]		= "#eeeeee";
static char bar_bg[]		= "#222222";
static char bar_brd[]		= "#222222";
static char bar_flo[]		= "#222222";
static char tag_fg[]		= "#005577";
static char tag_bg[]		= "#eeeeee";
static char tag_brd[]		= "#222222";
static char tag_flo[]		= "#222222";
static char brd_fg[]		= "#eeeeee";
static char brd_bg[]		= "#222222";
static char brd_brd[]		= "#444444";
static char brd_flo[]		= "#444444";
static char sel_fg[]		= "#eeeeee";
static char sel_bg[]		= "#005577";
static char sel_brd[]		= "#444444";
static char sel_flo[]		= "#bbbbbb";
static char foc_fg[]		= "#222222";
static char foc_bg[]		= "#eeeeee";
static char foc_brd[]		= "#444444";
static char foc_flo[]		= "#bbbbbb";
static char unf_fg[]		= "#eeeeee";
static char unf_bg[]		= "#444444";
static char unf_brd[]		= "#222222";
static char unf_flo[]		= "#222222";
static const float transp    = 0.0;
static const float semitr    = 0.8;
static char *colors[][4] = {
    /*                    fg            bg          border      float       */
    [SchemeBar] =		{ bar_fg,		bar_bg,		bar_brd,	bar_flo		},
    [SchemeTag] =		{ tag_fg,		tag_bg,		tag_brd,	tag_flo		},
    [SchemeBorder] =	{ brd_fg,		brd_bg,		brd_brd,	brd_flo		},
    [SchemeSelect] =	{ sel_fg,		sel_bg,		sel_brd,	sel_flo		},
    [SchemeFocus] =		{ foc_fg,		foc_bg,		foc_brd,	foc_flo		},
    [SchemeUnfocus] =	{ unf_fg,		unf_bg,		unf_brd,	unf_flo  	}
};
static const float alphas[][4] = {
    /*                    fg            bg          border      float       */
    [SchemeBar] =		{ OPAQUE,		OPAQUE,		OPAQUE,		OPAQUE		},
    [SchemeTag] =		{ OPAQUE,		OPAQUE,		OPAQUE,		OPAQUE		},
    [SchemeBorder] =	{ OPAQUE,		OPAQUE,		OPAQUE,		OPAQUE		},
    [SchemeSelect] =	{ OPAQUE,		OPAQUE,		OPAQUE,		OPAQUE		},
    [SchemeFocus] =		{ OPAQUE,		OPAQUE,		OPAQUE,		OPAQUE		},
    [SchemeUnfocus] =	{ OPAQUE,		OPAQUE,		OPAQUE,		OPAQUE		}
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6" };

/* grid of tags */
#define DRAWCLASSICTAGS             1 << 0
#define DRAWTAGGRID                 1 << 1

#define SWITCHTAG_UP                1 << 0
#define SWITCHTAG_DOWN              1 << 1
#define SWITCHTAG_LEFT              1 << 2
#define SWITCHTAG_RIGHT             1 << 3
#define SWITCHTAG_TOGGLETAG         1 << 4
#define SWITCHTAG_TAG               1 << 5
#define SWITCHTAG_VIEW              1 << 6
#define SWITCHTAG_TOGGLEVIEW        1 << 7

static const unsigned int drawtagmask = DRAWCLASSICTAGS; /*  | DRAWCLASSICTAGS | DRAWTAGGRID |  */
static const int tagrows = 2;

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class     	instance  title           tags mask  switchtotag  isfloating  isterminal  noswallow  monitor  scratch key  float x,y,w,h   border (-1 ==> off; 0 ==> '0') */
	{ NULL,			NULL,		NULL,			0,			0,			 0,          0,           0,        -1,      0,           -10,-10,-10,-10,		-1  }, /* default */
	{ "Gimp",		NULL,		NULL,			0,			0,           1,          0,           0,        -1,      0,           -11,-11,-11,-11,		-1	},
	{ "qutebrowser",NULL,		NULL,			0,			0,           0,          0,           0,        -1,      0,           -10,-10,-10,-10,		-1	},
	{ "Onboard",	NULL,		NULL,			0,			0,           1,          0,           0,        -1,      0,           -10,-10,-10,-10,		0	},
	{ "St",			NULL,		NULL,			0,			0,           0,          1,           0,        -1,      0,           -10,-10,-10,-10,		-1	},
	{ NULL,			NULL,		"Event Tester",	0,			0,           1,          0,           1,        -1,      0,           -10,-10,-10,-10,		-1	}, /* xev */
	{ NULL,			NULL,		"scratchpad",	0,			0,           1,          1,           0,        -1,     's',          -10,-10,1200,500,		-1	},
	{ NULL,			"wallman",	NULL,			0,			0,           1,          0,           0,        -1,      0,           0,0,-9,110,			0	},
	/* dwmblocks click */
	{ "dbar",		"alsam",	NULL,			0,			0,           1,          0,           0,        -1,     'a',          0,-1,400,500,			1	},
	{ "dbar",		"pulsm",	NULL,			0,			0,           1,          0,           0,        -1,     'p',          0,-1,600,600,			1	},
	{ "dbar",		"psmem",	NULL,			0,			0,           1,          0,           0,        -1,      0,           730,-1,610,650,		1	},
	{ "dbar",		"calen",	NULL,			0,			0,           1,          0,           0,        -1,      0,           330,-1,170,160,		1	},
	{ "dbar",		"calcurse",	NULL,			0,			0,           1,          0,           0,        -1,      0,           330,-1,600,600,		1	},
	{ "dbar",		"htop",		NULL,			0,			0,           1,          0,           0,        -1,      0,           520,-1,800,650,		1	},
};

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */
static const int resizehintsbm = 3;    /* resizehints for > resizehintsbm * borderpixel */
static const int reshintsoffondrag = 0;  /* deaktivate resizehints on dragfact */
static const int attachdirection = 2;    /* 0 default, 1 above, 2 aside, 3 below, 4 bottom, 5 top */
static const int  layoutaxis[] = {
	1,    /* layout axis: 1 = x, 2 = y; negative values mirror the layout */
	2,    /* master axis: 1 = x (left to right), 2 = y (top to bottom), 3 = z (monocle) */
	2,    /* stack  axis: 1 = x (left to right), 2 = y (top to bottom), 3 = z (monocle) */
};

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },    /* first entry is default */
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ "[M]",      monocle },
};

/* key definitions */
#define MODKEY Mod4Mask
#define ALT Mod1Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, topbar ? NULL : "-b", NULL };
static const char *termcmd[]  = { "stmux",  NULL };
static const char *browser[] = { "qutebrowser", NULL, NULL, NULL, "Qutebrowser" };
static const char *scratchpadcmd[] = { "s", "scratchpad", NULL };
/* dwmblocks click */
static const char *alsam[] = { "a", "st", "-c", "dbar", "-n", "alsam", "-e", "alsamixer", NULL };
static const char *pulsm[] = { "p", "pavucontrol", "--class", "dbar", "--name", "pulsm", NULL, NULL, NULL };

#include <X11/XF86keysym.h>
static Key keys[] = {
	/* modifier                     key        function        argument */
    { 0,                            XF86XK_AudioLowerVolume, spawn, SHCMD("/usr/bin/amixer -q sset Master 5%-; kill -35 $(pidof dwmblocks)"    ) },
	{ 0,                            XF86XK_AudioRaiseVolume, spawn, SHCMD("/usr/bin/amixer -q sset Master 5%+; kill -35 $(pidof dwmblocks)"    ) },
	{ 0,                            XF86XK_AudioMute,        spawn, SHCMD("/usr/bin/amixer set Master toggle; kill -35 $(pidof dwmblocks)" ) },
	{ 0,                            XF86XK_AudioMicMute,     spawn, SHCMD("/usr/bin/amixer set Capture toggle" ) },
    { 0,                            XF86XK_AudioPlay,        spawn, SHCMD("playerctl play-pause") },
    { 0,                            XF86XK_AudioNext,        spawn, SHCMD("playerctl next") },
    { 0,                            XF86XK_AudioPrev,        spawn, SHCMD("playerctl previous") },
    { 0,                            XF86XK_MonBrightnessUp,  spawn, SHCMD("sleep 0.1; kill -36 $(pidof dwmblocks)") },
    { 0,                            XF86XK_MonBrightnessDown,spawn, SHCMD("sleep 0.1; kill -36 $(pidof dwmblocks)") },
	{ 0,                            XF86XK_Launch1,          spawn, {.v = dmenucmd } },
    { 0,                            XF86XK_Launch5,          spawn, SHCMD("thinkpad-rotate flip") },
    { 0,                            XF86XK_Launch6,          spawn, SHCMD("thinkpad-rotate left") },
	{ MODKEY|ControlMask,           XK_w,      runorraise,     {.v = browser } },
	{ MODKEY,                       XK_Return, spawn,          {.v = dmenucmd } },
	{ MODKEY|ShiftMask,             XK_Return, spawn,          {.v = termcmd } },
	{ MODKEY|ControlMask,           XK_Return, togglescratch,  {.v = scratchpadcmd } },
	{ MODKEY|ControlMask,           XK_s,      riospawn,       {.v = termcmd } },
	{ MODKEY,                       XK_s,      rioresize,      {0} },
	{ MODKEY,                       XK_b,      togglebars,     {0} },
	{ MODKEY|ControlMask,           XK_b,      togglebar,      {0} },
	{ MODKEY|ControlMask|ShiftMask, XK_b,      toggleebar,     {0} },
	{ MODKEY,                       XK_j,      switchcol,      {0} },
	{ MODKEY,                       XK_k,      switchcol,      {0} },
	{ MODKEY,                       XK_l,      focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_h,      focusstack,     {.i = -1 } },
	{ MODKEY|ControlMask|ShiftMask, XK_l,      inplacerotate,  {.i = +1} },
	{ MODKEY|ControlMask|ShiftMask, XK_h,      inplacerotate,  {.i = -1} },
	{ MODKEY|ControlMask|ShiftMask, XK_k,      incnmaster,     {.i = +1 } },
	{ MODKEY|ControlMask|ShiftMask, XK_j,      incnmaster,     {.i = -1 } },
	{ MODKEY|ControlMask,           XK_h,      setmfact,       {.f = -0.05} },
	{ MODKEY|ControlMask,           XK_l,      setmfact,       {.f = +0.05} },
	{ MODKEY|ControlMask,           XK_k,      setcfact,       {.f = +0.25} },
	{ MODKEY|ControlMask,           XK_j,      setcfact,       {.f = -0.25} },
	{ MODKEY|ControlMask,           XK_o,      setcfact,       {.f =  0.00} },
	{ MODKEY,                       XK_Down,   moveresize,     {.v = "0x 25y 0w 0h" } },
	{ MODKEY,                       XK_Up,     moveresize,     {.v = "0x -25y 0w 0h" } },
	{ MODKEY,                       XK_Right,  moveresize,     {.v = "25x 0y 0w 0h" } },
	{ MODKEY,                       XK_Left,   moveresize,     {.v = "-25x 0y 0w 0h" } },
	{ MODKEY|ShiftMask,             XK_Down,   moveresize,     {.v = "0x 0y 0w 25h" } },
	{ MODKEY|ShiftMask,             XK_Up,     moveresize,     {.v = "0x 0y 0w -25h" } },
	{ MODKEY|ShiftMask,             XK_Right,  moveresize,     {.v = "0x 0y 25w 0h" } },
	{ MODKEY|ShiftMask,             XK_Left,   moveresize,     {.v = "0x 0y -25w 0h" } },
	{ MODKEY|ControlMask,           XK_Up,     moveresizeedge, {.v = "t"} },
	{ MODKEY|ControlMask,           XK_Down,   moveresizeedge, {.v = "b"} },
	{ MODKEY|ControlMask,           XK_Left,   moveresizeedge, {.v = "l"} },
	{ MODKEY|ControlMask,           XK_Right,  moveresizeedge, {.v = "r"} },
	{ MODKEY|ControlMask|ShiftMask, XK_Up,     moveresizeedge, {.v = "T"} },
	{ MODKEY|ControlMask|ShiftMask, XK_Down,   moveresizeedge, {.v = "B"} },
	{ MODKEY|ControlMask|ShiftMask, XK_Left,   moveresizeedge, {.v = "L"} },
	{ MODKEY|ControlMask|ShiftMask, XK_Right,  moveresizeedge, {.v = "R"} },
	{ MODKEY|ShiftMask,             XK_l,      pushdown,       {0} },
	{ MODKEY|ShiftMask,             XK_h,      pushup,         {0} },
	{ MODKEY|ShiftMask,             XK_j,      transfer,       {0} },
	{ MODKEY|ShiftMask,             XK_k,      transfer,       {0} },
	{ MODKEY|ShiftMask,             XK_z,      zoom,           {0} },
	{ MODKEY,                       XK_Tab,    view,           {0} },
	{ MODKEY,                       XK_t,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                       XK_f,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       XK_m,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY|ControlMask,           XK_space,  setlayout,      {0} },
	{ MODKEY,                       XK_space,  togglefloating, {0} },
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_f,      togglefullscr,  {0} },
	{ MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
	{ MODKEY,                       XK_z,      setgaps,        {.i = -2 } },
	{ MODKEY,                       XK_u,      setgaps,        {.i = +2 } },
	{ MODKEY|ShiftMask,             XK_u,      setgaps,        {.i = 0  } },
	{ MODKEY|ControlMask|ShiftMask, XK_t,      rotatelayoutaxis, {.i = 0} },    /* 0 = layout axis */
	{ MODKEY|ShiftMask,             XK_t,      rotatelayoutaxis, {.i = 1} },    /* 1 = master axis */
	{ MODKEY|ControlMask,           XK_t,      rotatelayoutaxis, {.i = 2} },    /* 2 = stack axis */
	{ MODKEY|ControlMask,           XK_m,      mirrorlayout,     {0} },
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	{ MODKEY|ControlMask|ShiftMask, XK_q,      spawn,          SHCMD("killall xinit") },
//	{ MODKEY,                       XK_q,      spawn,          SHCMD("sleep 0.1 && xdotool key --clearmodifiers ctrl+c && tmux kill-window && xsetroot -name 'fsignal:3'") },
	{ MODKEY|ShiftMask,             XK_q,      killclient,     {0} },
	{ MODKEY|ControlMask|ShiftMask, XK_r,      quit,           {0} },
	{ MODKEY|ShiftMask,             XK_r,      xrdb,           {.v = NULL } },
    { MODKEY|ALT,                   XK_k,      switchtag,      { .ui = SWITCHTAG_UP     | SWITCHTAG_VIEW } },
    { MODKEY|ALT,                   XK_j,      switchtag,      { .ui = SWITCHTAG_DOWN   | SWITCHTAG_VIEW } },
    { MODKEY|ALT,                   XK_l,      switchtag,      { .ui = SWITCHTAG_RIGHT  | SWITCHTAG_VIEW } },
    { MODKEY|ALT,                   XK_h,      switchtag,      { .ui = SWITCHTAG_LEFT   | SWITCHTAG_VIEW } },
    { MODKEY|ShiftMask|ALT,         XK_k,      switchtag,      { .ui = SWITCHTAG_UP     | SWITCHTAG_TAG | SWITCHTAG_VIEW } },
    { MODKEY|ShiftMask|ALT,         XK_j,      switchtag,      { .ui = SWITCHTAG_DOWN   | SWITCHTAG_TAG | SWITCHTAG_VIEW } },
    { MODKEY|ShiftMask|ALT,         XK_l,      switchtag,      { .ui = SWITCHTAG_RIGHT  | SWITCHTAG_TAG | SWITCHTAG_VIEW } },
    { MODKEY|ShiftMask|ALT,         XK_h,      switchtag,      { .ui = SWITCHTAG_LEFT   | SWITCHTAG_TAG | SWITCHTAG_VIEW } },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
	{ ClkStatusText,        0,              Button1,        sigdwmblocks,   {.i = 1 } },
	{ ClkStatusText,        0,              Button2,        sigdwmblocks,   {.i = 2 } },
	{ ClkStatusText,        0,              Button3,        sigdwmblocks,   {.i = 3 } },
	{ ClkStatusText,        0,              Button4,        sigdwmblocks,   {.i = 4 } },
	{ ClkStatusText,        0,              Button5,        sigdwmblocks,   {.i = 5 } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
	{ ClkRootWin,           0,              Button1,        dragfact,       {0} },
	{ ClkRootWin,           0,              Button3,        spawn,          SHCMD("xmenu.sh") },
};

/* signal definitions */
/* signum must be greater than 0 */
/* trigger signals using `xsetroot -name "fsignal:<signum>"` */
static Signal signals[] = {
	/* signum       function        argument  */
	{ 1,            xrdb,           {.v = NULL } },
	{ 2,            demoncleaner,   {0} },
	{ 3,            killclient,     {0} },
	/* dwmblocks click */
	{ 4,            togglescratch,  {.v = alsam } },
	{ 5,            togglescratch,  {.v = pulsm } },
};
