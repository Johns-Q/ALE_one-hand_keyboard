//

/**@name aohk.c	-	ALE OneHand Keyboard handler.	*/
//
//      Copyright (c) 2007 by Lutz Sammer.  All Rights Reserved.
//
//      Contributor(s):
//
//      This file is part of ALE one-hand keyboard
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; only version 2 of the License.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      $Id$
////////////////////////////////////////////////////////////////////////////

#include <linux/input.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>

#include "aohk.h"

#define DEFAULT

//
//      States
//      Bug alert: SecondKeys must be FirstKey+1!
//
enum
{
    OHFirstKey,				// waiting for first key
    OHSecondKey,			// waiting for second key
    OHQuoteFirstKey,			// have quote key, waiting for first key
    OHQuoteSecondKey,			// have quote state, waiting for second key
    OHSuperFirstKey,			// have super quote key, waiting for first key
    OHSuperSecondKey,			// have super quoted state, waiting for second key
    OHMacroFirstKey,			// have macro key, ...
    OHMacroSecondKey,			// macro key, first key ...
    OHMacroQuoteFirstKey,		// have macro key, ...
    OHMacroQuoteSecondKey,		// macro key, first key ...

    OHGameMode,				// gamemode: key = key
    OHSpecial,				// have seen the special key
    OHSoftOff,				// turned off
    OHHardOff				// 100% turned off
};

typedef struct _oh_key_ OHKey;

struct _oh_key_
{
    unsigned char Modifier;		// modifier flags
    unsigned char KeyCode;		// scancode
};

static char AOHKState;			// state machine

static char AOHKOnlyMe;			// enable only my keys

static int AOHKDownKeys;		// bitmap pressed keys
static unsigned char AOHKLastKey;	// last scancode got

//
//      Sequences
//
static unsigned char AOHKRelease;	// release must be send

static unsigned char AOHKStickyModifier;	// sticky modifiers
static unsigned char AOHKModifier;	// modifiers wanted

static unsigned char AOHKLastModifier;	// last key modifiers
static const OHKey *AOHKLastSequence;	// last keycode

//
//      Table: Maps input keycodes to internal keys.
//
static unsigned char AOHKConvertTable[256];

// ---------------------------------------------------

//
//      Keyboard mapping modifier
//
#define SHIFT	1			// keycode + SHIFT
#define CTL	2			// keycode + CTRL
#define ALT	4			// keycode + ALT
#define ALTGR	8			// keycode + ALT-GR

// remaining are available

#define QUAL	128			// no keycode only qualifiers
#define STICKY	129			// sticky qualifiers
#define QUOTE	130			// quote state
#define RESET	131			// reset state
#define TOGAME	132			// enter game mode
#define SPECIAL	133			// enter special mode

//
//      Modifier bitmap
//
#define Q_SHFT_L	1		// shift left
#define Q_CTRL_L	2		// control left
#define Q_ALT_L		4		// alt left
#define Q_GUI_L		8		// gui (flag) left
#define Q_SHFT_R	16		// shift right
#define Q_CTRL_R	32		// control right
#define Q_ALT_R		64		// alt right
#define Q_GUI_R		128		// gui (flag) right

static char AOHKGameSendQuote;		// send delayed quote in game mode

static unsigned AOHKGamePressed;	// keys pressed in game mode
static unsigned AOHKGameQuotePressed;	// quoted keys pressed in game mode

#define OH_TIMEOUT	(1*1000)	// default 1s
static int AOHKTimeout = OH_TIMEOUT;	// timeout in jiffies

static unsigned long AOHKLastJiffies;	// last key jiffy

//
//      LED Macros for more hardware support
//
//#define SecondStateLedOn()            // led not used, only troubles
//#define SecondStateLedOff()           // led not used, only troubles
//#define QuoteStateLedOn()             // led not used, only troubles
//#define QuoteStateLedOff()            // led not used, only troubles
//#define GameModeLedOn()               // led not used, only troubles
//#define GameModeLedOff()              // led not used, only troubles
#define SpecialStateLedOn()		// led not used, only troubles
#define SpecialStateLedOff()		// led not used, only troubles

//	*INDENT-OFF*

/*
**	Table sequences to scancodes.
**		The first value are the flags and modifiers.
**		The second value is the scancode to send.
**		Comments are the ascii output of the keyboard driver.
*/
static OHKey AOHKTable[10*10+1+8] = {
#ifdef DEFAULT
/*1->0*/ { RESET,	0	},	// RESET
/*1->1*/ { 0,		KEY_1	},	// 1
/*1->2*/ { 0,		0x17	},	// i
/*1->3*/ { 0,		0x11	},	// w
/*1->4*/ { ALTGR,	0x1B	},	// ~
/*1->5*/ { 0,		0x13	},	// r
/*1->6*/ { 0,		0x2F	},	// v
/*1->7*/ { 0,		0x28	},	// ä
/*1->8*/ { 0,		0x20	},	// d
/*1->9*/ { 0,		0x10	},	// q
/*2->0*/ { RESET,	0	},	// RESET
/*2->1*/ { 0,		0x1C	},	// Return
/*2->2*/ { 0,		0x03	},	// 2
/*2->3*/ { 0,		0x34	},	// .
/*2->4*/ { QUAL,	Q_CTRL_L },	// Control_L
/*2->5*/ { 0,		KEY_GRAVE	},	// ^
/*2->6*/ { 0,		0x35	},	// -
/*2->7*/ { 0,		0x33	},	// ,
/*2->8*/ { 0,		0x27	},	// ö
/*2->9*/ { SHIFT,	KEY_1	},	// !
/*3->0*/ { RESET,	0	},	// RESET
/*3->1*/ { 0,		0x10	},	// q
/*3->2*/ { 0,		0x26	},	// l
/*3->3*/ { 0,		0x04	},	// 3
/*3->4*/ { SHIFT,	0x03	},	// "
/*3->5*/ { 0,		0x32	},	// m
/*3->6*/ { 0,		0x56	},	// <
/*3->7*/ { 0,		0x0D	},	// '
/*3->8*/ { 0,		0x21	},	// f
/*3->9*/ { 0,		0x1A	},	// ü
/*4->0*/ { RESET,	0	},	// RESET
/*4->1*/ { SHIFT,	0x05	},	// $
/*4->2*/ { 0,		0x1E	},	// a
/*4->3*/ { 0,		KEY_Z	},	// y
/*4->4*/ { 0,		0x05	},	// 4
/*4->5*/ { 0,		0x12	},	// e
/*4->6*/ { 0,		0x25	},	// k
/*4->7*/ { SHIFT,	0x06	},	// %
/*4->8*/ { 0,		0x1F	},	// s
/*4->9*/ { 0,		0x2D	},	// x
/*5->0*/ { RESET,	0	},	// RESET
/*5->1*/ { QUAL,	Q_SHFT_L },	// Shift_L
/*5->2*/ { ALTGR,	0x0C	},	// \ FUCK GNU (gcc >2.95.4 broken)
/*5->3*/ { SHIFT,	0x09	},	// (
/*5->4*/ { 0,		KEY_SPACE	},	// Space
/*5->5*/ { 0,		0x06	},	// 5
/*5->6*/ { 0,		0x0F	},	// Tabulator
/*5->7*/ { QUAL,	Q_GUI_L },	// Gui_L
/*5->8*/ { ALTGR,	0x09	},	// [
/*5->9*/ { SHIFT,	0x0A	},	// )
/*6->0*/ { RESET,	0	},	// RESET
/*6->1*/ { SHIFT,	0x07	},	// &
/*6->2*/ { 0,		0x30	},	// b
/*6->3*/ { SHIFT,	0x56	},	// >
/*6->4*/ { SHIFT,	0x1B	},	// *
/*6->5*/ { 0,		0x18	},	// o
/*6->6*/ { 0,		0x07	},	// 6
/*6->7*/ { 0,		0x1B	},	// +
/*6->8*/ { 0,		0x16	},	// u
/*6->9*/ { ALTGR,	0x08	},	// {
/*7->0*/ { RESET,	0	},	// RESET
/*7->1*/ { 0,		0x0C	},	// ß
/*7->2*/ { 0,		0x23	},	// h
/*7->3*/ { SHIFT,	0x0B	},	// =
/*7->4*/ { SHIFT,	0x0C	},	// ?
/*7->5*/ { 0,		0x14	},	// t
/*7->6*/ { 0,		0x24	},	// j
/*7->7*/ { 0,		0x08	},	// 7
/*7->8*/ { 0,		KEY_N	},	// n
/*7->9*/ { 0,		KEY_Y	},	// z
/*8->0*/ { RESET,	0	},	// RESET
/*8->1*/ { SHIFT,	0x33	},	// ;
/*8->2*/ { ALTGR,	0x10	},	// @
/*8->3*/ { SHIFT,	0x34	},	// :
/*8->4*/ { QUAL,	Q_ALT_L	},	// Alt_L
/*8->5*/ { ALTGR,	0x0A	},	// ]
/*8->6*/ { ALTGR,	0x56	},	// |
/*8->7*/ { 0,		0x0E	},	// BackSpace
/*8->8*/ { 0,		0x09	},	// 8
/*8->9*/ { 0,		0x01	},	// Escape
/*9->0*/ { RESET,	0	},	// RESET
/*9->1*/ { 0,		0x2B	},	// #
/*9->2*/ { 0,		0x19	},	// p
/*9->3*/ { SHIFT,	0x0D	},	// `
/*9->4*/ { SHIFT,	0x35	},	// _
/*9->5*/ { 0,		0x22	},	// g
/*9->6*/ { ALTGR,	0x0B	},	// }
/*9->7*/ { SHIFT,	0x08	},	// /
/*9->8*/ { 0,		0x2E	},	// c
/*9->9*/ { 0,		0x0A	},	// 9

/*0->#*/ { 0,		0x6E	},	// INSERT
/*1->#*/ { 0,		0x6B	},	// END
/*2->#*/ { 0,		0x6C	},	// CURSOR-DOWN
/*3->#*/ { 0,		0x6D	},	// PAGE-DOWN
/*4->#*/ { 0,		0x69	},	// CURSOR-LEFT
/*5->#*/ { 0,		0x6F	},	// DELETE ( ,->5 )
/*6->#*/ { 0,		0x6A	},	// CURSOR-RIGHT
/*7->#*/ { 0,		0x66	},	// HOME
/*8->#*/ { 0,		0x67	},	// CURSOR-UP
/*9->#*/ { 0,		0x68	},	// PAGE-UP

/*0->0*/ { 0,		0x0B	},	// 0

/*USR1*/ { QUAL,	Q_SHFT_L },	// Shift_L
/*USR2*/ { QUAL,	Q_CTRL_L },	// Ctrl_L
/*USR3*/ { QUAL,	Q_ALT_L },	// Alt_L
/*USR4*/ { QUAL,	Q_GUI_L },	// Gui_L
/*USR5*/ { QUAL,	Q_SHFT_R },	// Shift_R
/*USR6*/ { QUAL,	Q_CTRL_R },	// Ctrl_R
/*USR7*/ { QUAL,	Q_ALT_R },	// Alt_R
/*USR8*/ { QUAL,	Q_GUI_R },	// Gui_R

// FIXME: Should I move game mode table to here?
#endif
};

/*
**	Table quoted sequences to scancodes.
**		The first value are the flags and modifiers.
**		The second value is the scancode to send.
*/
static OHKey AOHKQuoteTable[10*10+1+8] = {
#ifdef DEFAULT
/*0->1->0*/ { RESET,	0	},	// RESET
/*0->1->1*/ { 0,	0x3B	},	// F1
/*0->1->2*/ { SHIFT,	0x17	},	// I
/*0->1->3*/ { SHIFT,	0x11	},	// W
/*0->1->4*/ { 0,	KEY_NUMLOCK	},	// NUM-LOCK
/*0->1->5*/ { SHIFT,	0x13	},	// R
/*0->1->6*/ { SHIFT,	0x2F	},	// V
/*0->1->7*/ { SHIFT,	0x28	},	// Ä
/*0->1->8*/ { SHIFT,	0x20	},	// D
/*0->1->9*/ { SHIFT,	0x10	},	// Q
/*0->2->0*/ { RESET,	0	},	// RESET
/*0->2->1*/ { 0,	0x60	},	// KP-ENTER
/*0->2->2*/ { 0,	0x3C	},	// F2
/*0->2->3*/ { 0,	0x53	},	// KP-.
/*0->2->4*/ { QUAL,	Q_CTRL_R },	// Control_R
/*0->2->5*/ { 0,	KEY_CAPSLOCK },	// CAPS-LOCK
/*0->2->6*/ { 0,	0x4A	},	// KP--
/*0->2->7*/ { 0,	0x53	},	// KP-,
/*0->2->8*/ { SHIFT,	0x27	},	// Ö
/*0->2->9*/ { 0,	0x52	},	// KP-0 (!not better place found)
/*0->3->0*/ { RESET,	0	},	// RESET
/*0->3->1*/ { SHIFT,	0x10	},	// Q
/*0->3->2*/ { SHIFT,	0x26	},	// L
/*0->3->3*/ { 0,	0x3D	},	// F3
/*0->3->4*/ { SHIFT,	0x04	},	// §
/*0->3->5*/ { SHIFT,	0x32	},	// M
/*0->3->6*/ { 0,	0x46	},	// SCROLL-LOCK
/*0->3->7*/ { 0,	0x2B	},	// ' (SHIFT #, doubled)
/*0->3->8*/ { SHIFT,	0x21	},	// F
/*0->3->9*/ { SHIFT,	0x1A	},	// Ü
/*0->4->0*/ { RESET,	0	},	// RESET
/*0->4->1*/ { SHIFT,	KEY_GRAVE	},	// °
/*0->4->2*/ { SHIFT,	0x1E	},	// A
/*0->4->3*/ { SHIFT,	KEY_Z	},	// Y
/*0->4->4*/ { 0,	0x3E	},	// F4
/*0->4->5*/ { SHIFT,	0x12	},	// E
/*0->4->6*/ { SHIFT,	0x25	},	// K
/*0->4->7*/ { 0,	0x62	},	// KP-% (doubled)
/*0->4->8*/ { SHIFT,	0x1F	},	// S
/*0->4->9*/ { SHIFT,	0x2D	},	// X
/*0->5->0*/ { RESET,	0	},	// RESET
/*0->5->1*/ { QUAL,	Q_SHFT_R },	// Shift_R
/*0->5->2*/ { ALTGR,	0x56	},	// | (QUOTE \, doubled)
/*0->5->3*/ { SHIFT,	0x09	},	// ( (FREE)
/*0->5->4*/ { SHIFT,	KEY_SPACE	},	// SHIFT Space
/*0->5->5*/ { 0,	0x3F	},	// F5
/*0->5->6*/ { SHIFT,	0x0F	},	// SHIFT Tabulator
/*0->5->7*/ { QUAL,	Q_GUI_R },	// Gui_R
/*0->5->8*/ { ALTGR,	0x07	},	// { (QUOTE [, doubled)
/*0->5->9*/ { SHIFT,	0x0A	},	// ) (FREE)
/*0->6->0*/ { RESET,	0	},	// RESET
/*0->6->1*/ { ALTGR,	0x12	},	// ¤
/*0->6->2*/ { SHIFT,	0x30	},	// B
/*0->6->3*/ { ALTGR,	0x2E	},	// ¢
/*0->6->4*/ { 0,	0x37	},	// KP-*
/*0->6->5*/ { SHIFT,	0x18	},	// O
/*0->6->6*/ { 0,	0x40	},	// F6
/*0->6->7*/ { 0,	0x4E	},	// KP-+
/*0->6->8*/ { SHIFT,	0x16	},	// U
/*0->6->9*/ { ALTGR,	0x08	},	// { (FREE)
/*0->7->0*/ { RESET,	0	},	// RESET
/*0->7->1*/ { 0,	0x63	},	// PRINT/SYSRQ
/*0->7->2*/ { SHIFT,	0x23	},	// H
/*0->7->3*/ { 0,	0x44	},	// F10
/*0->7->4*/ { 0,	0x57	},	// F11
/*0->7->5*/ { SHIFT,	0x14	},	// T
/*0->7->6*/ { SHIFT,	0x24	},	// J
/*0->7->7*/ { 0,	0x41	},	// F7
/*0->7->8*/ { SHIFT,	KEY_N	},	// N
/*0->7->9*/ { SHIFT,	KEY_Y	},	// Z
/*0->8->0*/ { RESET,	0	},	// RESET
/*0->8->1*/ { 0,	0x77	},	// PAUSE
/*0->8->2*/ { SHIFT,	0x0D	},	// ` (QUOTE @, doubled)
/*0->8->3*/ { 0,	0x65	},	// BREAK
/*0->8->4*/ { QUAL,	Q_ALT_R	},	// Alt_R
/*0->8->5*/ { ALTGR,	0x0B	},	// } (QUOTE ], doubled)
/*0->8->6*/ { 0,	0x54	},	// SYSRQ
/*0->8->7*/ { SHIFT,	0x0E	},	// SHIFT BackSpace
/*0->8->8*/ { 0,	0x42	},	// F8
/*0->8->9*/ { SHIFT,	0x01	},	// SHIFT Escape
/*0->9->0*/ { RESET,	0	},	// RESET
/*0->9->1*/ { 0,	0x7F	},	// MENU
/*0->9->2*/ { SHIFT,	0x19	},	// P
/*0->9->3*/ { 0,	0x58	},	// F12
/*0->9->4*/ { 0,	0x0E	},	// DEL 0x7F (QUOTE _, doubled)
/*0->9->5*/ { SHIFT,	0x22	},	// G
/*0->9->6*/ { SPECIAL,	0	},	// } (FREE)
/*0->9->7*/ { 0,	0x62	},	// KP-/
/*0->9->8*/ { SHIFT,	0x2E	},	// C
/*0->9->9*/ { 0,	0x43	},	// F9

/*0->0->#*/ { 0,	0	},	// Not possible! 00 is 0
/*0->1->#*/ { 0,	0x4F	},	// KP-1
/*0->2->#*/ { 0,	0x50	},	// KP-2
/*0->3->#*/ { 0,	0x51	},	// KP-3
/*0->4->#*/ { 0,	0x4B	},	// KP-4
/*0->5->#*/ { 0,	0x53	},	// KP-5
/*0->6->#*/ { 0,	0x4D	},	// KP-6
/*0->7->#*/ { 0,	0x47	},	// KP-7
/*0->8->#*/ { 0,	0x48	},	// KP-8
/*0->9->#*/ { 0,	0x49	},	// KP-9

/*0->0->0*/ { 0,	0	},	// Not possible! 00 is 0

/*0->USR1*/ { QUAL,	Q_SHFT_R },	// Shift_R
/*0->USR2*/ { QUAL,	Q_CTRL_R },	// Ctrl_R
/*0->USR3*/ { QUAL,	Q_ALT_R },	// Alt_R
/*0->USR4*/ { QUAL,	Q_GUI_R },	// Gui_R
/*0->USR5*/ { QUAL,	Q_SHFT_L },	// Shift_L
/*0->USR6*/ { QUAL,	Q_CTRL_L },	// Ctrl_L
/*0->USR7*/ { QUAL,	Q_ALT_L },	// Alt_L
/*0->USR8*/ { QUAL,	Q_GUI_L },	// Gui_L
#endif
};

/*
**	Table super quoted sequences to scancodes.
**	If we need more codes, they can be added here.
**		The first value are the flags and modifiers.
**		The second value is the scancode to send.
**	FIXME:	ALT? Keycode?
*/
static OHKey AOHKSuperTable[10*10+1+8] = {
#ifdef DEFAULT
/*1->0*/ { RESET,	0	},	// RESET
/*1->1*/ { ALT,		KEY_1	},	// 1
/*1->2*/ { ALT,		0x17	},	// i
/*1->3*/ { ALT,		0x11	},	// w
/*1->4*/ { ALTGR,	0x1B	},	// ~
/*1->5*/ { ALT,		0x13	},	// r
/*1->6*/ { ALT,		0x2F	},	// v
/*1->7*/ { ALT,		0x28	},	// ä
/*1->8*/ { ALT,		0x20	},	// d
/*1->9*/ { ALT,		0x10	},	// q
/*2->0*/ { RESET,	0	},	// RESET
/*2->1*/ { ALT,		0x1C	},	// Return
/*2->2*/ { ALT,		0x03	},	// 2
/*2->3*/ { ALT,		0x34	},	// .
/*2->4*/ { QUAL,	Q_CTRL_L },	// Control_L
/*2->5*/ { ALT,		KEY_GRAVE	},	// ^
/*2->6*/ { ALT,		0x35	},	// -
/*2->7*/ { ALT,		0x33	},	// ,
/*2->8*/ { ALT,		0x27	},	// ö
/*2->9*/ { ALT|SHIFT,	KEY_1	},	// !
/*3->0*/ { RESET,	0	},	// RESET
/*3->1*/ { ALT,		0x10	},	// q
/*3->2*/ { ALT,		0x26	},	// l
/*3->3*/ { ALT,		0x04	},	// 3
/*3->4*/ { ALT|SHIFT,	0x03	},	// "
/*3->5*/ { ALT,		0x32	},	// m
/*3->6*/ { ALT,		0x56	},	// <
/*3->7*/ { ALT,		0x0D	},	// '
/*3->8*/ { ALT,		0x21	},	// f
/*3->9*/ { ALT,		0x1A	},	// ü
/*4->0*/ { RESET,	0	},	// RESET
/*4->1*/ { ALT|SHIFT,	0x05	},	// $
/*4->2*/ { ALT,		0x1E	},	// a
/*4->3*/ { ALT,		KEY_Z	},	// y
/*4->4*/ { ALT,		0x05	},	// 4
/*4->5*/ { ALT,		0x12	},	// e
/*4->6*/ { ALT,		0x25	},	// k
/*4->7*/ { ALT|SHIFT,	0x06	},	// %
/*4->8*/ { ALT,		0x1F	},	// s
/*4->9*/ { ALT,		0x2D	},	// x
/*5->0*/ { RESET,	0	},	// RESET
/*5->1*/ { QUAL,	Q_SHFT_L },	// Shift_L
/*5->2*/ { ALTGR,	0x0C	},	// \ FUCK GNU (gcc >2.95.4 broken)
/*5->3*/ { ALT|SHIFT,	0x09	},	// (
/*5->4*/ { ALT,		KEY_SPACE	},	// Space
/*5->5*/ { ALT,		0x06	},	// 5
/*5->6*/ { ALT,		0x0F	},	// Tabulator
/*5->7*/ { QUAL,	Q_GUI_L },	// Gui_L
/*5->8*/ { ALTGR,	0x09	},	// [
/*5->9*/ { ALT|SHIFT,	0x0A	},	// )
/*6->0*/ { RESET,	0	},	// RESET
/*6->1*/ { ALT|SHIFT,	0x07	},	// &
/*6->2*/ { ALT,		0x30	},	// b
/*6->3*/ { ALT|SHIFT,	0x56	},	// >
/*6->4*/ { ALT|SHIFT,	0x1B	},	// *
/*6->5*/ { ALT,		0x18	},	// o
/*6->6*/ { ALT,		0x07	},	// 6
/*6->7*/ { ALT,		0x1B	},	// +
/*6->8*/ { ALT,		0x16	},	// u
/*6->9*/ { ALTGR,	0x08	},	// {
/*7->0*/ { RESET,	0	},	// RESET
/*7->1*/ { ALT,		0x0C	},	// ß
/*7->2*/ { ALT,		0x23	},	// h
/*7->3*/ { ALT|SHIFT,	0x0B	},	// =
/*7->4*/ { ALT|SHIFT,	0x0C	},	// ?
/*7->5*/ { ALT,		0x14	},	// t
/*7->6*/ { ALT,		0x24	},	// j
/*7->7*/ { ALT,		0x08	},	// 7
/*7->8*/ { ALT,		KEY_N	},	// n
/*7->9*/ { ALT,		KEY_Y	},	// z
/*8->0*/ { RESET,	0	},	// RESET
/*8->1*/ { ALT|SHIFT,	0x33	},	// ;
/*8->2*/ { ALTGR,	0x10	},	// @
/*8->3*/ { ALT|SHIFT,	0x34	},	// :
/*8->4*/ { QUAL,	Q_ALT_L	},	// Alt_L
/*8->5*/ { ALTGR,	0x0A	},	// ]
/*8->6*/ { ALTGR,	0x56	},	// |
/*8->7*/ { ALT,		0x0E	},	// BackSpace
/*8->8*/ { ALT,		0x09	},	// 8
/*8->9*/ { ALT,		0x01	},	// Escape
/*9->0*/ { RESET,	0	},	// RESET
/*9->1*/ { ALT,		0x2B	},	// #
/*9->2*/ { ALT,		0x19	},	// p
/*9->3*/ { ALT|SHIFT,	0x0D	},	// `
/*9->4*/ { ALT|SHIFT,	0x35	},	// _
/*9->5*/ { ALT,		0x22	},	// g
/*9->6*/ { ALTGR,	0x0B	},	// }
/*9->7*/ { ALT|SHIFT,	0x08	},	// /
/*9->8*/ { ALT,		0x2E	},	// c
/*9->9*/ { ALT,		0x0A	},	// 9

/*0->#*/ { ALT,		0x6E	},	// INSERT
/*1->#*/ { ALT,		0x6B	},	// END
/*2->#*/ { ALT,		0x6C	},	// CURSOR-DOWN
/*3->#*/ { ALT,		0x6D	},	// PAGE-DOWN
/*4->#*/ { ALT,		0x69	},	// CURSOR-LEFT
/*5->#*/ { ALT,		0x6F	},	// DELETE ( ,->5 )
/*6->#*/ { ALT,		0x6A	},	// CURSOR-RIGHT
/*7->#*/ { ALT,		0x66	},	// HOME
/*8->#*/ { ALT,		0x67	},	// CURSOR-UP
/*9->#*/ { ALT,		0x68	},	// PAGE-UP

/*0->0*/ { ALT,		0x0B	},	// 0

/*USR1*/ { QUAL,	Q_SHFT_L },	// Shift_L
/*USR2*/ { QUAL,	Q_CTRL_L },	// Ctrl_L
/*USR3*/ { QUAL,	Q_ALT_L },	// Alt_L
/*USR4*/ { QUAL,	Q_GUI_L },	// Gui_L
/*USR5*/ { QUAL,	Q_SHFT_R },	// Shift_R
/*USR6*/ { QUAL,	Q_CTRL_R },	// Ctrl_R
/*USR7*/ { QUAL,	Q_ALT_R },	// Alt_R
/*USR8*/ { QUAL,	Q_GUI_R },	// Gui_R
#endif
};

/*
**	Game Table sequences to scancodes.
**		The first value are the flags and modifiers.
**		The second value is the scancode to send.
*/
static OHKey AOHKGameTable[OH_SPECIAL+1] = {
#ifdef DEFAULT
// This can't be used, quote is used insteed.
/* 0 */ { QUOTE,	KEY_SPACE	},	// SPACE
/* 1 */ { 0,		KEY_Z	},	// y
/* 2 */ { 0,		0x2D	},	// x
/* 3 */ { 0,		0x2E	},	// c

/* 4 */ { 0,		0x1E	},	// a
/* 5 */ { 0,		0x1F	},	// s
/* 6 */ { 0,		0x20	},	// d

/* 7 */ { 0,		0x10	},	// q
/* 8 */ { 0,		0x11	},	// w
/* 9 */ { 0,		0x12	},	// e

/* # */ { 0,		0x1D	},	// Ctrl_L
/* . */ { 0,		0x38	},	// Alt_L

/* % */ { 0,		0x03	},	// 2
/* * */ { 0,		0x04	},	// 3
/* - */ { 0,		0x05	},	// 4
/* + */ { 0,		0x13	},	// r

/* ? */ { 0,		0x3B	},	// F1
/* ? */ { 0,		0x3C	},	// F2
/* ? */ { 0,		0x3D	},	// F3
/* ? */ { 0,		0x3E	},	// F4

/*NUM*/ { 0,		KEY_1	},	// 1
#endif
};

/*
**	Quote Game Table sequences to scancodes.
*/
static OHKey AOHKQuoteGameTable[OH_SPECIAL+1] = {
#ifdef DEFAULT
/* 0 */ { 0,		KEY_SPACE	},	// SPACE
/* 1 */ { 0,		0x2F	},	// v
/* 2 */ { 0,		0x30	},	// b
/* 3 */ { 0,		KEY_N	},	// n

/* 4 */ { 0,		0x21	},	// f
/* 5 */ { 0,		0x22	},	// g
/* 6 */ { 0,		0x23	},	// h

/* 7 */ { 0,		0x14	},	// t
/* 8 */ { 0,		KEY_Y	},	// z
/* 9 */ { 0,		0x16	},	// u

/* # */ { RESET,	0	},	// Return to normal mode
/* . */ { 0,		0x0F	},	// TAB

/* % */ { 0,		0x07	},	// 6
/* * */ { 0,		0x08	},	// 7
/* - */ { 0,		0x09	},	// 8
/* + */ { 0,		0x17	},	// i

/* ? */ { 0,		0x3F	},	// F5
/* ? */ { 0,		0x40	},	// F6
/* ? */ { 0,		0x41	},	// F7
/* ? */ { 0,		0x42	},	// F8

/*NUM*/ { 0,		0x06	},	// 5
#endif
};

//
//	Macro Table sequences to scancodes.
//
static unsigned char* AOHKMacroTable[10*10+1+8] = {
#ifdef DEFAULT
// FIXME: Need because of static pointers.
#endif
/**->1->0*/ (unsigned char[]){ RESET,	0	},	// RESET
/**->1->1*/ (unsigned char[]){ CTL,		KEY_1	},	// 1
/**->1->2*/ (unsigned char[]){ CTL,		0x17	},	// i
/**->1->3*/ (unsigned char[]){ CTL,		0x11	},	// w
/**->1->4*/ (unsigned char[]){ CTL|ALTGR,	0x1B	},	// ~
/**->1->5*/ (unsigned char[]){ CTL,		0x13	},	// r
/**->1->6*/ (unsigned char[]){ CTL,		0x2F	},	// v
/**->1->7*/ (unsigned char[]){ CTL,		0x28	},	// ä
/**->1->8*/ (unsigned char[]){ CTL,		0x20	},	// d
/**->1->9*/ (unsigned char[]){ CTL,		0x10	},	// q
/**->2->0*/ (unsigned char[]){ RESET,	0	},	// RESET
/**->2->1*/ (unsigned char[]){ CTL,		0x1C	},	// Return
/**->2->2*/ (unsigned char[]){ CTL,		0x03	},	// 2
/**->2->3*/ (unsigned char[]){ CTL,		0x34	},	// .
/**->2->4*/ (unsigned char[]){ QUAL,	Q_CTRL_L },	// Control_L
/**->2->5*/ (unsigned char[]){ CTL,		KEY_GRAVE	},	// ^
/**->2->6*/ (unsigned char[]){ CTL,		0x35	},	// -
/**->2->7*/ (unsigned char[]){ CTL,		0x33	},	// ,
/**->2->8*/ (unsigned char[]){ CTL,		0x27	},	// ö
/**->2->9*/ (unsigned char[]){ CTL|SHIFT,	KEY_1	},	// !
/**->3->0*/ (unsigned char[]){ RESET,	0	},	// RESET
/**->3->1*/ (unsigned char[]){ CTL,		0x10	},	// q
/**->3->2*/ (unsigned char[]){ CTL,		0x26	},	// l
/**->3->3*/ (unsigned char[]){ CTL,		0x04	},	// 3
/**->3->4*/ (unsigned char[]){ CTL|SHIFT,	0x03	},	// "
/**->3->5*/ (unsigned char[]){ CTL,		0x32	},	// m
/**->3->6*/ (unsigned char[]){ CTL,		0x56	},	// <
/**->3->7*/ (unsigned char[]){ CTL,		0x0D	},	// '
/**->3->8*/ (unsigned char[]){ CTL,		0x21	},	// f
/**->3->9*/ (unsigned char[]){ CTL,		0x1A	},	// ü
/**->4->0*/ (unsigned char[]){ RESET,	0	},	// RESET
/**->4->1*/ (unsigned char[]){ CTL|SHIFT,	0x05	},	// $
/**->4->2*/ (unsigned char[]){ CTL,		0x1E	},	// a
/**->4->3*/ (unsigned char[]){ CTL,		KEY_Z	},	// y
/**->4->4*/ (unsigned char[]){ CTL,		0x05	},	// 4
/**->4->5*/ (unsigned char[]){ CTL,		0x12	},	// e
/**->4->6*/ (unsigned char[]){ CTL,		0x25	},	// k
/**->4->7*/ (unsigned char[]){ CTL|SHIFT,	0x06	},	// %
/**->4->8*/ (unsigned char[]){ CTL,		0x1F	},	// s
/**->4->9*/ (unsigned char[]){ CTL,		0x2D	},	// x
/**->5->0*/ (unsigned char[]){ RESET,	0	},	// RESET
/**->5->1*/ (unsigned char[]){ QUAL,	Q_SHFT_L },	// Shift_L
/**->5->2*/ (unsigned char[]){ CTL|ALTGR,	0x0C	},	// \ FUCK GNU (gcc >2.95.4 broken)
/**->5->3*/ (unsigned char[]){ CTL|SHIFT,	0x09	},	// (
/**->5->4*/ (unsigned char[]){ CTL,		KEY_SPACE	},	// Space
/**->5->5*/ (unsigned char[]){ CTL,		0x06	},	// 5
/**->5->6*/ (unsigned char[]){ CTL,		0x0F	},	// Tabulator
/**->5->7*/ (unsigned char[]){ QUAL,	Q_GUI_L },	// Gui_L
/**->5->8*/ (unsigned char[]){ CTL|ALTGR,	0x09	},	// [
/**->5->9*/ (unsigned char[]){ CTL|SHIFT,	0x0A	},	// )
/**->6->0*/ (unsigned char[]){ RESET,	0	},	// RESET
/**->6->1*/ (unsigned char[]){ CTL|SHIFT,	0x07	},	// &
/**->6->2*/ (unsigned char[]){ CTL,		0x30	},	// b
/**->6->3*/ (unsigned char[]){ CTL|SHIFT,	0x56	},	// >
/**->6->4*/ (unsigned char[]){ CTL|SHIFT,	0x1B	},	// *
/**->6->5*/ (unsigned char[]){ CTL,		0x18	},	// o
/**->6->6*/ (unsigned char[]){ CTL,		0x07	},	// 6
/**->6->7*/ (unsigned char[]){ CTL,		0x1B	},	// +
/**->6->8*/ (unsigned char[]){ CTL,		0x16	},	// u
/**->6->9*/ (unsigned char[]){ CTL|ALTGR,	0x08	},	// {
/**->7->0*/ (unsigned char[]){ RESET,	0	},	// RESET
/**->7->1*/ (unsigned char[]){ CTL,		0x0C	},	// ß
/**->7->2*/ (unsigned char[]){ CTL,		0x23	},	// h
/**->7->3*/ (unsigned char[]){ CTL|SHIFT,	0x0B	},	// =
/**->7->4*/ (unsigned char[]){ CTL|SHIFT,	0x0C	},	// ?
/**->7->5*/ (unsigned char[]){ CTL,		0x14	},	// t
/**->7->6*/ (unsigned char[]){ CTL,		0x24	},	// j
/**->7->7*/ (unsigned char[]){ CTL,		0x08	},	// 7
/**->7->8*/ (unsigned char[]){ CTL,		KEY_N	},	// n
/**->7->9*/ (unsigned char[]){ CTL,		KEY_Y	},	// z
/**->8->0*/ (unsigned char[]){ RESET,	0	},	// RESET
/**->8->1*/ (unsigned char[]){ CTL|SHIFT,	0x33	},	// ;
/**->8->2*/ (unsigned char[]){ CTL|ALTGR,	0x10	},	// @
/**->8->3*/ (unsigned char[]){ CTL|SHIFT,	0x34	},	// :
/**->8->4*/ (unsigned char[]){ QUAL,	Q_ALT_L	},	// Alt_L
/**->8->5*/ (unsigned char[]){ CTL|ALTGR,	0x0A	},	// ]
/**->8->6*/ (unsigned char[]){ CTL|ALTGR,	0x56	},	// |
/**->8->7*/ (unsigned char[]){ CTL,		0x0E	},	// BackSpace
/**->8->8*/ (unsigned char[]){ CTL,		0x09	},	// 8
/**->8->9*/ (unsigned char[]){ CTL,		0x01	},	// Escape
/**->9->0*/ (unsigned char[]){ RESET,	0	},	// RESET
/**->9->1*/ (unsigned char[]){ CTL,		0x2B	},	// #
/**->9->2*/ (unsigned char[]){ CTL,		0x19	},	// p
/**->9->3*/ (unsigned char[]){ CTL|SHIFT,	0x0D	},	// `
/**->9->4*/ (unsigned char[]){ CTL|SHIFT,	0x35	},	// _
/**->9->5*/ (unsigned char[]){ CTL,		0x22	},	// g
/**->9->6*/ (unsigned char[]){ CTL|ALTGR,	0x0B	},	// }
/**->9->7*/ (unsigned char[]){ CTL|SHIFT,	0x08	},	// /
/**->9->8*/ (unsigned char[]){ CTL,		0x2E	},	// c
/**->9->9*/ (unsigned char[]){ CTL,		0x0A	},	// 9

/**->0->#*/ (unsigned char[]){ CTL,		0x6E	},	// INSERT
/**->1->#*/ (unsigned char[]){ CTL,		0x6B	},	// END
/**->2->#*/ (unsigned char[]){ CTL,		0x6C	},	// CURSOR-DOWN
/**->3->#*/ (unsigned char[]){ CTL,		0x6D	},	// PAGE-DOWN
/**->4->#*/ (unsigned char[]){ CTL,		0x69	},	// CURSOR-LEFT
/**->5->#*/ (unsigned char[]){ CTL,		0x6F	},	// DELETE ( ,->5 )
/**->6->#*/ (unsigned char[]){ CTL,		0x6A	},	// CURSOR-RIGHT
/**->7->#*/ (unsigned char[]){ CTL,		0x66	},	// HOME
/**->8->#*/ (unsigned char[]){ CTL,		0x67	},	// CURSOR-UP
/**->9->#*/ (unsigned char[]){ CTL,		0x68	},	// PAGE-UP

/**->0->0*/ (unsigned char[]){ CTL,		0x0B	},	// 0

/**->USR1*/ (unsigned char[]){ QUAL,	Q_SHFT_L },	// Shift_L
/**->USR2*/ (unsigned char[]){ QUAL,	Q_CTRL_L },	// Ctrl_L
/**->USR3*/ (unsigned char[]){ QUAL,	Q_ALT_L },	// Alt_L
/**->USR4*/ (unsigned char[]){ QUAL,	Q_GUI_L },	// Gui_L
/**->USR5*/ (unsigned char[]){ QUAL,	Q_SHFT_R },	// Shift_R
/**->USR6*/ (unsigned char[]){ QUAL,	Q_CTRL_R },	// Ctrl_R
/**->USR7*/ (unsigned char[]){ QUAL,	Q_ALT_R },	// Alt_R
/**->USR8*/ (unsigned char[]){ QUAL,	Q_GUI_R },	// Gui_R
};


//	*INDENT-ON*

int DebugLevel = 1;

//
//      Debug output function.
//
#define Debug(level, fmt...) \
    do { if (level>DebugLevel) { printf(fmt); } } while (0)

//----------------------------------------------------------------------------
//      Send
//----------------------------------------------------------------------------

//
//      Send modifier press.
//      modifier ist a bitfield of modifier
//
static void AOHKSendPressModifier(int modifier)
{
    if (modifier & Q_SHFT_L) {
	KeyOut(KEY_LEFTSHIFT, 1);
    }
    if (modifier & Q_SHFT_R) {
	KeyOut(KEY_RIGHTSHIFT, 1);
    }
    if (modifier & Q_CTRL_L) {
	KeyOut(KEY_LEFTCTRL, 1);
    }
    if (modifier & Q_CTRL_R) {
	KeyOut(KEY_RIGHTCTRL, 1);
    }
    if (modifier & Q_ALT_L) {
	KeyOut(KEY_LEFTALT, 1);
    }
    if (modifier & Q_ALT_R) {
	KeyOut(KEY_RIGHTALT, 1);
    }
    if (modifier & Q_GUI_L) {
	KeyOut(KEY_LEFTMETA, 1);
    }
    if (modifier & Q_GUI_R) {
	KeyOut(KEY_RIGHTMETA, 1);
    }
}

//
//      Send an internal sequence as press events to old routines.
//
static void AOHKSendPressSequence(int modifier, const OHKey * sequence)
{
    // First all modifieres
    if (modifier) {
	AOHKSendPressModifier(modifier);
    }
    // This are special qualifiers
    if (sequence->Modifier) {
	if (sequence->Modifier & ALTGR && !(modifier & Q_ALT_R)) {
	    KeyOut(KEY_RIGHTALT, 1);
	}
	if (sequence->Modifier & ALT && !(modifier & Q_ALT_L)) {
	    KeyOut(KEY_LEFTALT, 1);
	}
	if (sequence->Modifier & CTL && !(modifier & Q_CTRL_L)) {
	    KeyOut(KEY_LEFTCTRL, 1);
	}
	if (sequence->Modifier & SHIFT && !(modifier & Q_SHFT_L)) {
	    KeyOut(KEY_LEFTSHIFT, 1);
	}
    }
    KeyOut(sequence->KeyCode, 1);

    AOHKRelease = 1;			// Set flag release send needed
}

//
//      Send modifier release. (reverse press order)
//
static void AOHKSendReleaseModifier(int modifier)
{
    if (modifier & Q_GUI_R) {
	KeyOut(KEY_RIGHTMETA, 0);
    }
    if (modifier & Q_GUI_L) {
	KeyOut(KEY_LEFTMETA, 0);
    }
    if (modifier & Q_ALT_R) {
	KeyOut(KEY_RIGHTALT, 0);
    }
    if (modifier & Q_ALT_L) {
	KeyOut(KEY_LEFTALT, 0);
    }
    if (modifier & Q_CTRL_R) {
	KeyOut(KEY_RIGHTCTRL, 0);
    }
    if (modifier & Q_CTRL_L) {
	KeyOut(KEY_LEFTCTRL, 0);
    }
    if (modifier & Q_SHFT_R) {
	KeyOut(KEY_RIGHTSHIFT, 0);
    }
    if (modifier & Q_SHFT_L) {
	KeyOut(KEY_LEFTSHIFT, 0);
    }
}

//
//      Send an internal sequence as release events.
//
static void AOHKSendReleaseSequence(int modifier, const OHKey * sequence)
{
    KeyOut(sequence->KeyCode, 0);

    // This are special qualifiers
    if (sequence->Modifier) {
	if (sequence->Modifier & SHIFT && !(modifier & Q_SHFT_L)) {
	    KeyOut(KEY_LEFTSHIFT, 0);
	}
	if (sequence->Modifier & CTL && !(modifier & Q_CTRL_L)) {
	    KeyOut(KEY_LEFTCTRL, 0);
	}
	if (sequence->Modifier & ALT && !(modifier & Q_ALT_L)) {
	    KeyOut(KEY_LEFTALT, 0);
	}
	if (sequence->Modifier & ALTGR && !(modifier & Q_ALT_R)) {
	    KeyOut(KEY_RIGHTALT, 0);
	}
    }
    if (modifier) {
	AOHKSendReleaseModifier(modifier);
    }

    AOHKRelease = 0;			// release is done
}

//
//      Game mode release key.
//
static void AOHKGameModeRelease(int key)
{
    const OHKey *sequence;

    if (AOHKGamePressed & (1 << key)) {
	sequence = &AOHKGameTable[key];
	if (sequence->Modifier == QUOTE) {
	    AOHKLastKey = 0;
	} else {
	    AOHKSendReleaseSequence(0, sequence);
	}
	AOHKGamePressed &= ~(1 << key);
    }
    if (AOHKGameQuotePressed & (1 << key)) {
	sequence = &AOHKQuoteGameTable[key];
	AOHKSendReleaseSequence(0, sequence);
	AOHKGameQuotePressed &= ~(1 << key);
    }
}

//
//      Game mode release keys.
//
static void AOHKGameModeReleaseAll(void)
{
    int i;

    for (i = 0; i <= OH_SPECIAL; ++i) {
	AOHKGameModeRelease(i);
    }
}

//----------------------------------------------------------------------------
//      Leds
//----------------------------------------------------------------------------

//
//      Quote State led
//
static void QuoteStateLedOn(void)
{
    ShowLED(0, 1);
}

//
//      Quote State led
//
static void QuoteStateLedOff(void)
{
    ShowLED(0, 0);
}

//
//      Second State led
//
static void SecondStateLedOn(void)
{
    ShowLED(1, 1);
}

//
//      Second State led
//
static void SecondStateLedOff(void)
{
    ShowLED(1, 0);
}

//
//      Gamemode led
//
static void GameModeLedOn(void)
{
    ShowLED(2, 1);
}

//
//      Gamemode led
//
static void GameModeLedOff(void)
{
    ShowLED(2, 0);
}

//----------------------------------------------------------------------------
//
//      Reset to normal state.
//
static void AOHKReset(void)
{
    if (AOHKState == OHSoftOff) {	// Disabled do nothing
	return;
    }

    if (AOHKState == OHGameMode) {
	AOHKGameModeReleaseAll();
	AOHKRelease = 0;
	AOHKGameSendQuote = 0;
    } else if (AOHKRelease) {		// Release old keys pressed
	if (AOHKLastSequence) {
	    AOHKSendReleaseSequence(AOHKLastModifier, AOHKLastSequence);
	} else if (AOHKLastModifier) {
	    AOHKSendReleaseModifier(AOHKLastModifier);
	} else {
	    Debug(5, "FIXME: release lost\n");
	}
	AOHKRelease = 0;
    }

    AOHKModifier = 0;
    AOHKStickyModifier = 0;
    AOHKLastKey = 0;
    AOHKState = OHFirstKey;
    // FIXME: Check if leds are on!
    SecondStateLedOff();
    QuoteStateLedOff();
    GameModeLedOff();
    SpecialStateLedOff();
}

//----------------------------------------------------------------------------
//      Helper functions
//----------------------------------------------------------------------------

//
//      Handle modifier key sequence.
//
//      First press sends press, second press sends release.
//      Next key also releases the modifier.
//      
static void AOHKDoModifier(int modifier)
{
    if (AOHKModifier & modifier) {	// release it
	AOHKSendReleaseModifier(modifier);
    } else {				// press it
	AOHKSendPressModifier(modifier);
    }
    AOHKModifier ^= modifier;

    AOHKLastModifier = AOHKModifier;
    AOHKLastSequence = NULL;
    AOHKRelease = AOHKModifier != 0;	// release required
}

static void AOHKEnterSpecialState(void);

//
//      Handle key sequence.
//
static void AOHKDoSequence(const OHKey * sequence)
{

    switch (sequence->Modifier) {
	case RESET:
	    Debug(2, "Soft reset\n");
	    AOHKReset();
	    break;
	case QUAL:
	    AOHKDoModifier(sequence->KeyCode);
	    break;
	case STICKY:
	    // FIXME: sticky qualifier
	    break;
	case SPECIAL:
	    AOHKEnterSpecialState();
	    break;
	default:			// QUOTE or nothing
	    AOHKSendPressSequence(AOHKLastModifier =
		AOHKModifier, AOHKLastSequence = sequence);
	    AOHKModifier = AOHKStickyModifier;
	    break;
    }
}

//----------------------------------------------------------------------------
//      Game Mode
//----------------------------------------------------------------------------

//
//      Map to internal key.
//      OH_... 
//      -1 if not mapable.
//
static int AOHKMapToInternal(int key, __attribute__ ((unused))
    int down)
{
    if (key < 0 || key > 255) {
	return -1;
    }
    return AOHKConvertTable[key] == 255 ? -1 : AOHKConvertTable[key];
}

//
//      Enter game mode
//
static void AOHKEnterGameMode(void)
{
    Debug(4, "Game mode on.\n");
    AOHKReset();			// release keys, ...
    AOHKState = OHGameMode;
    GameModeLedOn();
}

//
//      Enter special state
//
static void AOHKEnterSpecialState(void)
{
    Debug(4, "Special state start.\n");
    AOHKState = OHSpecial;
    SpecialStateLedOff();
}

//----------------------------------------------------------------------------
//      State machine
//----------------------------------------------------------------------------

//
//      Send sequence from state machine
//
static void AOHKSendSequence(int key, const OHKey * sequence)
{
    //
    //  if quote or quote+repeat (=super) still be pressed
    //  reenter this state.
    //
    if (key != OH_QUOTE && (AOHKDownKeys & (1 << OH_QUOTE))) {
	if (AOHKDownKeys & (1 << OH_REPEAT)) {
	    AOHKState = OHSuperFirstKey;
	} else {
	    AOHKState = OHQuoteFirstKey;
	}
	QuoteStateLedOn();
    } else {
	AOHKState = OHFirstKey;
	QuoteStateLedOff();
    }
    SecondStateLedOff();

    AOHKDoSequence(sequence);
}

//
//      Handle the Firstkey state.
//
static void AOHKFirstKey(int key)
{
    switch (key) {

	case OH_MACRO:			// macro key
	    if (AOHKDownKeys & (1 << OH_REPEAT)) {	// game mode
		AOHKEnterGameMode();
		break;
	    }
	    // macro -> macro is ignored
	    AOHKState = OHMacroFirstKey;
	    break;

	case OH_QUOTE:			// enter quote state
	    if (AOHKDownKeys & (1 << OH_REPEAT)) {	// super quote
		Debug(2, "super quote.\n");
		AOHKState = OHSuperFirstKey;
		QuoteStateLedOn();
		break;
	    }
	    if (AOHKState == OHMacroFirstKey) {
		Debug(5, "FIXME: Macro quote\n");
		AOHKState = OHMacroQuoteFirstKey;
	    } else {
		AOHKState = OHQuoteFirstKey;
	    }
	    QuoteStateLedOn();
	    break;

	case OH_SPECIAL:		// enter special state
	    AOHKEnterSpecialState();
	    break;

	case OH_REPEAT:		// repeat last sequence
	    if (AOHKDownKeys & (1 << OH_MACRO)) {	// game mode
		// This directions didn't work good, better MACRO+REPEAT
		AOHKEnterGameMode();
		break;
	    }
	    if (AOHKLastSequence) {
		AOHKSendPressSequence(AOHKLastModifier, AOHKLastSequence);
	    } else if (AOHKLastModifier) {
		AOHKDoModifier(AOHKLastModifier);
	    }
	    return;

	case OH_USR_1:
	case OH_USR_2:
	case OH_USR_3:
	case OH_USR_4:
	case OH_USR_5:
	case OH_USR_6:
	case OH_USR_7:
	case OH_USR_8:
	    AOHKSendSequence(key, &AOHKTable[10 * 10 + 1 + key - OH_USR_1]);
	    return;

	default:
	    if (AOHKDownKeys & (1 << OH_REPEAT)) {	// cursor mode?
		Debug(2, "cursor mode.\n");
		AOHKSendSequence(key, &AOHKTable[9 * 10 + key - OH_QUOTE]);
		return;
	    }
	    AOHKLastKey = key;
	    if (AOHKState == OHMacroFirstKey) {
		AOHKState = OHMacroSecondKey;
	    } else {
		AOHKState = OHSecondKey;
	    }
	    SecondStateLedOn();
	    break;
    }
    Timeout = AOHKTimeout;
}

//
//      Handle the Super/Quote Firstkey state.
//
static void AOHKQuoteFirstKey(int key)
{
    switch (key) {
	case OH_MACRO:			// macro key
	    // QUOTE->MACRO free
	    // FIXNE:
	    AOHKState = OHMacroFirstKey;
	    return;

	case OH_QUOTE:			// double quote extra key
	    if (AOHKState == OHSuperFirstKey) {	// super quote quote
		AOHKSendSequence(key, &AOHKSuperTable[10 * 10]);
	    } else if (AOHKState == OHMacroFirstKey) {	// macro quote quote
		AOHKSendSequence(key, (const OHKey *)AOHKMacroTable[10 * 10]);
	    } else {
		AOHKSendSequence(key, &AOHKTable[10 * 10]);
	    }
	    break;

	case OH_SPECIAL:		// enter special state
	    AOHKEnterSpecialState();
	    return;

	case OH_REPEAT:		// quoted repeat extra key
	    if (AOHKDownKeys & (1 << OH_QUOTE)) {	// super quote
		// Use this works good
		Debug(2, "super quote.\n");
		AOHKState = OHSuperFirstKey;
		QuoteStateLedOn();
		break;
	    }
	    if (AOHKState == OHSuperFirstKey) {	// super quote repeat
		AOHKSendSequence(key, &AOHKSuperTable[9 * 10]);
	    } else if (AOHKState == OHMacroFirstKey) {	// macro quote repeat
		AOHKSendSequence(key, (const OHKey *)AOHKMacroTable[9 * 10]);
	    } else {
		AOHKSendSequence(key, &AOHKTable[9 * 10]);
	    }
	    break;

	case OH_USR_1:
	case OH_USR_2:
	case OH_USR_3:
	case OH_USR_4:
	case OH_USR_5:
	case OH_USR_6:
	case OH_USR_7:
	case OH_USR_8:
	    if (AOHKState == OHSuperFirstKey) {	// super quote quote
		AOHKSendSequence(key,
		    &AOHKSuperTable[10 * 10 + 1 + key - OH_USR_1]);
	    } else {
		AOHKSendSequence(key,
		    &AOHKQuoteTable[10 * 10 + 1 + key - OH_USR_1]);
	    }
	    break;

	default:
	    AOHKLastKey = key;
	    AOHKState++;
	    SecondStateLedOn();
	    break;
    }
}

//
//      Handle the Normal/Super/Quote SecondKey state.
//
void AOHKSecondKey(int key)
{
    const OHKey *sequence;
    int n;

    if (key == OH_MACRO || key == OH_SPECIAL || (OH_USR_1 <= key && key <= OH_USR_8)) {	// oops
	AOHKReset();
	return;
    }
    // This allows pressing first key and quote together.
    if (key == OH_QUOTE && AOHKState == OHSecondKey) {
	AOHKState = OHQuoteSecondKey;
	QuoteStateLedOn();
	return;;
    }
    // OH_QUOTE ok

    if (key == OH_REPEAT) {		// second repeat 9 extra keys
	n = 9 * 10 + AOHKLastKey - OH_QUOTE;
    } else {				// normal key sequence
	n = (AOHKLastKey - OH_KEY_1) * 10 + key - OH_QUOTE;
    }

    switch (AOHKState) {
	case OHSecondKey:
	    sequence = &AOHKTable[n];
	    break;
	case OHQuoteSecondKey:
	    sequence = &AOHKQuoteTable[n];
	    break;
	case OHMacroSecondKey:
	    // FIXME: string
	    sequence = (const OHKey *)AOHKMacroTable[n];
	    break;
	case OHSuperSecondKey:
	default:
	    sequence = &AOHKSuperTable[n];
	    break;
    }

    // This allows pressing first key and quote together.
    if (sequence->Modifier == QUOTE) {
	Debug(4, "FIXME: Not sure if required\n");
	AOHKState = OHQuoteSecondKey;
	QuoteStateLedOn();
	return;
    }
    AOHKSendSequence(key, sequence);
}

//
//      Handle the game mode state.
//
//      Game mode:      For games keys are direct mapped.
//
//
void AOHKGameMode(int key)
{
    const OHKey *sequence;

    Debug(1, "Gamemode key %d.\n", key);

    // FIXME: QUAL shouldn't work correct

    AOHKGameSendQuote = 0;
    sequence = AOHKLastKey ? &AOHKQuoteGameTable[key] : &AOHKGameTable[key];

    //
    //  Reset is used to return to normal mode.
    //
    if (sequence->Modifier == RESET) {
	Debug(4, "Game mode off.\n");
	AOHKGameModeReleaseAll();
	AOHKState = OHFirstKey;
	GameModeLedOff();
	return;
    }
    //
    //  Holding down QUOTE shifts to second table.
    //
    if (sequence->Modifier == QUOTE) {
	Debug(2, "Game mode quote.\n");
	AOHKLastKey = 1;
	AOHKGameSendQuote = 1;
	AOHKLastModifier = AOHKModifier;
	AOHKLastSequence = &AOHKQuoteGameTable[key];
	AOHKModifier = AOHKStickyModifier;
	AOHKGamePressed |= (1 << key);
    } else {
	AOHKDoSequence(sequence);
	if (AOHKLastKey) {
	    AOHKGameQuotePressed |= (1 << key);
	} else {
	    AOHKGamePressed |= (1 << key);
	}
    }
}

//
//      Handle the special state.
//
static void AOHKSpecialMode(int key)
{
    SpecialStateLedOff();
    AOHKReset();

    Debug(2, "Special Key %d.\n", key);
    switch (key) {
	case OH_QUOTE:
	    // FIXME: nolonger used
	    // FIXME: swap QUOTE AND REPEAT keys.
	    return;
	case OH_KEY_4:			// Version
	    // FIXME: Ugly hack
	    KeyOut(KEY_A, 1);
	    KeyOut(KEY_A, 0);
	    KeyOut(KEY_O, 1);
	    KeyOut(KEY_O, 0);
	    KeyOut(KEY_H, 1);
	    KeyOut(KEY_H, 0);
	    KeyOut(KEY_K, 1);
	    KeyOut(KEY_K, 0);
	    KeyOut(KEY_SPACE, 1);
	    KeyOut(KEY_SPACE, 0);
	    KeyOut(KEY_V, 1);
	    KeyOut(KEY_V, 0);
	    KeyOut(KEY_0, 1);
	    KeyOut(KEY_0, 0);
	    KeyOut(KEY_DOT, 1);
	    KeyOut(KEY_DOT, 0);
	    KeyOut(KEY_0, 1);
	    KeyOut(KEY_0, 0);
	    KeyOut(KEY_3, 1);
	    KeyOut(KEY_3, 0);
	    return;

	case OH_KEY_5:			// exit
	    AOHKReset();
	    Exit = 1;
	    return;
	case OH_SPECIAL:		// turn it off
	    AOHKState = OHSoftOff;
	    Debug(4, "Soft turned off.\n");
	    return;
    }
    if (key == OH_KEY_1) {		// enable only me mode
	AOHKOnlyMe ^= 1;
	Debug(4, "Toggle only me mode %s.\n", AOHKOnlyMe ? "on" : "off");
	return;
    }
    if (key == OH_KEY_2) {		// double timeout
	AOHKTimeout <<= 1;
	AOHKTimeout |= 1;
	Timeout = AOHKTimeout;
	Debug(4, "Double timeout %d.\n", AOHKTimeout);
	return;
    }
    if (key == OH_KEY_3) {		// half timeout
	AOHKTimeout >>= 1;
	AOHKTimeout |= 1;
	Timeout = AOHKTimeout;
	Debug(4, "Half timeout %d.\n", AOHKTimeout);
	return;
    }
    if (key == OH_KEY_5) {		// rotated mode
	// FIXME: nolonger used
	return;
    }
    if (key == OH_KEY_6) {		// game mode
	Debug(4, "Game mode on.\n");
	AOHKEnterGameMode();
	return;
    }
    if (key == OH_KEY_9) {		// turn it off
	AOHKState = OHHardOff;
	Debug(4, "one-hand: Turned off.\n");
	return;
    }
    Debug(5, "one-hand: unsupported special %d.\n", key);
}

//
//      OneHand Statemachine.
//
void AOHKFeedKey(unsigned long timestamp, int inkey, int down)
{
    int key;

    //
    //  Completly turned off
    //
    if (AOHKState == OHHardOff) {
	KeyOut(inkey, down);
	return;
    }

    Debug(1, "Keyin %02X %s\n", inkey, down ? "down" : "up");
    key = AOHKMapToInternal(inkey, down);

    //
    //
    //  Turned soft off
    //
    if (AOHKState == OHSoftOff) {
	//
	//  Next special key reenables us
	//  down! otherwise we get the release of the disable press
	//
	if (down && key == OH_SPECIAL) {
	    Debug(4, "Reenable.\n");
	    AOHKState = OHFirstKey;
	    AOHKDownKeys = 0;
	    return;
	}
	KeyOut(inkey, down);
	return;
    }
    //
    //  Timeout with timestamps
    //
    if (AOHKLastJiffies + AOHKTimeout < timestamp) {
	Debug(1, "Timeout %lu %lu\n", AOHKLastJiffies, timestamp);
	AOHKFeedTimeout(timestamp - AOHKLastJiffies);
    }
    AOHKLastJiffies = timestamp;

    //
    //  Handling of unsupported input keys.
    //
    if (key == -1) {
	if (!AOHKState == OHGameMode) {
	    AOHKReset();		// any unsupported key reset us
	}
	if (!AOHKOnlyMe) {		// other keys aren't disabled
	    KeyOut(inkey, down);
	}
	return;
    }

    Debug(1, "Key %02x -> %d of state %d.\n", inkey, key, AOHKState);

    //
    //  Test for repeat and handle release
    //
    if (!down) {			// release
	//
	//     Game mode, release and press aren't ordered.
	//
	if (AOHKState == OHGameMode) {
	    //
	    //  Game mode, quote delayed to release.
	    //
	    if (AOHKGameSendQuote) {
		AOHKSendPressSequence(AOHKLastModifier, AOHKLastSequence);
		AOHKSendReleaseSequence(AOHKLastModifier, AOHKLastSequence);
		AOHKGameSendQuote = 0;
	    }
	    AOHKGameModeRelease(key);
	} else {
	    //
	    //  Need to send a release sequence.
	    //
	    // FIXME: if (AOHKState == AOHKFirstKey && AOHKRelease)
	    if (AOHKRelease) {
		if (AOHKLastSequence) {
		    AOHKSendReleaseSequence(AOHKLastModifier,
			AOHKLastSequence);
		} else if (OH_USR_1 <= key && key <= OH_USR_8) {
		    // FIXME: hold USR than press a sequence,
		    // FIXME: than release USR is not supported!
		    if (AOHKLastModifier) {
			AOHKSendReleaseModifier(AOHKLastModifier);
			AOHKModifier = AOHKStickyModifier;
			AOHKRelease = 0;
		    }
		} else {
		    Debug(1, "Release for modifier!\n");
		    goto ignore;
		}
		if (AOHKRelease) {
		    Debug(5, "Release not done!\n");
		}
	    }
	}
      ignore:
	AOHKDownKeys &= ~(1 << key);
	return;
    } else if (AOHKDownKeys & (1 << key)) {	// repeating
	//
	//      internal key repeating, ignore
	//      (note: 1 repeated does nothing. 11 repeated does someting!)
	//      QUOTE is repeated in game mode, is this good?
	//      QUAL modifiers aren't repeated
	//
	if ((AOHKState == OHGameMode || AOHKState == OHFirstKey)
	    && AOHKLastSequence) {
	    AOHKSendPressSequence(AOHKLastModifier, AOHKLastSequence);
	}
	return;
    }
    AOHKDownKeys |= (1 << key);

    //
    //  Convert internal code into scancodes (only down events!)
    //  The fat state machine
    //
    switch (AOHKState) {

	case OHFirstKey:
	case OHMacroFirstKey:
	    AOHKFirstKey(key);
	    break;

	case OHSuperFirstKey:
	case OHQuoteFirstKey:
	case OHMacroQuoteFirstKey:
	    AOHKQuoteFirstKey(key);
	    break;

	case OHSecondKey:
	case OHQuoteSecondKey:
	case OHSuperSecondKey:
	case OHMacroSecondKey:
	case OHMacroQuoteSecondKey:
	    AOHKSecondKey(key);
	    break;

	case OHGameMode:
	    AOHKGameMode(key);
	    break;

	case OHSpecial:
	    AOHKSpecialMode(key);
	    break;

	default:
	    Debug(5, "Unkown state %d reached\n", AOHKState);
	    break;
    }
}

//
//      Feed Timeouts
//
void AOHKFeedTimeout(int which)
{
    //
    //  Timeout -> reset to intial state
    //
    if (AOHKState != OHGameMode && AOHKState!= OHSoftOff
	    && AOHKState != OHHardOff) {
	// Long time: total reset
	if (which >= AOHKTimeout * 10) {
	    Debug(1, "Timeout long %d\n", which);
	    AOHKReset();
	    AOHKDownKeys = 0;
	    Timeout = 0;
	    return;
	}
	if (AOHKState != OHFirstKey) {
	    Debug(1, "Timeout short %d\n", which);
	    // FIXME: Check if leds are on!
	    SecondStateLedOff();
	    QuoteStateLedOff();

	    // Short time: only reset state
	    AOHKState = OHFirstKey;
	}
	Timeout = 10 * AOHKTimeout;
    }
}

//
//      Reset convert table
//
void AOHKResetConvertTable(void)
{
    int idx;

    for (idx = 0; idx < 255; ++idx) {
	AOHKConvertTable[idx] = 255;
    }
}

//
//      Reset mapping table
//
void AOHKResetMappingTable(void)
{
    size_t idx;

    for (idx = 0; idx < sizeof(AOHKTable) / sizeof(*AOHKTable); ++idx) {
	AOHKTable[idx].Modifier = RESET;
	AOHKTable[idx].KeyCode = KEY_RESERVED;
    }
    for (idx = 0; idx < sizeof(AOHKQuoteTable) / sizeof(*AOHKQuoteTable);
	++idx) {
	AOHKQuoteTable[idx].Modifier = RESET;
	AOHKQuoteTable[idx].KeyCode = KEY_RESERVED;
    }
    for (idx = 0; idx < sizeof(AOHKSuperTable) / sizeof(*AOHKSuperTable);
	++idx) {
	AOHKSuperTable[idx].Modifier = RESET;
	AOHKSuperTable[idx].KeyCode = KEY_RESERVED;
    }
    for (idx = 0; idx < sizeof(AOHKGameTable) / sizeof(*AOHKGameTable); ++idx) {
	AOHKGameTable[idx].Modifier = RESET;
	AOHKGameTable[idx].KeyCode = KEY_RESERVED;
    }
    for (idx = 0;
	idx < sizeof(AOHKQuoteGameTable) / sizeof(*AOHKQuoteGameTable);
	++idx) {
	AOHKQuoteGameTable[idx].Modifier = RESET;
	AOHKQuoteGameTable[idx].KeyCode = KEY_RESERVED;
    }
}

//
//      Reset macro table
//
void AOHKResetMacroTable(void)
{
    size_t idx;

    for (idx = 0; idx < sizeof(AOHKMacroTable) / sizeof(*AOHKMacroTable);
	++idx) {
#if 0
	extern void _end[];

	if (AOHKMacroTable[idx] > _end) {
	    free(AOHKMacroTable[idx]);
	}
	AOHKMacroTable[idx] = NULL;
#endif
	AOHKMacroTable[idx][0] = RESET;
	AOHKMacroTable[idx][1] = KEY_RESERVED;
    }
}

//
//      Setup table which converts input keycodes to internal key symbols.
//
void AOHKSetupConvertTable(const int *table)
{
    int idx;

    AOHKResetConvertTable();

    while ((idx = *table++)) {
	if (idx > 0 && idx < 256) {
	    AOHKConvertTable[idx] = *table;
	}
	++table;
    }
}

//----------------------------------------------------------------------------
//      Save + Load
//----------------------------------------------------------------------------

//	*INDENT-OFF*

//
//	Key Scancode - string (US Layout used)
//
static const char* AOHKKey2String[128] = {
    "RESERVED",
    "ESC",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "0",
    "-",
    "=",
    "BackSpace",
    "Tab",		// 0x0F

    "q",		// 0x10
    "w",
    "e",
    "r",
    "t",
    "y",
    "u",
    "i",
    "o",
    "p",
    "[",
    "]",
    "Enter",
    "LeftCtrl",
    "a",
    "s",

    "d",		// 0x20
    "f",
    "g",
    "h",
    "j",
    "k",
    "l",
    ";",
    "'",
    "`",
    "LeftShift",
    "\\",
    "z",
    "x",
    "c",
    "v",

    "b",		// 0x30
    "n",
    "m",
    ",",
    ".",
    "/",
    "RightShift",
    "KP_Multiply",
    "LeftAlt",
    "Space",
    "CapsLock",
    "F1",
    "F2",
    "F3",
    "F4",
    "F5",

    "F6",		// 0x40
    "F7",
    "F8",
    "F9",
    "F10",
    "NumLock",
    "ScrollLock",
    "KP_7",
    "KP_8",
    "KP_9",
    "KP_Subtract",
    "KP_4",
    "KP_5",
    "KP_6",
    "KP_Add",
    "KP_1",

    "KP_2",		// 0x50
    "KP_3",
    "KP_0",
    "KP_Period",
    "SYSRQ",
    "F13",
    "<",
    "F11",
    "F12",
    "F14",
    "F15",
    "F16",
    "F17",
    "F18",
    "F19",
    "F20",

    "KP_Enter",		// 0x60
    "RightCtrl",
    "KP_Divide",
    "SYSRQ",
    "RightAlt",
    "Linefeed",
    "Home",
    "Up",
    "PageUp",
    "Left",
    "Right",
    "End",
    "Down",
    "PageDown",
    "Insert",
    "Delete",

    "Macro",		// 0x70
    "Mute",
    "VolumeDown",
    "VolumeUp",
    "Power",
    "KP_Equal",
    "KP_PlusMinus",
    "Pause",
    "0x78",
    "KP_Comma",
    "Hanguel",
    "Hanja",
    "Yen",
    "LeftMeta",
    "RightMeta",
    "Menu",
};

//
//	Alias names.
//
static const struct {
    const char* Name;
    unsigned char Key;
} AOHKKeyAlias[] = {
    {"AltGr",	KEY_RIGHTALT },
    {"KP0",	KEY_KP0 },
    {"KP1",	KEY_KP1 },
    {"KP2",	KEY_KP2 },
    {"KP3",	KEY_KP3 },
    {"KP4",	KEY_KP4 },
    {"KP5",	KEY_KP5 },
    {"KP6",	KEY_KP6 },
    {"KP7",	KEY_KP7 },
    {"KP8",	KEY_KP8 },
    {"KP9",	KEY_KP9 },
};

//
//	Internal key - string
//
static const char* AOHKInternal2String[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "#", "*",
    "USR1", "USR2", "USR3", "USR4", "USR5", "USR6", "USR7", "USR8",
    "SPECIAL"
};

//	*INDENT-ON*

//
//      Save convert table.
//
static void AOHKSaveConvertTable(FILE * fp, const unsigned char *t)
{
    int i;

    fprintf(fp, "\n//\tConverts input keys to internal symbols\nconvert:\n");
    for (i = 0;
	i < 255
	&& (unsigned)i < sizeof(AOHKKey2String) / sizeof(*AOHKKey2String);
	++i) {
	if (t[i] != 255) {
	    fprintf(fp, "%-10s\t-> %s\n", AOHKKey2String[i],
		AOHKInternal2String[t[i]]);
	}
    }
}

//
//      Save sequence
//
static void AOHKSaveSequence(FILE * fp, const unsigned char *s)
{
    switch (*s) {			// modifier code
	case RESET:
	    fprintf(fp, "RESET\n");
	    break;
	case QUOTE:
	    fprintf(fp, "QUOTE\n");
	    break;
	case TOGAME:
	    fprintf(fp, "GAME\n");
	    break;
	case SPECIAL:
	    fprintf(fp, "SPECIAL\n");
	    break;
	case QUAL:
	case STICKY:
	    if (*s == QUAL) {
		fprintf(fp, "QUAL ");
	    } else {
		fprintf(fp, "STICKY ");
	    }
	    if (s[1] & Q_SHFT_L) {
		fprintf(fp, "LeftShift ");
	    }
	    if (s[1] & Q_CTRL_L) {
		fprintf(fp, "LeftCtrl ");
	    }
	    if (s[1] & Q_ALT_L) {
		fprintf(fp, "LeftAlt ");
	    }
	    if (s[1] & Q_GUI_L) {
		fprintf(fp, "LeftMeta ");
	    }
	    if (s[1] & Q_SHFT_R) {
		fprintf(fp, "RightShift ");
	    }
	    if (s[1] & Q_CTRL_R) {
		fprintf(fp, "RightCtrl ");
	    }
	    if (s[1] & Q_ALT_R) {
		fprintf(fp, "RightAlt ");
	    }
	    if (s[1] & Q_GUI_R) {
		fprintf(fp, "RightMeta ");
	    }
	    fprintf(fp, "\n");
	    break;
	default:
	    if (s[0] & SHIFT) {
		fprintf(fp, "LeftShift ");
	    }
	    if (s[0] & CTL) {
		fprintf(fp, "LeftCtrl ");
	    }
	    if (s[0] & ALT) {
		fprintf(fp, "LeftAlt ");
	    }
	    if (s[0] & ALTGR) {
		fprintf(fp, "AltGr ");
	    }
	    fprintf(fp, "%s\n", AOHKKey2String[s[1]]);
    }
}

//
//      Save sequence table.
//
static void AOHKSaveMapping(FILE * fp, const char *prefix,
    const unsigned char *t, int n)
{
    int i;

    for (i = 0; i < n; i += 2) {
	//
	//      Print internal sequence code
	//
	fprintf(fp, "%s", prefix);
	if (i == 100 * 2) {
	    fprintf(fp, "00\t-> ");
	} else if (i > 100 * 2) {
	    fprintf(fp, "USR%d\t-> ", (i >> 1) - 100);
	} else if (i > 89 * 2) {
	    fprintf(fp, "%d#\t-> ", (i >> 1) % 10);
	} else {
	    fprintf(fp, "%d%d\t-> ", (i >> 1) / 10 + 1, (i >> 1) % 10);
	}

	//
	//      Print output sequence key
	//
	AOHKSaveSequence(fp, t + i);
    }
}

//
//      Save game table
//
static void AOHKSaveGameTable(FILE * fp, const char *prefix,
    const unsigned char *t, int n)
{
    int i;

    for (i = 0; i < n; i += 2) {
	fprintf(fp, "%s%s\t-> ", prefix, AOHKInternal2String[i / 2]);
	AOHKSaveSequence(fp, t + i);
    }
}

//
//      Save macro table
//
static void AOHKSaveMacroTable(FILE * fp, const char *prefix,
    const unsigned char **t, int n)
{
    int i;

    for (i = 0; i < n; ++i) {
	//
	//      Print internal sequence code
	//
	fprintf(fp, "%s", prefix);
	if (i == 100) {
	    fprintf(fp, "00\t-> ");
	} else if (i > 100) {
	    fprintf(fp, "USR%d\t-> ", i - 100);
	} else if (i > 89) {
	    fprintf(fp, "%d#\t-> ", i % 10);
	} else {
	    fprintf(fp, "%d%d\t-> ", i / 10 + 1, i % 10);
	}

	//
	//      Print output sequence key
	//
	AOHKSaveSequence(fp, t[i]);
    }
}

//
//      Save internals tables in a nice format.
//
void AOHKSaveTable(const char *file)
{
    FILE *fp;

    if (!strcmp(file, "-")) {		// stdout
	fp = stdout;
    } else if (!(fp = fopen(file, "w+"))) {
	Debug(5, "Can't open save file '%s'\n", file);
	return;
    }
    fprintf(fp,
	"//\n" "//\t%s\t-\tALE one-hand keyboard mapping.\n" "//\n"
	"//\tThis file is part of ALE one-hand keyboard\n" "//\n"
	"//\tThis program is free software; you can redistribute it and/or modify\n"
	"//\tit under the terms of the GNU General Public License as published by\n"
	"//\tthe Free Software Foundation; only version 2 of the License.\n"
	"//\n"
	"//\tThis program is distributed in the hope that it will be useful,\n"
	"//\tbut WITHOUT ANY WARRANTY; without even the implied warranty of\n"
	"//\tMERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
	"//\tGNU General Public License for more details.\n" "//\n", file);

    AOHKSaveConvertTable(fp, AOHKConvertTable);
    fprintf(fp,
	"\n//\tMapping of internal symbol sequences to keys\n" "mapping:\n");

    fprintf(fp, "//\tnormal\n");
    AOHKSaveMapping(fp, "", (unsigned char *)AOHKTable, sizeof(AOHKTable));
    fprintf(fp, "//\tquote 0 key prefix\n");
    AOHKSaveMapping(fp, "0", (unsigned char *)AOHKQuoteTable,
	sizeof(AOHKQuoteTable));
    fprintf(fp, "//\tsuper 0# key prefix\n");
    AOHKSaveMapping(fp, "0#", (unsigned char *)AOHKSuperTable,
	sizeof(AOHKSuperTable));
    fprintf(fp, "//\tgame *# key prefix\n");
    AOHKSaveGameTable(fp, "*#", (unsigned char *)AOHKGameTable,
	sizeof(AOHKGameTable));
    fprintf(fp, "//\tquote game 0*# key prefix\n");
    AOHKSaveGameTable(fp, "0*#", (unsigned char *)AOHKQuoteGameTable,
	sizeof(AOHKQuoteGameTable));

    fprintf(fp, "//\t# Macro strings * key prefix\nmacro:\n");
    AOHKSaveMacroTable(fp, "*", (const unsigned char **)AOHKMacroTable,
	sizeof(AOHKMacroTable) / sizeof(*AOHKMacroTable));

    if (strcmp(file, "-")) {		// !stdout
	fclose(fp);
    }
}

//
//      Junk at end of line
//
static void AOHKIsJunk(int linenr, const char *s)
{
    for (; *s && isspace(*s); ++s) {
    }
    if (*s) {
	Debug(5, "%d: Junk '%s' at end of line\n", linenr, s);
    }
}

//
//      Convert a string to key code.
//
static int AOHKString2Key(const char *line, size_t l)
{
    size_t i;

    //
    //  First lookup alias names.
    //
    for (i = 0; i < sizeof(AOHKKeyAlias) / sizeof(*AOHKKeyAlias); ++i) {
	if (l == strlen(AOHKKeyAlias[i].Name)
	    && !strncasecmp(line, AOHKKeyAlias[i].Name, l)) {
	    return AOHKKeyAlias[i].Key;
	}
    }
    //
    //  Not found, lookup normal names.
    //
    for (i = 0; i < sizeof(AOHKKey2String) / sizeof(*AOHKKey2String); ++i) {
	if (l == strlen(AOHKKey2String[i])
	    && !strncasecmp(line, AOHKKey2String[i], l)) {
	    return i;
	}
    }
    return KEY_RESERVED;
}

//
//      Convert a string to internal code.
//
static int AOHKString2Internal(const char *line, size_t l)
{
    size_t i;

    for (i = 0; i < sizeof(AOHKInternal2String) / sizeof(*AOHKInternal2String);
	++i) {
	if (l == strlen(AOHKInternal2String[i])
	    && !strncasecmp(line, AOHKInternal2String[i], l)) {
	    return i;
	}
    }
    return -1;
}

//
//      Parse convert line.
//
static void AOHKParseConvert(char *line)
{
    char *s;
    size_t l;
    int key;
    int internal;

    //
    //  Get input key code
    //
    for (s = line; *s && !isspace(*s); ++s) {
    }
    l = s - line;
    key = AOHKString2Key(line, l);
    if (key == KEY_RESERVED) {		// Still not found giving up.
	Debug(5, "Key '%.*s' not found\n", l, line);
	return;
    }
    //
    //      Skip white spaces
    //
    for (; *s && isspace(*s); ++s) {
    }
    //
    //      Need ->
    //
    if (strncmp(s, "->", 2)) {
	Debug(5, "'->' expected not '%s'\n", s);
	return;
    }
    s += 2;
    //
    //      Skip white spaces
    //
    for (; *s && isspace(*s); ++s) {
    }

    //
    //  Get internal key name
    //
    for (line = s; *s && !isspace(*s); ++s) {
    }
    l = s - line;

    //
    //  Lookup internal names.
    //
    internal = AOHKString2Internal(line, l);
    if (internal == -1) {
	Debug(5, "Key '%.*s' not found\n",l, line);
	return;
    }

    Debug(1, "Key %d -> %d\n", key, internal);

    if (key > 0 && key < 256) {
	AOHKConvertTable[key] = internal;
    } else {
	Debug(5, "Key %d out of range\n", key);
    }
}

//
//      Parse sequence '   ->  ...'
//
static void AOHKParseOutput(char *line, OHKey * out)
{
    char *s;
    size_t l;
    int i;
    int key;
    int modifier;

    //
    //      Skip white spaces
    //
    for (s = line; *s && isspace(*s); ++s) {
    }
    //
    //      Need ->
    //
    if (strncmp(s, "->", 2)) {
	Debug(5, "'->' expected not '%s'\n", s);
	return;
    }
    s += 2;
    modifier = 0;
    key = KEY_RESERVED;

    //
    //  Parse all keys.
    //
  next:
    //
    //      Skip white spaces
    //
    for (; *s && isspace(*s); ++s) {
    }

    //
    //  Get internal key name
    //
    for (line = s; *s && !isspace(*s); ++s) {
    }
    l = s - line;

    //
    //  Special commands FIXME: not if modifier or qual
    //
    if (l == sizeof("qual") - 1
	&& !strncasecmp(line, "qual", sizeof("qual") - 1)) {
	modifier = QUAL;
	goto next;
    } else if (l == sizeof("sticky") - 1
	&& !strncasecmp(line, "sticky", sizeof("sticky") - 1)) {
	modifier = STICKY;
	goto next;
    } else if (l == sizeof("quote") - 1
	&& !strncasecmp(line, "quote", sizeof("quote") - 1)) {
	modifier = QUOTE;
    } else if (l == sizeof("reset") - 1
	&& !strncasecmp(line, "reset", sizeof("reset") - 1)) {
	modifier = RESET;
    } else if (l == sizeof("togame") - 1
	&& !strncasecmp(line, "togame", sizeof("togame") - 1)) {
	modifier = TOGAME;
    } else if (l == sizeof("special") - 1
	&& !strncasecmp(line, "special", sizeof("special") - 1)) {
	modifier = SPECIAL;
    } else if (l == sizeof("reserved") - 1
	&& !strncasecmp(line, "reserved", sizeof("reserved") - 1)) {
	modifier = 0;
    } else if (l) {
	i = AOHKString2Key(line, l);
	if (i == KEY_RESERVED) {	// Still not found giving up.
	    Debug(5, "Key '%.*s' not found\n",l, line);
	    return;
	}
	// Look if its a modifier
	if (modifier == QUAL || modifier == STICKY) {
	    switch (i) {
		case KEY_LEFTSHIFT:
		    key ^= Q_SHFT_L;
		    goto next;
		case KEY_LEFTCTRL:
		    key ^= Q_CTRL_L;
		    goto next;
		case KEY_LEFTALT:
		    key ^= Q_ALT_L;
		    goto next;
		case KEY_LEFTMETA:
		    key ^= Q_GUI_L;
		    goto next;
		case KEY_RIGHTSHIFT:
		    key ^= Q_SHFT_R;
		    goto next;
		case KEY_RIGHTCTRL:
		    key ^= Q_CTRL_R;
		    goto next;
		case KEY_RIGHTALT:
		    key ^= Q_ALT_R;
		    goto next;
		case KEY_RIGHTMETA:
		    key ^= Q_GUI_R;
		    goto next;
		default:
		    Debug(5, "Key '%s' not found\n", line);
		    return;
	    }
	} else if (*s && isspace(*s) && *s != '\n') {
	    // Last modifier in line is a normal key.
	    switch (i) {
		case KEY_LEFTSHIFT:
		    modifier ^= SHIFT;
		    goto next;
		case KEY_LEFTCTRL:
		    modifier ^= CTL;
		    goto next;
		case KEY_LEFTALT:
		    modifier ^= ALT;
		    goto next;
		case KEY_RIGHTALT:
		    modifier ^= ALTGR;
		    goto next;
		default:
		    key = i;
		    break;
	    }
	} else {
	    key = i;
	}
    }
    Debug(1, "Found %d, %d\n", modifier, key);
    if (out) {
	out->Modifier = modifier;
	out->KeyCode = key;
    }
    AOHKIsJunk(-1, s);
}

//
//      Parse mapping line.
//
static void AOHKParseMapping(char *line)
{
    char *s;
    int internal;
    size_t l;
    int macro;
    int quote;
    int super;

    macro = 0;
    quote = 0;
    super = 0;

    //
    //  Get input key code
    //
    for (s = line; *s && !isspace(*s); ++s) {
    }
    l = s - line;

    // Quoted game mode
    if (line[0] == '0' && line[1] == '*' && line[2] == '#') {
	internal = AOHKString2Internal(line + 3, l - 3);
	if (internal == -1) {
	    Debug(5, "Key '%s' not found\n", line);
	    return;
	}
	Debug(1, "Quoted game mode: %d\n", internal);
	AOHKParseOutput(s, AOHKQuoteGameTable + internal);
	return;
    }
    // Game mode '*#' internal key name
    if (line[0] == '*' && line[1] == '#') {
	internal = AOHKString2Internal(line + 2, l - 2);
	if (internal == -1) {
	    Debug(5, "Key '%s' not found\n", line);
	    return;
	}
	Debug(1, "Game mode: %d\n", internal);
	AOHKParseOutput(s, AOHKGameTable + internal);
	return;
    }
    // Macro key '*'
    if (line[0] == '*') {
	Debug(1, "Macro: ");
	macro = 1;
	++line;
	--l;
    }
    // Super Quote
    if (line[0] == '0' && line[1] == '#') {
	Debug(1, "Super quote ");
	super = 1;
	line += 2;
	l -= 2;
	// Quote
    } else if (line[0] == '0') {
	Debug(1, "Quote ");
	quote = 1;
	line += 1;
	l -= 1;
    }

    if (!l) {				// only 0#
	super = 0;
	internal = 90;
	goto parseon;
    }

    if (l == 1 && quote && line[0] == '0') {	// 00
	quote = 0;
	internal = 100;
	goto parseon;
    }

    if (l == 2) {
	if (line[0] == '0' && line[1] == '#') {
	    internal = 90;
	} else if (line[0] == '0' && line[1] == '0') {
	    internal = 100;
	} else if (line[0] >= '1' && line[0] <= '9' && line[1] >= '0'
	    && line[1] <= '9') {
	    internal = (line[0] - '1') * 10 + line[1] - '0';
	} else if (line[0] >= '1' && line[0] <= '9' && line[1] == '#') {
	    internal = 90 + line[0] - '0';
	} else {
	    Debug(5, "Illegal internal key '%s'\n", line);
	    return;
	}
	goto parseon;
    }

    if (l == 4 && line[0] == 'U' && line[1] == 'S' && line[2] == 'R') {
	if (line[3] < '1' || line[3] > '8') {
	    Debug(5, "Key '%s' not found\n", line);
	    return;
	}
	internal = 100 + line[3] - '0';
	goto parseon;
    }

    Debug(5, "Unsupported internal key sequence: %s", line);
    return;

  parseon:
    Debug(1, "Key %d\n", internal);
    if (macro && quote) {
	Debug(5, "Macro + quote not supported\n");
    } else if (macro && super) {
	Debug(5, "Macro + super not supported\n");
    } else if (macro) {
	AOHKParseOutput(s, (OHKey *) AOHKMacroTable[internal]);
    } else if (super) {
	AOHKParseOutput(s, AOHKSuperTable + internal);
    } else if (quote) {
	AOHKParseOutput(s, AOHKQuoteTable + internal);
    } else {
	AOHKParseOutput(s, AOHKTable + internal);
    }
}

//
//      Load key mapping.
//
void AOHKLoadTable(const char *file)
{
    FILE *fp;
    char buf[256];
    char *s;
    char *line;
    int linenr;
    enum
    { Nothing, Convert, Mapping, Macro } state;

    Debug(2, "Load keymap '%s'\n", file);

    if (!strcmp(file, "-")) {		// stdin
	fp = stdin;
    } else if (!(fp = fopen(file, "r"))) {
	Debug(5, "Can't open load file '%s'\n", file);
	return;
    }

    linenr = 0;
    state = Nothing;
    while (fgets(buf, sizeof(buf), fp)) {
	++linenr;
	//
	//      Skip leading white spaces
	//
	for (line = buf; *line && isspace(*line); ++line) {
	}
	if (!*line) {			// Empty line
	    continue;
	}
	//
	//      Remove comments // to end of line
	//
	for (s = line; *s; ++s) {
	    if (*s == '/' && s[1] == '/') {
		*s = '\0';
		break;
	    }
	}
	if (!*line) {			// Empty line
	    continue;
	}
	Debug(1, "Parse '%s'\n", line);
	if (!strncasecmp(line, "convert:", sizeof("convert:") - 1)) {
	    Debug(1, "'%s'\n", line);
	    state = Convert;
	    AOHKResetConvertTable();
	    AOHKIsJunk(linenr, line + sizeof("convert:") - 1);
	    continue;
	}
	if (!strncasecmp(line, "mapping:", sizeof("mapping:") - 1)) {
	    Debug(1, "'%s'\n", line);
	    state = Mapping;
	    AOHKResetMappingTable();
	    AOHKIsJunk(linenr, line + sizeof("mapping:") - 1);
	    continue;
	}
	if (!strncasecmp(line, "macro:", sizeof("macro:") - 1)) {
	    Debug(1, "'%s'\n", line);
	    state = Macro;
	    AOHKResetMacroTable();
	    AOHKIsJunk(linenr, line + sizeof("macro:") - 1);
	    continue;
	}
	switch (state) {
	    case Nothing:
		Debug(5, "%d: Need convert: or mapping: or macro: first\n",
		    linenr);
		break;
	    case Convert:
		AOHKParseConvert(line);
		break;
	    case Mapping:
	    case Macro:
		AOHKParseMapping(line);
		break;
	}
    }

    if (!linenr) {
	Debug(5, "Empty file '%s'\n", file);
    }

    if (strcmp(file, "-")) {		// !stdin
	fclose(fp);
    }
}
