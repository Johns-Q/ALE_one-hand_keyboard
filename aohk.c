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
#include <sys/types.h>

#include "aohk.h"

//
//      States
//      Bug alert: SecondKeys must be FirstKey+1!
//
enum
{
    OHFirstKey,				// waiting for first key
    OHSecondKey,			// waiting for second key
    OHQuotedFirstKey,			// have quote key, waiting for first key
    OHQuotedSecondKey,			// have quoted state, waiting for second key
    OHSuperFirstKey,			// have super quote key, waiting for first key
    OHSuperSecondKey,			// have super quoted state, waiting for second key
    OHGameMode,				// gamemode: key = key
    OHSpecial,				// have seen the special key
    OHSoftOff,				// turned off
    OHTotalOff				// 100% turned off
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
#define QUOTE	129			// quote state
#define RESET	130			// reset state
#define TOGAME	131			// enter game mode

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

/*
**	LED Macros for more hardware support
*/
#define SecondStateLedOn()		// led not used, only troubles
#define SecondStateLedOff()		// led not used, only troubles
#define QuoteStateLedOn()		// led not used, only troubles
#define QuoteStateLedOff()		// led not used, only troubles
#define GameStateLedOn()		// led not used, only troubles
#define GameStateLedOff()		// led not used, only troubles
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
/*1->0*/ { RESET,	0x00	},	// RESET
/*1->1*/ { 0,		0x02	},	// 1
/*1->2*/ { 0,		0x17	},	// i
/*1->3*/ { 0,		0x11	},	// w
/*1->4*/ { ALTGR,	0x1B	},	// ~
/*1->5*/ { 0,		0x13	},	// r
/*1->6*/ { 0,		0x2F	},	// v
/*1->7*/ { 0,		0x28	},	// ä
/*1->8*/ { 0,		0x20	},	// d
/*1->9*/ { 0,		0x10	},	// q
/*2->0*/ { RESET,	0x00	},	// RESET
/*2->1*/ { 0,		0x1C	},	// Return
/*2->2*/ { 0,		0x03	},	// 2
/*2->3*/ { 0,		0x34	},	// .
/*2->4*/ { QUAL,	Q_CTRL_L },	// Control_L
/*2->5*/ { 0,		0x29	},	// ^
/*2->6*/ { 0,		0x35	},	// -
/*2->7*/ { 0,		0x33	},	// ,
/*2->8*/ { 0,		0x27	},	// ö
/*2->9*/ { SHIFT,	0x02	},	// !
/*3->0*/ { RESET,	0x00	},	// RESET
/*3->1*/ { 0,		0x10	},	// q
/*3->2*/ { 0,		0x26	},	// l
/*3->3*/ { 0,		0x04	},	// 3
/*3->4*/ { SHIFT,	0x03	},	// "
/*3->5*/ { 0,		0x32	},	// m
/*3->6*/ { 0,		0x56	},	// <
/*3->7*/ { 0,		0x0D	},	// '
/*3->8*/ { 0,		0x21	},	// f
/*3->9*/ { 0,		0x1A	},	// ü
/*4->0*/ { RESET,	0x00	},	// RESET
/*4->1*/ { SHIFT,	0x05	},	// $
/*4->2*/ { 0,		0x1E	},	// a
/*4->3*/ { 0,		0x2C	},	// y
/*4->4*/ { 0,		0x05	},	// 4
/*4->5*/ { 0,		0x12	},	// e
/*4->6*/ { 0,		0x25	},	// k
/*4->7*/ { SHIFT,	0x06	},	// %
/*4->8*/ { 0,		0x1F	},	// s
/*4->9*/ { 0,		0x2D	},	// x
/*5->0*/ { RESET,	0x00	},	// RESET
/*5->1*/ { QUAL,	Q_SHFT_L },	// Shift_L
/*5->2*/ { ALTGR,	0x0C	},	// \ FUCK GNU (gcc >2.95.4 broken)
/*5->3*/ { SHIFT,	0x09	},	// (
/*5->4*/ { 0,		0x39	},	// Space
/*5->5*/ { 0,		0x06	},	// 5
/*5->6*/ { 0,		0x0F	},	// Tabulator
/*5->7*/ { QUAL,	Q_GUI_L },	// Gui_L
/*5->8*/ { ALTGR,	0x09	},	// [
/*5->9*/ { SHIFT,	0x0A	},	// )
/*6->0*/ { RESET,	0x00	},	// RESET
/*6->1*/ { SHIFT,	0x07	},	// &
/*6->2*/ { 0,		0x30	},	// b
/*6->3*/ { SHIFT,	0x56	},	// >
/*6->4*/ { SHIFT,	0x1B	},	// *
/*6->5*/ { 0,		0x18	},	// o
/*6->6*/ { 0,		0x07	},	// 6
/*6->7*/ { 0,		0x1B	},	// +
/*6->8*/ { 0,		0x16	},	// u
/*6->9*/ { ALTGR,	0x08	},	// {
/*7->0*/ { RESET,	0x00	},	// RESET
/*7->1*/ { 0,		0x0C	},	// ß
/*7->2*/ { 0,		0x23	},	// h
/*7->3*/ { SHIFT,	0x0B	},	// =
/*7->4*/ { SHIFT,	0x0C	},	// ?
/*7->5*/ { 0,		0x14	},	// t
/*7->6*/ { 0,		0x24	},	// j
/*7->7*/ { 0,		0x08	},	// 7
/*7->8*/ { 0,		0x31	},	// n
/*7->9*/ { 0,		0x15	},	// z
/*8->0*/ { RESET,	0x00	},	// RESET
/*8->1*/ { SHIFT,	0x33	},	// ;
/*8->2*/ { ALTGR,	0x10	},	// @
/*8->3*/ { SHIFT,	0x34	},	// :
/*8->4*/ { QUAL,	Q_ALT_L	},	// Alt_L
/*8->5*/ { ALTGR,	0x0A	},	// ]
/*8->6*/ { ALTGR,	0x56	},	// |
/*8->7*/ { 0,		0x0E	},	// BackSpace
/*8->8*/ { 0,		0x09	},	// 8
/*8->9*/ { 0,		0x01	},	// Escape
/*9->0*/ { RESET,	0x00	},	// RESET
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
};

/*
**	Table quoted sequences to scancodes.
**		The first value are the flags and modifiers.
**		The second value is the scancode to send.
*/
static OHKey AOHKQuotedTable[10*10+1+8] = {
/*1->0*/ { RESET,	0x00	},	// RESET
/*1->1*/ { 0,		0x3B	},	// F1
/*1->2*/ { SHIFT,	0x17	},	// I
/*1->3*/ { SHIFT,	0x11	},	// W
/*1->4*/ { 0,		0x45	},	// NUM-LOCK
/*1->5*/ { SHIFT,	0x13	},	// R
/*1->6*/ { SHIFT,	0x2F	},	// V
/*1->7*/ { SHIFT,	0x28	},	// Ä
/*1->8*/ { SHIFT,	0x20	},	// D
/*1->9*/ { SHIFT,	0x10	},	// Q
/*2->0*/ { RESET,	0x00	},	// RESET
/*2->1*/ { 0,		0x60	},	// KP-ENTER
/*2->2*/ { 0,		0x3C	},	// F2
/*2->3*/ { 0,		0x53	},	// KP-.
/*2->4*/ { QUAL,	Q_CTRL_R },	// Control_R
/*2->5*/ { 0,		0x3A	},	// CAPS-LOCK
/*2->6*/ { 0,		0x4A	},	// KP--
/*2->7*/ { 0,		0x53	},	// KP-,
/*2->8*/ { SHIFT,	0x27	},	// Ö
/*2->9*/ { 0,		0x52	},	// KP-0 (!not better place found)
/*3->0*/ { RESET,	0x00	},	// RESET
/*3->1*/ { SHIFT,	0x10	},	// Q
/*3->2*/ { SHIFT,	0x26	},	// L
/*3->3*/ { 0,		0x3D	},	// F3
/*3->4*/ { SHIFT,	0x04	},	// §
/*3->5*/ { SHIFT,	0x32	},	// M
/*3->6*/ { 0,		0x46	},	// SCROLL-LOCK
/*3->7*/ { 0,		0x2B	},	// ' (SHIFT #, doubled)
/*3->8*/ { SHIFT,	0x21	},	// F
/*3->9*/ { SHIFT,	0x1A	},	// Ü
/*4->0*/ { RESET,	0x00	},	// RESET
/*4->1*/ { SHIFT,	0x29	},	// °
/*4->2*/ { SHIFT,	0x1E	},	// A
/*4->3*/ { SHIFT,	0x2C	},	// Y
/*4->4*/ { 0,		0x3E	},	// F4
/*4->5*/ { SHIFT,	0x12	},	// E
/*4->6*/ { SHIFT,	0x25	},	// K
/*4->7*/ { 0,		0x62	},	// KP-% (doubled)
/*4->8*/ { SHIFT,	0x1F	},	// S
/*4->9*/ { SHIFT,	0x2D	},	// X
/*5->0*/ { RESET,	0x00	},	// RESET
/*5->1*/ { QUAL,	Q_SHFT_R },	// Shift_R
/*5->2*/ { ALTGR,	0x56	},	// | (QUOTE \, doubled)
/*5->3*/ { SHIFT,	0x09	},	// ( (FREE)
/*5->4*/ { SHIFT,	0x39	},	// SHIFT Space
/*5->5*/ { 0,		0x3F	},	// F5
/*5->6*/ { SHIFT,	0x0F	},	// SHIFT Tabulator
/*5->7*/ { QUAL,	Q_GUI_R },	// Gui_R
/*5->8*/ { ALTGR,	0x07	},	// { (QUOTE [, doubled)
/*5->9*/ { SHIFT,	0x0A	},	// ) (FREE)
/*6->0*/ { RESET,	0x00	},	// RESET
/*6->1*/ { ALTGR,	0x12	},	// ¤
/*6->2*/ { SHIFT,	0x30	},	// B
/*6->3*/ { ALTGR,	0x2E	},	// ¢
/*6->4*/ { 0,		0x37	},	// KP-*
/*6->5*/ { SHIFT,	0x18	},	// O
/*6->6*/ { 0,		0x40	},	// F6
/*6->7*/ { 0,		0x4E	},	// KP-+
/*6->8*/ { SHIFT,	0x16	},	// U
/*6->9*/ { ALTGR,	0x08	},	// { (FREE)
/*7->0*/ { RESET,	0x00	},	// RESET
/*7->1*/ { 0,		0x63	},	// PRINT/SYSRQ
/*7->2*/ { SHIFT,	0x23	},	// H
/*7->3*/ { 0,		0x44	},	// F10
/*7->4*/ { 0,		0x57	},	// F11
/*7->5*/ { SHIFT,	0x14	},	// T
/*7->6*/ { SHIFT,	0x24	},	// J
/*7->7*/ { 0,		0x41	},	// F7
/*7->8*/ { SHIFT,	0x31	},	// N
/*7->9*/ { SHIFT,	0x15	},	// Z
/*8->0*/ { RESET,	0x00	},	// RESET
/*8->1*/ { 0,		0x77	},	// PAUSE
/*8->2*/ { SHIFT,	0x0D	},	// ` (QUOTE @, doubled)
/*8->3*/ { 0,		0x65	},	// BREAK
/*8->4*/ { QUAL,	Q_ALT_R	},	// Alt_R
/*8->5*/ { ALTGR,	0x0B	},	// } (QUOTE ], doubled)
/*8->6*/ { 0,		0x54	},	// SYSRQ
/*8->7*/ { SHIFT,	0x0E	},	// SHIFT BackSpace
/*8->8*/ { 0,		0x42	},	// F8
/*8->9*/ { SHIFT,	0x01	},	// SHIFT Escape
/*9->0*/ { RESET,	0x00	},	// RESET
/*9->1*/ { 0,		0x7F	},	// MENU
/*9->2*/ { SHIFT,	0x19	},	// P
/*9->3*/ { 0,		0x58	},	// F12
/*9->4*/ { 0,		0x0E	},	// DEL 0x7F (QUOTE _, doubled)
/*9->5*/ { SHIFT,	0x22	},	// G
/*9->6*/ { ALTGR,	0x0B	},	// } (FREE)
/*9->7*/ { 0,		0x62	},	// KP-/
/*9->8*/ { SHIFT,	0x2E	},	// C
/*9->9*/ { 0,		0x43	},	// F9

/*0->#*/ { 0,		0	},	// Not possible! 00 is 0
/*1->#*/ { 0,		0x4F	},	// KP-1
/*2->#*/ { 0,		0x50	},	// KP-2
/*3->#*/ { 0,		0x51	},	// KP-3
/*4->#*/ { 0,		0x4B	},	// KP-4
/*5->#*/ { 0,		0x53	},	// KP-5
/*6->#*/ { 0,		0x4D	},	// KP-6
/*7->#*/ { 0,		0x47	},	// KP-7
/*8->#*/ { 0,		0x48	},	// KP-8
/*9->#*/ { 0,		0x49	},	// KP-9

/*0->0*/ { 0,		0	},	// Not possible! 00 is 0

/*USR1*/ { QUAL,	Q_SHFT_R },	// Shift_R
/*USR2*/ { QUAL,	Q_CTRL_R },	// Ctrl_R
/*USR3*/ { QUAL,	Q_ALT_R },	// Alt_R
/*USR4*/ { QUAL,	Q_GUI_R },	// Gui_R
/*USR5*/ { QUAL,	Q_SHFT_L },	// Shift_L
/*USR6*/ { QUAL,	Q_CTRL_L },	// Ctrl_L
/*USR7*/ { QUAL,	Q_ALT_L },	// Alt_L
/*USR8*/ { QUAL,	Q_GUI_L },	// Gui_L
};

/*
**	Table super quoted sequences to scancodes.
**	If we need more codes, they can be added here.
**		The first value are the flags and modifiers.
**		The second value is the scancode to send.
**	FIXME:	ALT? Keycode?
*/
static OHKey AOHKSuperTable[10*10+1+8] = {
/*1->0*/ { RESET,	0x00	},	// RESET
/*1->1*/ { ALT,		0x02	},	// 1
/*1->2*/ { ALT,		0x17	},	// i
/*1->3*/ { ALT,		0x11	},	// w
/*1->4*/ { ALTGR,	0x1B	},	// ~
/*1->5*/ { ALT,		0x13	},	// r
/*1->6*/ { ALT,		0x2F	},	// v
/*1->7*/ { ALT,		0x28	},	// ä
/*1->8*/ { ALT,		0x20	},	// d
/*1->9*/ { ALT,		0x10	},	// q
/*2->0*/ { RESET,	0x00	},	// RESET
/*2->1*/ { ALT,		0x1C	},	// Return
/*2->2*/ { ALT,		0x03	},	// 2
/*2->3*/ { ALT,		0x34	},	// .
/*2->4*/ { QUAL,	Q_CTRL_L },	// Control_L
/*2->5*/ { ALT,		0x29	},	// ^
/*2->6*/ { ALT,		0x35	},	// -
/*2->7*/ { ALT,		0x33	},	// ,
/*2->8*/ { ALT,		0x27	},	// ö
/*2->9*/ { ALT|SHIFT,	0x02	},	// !
/*3->0*/ { RESET,	0x00	},	// RESET
/*3->1*/ { ALT,		0x10	},	// q
/*3->2*/ { ALT,		0x26	},	// l
/*3->3*/ { ALT,		0x04	},	// 3
/*3->4*/ { ALT|SHIFT,	0x03	},	// "
/*3->5*/ { ALT,		0x32	},	// m
/*3->6*/ { ALT,		0x56	},	// <
/*3->7*/ { ALT,		0x0D	},	// '
/*3->8*/ { ALT,		0x21	},	// f
/*3->9*/ { ALT,		0x1A	},	// ü
/*4->0*/ { RESET,	0x00	},	// RESET
/*4->1*/ { ALT|SHIFT,	0x05	},	// $
/*4->2*/ { ALT,		0x1E	},	// a
/*4->3*/ { ALT,		0x2C	},	// y
/*4->4*/ { ALT,		0x05	},	// 4
/*4->5*/ { ALT,		0x12	},	// e
/*4->6*/ { ALT,		0x25	},	// k
/*4->7*/ { ALT|SHIFT,	0x06	},	// %
/*4->8*/ { ALT,		0x1F	},	// s
/*4->9*/ { ALT,		0x2D	},	// x
/*5->0*/ { RESET,	0x00	},	// RESET
/*5->1*/ { QUAL,	Q_SHFT_L },	// Shift_L
/*5->2*/ { ALTGR,	0x0C	},	// \ FUCK GNU (gcc >2.95.4 broken)
/*5->3*/ { ALT|SHIFT,	0x09	},	// (
/*5->4*/ { ALT,		0x39	},	// Space
/*5->5*/ { ALT,		0x06	},	// 5
/*5->6*/ { ALT,		0x0F	},	// Tabulator
/*5->7*/ { QUAL,	Q_GUI_L },	// Gui_L
/*5->8*/ { ALTGR,	0x09	},	// [
/*5->9*/ { ALT|SHIFT,	0x0A	},	// )
/*6->0*/ { RESET,	0x00	},	// RESET
/*6->1*/ { ALT|SHIFT,	0x07	},	// &
/*6->2*/ { ALT,		0x30	},	// b
/*6->3*/ { ALT|SHIFT,	0x56	},	// >
/*6->4*/ { ALT|SHIFT,	0x1B	},	// *
/*6->5*/ { ALT,		0x18	},	// o
/*6->6*/ { ALT,		0x07	},	// 6
/*6->7*/ { ALT,		0x1B	},	// +
/*6->8*/ { ALT,		0x16	},	// u
/*6->9*/ { ALTGR,	0x08	},	// {
/*7->0*/ { RESET,	0x00	},	// RESET
/*7->1*/ { ALT,		0x0C	},	// ß
/*7->2*/ { ALT,		0x23	},	// h
/*7->3*/ { ALT|SHIFT,	0x0B	},	// =
/*7->4*/ { ALT|SHIFT,	0x0C	},	// ?
/*7->5*/ { ALT,		0x14	},	// t
/*7->6*/ { ALT,		0x24	},	// j
/*7->7*/ { ALT,		0x08	},	// 7
/*7->8*/ { ALT,		0x31	},	// n
/*7->9*/ { ALT,		0x15	},	// z
/*8->0*/ { RESET,	0x00	},	// RESET
/*8->1*/ { ALT|SHIFT,	0x33	},	// ;
/*8->2*/ { ALTGR,	0x10	},	// @
/*8->3*/ { ALT|SHIFT,	0x34	},	// :
/*8->4*/ { QUAL,	Q_ALT_L	},	// Alt_L
/*8->5*/ { ALTGR,	0x0A	},	// ]
/*8->6*/ { ALTGR,	0x56	},	// |
/*8->7*/ { ALT,		0x0E	},	// BackSpace
/*8->8*/ { ALT,		0x09	},	// 8
/*8->9*/ { ALT,		0x01	},	// Escape
/*9->0*/ { RESET,	0x00	},	// RESET
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
};

/*
**	Game Table sequences to scancodes.
**		The first value are the flags and modifiers.
**		The second value is the scancode to send.
*/
static OHKey AOHKGameTable[OH_SPECIAL+1] = {
// This can't be used, quote is used insteed.
/* 0 */ { QUOTE,	0x39	},	// SPACE
/* 1 */ { 0,		0x2C	},	// y
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

/*NUM*/ { 0,		0x02	},	// 1
};

/*
**	Quoted Game Table sequences to scancodes.
*/
static OHKey AOHKQuotedGameTable[OH_SPECIAL+1] = {
/* 0 */ { 0,		0x39	},	// SPACE
/* 1 */ { 0,		0x2F	},	// v
/* 2 */ { 0,		0x30	},	// b
/* 3 */ { 0,		0x31	},	// n

/* 4 */ { 0,		0x21	},	// f
/* 5 */ { 0,		0x22	},	// g
/* 6 */ { 0,		0x23	},	// h

/* 7 */ { 0,		0x14	},	// t
/* 8 */ { 0,		0x15	},	// z
/* 9 */ { 0,		0x16	},	// u

/* # */ { RESET,	0x00	},	// Return to normal mode
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

/*
**	Game mode release key.
*/
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
	sequence = &AOHKQuotedGameTable[key];
	AOHKSendReleaseSequence(0, sequence);
	AOHKGameQuotePressed &= ~(1 << key);
    }
}

/*
**	Game mode release keys.
*/
static void AOHKGameModeReleaseAll(void)
{
    int i;

    for (i = 0; i <= OH_SPECIAL; ++i) {
	AOHKGameModeRelease(i);
    }
}

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
    GameStateLedOff();
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

//
//      Handle key sequence.
//
static void AOHKDoSequence(const OHKey * sequence)
{
    if (sequence->Modifier == RESET) {
	AOHKReset();
    } else if (sequence->Modifier == QUAL) {
	// FIXME: sticky qualifier
	AOHKDoModifier(sequence->KeyCode);
    } else {				// QUOTE or nothing
	AOHKSendPressSequence(AOHKLastModifier =
	    AOHKModifier, AOHKLastSequence = sequence);
	AOHKModifier = AOHKStickyModifier;
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
    return AOHKConvertTable[key] ? : -1;
}

//
//      Enter game mode
//
static void AOHKEnterGameMode(void)
{
    Debug(4, "Game mode on.\n");
    AOHKReset();			// release keys, ...
    AOHKState = OHGameMode;
    GameStateLedOn();
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
	    AOHKState = OHQuotedFirstKey;
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

	case OH_MACRO:			// word quote not yet supported
	    if (AOHKDownKeys & (1 << OH_REPEAT)) {	// game mode
		AOHKEnterGameMode();
		break;
	    }
	    // Act as CTRL key prefix.
	    AOHKDoModifier(Q_CTRL_L);
	    break;

	case OH_QUOTE:			// enter quote state
	    if (AOHKDownKeys & (1 << OH_REPEAT)) {	// super quote
		Debug(2, "super quote.\n");
		AOHKState = OHSuperFirstKey;
		QuoteStateLedOn();
		break;
	    }
	    AOHKState = OHQuotedFirstKey;
	    QuoteStateLedOn();
	    break;

	case OH_SPECIAL:		// enter special state
	    AOHKState = OHSpecial;
	    SpecialStateLedOn();
	    break;

	case OH_REPEAT:		// repeat last sequence
	    if (AOHKDownKeys & (1 << OH_MACRO)) {	// game mode
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
	    break;

	default:
	    if (AOHKDownKeys & (1 << OH_REPEAT)) {	// cursor mode?
		Debug(2, "cursor mode.\n");
		AOHKSendSequence(key, &AOHKTable[9 * 10 + key - OH_QUOTE]);
		break;
	    }
	    AOHKLastKey = key;
	    AOHKState = OHSecondKey;
	    SecondStateLedOn();
	    break;
    }
}

//
//      Handle the Super/Quoted Firstkey state.
//
static void AOHKQuotedFirstKey(int key)
{
    switch (key) {
	case OH_MACRO:			// word quote not yet supported
	    // QUOTE+MACRO free
	    // Act as CTRL Key prefix.
	    AOHKDoModifier(Q_CTRL_L);
	    return;

	case OH_QUOTE:			// double quote extra key
	    if (AOHKState == OHSuperFirstKey) {	// super quote quote
		AOHKSendSequence(key, &AOHKSuperTable[10 * 10]);
	    } else {
		AOHKSendSequence(key, &AOHKTable[10 * 10]);
	    }
	    break;

	case OH_SPECIAL:		// enter special state
	    AOHKState = OHSpecial;
	    SpecialStateLedOff();
	    return;

	case OH_REPEAT:		// quoted repeat extra key
	    if (AOHKDownKeys & (1 << OH_QUOTE)) {	// super quote
		Debug(2, "super quote.\n");
		AOHKState = OHSuperFirstKey;
		QuoteStateLedOn();
		break;
	    }
	    if (AOHKState == OHSuperFirstKey) {	// super quote quote
		AOHKSendSequence(key, &AOHKSuperTable[9 * 10]);
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
		    &AOHKQuotedTable[10 * 10 + 1 + key - OH_USR_1]);
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
//      Handle the Normal/Super/Quoted SecondKey state.
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
	AOHKState = OHQuotedSecondKey;
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
	case OHQuotedSecondKey:
	    sequence = &AOHKQuotedTable[n];
	    break;
	case OHSuperSecondKey:
	default:
	    sequence = &AOHKSuperTable[n];
	    break;
    }

    // This allows pressing first key and quote together.
    if (sequence->Modifier == QUOTE) {
	AOHKState = OHQuotedSecondKey;
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
    sequence = AOHKLastKey ? &AOHKQuotedGameTable[key] : &AOHKGameTable[key];

    //
    //  Reset is used to return to normal mode.
    //
    if (sequence->Modifier == RESET) {
	Debug(4, "Game mode off.\n");
	AOHKGameModeReleaseAll();
	AOHKState = OHFirstKey;
	GameStateLedOff();
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
	AOHKLastSequence = sequence;
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
    if (key == OH_QUOTE) {		// toggle left/right hand mode
	// FIXME: nolonger used
	// FIXME: swap QUOTE AND REPEAT keys.
    }
    if (key == OH_SPECIAL) {		// turn it off
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
	Debug(4, "Double timeout %d.\n", AOHKTimeout);
	return;
    }
    if (key == OH_KEY_3) {		// half timeout
	AOHKTimeout >>= 1;
	AOHKTimeout |= 1;
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
	AOHKState = OHTotalOff;
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
    if (AOHKState == OHTotalOff) {
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
	Debug(2, "Timeout %lu %lu\n", AOHKLastJiffies, timestamp);
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
	/*
	 **     Game mode, release and press aren't ordered.
	 */
	if (AOHKState == OHGameMode) {
	    /*
	     ** Game mode, quote delayed to release.
	     */
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
		    // FIXME: check if correct for modifiers?
		    // AOHKSendReleaseModifier(AOHKLastModifier);
		    // AOHKModifier = AOHKStickyModifier;
		    // AOHKRelease = 0;
		}
		if (AOHKRelease) {
		    Debug(5, "Release not done!\n");
		}
	    }
	}
	AOHKDownKeys &= ~(1 << key);
	return;
    } else if (AOHKDownKeys & (1 << key)) {	// repeating
	//
	//      internal key repeating, ignore
	//      (note: 1 repeated does nothing. 11 repeated does someting!)
	//      QUOTE is repeated in game mode, is this good?
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
	    AOHKFirstKey(key);
	    break;

	case OHSuperFirstKey:
	case OHQuotedFirstKey:
	    AOHKQuotedFirstKey(key);
	    break;

	case OHSecondKey:
	case OHQuotedSecondKey:
	case OHSuperSecondKey:
	    AOHKSecondKey(key);
	    break;

	case OHGameMode:
	    AOHKGameMode(key);
	    break;

	case OHSpecial:
	    AOHKSpecialMode(key);
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
    if (AOHKState != OHGameMode) {
	// Long time: total reset
	if (which > AOHKTimeout * 10) {
	    Debug(2, "Timeout long %d\n", which);
	    AOHKReset();
	    AOHKDownKeys = 0;
	}
	if (AOHKState != OHFirstKey) {
	    Debug(2, "Timeout short %d\n", which);
	    // FIXME: Check if leds are on!
	    SecondStateLedOff();
	    QuoteStateLedOff();

	    // Short time: only reset state
	    AOHKState = OHFirstKey;
	}
    }
}

//
//      Setup table which converts input keycodes to internal key symbols.
//
void AOHKSetupConvertTable(const int *table)
{
    int idx;

    while ((idx = *table++)) {
	if (idx > 0 && idx < 256) {
	    AOHKConvertTable[idx] = *table;
	}
	++table;
    }
}
