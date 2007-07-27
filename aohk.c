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
#include <stdlib.h>
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
static OHKey AOHKTable[10*10+1+8];

/*
**	Table quoted sequences to scancodes.
**		The first value are the flags and modifiers.
**		The second value is the scancode to send.
*/
static OHKey AOHKQuoteTable[10*10+1+8];

/*
**	Table super quoted sequences to scancodes.
**	If we need more codes, they can be added here.
**		The first value are the flags and modifiers.
**		The second value is the scancode to send.
**	FIXME:	ALT? Keycode?
*/
static OHKey AOHKSuperTable[10*10+1+8];

/*
**	Game Table sequences to scancodes.
**		The first value are the flags and modifiers.
**		The second value is the scancode to send.
*/
static OHKey AOHKGameTable[OH_SPECIAL+1] = {
#ifdef DEFAULT
// This can't be used, quote is used insteed.
/* 0 */ { QUOTE,	KEY_SPACE },	// SPACE
/* 1 */ { 0,		KEY_Z	},	// y
/* 2 */ { 0,		KEY_X	},	// x
/* 3 */ { 0,		KEY_C	},	// c

/* 4 */ { 0,		KEY_A	},	// a
/* 5 */ { 0,		KEY_S	},	// s
/* 6 */ { 0,		KEY_D	},	// d

/* 7 */ { 0,		KEY_Q	},	// q
/* 8 */ { 0,		KEY_W	},	// w
/* 9 */ { 0,		KEY_E	},	// e

/* # */ { 0,		KEY_LEFTCTRL },	// Ctrl_L
/* . */ { 0,		KEY_LEFTALT },	// Alt_L

/* % */ { 0,		KEY_2	},	// 2
/* * */ { 0,		KEY_3	},	// 3
/* - */ { 0,		KEY_4	},	// 4
/* + */ { 0,		KEY_R	},	// r

/* ? */ { 0,		KEY_F1	},	// F1
/* ? */ { 0,		KEY_F2	},	// F2
/* ? */ { 0,		KEY_F3	},	// F3
/* ? */ { 0,		KEY_F4	},	// F4

/*NUM*/ { 0,		KEY_1	},	// 1
#endif
};

/*
**	Quote Game Table sequences to scancodes.
*/
static OHKey AOHKQuoteGameTable[OH_SPECIAL+1] = {
#ifdef DEFAULT
/* 0 */ { 0,		KEY_SPACE },	// SPACE
/* 1 */ { 0,		KEY_V	},	// v
/* 2 */ { 0,		KEY_B	},	// b
/* 3 */ { 0,		KEY_N	},	// n

/* 4 */ { 0,		KEY_F	},	// f
/* 5 */ { 0,		KEY_G	},	// g
/* 6 */ { 0,		KEY_H	},	// h

/* 7 */ { 0,		KEY_T	},	// t
/* 8 */ { 0,		KEY_Y	},	// z
/* 9 */ { 0,		KEY_U	},	// u

/* # */ { RESET,	KEY_RESERVED },	// Return to normal mode
/* . */ { 0,		KEY_TAB	},	// TAB

/* % */ { 0,		KEY_6	},	// 6
/* * */ { 0,		KEY_7	},	// 7
/* - */ { 0,		KEY_8	},	// 8
/* + */ { 0,		KEY_I	},	// i

/* ? */ { 0,		KEY_F5	},	// F5
/* ? */ { 0,		KEY_F6	},	// F6
/* ? */ { 0,		KEY_F7	},	// F7
/* ? */ { 0,		KEY_F8	},	// F8

/*NUM*/ { 0,		KEY_5	},	// 5
#endif
};
//
//	*INDENT-ON*

//
//      Macro Table sequences to scancodes.
//
static unsigned char *AOHKMacroTable[10 * 10 + 1 + 8];

//
//      Macro Table quoted sequences to scancodes.
//
static unsigned char *AOHKMacroQuoteTable[10 * 10 + 1 + 8];

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
		// This directions didn't work good, better MACRO+REPEAT
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
	    // MACRO -> REPEAT
	    if (AOHKState == OHMacroFirstKey) {
		AOHKEnterGameMode();
		break;
	    }
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

    //
    //  Every not supported key, does a soft reset.
    //
    if (key == OH_MACRO || key == OH_SPECIAL || (OH_USR_1 <= key && key <= OH_USR_8)) {	// oops
	AOHKReset();
	return;
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
	case OHMacroQuoteSecondKey:
	    // FIXME: string
	    sequence = (const OHKey *)AOHKMacroQuoteTable[n];
	    break;
	case OHSuperSecondKey:
	default:
	    sequence = &AOHKSuperTable[n];
	    break;
    }

    // This allows pressing first key and quote together.
    if (sequence->Modifier == QUOTE) {
	Debug(2, "Quote as second key\n");
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
    if (AOHKState != OHGameMode && AOHKState != OHSoftOff
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

	if (AOHKMacroTable[idx]) {
	    free(AOHKMacroTable[idx]);
	}
	AOHKMacroTable[idx] = NULL;
    }

    for (idx = 0;
	idx < sizeof(AOHKMacroQuoteTable) / sizeof(*AOHKMacroQuoteTable);
	++idx) {

	if (AOHKMacroQuoteTable[idx]) {
	    free(AOHKMacroQuoteTable[idx]);
	}
	AOHKMacroQuoteTable[idx] = NULL;
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
    "Tab",

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
    "0x54",
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
	Debug(5, "Key '%.*s' not found\n", l, line);
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
	    Debug(5, "Key '%.*s' not found\n", l, line);
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
	if (!AOHKMacroTable[internal]) {
	    AOHKMacroTable[internal] = malloc(2);
	}
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

//----------------------------------------------------------------------------
//      Default Keymaps.
//----------------------------------------------------------------------------

//	*INDENT-OFF*

//
//	American keyboard layout default mapping.
//
static OHKey AOHKUsTable[10*10+1+8] = {
/*10*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*11*/ { 0,		KEY_1	},	// 1
/*12*/ { 0,		KEY_I	},	// i
/*13*/ { 0,		KEY_W	},	// w
/*14*/ { SHIFT,		KEY_GRAVE },	// ~
/*15*/ { 0,		KEY_R	},	// r
/*16*/ { 0,		KEY_V	},	// v
/*17*/ { 0,		KEY_A	},	// NO: ä
/*18*/ { 0,		KEY_D	},	// d
/*19*/ { RESET,		KEY_RESERVED },	// (FREE)
/*20*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*21*/ { 0,		KEY_ENTER },	// Return
/*22*/ { 0,		KEY_2	},	// 2
/*23*/ { 0,		KEY_DOT	},	// .
/*24*/ { QUAL,		Q_CTRL_L },	// Control_L
/*25*/ { SHIFT,		KEY_6 },	// ^
/*26*/ { 0,		KEY_MINUS },	// -
/*27*/ { 0,		KEY_COMMA },	// ,
/*28*/ { 0,		KEY_O },	// NO: ö
/*29*/ { SHIFT,		KEY_1	},	// !
/*30*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*31*/ { 0,		KEY_Q	},	// q
/*32*/ { 0,		KEY_L	},	// l
/*33*/ { 0,		KEY_3	},	// 3
/*34*/ { SHIFT,		KEY_APOSTROPHE },// "
/*35*/ { 0,		KEY_M	},	// m
/*36*/ { SHIFT,		KEY_COMMA },	// <
/*37*/ { 0,		KEY_APOSTROPHE },// '
/*38*/ { 0,		KEY_F	},	// f
/*39*/ { 0,		KEY_U	},	// NO: ü
/*40*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*41*/ { SHIFT,		KEY_4	},	// $
/*42*/ { 0,		KEY_A	},	// a
/*43*/ { 0,		KEY_Y	},	// y
/*44*/ { 0,		KEY_4	},	// 4
/*45*/ { 0,		KEY_E	},	// e
/*46*/ { 0,		KEY_K	},	// k
/*47*/ { SHIFT,		KEY_5	},	// %
/*48*/ { 0,		KEY_S	},	// s
/*49*/ { 0,		KEY_X	},	// x
/*50*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*51*/ { QUAL,		Q_SHFT_L },	// Shift_L
/*52*/ { 0,		KEY_BACKSLASH },// \ GNU idiotism
/*53*/ { SHIFT,		KEY_9	},	// (
/*54*/ { 0,		KEY_SPACE },	// Space
/*55*/ { 0,		KEY_5	},	// 5
/*56*/ { 0,		KEY_TAB	},	// Tabulator
/*57*/ { QUAL,		Q_GUI_L },	// Gui_L
/*58*/ { 0,		KEY_LEFTBRACE },// [
/*59*/ { SHIFT,		KEY_0	},	// )
/*60*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*61*/ { SHIFT,		KEY_7	},	// &
/*62*/ { 0,		KEY_B	},	// b
/*63*/ { SHIFT,		KEY_DOT },	// >
/*64*/ { SHIFT,		KEY_8 },	// *
/*65*/ { 0,		KEY_O	},	// o
/*66*/ { 0,		KEY_6	},	// 6
/*67*/ { SHIFT,		KEY_EQUAL },	// +
/*68*/ { 0,		KEY_U	},	// u
/*69*/ { SHIFT,		KEY_LEFTBRACE },// {
/*70*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*71*/ { 0,		KEY_S },	// NO: ß
/*72*/ { 0,		KEY_H	},	// h
/*73*/ { 0,		KEY_EQUAL },	// =
/*74*/ { SHIFT,		KEY_SLASH },	// ?
/*75*/ { 0,		KEY_T	},	// t
/*76*/ { 0,		KEY_J	},	// j
/*77*/ { 0,		KEY_7	},	// 7
/*78*/ { 0,		KEY_N	},	// n
/*79*/ { 0,		KEY_Z	},	// z
/*80*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*81*/ { 0,		KEY_SEMICOLON },// ;
/*82*/ { SHIFT,		KEY_2	},	// @
/*83*/ { SHIFT,		KEY_SEMICOLON },// :
/*84*/ { QUAL,		Q_ALT_L	},	// Alt_L
/*85*/ { 0,		KEY_RIGHTBRACE },// ]
/*86*/ { SHIFT,		KEY_BACKSLASH },// |
/*87*/ { 0,		KEY_BACKSPACE },// BackSpace
/*88*/ { 0,		KEY_8	},	// 8
/*89*/ { 0,		KEY_ESC	},	// Escape
/*90*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*91*/ { SHIFT,		KEY_3	 },	// #
/*92*/ { 0,		KEY_P	},	// p
/*93*/ { 0,		KEY_GRAVE },	// `
/*94*/ { SHIFT,		KEY_MINUS },	// _
/*95*/ { 0,		KEY_G	},	// g
/*96*/ { SHIFT,		KEY_RIGHTBRACE },// }
/*97*/ { 0,		KEY_SLASH },	// /
/*98*/ { 0,		KEY_C	},	// c
/*99*/ { 0,		KEY_9	},	// 9

/*0#*/ { 0,		KEY_INSERT },	// INSERT
/*1#*/ { 0,		KEY_END	},	// END
/*2#*/ { 0,		KEY_DOWN },	// CURSOR-DOWN
/*3#*/ { 0,		KEY_PAGEDOWN },	// PAGE-DOWN
/*4#*/ { 0,		KEY_LEFT },	// CURSOR-LEFT
/*5#*/ { 0,		KEY_DELETE },	// DELETE ( ,->5 )
/*6#*/ { 0,		KEY_RIGHT },	// CURSOR-RIGHT
/*7#*/ { 0,		KEY_HOME },	// HOME
/*8#*/ { 0,		KEY_UP	},	// CURSOR-UP
/*9#*/ { 0,		KEY_PAGEUP },	// PAGE-UP

/*00*/ { 0,		KEY_0	},	// 0

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

//
//	American keyboard layout default mapping.
//
static OHKey AOHKUsQuoteTable[10*10+1+8] = {
/*010*/ { RESET,	KEY_RESERVED },	// RESET
/*011*/ { 0,		KEY_F1	},	// F1
/*012*/ { SHIFT,	KEY_I	},	// I
/*013*/ { SHIFT,	KEY_W	},	// W
/*014*/ { 0,		KEY_NUMLOCK },	// NUM-LOCK
/*015*/ { SHIFT,	KEY_R	},	// R
/*016*/ { SHIFT,	KEY_V	},	// V
/*017*/ { SHIFT,	KEY_A	},	// NO: Ä
/*018*/ { SHIFT,	KEY_D	},	// D
/*019*/ { RESET,	KEY_RESERVED },	// (FREE)
/*020*/ { RESET,	KEY_RESERVED },	// RESET
/*021*/ { 0,		KEY_KPENTER },	// KP-ENTER
/*022*/ { 0,		KEY_F2	},	// F2
/*023*/ { 0,		KEY_KPDOT },	// KP-.
/*024*/ { QUAL,		Q_CTRL_R },	// Control_R
/*025*/ { 0,		KEY_CAPSLOCK },	// CAPS-LOCK
/*026*/ { 0,		KEY_KPMINUS },	// KP--
/*027*/ { 0,		KEY_KPDOT },	// KP-,
/*028*/ { SHIFT,	KEY_O	},	// NO: Ö
/*029*/ { 0,		KEY_KP0	},	// KP-0 (!no better place found)
/*030*/ { RESET,	KEY_RESERVED },	// RESET
/*031*/ { SHIFT,	KEY_Q	},	// Q
/*032*/ { SHIFT,	KEY_L	},	// L
/*033*/ { 0,		KEY_F3	},	// F3
/*034*/ { 0,		KEY_8	},	// NO: §
/*035*/ { SHIFT,	KEY_M	},	// M
/*036*/ { 0,		KEY_SCROLLLOCK },// SCROLL-LOCK
/*037*/ { 0,		KEY_APOSTROPHE },// ' (SHIFT #, doubled)
/*038*/ { SHIFT,	KEY_F	},	// F
/*039*/ { SHIFT,	KEY_U	},	// Ü
/*040*/ { RESET,	KEY_RESERVED },	// RESET
/*041*/ { 0,		KEY_0 },	// NO: °
/*042*/ { SHIFT,	KEY_A	},	// A
/*043*/ { SHIFT,	KEY_Y	},	// Y
/*044*/ { 0,		KEY_F4	},	// F4
/*045*/ { SHIFT,	KEY_E	},	// E
/*046*/ { SHIFT,	KEY_K	},	// K
/*047*/ { 0,		KEY_KPSLASH },	// KP-% (doubled)
/*048*/ { SHIFT,	KEY_S	},	// S
/*049*/ { SHIFT,	KEY_X	},	// X
/*050*/ { RESET,	KEY_RESERVED },	// RESET
/*051*/ { QUAL,		Q_SHFT_R },	// Shift_R
/*052*/ { SHIFT,	KEY_BACKSLASH},	// | (QUOTE \, doubled)
/*053*/ { SHIFT,	KEY_9	},	// ( (FREE)
/*054*/ { SHIFT,	KEY_SPACE },	// SHIFT Space
/*055*/ { 0,		KEY_F5	},	// F5
/*056*/ { SHIFT,	KEY_TAB	},	// SHIFT Tabulator
/*057*/ { QUAL,		Q_GUI_R },	// Gui_R
/*058*/ { SHIFT,	KEY_LEFTBRACE },// { (QUOTE [, doubled)
/*059*/ { SHIFT,	KEY_0	},	// ) (FREE)
/*060*/ { RESET,	KEY_RESERVED },	// RESET
/*061*/ { SHIFT,	KEY_E	},	// NO: ¤
/*062*/ { SHIFT,	KEY_B	},	// B
/*063*/ { SHIFT,	KEY_C	},	// NO: ¢
/*064*/ { 0,		KEY_KPASTERISK },// KP-*
/*065*/ { SHIFT,	KEY_O	},	// O
/*066*/ { 0,		KEY_F6	},	// F6
/*067*/ { 0,		KEY_KPPLUS },	// KP-+
/*068*/ { SHIFT,	KEY_U	},	// U
/*069*/ { SHIFT,	KEY_RIGHTBRACE},// { (FREE)
/*070*/ { RESET,	KEY_RESERVED },	// RESET
/*071*/ { 0,		KEY_SYSRQ },	// PRINT/SYSRQ
/*072*/ { SHIFT,	KEY_H	},	// H
/*073*/ { 0,		KEY_F10	},	// F10
/*074*/ { 0,		KEY_F11	},	// F11
/*075*/ { SHIFT,	KEY_T	},	// T
/*076*/ { SHIFT,	KEY_J	},	// J
/*077*/ { 0,		KEY_F7	},	// F7
/*078*/ { SHIFT,	KEY_N	},	// N
/*079*/ { SHIFT,	KEY_Z	},	// Z
/*080*/ { RESET,	KEY_RESERVED },	// RESET
/*081*/ { 0,		KEY_PAUSE },	// PAUSE
/*082*/ { 0,		KEY_GRAVE },	// ` (QUOTE @, doubled)
/*083*/ { CTL,		KEY_PAUSE },	// BREAK
/*084*/ { QUAL,		Q_ALT_R	},	// Alt_R
/*085*/ { SHIFT,	KEY_RIGHTBRACE },// } (QUOTE ], doubled)
/*086*/ { ALT,		KEY_SYSRQ },	// SYSRQ
/*087*/ { SHIFT,	KEY_BACKSPACE },// SHIFT BackSpace
/*088*/ { 0,		KEY_F8	},	// F8
/*089*/ { SHIFT,	KEY_ESC	},	// SHIFT Escape
/*090*/ { RESET,	KEY_RESERVED },	// RESET
/*091*/ { 0,		KEY_COMPOSE },	// MENU
/*092*/ { SHIFT,	KEY_P	},	// P
/*093*/ { 0,		KEY_F12	},	// F12
/*094*/ { 0,		KEY_RESERVED },	// (FREE)
/*095*/ { SHIFT,	KEY_G	},	// G
/*096*/ { SPECIAL,	KEY_RESERVED },	// ENTER SPECIAL MODE
/*097*/ { 0,		KEY_KPSLASH },	// KP-/
/*098*/ { SHIFT,	KEY_C	},	// C
/*099*/ { 0,		KEY_F9	},	// F9

/*00#*/ { 0,		KEY_RESERVED },	// Not possible! 00 is 0
/*01#*/ { 0,		KEY_KP1	},	// KP-1
/*02#*/ { 0,		KEY_KP2	},	// KP-2
/*03#*/ { 0,		KEY_KP3	},	// KP-3
/*04#*/ { 0,		KEY_KP4	},	// KP-4
/*05#*/ { 0,		KEY_KP5 },	// KP-5
/*06#*/ { 0,		KEY_KP6	},	// KP-6
/*07#*/ { 0,		KEY_KP7	},	// KP-7
/*08#*/ { 0,		KEY_KP8	},	// KP-8
/*09#*/ { 0,		KEY_KP9	},	// KP-9

/*000*/ { 0,		KEY_RESERVED },	// Not possible! 00 is 0

/*0USR1*/ { QUAL,	Q_SHFT_R },	// Shift_R
/*0USR2*/ { QUAL,	Q_CTRL_R },	// Ctrl_R
/*0USR3*/ { QUAL,	Q_ALT_R },	// Alt_R
/*0USR4*/ { QUAL,	Q_GUI_R },	// Gui_R
/*0USR5*/ { QUAL,	Q_SHFT_L },	// Shift_L
/*0USR6*/ { QUAL,	Q_CTRL_L },	// Ctrl_L
/*0USR7*/ { QUAL,	Q_ALT_L },	// Alt_L
/*0USR8*/ { QUAL,	Q_GUI_L },	// Gui_L
};

//
//	German keyboard layout default mapping.
//
static OHKey AOHKDeTable[10*10+1+8] = {
/*10*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*11*/ { 0,		KEY_1	},	// 1
/*12*/ { 0,		KEY_I	},	// i
/*13*/ { 0,		KEY_W	},	// w
/*14*/ { ALTGR,		KEY_RIGHTBRACE },// ~
/*15*/ { 0,		KEY_R	},	// r
/*16*/ { 0,		KEY_V	},	// v
/*17*/ { 0,		KEY_APOSTROPHE },// ä
/*18*/ { 0,		KEY_D	},	// d
/*19*/ { RESET,		KEY_RESERVED },	// (FREE)
/*20*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*21*/ { 0,		KEY_ENTER },	// Return
/*22*/ { 0,		KEY_2	},	// 2
/*23*/ { 0,		KEY_DOT	},	// .
/*24*/ { QUAL,		Q_CTRL_L },	// Control_L
/*25*/ { 0,		KEY_GRAVE },	// ^
/*26*/ { 0,		KEY_SLASH },	// -
/*27*/ { 0,		KEY_COMMA },	// ,
/*28*/ { 0,		KEY_SEMICOLON },// ö
/*29*/ { SHIFT,		KEY_1	},	// !
/*30*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*31*/ { 0,		KEY_Q	},	// q
/*32*/ { 0,		KEY_L	},	// l
/*33*/ { 0,		KEY_3	},	// 3
/*34*/ { SHIFT,		KEY_2	},	// "
/*35*/ { 0,		KEY_M	},	// m
/*36*/ { 0,		KEY_102ND },	// <
/*37*/ { 0,		KEY_EQUAL },	// '
/*38*/ { 0,		KEY_F	},	// f
/*39*/ { 0,		KEY_LEFTBRACE },// ü
/*40*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*41*/ { SHIFT,		KEY_4	},	// $
/*42*/ { 0,		KEY_A	},	// a
/*43*/ { 0,		KEY_Z	},	// y
/*44*/ { 0,		KEY_4	},	// 4
/*45*/ { 0,		KEY_E	},	// e
/*46*/ { 0,		KEY_K	},	// k
/*47*/ { SHIFT,		KEY_5	},	// %
/*48*/ { 0,		KEY_S	},	// s
/*49*/ { 0,		KEY_X	},	// x
/*50*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*51*/ { QUAL,		Q_SHFT_L },	// Shift_L
/*52*/ { ALTGR,		KEY_MINUS },	// \ FUCK GNU (gcc >2.95.4 broken)
/*53*/ { SHIFT,		KEY_8	},	// (
/*54*/ { 0,		KEY_SPACE },	// Space
/*55*/ { 0,		KEY_5	},	// 5
/*56*/ { 0,		KEY_TAB	},	// Tabulator
/*57*/ { QUAL,		Q_GUI_L },	// Gui_L
/*58*/ { ALTGR,		KEY_8	},	// [
/*59*/ { SHIFT,		KEY_9	},	// )
/*60*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*61*/ { SHIFT,		KEY_6	},	// &
/*62*/ { 0,		KEY_B	},	// b
/*63*/ { SHIFT,		KEY_102ND },	// >
/*64*/ { SHIFT,		KEY_RIGHTBRACE },// *
/*65*/ { 0,		KEY_O	},	// o
/*66*/ { 0,		KEY_6	},	// 6
/*67*/ { 0,		KEY_RIGHTBRACE },// +
/*68*/ { 0,		KEY_U	},	// u
/*69*/ { ALTGR,		KEY_7	},	// {
/*70*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*71*/ { 0,		KEY_MINUS },	// ß
/*72*/ { 0,		KEY_H	},	// h
/*73*/ { SHIFT,		KEY_0	},	// =
/*74*/ { SHIFT,		KEY_MINUS },	// ?
/*75*/ { 0,		KEY_T	},	// t
/*76*/ { 0,		KEY_J	},	// j
/*77*/ { 0,		KEY_7	},	// 7
/*78*/ { 0,		KEY_N	},	// n
/*79*/ { 0,		KEY_Y	},	// z
/*80*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*81*/ { SHIFT,		KEY_COMMA },	// ;
/*82*/ { ALTGR,		KEY_Q	},	// @
/*83*/ { SHIFT,		KEY_DOT	},	// :
/*84*/ { QUAL,		Q_ALT_L	},	// Alt_L
/*85*/ { ALTGR,		KEY_9	},	// ]
/*86*/ { ALTGR,		KEY_102ND },	// |
/*87*/ { 0,		KEY_BACKSPACE },// BackSpace
/*88*/ { 0,		KEY_8	},	// 8
/*89*/ { 0,		KEY_ESC	},	// Escape
/*90*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*91*/ { 0,		KEY_BACKSLASH },// #
/*92*/ { 0,		KEY_P	},	// p
/*93*/ { SHIFT,		KEY_EQUAL },	// `
/*94*/ { SHIFT,		KEY_SLASH },	// _
/*95*/ { 0,		KEY_G	},	// g
/*96*/ { ALTGR,		KEY_0	},	// }
/*97*/ { SHIFT,		KEY_7	},	// /
/*98*/ { 0,		KEY_C	},	// c
/*99*/ { 0,		KEY_9	},	// 9

/*0#*/ { 0,		KEY_INSERT },	// INSERT
/*1#*/ { 0,		KEY_END	},	// END
/*2#*/ { 0,		KEY_DOWN },	// CURSOR-DOWN
/*3#*/ { 0,		KEY_PAGEDOWN },	// PAGE-DOWN
/*4#*/ { 0,		KEY_LEFT },	// CURSOR-LEFT
/*5#*/ { 0,		KEY_DELETE },	// DELETE ( ,->5 )
/*6#*/ { 0,		KEY_RIGHT },	// CURSOR-RIGHT
/*7#*/ { 0,		KEY_HOME },	// HOME
/*8#*/ { 0,		KEY_UP	},	// CURSOR-UP
/*9#*/ { 0,		KEY_PAGEUP },	// PAGE-UP

/*00*/ { 0,		KEY_0	},	// 0

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

//
//	German keyboard layout default mapping.
//
static OHKey AOHKDeQuoteTable[10*10+1+8] = {
/*010*/ { RESET,	KEY_RESERVED },	// RESET
/*011*/ { 0,		KEY_F1	},	// F1
/*012*/ { SHIFT,	KEY_I	},	// I
/*013*/ { SHIFT,	KEY_W	},	// W
/*014*/ { 0,		KEY_NUMLOCK },	// NUM-LOCK
/*015*/ { SHIFT,	KEY_R	},	// R
/*016*/ { SHIFT,	KEY_V	},	// V
/*017*/ { SHIFT,	KEY_APOSTROPHE },// Ä
/*018*/ { SHIFT,	KEY_D	},	// D
/*019*/ { RESET,	KEY_RESERVED },	// (FREE)
/*020*/ { RESET,	KEY_RESERVED },	// RESET
/*021*/ { 0,		KEY_KPENTER },	// KP-ENTER
/*022*/ { 0,		KEY_F2	},	// F2
/*023*/ { 0,		KEY_KPDOT },	// KP-.
/*024*/ { QUAL,		Q_CTRL_R },	// Control_R
/*025*/ { 0,		KEY_CAPSLOCK },	// CAPS-LOCK
/*026*/ { 0,		KEY_KPMINUS },	// KP--
/*027*/ { 0,		KEY_KPDOT },	// KP-,
/*028*/ { SHIFT,	KEY_SEMICOLON },// Ö
/*029*/ { 0,		KEY_KP0	},	// KP-0 (!no better place found)
/*030*/ { RESET,	KEY_RESERVED },	// RESET
/*031*/ { SHIFT,	KEY_Q	},	// Q
/*032*/ { SHIFT,	KEY_L	},	// L
/*033*/ { 0,		KEY_F3	},	// F3
/*034*/ { SHIFT,	KEY_3	},	// §
/*035*/ { SHIFT,	KEY_M	},	// M
/*036*/ { 0,		KEY_SCROLLLOCK },// SCROLL-LOCK
/*037*/ { 0,		KEY_BACKSLASH },// ' (SHIFT #, doubled)
/*038*/ { SHIFT,	KEY_F	},	// F
/*039*/ { SHIFT,	KEY_LEFTBRACE },// Ü
/*040*/ { RESET,	KEY_RESERVED },	// RESET
/*041*/ { SHIFT,	KEY_GRAVE },	// °
/*042*/ { SHIFT,	KEY_A	},	// A
/*043*/ { SHIFT,	KEY_Z	},	// Y
/*044*/ { 0,		KEY_F4	},	// F4
/*045*/ { SHIFT,	KEY_E	},	// E
/*046*/ { SHIFT,	KEY_K	},	// K
/*047*/ { 0,		KEY_KPSLASH },	// KP-% (doubled)
/*048*/ { SHIFT,	KEY_S	},	// S
/*049*/ { SHIFT,	KEY_X	},	// X
/*050*/ { RESET,	KEY_RESERVED },	// RESET
/*051*/ { QUAL,		Q_SHFT_R },	// Shift_R
/*052*/ { ALTGR,	KEY_102ND },	// | (QUOTE \, doubled)
/*053*/ { SHIFT,	KEY_8	},	// ( (FREE)
/*054*/ { SHIFT,	KEY_SPACE },	// SHIFT Space
/*055*/ { 0,		KEY_F5	},	// F5
/*056*/ { SHIFT,	KEY_TAB	},	// SHIFT Tabulator
/*057*/ { QUAL,		Q_GUI_R },	// Gui_R
/*058*/ { ALTGR,	KEY_6	},	// { (QUOTE [, doubled)
/*059*/ { SHIFT,	KEY_9	},	// ) (FREE)
/*060*/ { RESET,	KEY_RESERVED },	// RESET
/*061*/ { ALTGR,	KEY_E	},	// ¤
/*062*/ { SHIFT,	KEY_B	},	// B
/*063*/ { ALTGR,	KEY_C	},	// ¢
/*064*/ { 0,		KEY_KPASTERISK },// KP-*
/*065*/ { SHIFT,	KEY_O	},	// O
/*066*/ { 0,		KEY_F6	},	// F6
/*067*/ { 0,		KEY_KPPLUS },	// KP-+
/*068*/ { SHIFT,	KEY_U	},	// U
/*069*/ { ALTGR,	KEY_7	},	// { (FREE)
/*070*/ { RESET,	KEY_RESERVED },	// RESET
/*071*/ { 0,		KEY_SYSRQ },	// PRINT/SYSRQ
/*072*/ { SHIFT,	KEY_H	},	// H
/*073*/ { 0,		KEY_F10	},	// F10
/*074*/ { 0,		KEY_F11	},	// F11
/*075*/ { SHIFT,	KEY_T	},	// T
/*076*/ { SHIFT,	KEY_J	},	// J
/*077*/ { 0,		KEY_F7	},	// F7
/*078*/ { SHIFT,	KEY_N	},	// N
/*079*/ { SHIFT,	KEY_Y	},	// Z
/*080*/ { RESET,	KEY_RESERVED },	// RESET
/*081*/ { 0,		KEY_PAUSE },	// PAUSE
/*082*/ { SHIFT,	KEY_EQUAL },	// ` (QUOTE @, doubled)
/*083*/ { CTL,		KEY_PAUSE },	// BREAK
/*084*/ { QUAL,		Q_ALT_R	},	// Alt_R
/*085*/ { ALTGR,	KEY_0	},	// } (QUOTE ], doubled)
/*086*/ { ALT,		KEY_SYSRQ },	// SYSRQ
/*087*/ { SHIFT,	KEY_BACKSPACE },// SHIFT BackSpace
/*088*/ { 0,		KEY_F8	},	// F8
/*089*/ { SHIFT,	KEY_ESC	},	// SHIFT Escape
/*090*/ { RESET,	KEY_RESERVED },	// RESET
/*091*/ { 0,		KEY_COMPOSE },	// MENU
/*092*/ { SHIFT,	KEY_P	},	// P
/*093*/ { 0,		KEY_F12	},	// F12
/*094*/ { 0,		KEY_RESERVED },	// (FREE)
/*095*/ { SHIFT,	KEY_G	},	// G
/*096*/ { SPECIAL,	KEY_RESERVED },	// ENTER SPECIAL MODE
/*097*/ { 0,		KEY_KPSLASH },	// KP-/
/*098*/ { SHIFT,	KEY_C	},	// C
/*099*/ { 0,		KEY_F9	},	// F9

/*00#*/ { 0,		KEY_RESERVED },	// Not possible! 00 is 0
/*01#*/ { 0,		KEY_KP1	},	// KP-1
/*02#*/ { 0,		KEY_KP2	},	// KP-2
/*03#*/ { 0,		KEY_KP3	},	// KP-3
/*04#*/ { 0,		KEY_KP4	},	// KP-4
/*05#*/ { 0,		KEY_KP5 },	// KP-5
/*06#*/ { 0,		KEY_KP6	},	// KP-6
/*07#*/ { 0,		KEY_KP7	},	// KP-7
/*08#*/ { 0,		KEY_KP8	},	// KP-8
/*09#*/ { 0,		KEY_KP9	},	// KP-9

/*000*/ { 0,		KEY_RESERVED },	// Not possible! 00 is 0

/*0USR1*/ { QUAL,	Q_SHFT_R },	// Shift_R
/*0USR2*/ { QUAL,	Q_CTRL_R },	// Ctrl_R
/*0USR3*/ { QUAL,	Q_ALT_R },	// Alt_R
/*0USR4*/ { QUAL,	Q_GUI_R },	// Gui_R
/*0USR5*/ { QUAL,	Q_SHFT_L },	// Shift_L
/*0USR6*/ { QUAL,	Q_CTRL_L },	// Ctrl_L
/*0USR7*/ { QUAL,	Q_ALT_L },	// Alt_L
/*0USR8*/ { QUAL,	Q_GUI_L },	// Gui_L
};

//	*INDENT-ON*

//
//      Setup compiled keyboard mappings.
//
void AOHKSetLanguage(const char *lang)
{
    unsigned i;
    unsigned char *s;

    Debug(2, "Set Language '%s'\n", lang);
    if (!strcmp("de", lang)) {
	memcpy(AOHKTable, AOHKDeTable, sizeof(AOHKTable));
	memcpy(AOHKQuoteTable, AOHKDeQuoteTable, sizeof(AOHKQuoteTable));
    } else if (!strcmp("us", lang)) {
	memcpy(AOHKTable, AOHKUsTable, sizeof(AOHKTable));
	memcpy(AOHKQuoteTable, AOHKUsQuoteTable, sizeof(AOHKQuoteTable));
    } else {
	Debug(5, "Language '%s' isn't suported\n", lang);
	return;
    }

    //
    //  Convert Normal table into macro table. Adding Ctrl
    //
    for (i = 0; i < sizeof(AOHKTable) / sizeof(*AOHKTable); ++i) {
	s = malloc(2);
	switch (AOHKTable[i].Modifier) {
	    case RESET:
	    case QUOTE:
	    case QUAL:
	    case STICKY:
	    case TOGAME:
	    case SPECIAL:
		s[0] = AOHKTable[i].Modifier;
		break;
	    default:
		s[0] = AOHKTable[i].Modifier ^ CTL;
		break;
	}
	s[1] = AOHKTable[i].KeyCode;
	AOHKMacroTable[i] = s;
    }
    //
    //  Convert Normal table into super quote table. Adding Alt
    //
    for (i = 0; i < sizeof(AOHKTable) / sizeof(*AOHKTable); ++i) {
	s = (unsigned char *)&AOHKSuperTable[i];
	switch (AOHKTable[i].Modifier) {
	    case RESET:
	    case QUOTE:
	    case QUAL:
	    case STICKY:
	    case TOGAME:
	    case SPECIAL:
		s[0] = AOHKTable[i].Modifier;
		break;
	    default:
		s[0] = AOHKTable[i].Modifier ^ ALT;
		break;
	}
	s[1] = AOHKTable[i].KeyCode;
    }
}
