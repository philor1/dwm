/* See LICENSE file for copyright and license details. */

#define BARTABGROUPS_BORDERS 1         // 0 = disable, 1 = enable
#define BARTABGROUPS_BOTTOMBORDER 0    // 0 = disable, 1 = enable
#define BARTABGROUPS_FUZZPX 12         // # pixels cutoff between bartab groups to merge (e.g. max gaps px)
#define BARTABGROUPS_FLOATINDICATOR 1  // 0 = disable, 1 = enable
#define BARTABGROUPS_FLOATPX 5         // # pixels for float box indicator
#define BARTABGROUPS_TAGSINDICATOR 1   // 0 = disable, 1 = enable when >1 client or view tag, 2 = enable always
#define BARTABGROUPS_TAGSPX 5          // # pixels for tag grid boxes
#define BARTABGROUPS_TAGSROWS 2        // # rows in tag grid
#define BARTABGROUPS_INDICATORSPADPX 2 // # pixels from l/r to pad float/tags indicators

/* appearance */
static const unsigned int gappx     = 4;        /* gaps between windows */
static const unsigned int borderpx  = 2;        /* border pixel of windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const int swallowfloating    = 0;        /* 1 means swallow floating windows by default */
static const int showbar            = 1;        /* 0 means no bar */
static const int showebar           = 1;        /* 0 means no extra bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const unsigned int systraypinning = 0;   /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayspacing = 0;   /* systray spacing */
static const int systraypinningfailfirst = 1;   /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
static const int showsystray             = 1;   /* 0 means no systray */
static const char *fonts[]          = { "monospace:size=10" };
static const char dmenufont[]       = "monospace:size=10";
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan[]        = "#005577";
static const unsigned int baralpha = 0xd0;
static const unsigned int borderalpha = OPAQUE;
static const char *colors[][3]      = {
	/*               fg         bg         border   */
	[SchemeNorm] = { col_gray3, col_gray1, col_gray2 },
	[SchemeSel]  = { col_gray4, col_cyan,  col_cyan  },
	[SchemeTabActiveGroup]  =   { col_gray2, col_gray3,  col_gray2 },
	[SchemeTabInactive]     =   { col_gray1, col_gray3,  col_gray1 }
};
static const unsigned int alphas[][3]      = {
	/*               fg      bg        border     */
	[SchemeNorm] = { OPAQUE, baralpha, borderalpha },
	[SchemeSel]  = { OPAQUE, baralpha, borderalpha },
	[SchemeTabActiveGroup]  =   { OPAQUE, baralpha, borderalpha },
	[SchemeTabInactive]     =   { OPAQUE, baralpha, borderalpha }
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

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

static const unsigned int drawtagmask = DRAWTAGGRID; /* | DRAWCLASSICTAGS to show classic row of tags */
static const int tagrows = 2;

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class     instance  title           tags mask  switchtotag  isfloating  isterminal  noswallow  monitor */
	{ "Gimp",    NULL,     NULL,           0,         0,           1,          0,           0,        -1 },
	{ "Firefox", NULL,     NULL,           1 << 8,    0,           0,          0,          -1,        -1 },
	{ "st",      NULL,     NULL,           0,         0,           0,          1,          -1,        -1 },
	{ NULL,      NULL,     "Event Tester", 0,         0,           1,          0,           1,        -1 }, /* xev */
};

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 0;    /* 1 means respect size hints in tiled resizals */
static const int attachdirection = 0;    /* 0 default, 1 above, 2 aside, 3 below, 4 bottom, 5 top */
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
#define MODKEY Mod1Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf", col_gray3, "-sb", col_cyan, "-sf", col_gray4, NULL };
static const char *termcmd[]  = { "st", NULL };
static const char *browser[] = { "luakit", NULL, NULL, NULL, "Luakit" };

/* commands spawned when clicking statusbar, the mouse button pressed is exported as BUTTON */
static char *statuscmds[][5] = {
/* 	button1 					button2 					button3 					button4 					button5*/

/* VOL */ {
	"st -e alsamixer", 			"pavucontrol", 				"amixer set Master toggle; pkill -RTMIN+11 dwmblocks", "/usr/bin/amixer -q sset Master 5%+; pkill -RTMIN+11 dwmblocks", "/usr/bin/amixer -q sset Master 5%-; pkill -RTMIN+11 dwmblocks" },
/* BKL */

/* TIME */

/* CPU */

/* RAM */

/* WIFI */

/* BAT */

/* END */ {
	NULL, 						NULL, 						NULL, 						NULL, 						NULL }
};
static char *statuscmd[] = { "/bin/sh", "-c", NULL, NULL };

static Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY|ControlMask,           XK_w,      runorraise,     {.v = browser } },
	{ MODKEY,                       XK_p,      spawn,          {.v = dmenucmd } },
	{ MODKEY|ShiftMask,             XK_Return, spawn,          {.v = termcmd } },
	{ MODKEY,                       XK_b,      togglebars,     {0} },
	{ MODKEY|ControlMask,           XK_b,      togglebar,      {0} },
	{ MODKEY|ControlMask|ShiftMask, XK_b,      toggleebar,     {0} },
	{ MODKEY,                       XK_n,      switchcol,      {0} },
	{ MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_k,      focusstack,     {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_j,      inplacerotate,  {.i = +1} },
	{ MODKEY|ShiftMask,             XK_k,      inplacerotate,  {.i = -1} },
	{ MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },
	{ MODKEY,                       XK_d,      incnmaster,     {.i = -1 } },
	{ MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },
	{ MODKEY,                       XK_l,      setmfact,       {.f = +0.05} },
	{ MODKEY|ShiftMask,             XK_h,      setcfact,       {.f = +0.25} },
	{ MODKEY|ShiftMask,             XK_l,      setcfact,       {.f = -0.25} },
	{ MODKEY|ShiftMask,             XK_o,      setcfact,       {.f =  0.00} },
	{ MODKEY|ControlMask,           XK_j,      pushdown,       {0} },
	{ MODKEY|ControlMask,           XK_k,      pushup,         {0} },
	{ MODKEY,                       XK_x,      transfer,       {0} },
	{ MODKEY,                       XK_Return, zoom,           {0} },
	{ MODKEY,                       XK_Tab,    view,           {0} },
	{ MODKEY|ShiftMask,             XK_c,      killclient,     {0} },
	{ MODKEY,                       XK_t,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                       XK_f,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       XK_m,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY,                       XK_space,  setlayout,      {0} },
	{ MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
	{ MODKEY|ShiftMask,             XK_f,      togglefullscr,  {0} },
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
	{ MODKEY,                       XK_z,      setgaps,        {.i = -2 } },
	{ MODKEY,                       XK_u,      setgaps,        {.i = +2 } },
	{ MODKEY|ShiftMask,             XK_u,      setgaps,        {.i = 0  } },
	{ MODKEY|ShiftMask,             XK_t,      rotatelayoutaxis, {.i = 0} },    /* 0 = layout axis */
	{ MODKEY|ControlMask,           XK_t,      rotatelayoutaxis, {.i = 1} },    /* 1 = master axis */
	{ MODKEY|ControlMask|ShiftMask, XK_t,      rotatelayoutaxis, {.i = 2} },    /* 2 = stack axis */
	{ MODKEY|ControlMask,           XK_Return, mirrorlayout,     {0} },
	{ MODKEY,			XK_Up,	   moveresize,	   {.v = "0x -25y 0w 0h"} },
	{ MODKEY,			XK_Down,   moveresize,	   {.v = "0x 25y 0w 0h"} },
	{ MODKEY,			XK_Left,   moveresize,	   {.v = "-25x 0y 0w 0h"} },
	{ MODKEY,			XK_Right,  moveresize,	   {.v = "25x 0y 0w 0h"} },
	{ MODKEY|ShiftMask,		XK_Up,	   moveresize,	   {.v = "0x 0y 0w -25h"} },
	{ MODKEY|ShiftMask,		XK_Down,   moveresize,	   {.v = "0x 0y 0w 25h"} },
	{ MODKEY|ShiftMask,		XK_Left,   moveresize,	   {.v = "0x 0y -25w 0h"} },
	{ MODKEY|ShiftMask,		XK_Right,  moveresize,	   {.v = "0x 0y 25w 0h"} },
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)
	{ MODKEY|ShiftMask,             XK_q,      quit,           {0} },

    { MODKEY|ControlMask,           XK_Up,     switchtag,      { .ui = SWITCHTAG_UP     | SWITCHTAG_VIEW } },
    { MODKEY|ControlMask,           XK_Down,   switchtag,      { .ui = SWITCHTAG_DOWN   | SWITCHTAG_VIEW } },
    { MODKEY|ControlMask,           XK_Right,  switchtag,      { .ui = SWITCHTAG_RIGHT  | SWITCHTAG_VIEW } },
    { MODKEY|ControlMask,           XK_Left,   switchtag,      { .ui = SWITCHTAG_LEFT   | SWITCHTAG_VIEW } },

    { MODKEY|Mod4Mask,              XK_Up,     switchtag,      { .ui = SWITCHTAG_UP     | SWITCHTAG_TAG | SWITCHTAG_VIEW } },
    { MODKEY|Mod4Mask,              XK_Down,   switchtag,      { .ui = SWITCHTAG_DOWN   | SWITCHTAG_TAG | SWITCHTAG_VIEW } },
    { MODKEY|Mod4Mask,              XK_Right,  switchtag,      { .ui = SWITCHTAG_RIGHT  | SWITCHTAG_TAG | SWITCHTAG_VIEW } },
    { MODKEY|Mod4Mask,              XK_Left,   switchtag,      { .ui = SWITCHTAG_LEFT   | SWITCHTAG_TAG | SWITCHTAG_VIEW } },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
	{ ClkStatusText,        0,              Button1,        spawn,          {.v = statuscmd } },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = statuscmd } },
	{ ClkStatusText,        0,              Button3,        spawn,          {.v = statuscmd } },
	{ ClkStatusText,        0,              Button4,        spawn,          {.v = statuscmd } },
	{ ClkStatusText,        0,              Button5,        spawn,          {.v = statuscmd } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};

/* signal definitions */
/* signum must be greater than 0 */
/* trigger signals using `xsetroot -name "fsignal:<signum>"` */
static Signal signals[] = {
	/* signum       function        argument  */
	{ 1,            setlayout,      {.v = 0} },
};
