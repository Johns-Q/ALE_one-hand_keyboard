///
///	@file aohk.c	@brief	ALE OneHand Keyboard handler.
///
///	Copyright (c) 2007,2009 by Lutz Sammer.	 All Rights Reserved.
///
///	Contributor(s):
///
///	This file is part of ALE one-hand keyboard
///
///	This program is free software; you can redistribute it and/or modify
///	it under the terms of the GNU General Public License as published by
///	the Free Software Foundation; only version 2 of the License.
///
///	This program is distributed in the hope that it will be useful,
///	but WITHOUT ANY WARRANTY; without even the implied warranty of
///	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
///	GNU General Public License for more details.
///
///	$Id$
////////////////////////////////////////////////////////////////////////////

///
///	@defgroup aohk The aohk module.
///
///	Can be used to add single hand (finger) input to any program.
///	Or to add alphanumeric input to any device (like remotes)
///
///	@par Usage:
///		Call AOHKSetLanguage() or/and AOHKLoadTable() to setup
///		sequence tables.
///
///		Call AOHKFeedKey() to let aohk handle raw scan codes.
///		Or call AOHKFeedSymbol() to let aohk handle internal symbols.
///		AOHKFeedTimeout() to tell aohk that a timeout happend.
///
///		AOHKKeyOut() is called from aohk for key press or releases.
///		AOHKShowLED() is called from aohk to show different states.
///
///	@todo
///		- more language support (please send mappings).
///		- support sticky qualifiers.
///		- build a shared library.
///		- chinese support.
///		- Macro aren't complete supported.
///		- Command to record macros.
///		- very long term: support multiple instance.
///
/// @{

#include <linux/input.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#include "aohk.h"

#ifndef NODEFAULT
#define NODEFAULT			///< define to exclude default tables
#define DEFAULT				///< define to include default tables
#endif

////////////////////////////////////////////////////////////////////////////

///
///	AOHK internal states.
///	To convert the key code sequence to real keys a state machine is used.
///
///	@warning Bug alert: ...SecondKeys must be FirstKey+1!
///
enum __aohk_states__
{
    OHFirstKey,				///< waiting for first key
    OHSecondKey,			///< waiting for second key
    OHQuoteFirstKey,			///< have quote key, waiting for first key
    OHQuoteSecondKey,			///< have quote state, waiting for second key
    OHSuperFirstKey,			///< have super quote key, waiting for first key
    OHSuperSecondKey,			///< have super quoted state, waiting for second key
    OHMacroFirstKey,			///< have macro key, ...
    OHMacroSecondKey,			///< macro key, first key ...
    OHMacroQuoteFirstKey,		///< have macro key, ...
    OHMacroQuoteSecondKey,		///< macro key, first key ...

    OHGameMode,				///< game mode: key = game key
    OHNumberMode,			///< number mode: key = number key
    OHSpecial,				///< have seen the special key
    OHSoftOff,				///< turned off
    OHHardOff				///< 100% turned off
};

///
///	Key mapping typedef
///
typedef struct _oh_key_ OHKey;

///
///	Key mapping structure
///
///	Defines the generated scan codes
///
///	@see Modifiers Qualifiers
///
struct _oh_key_
{
    unsigned char Modifier;		///< modifier flags
    unsigned char KeyCode;		///< scancode
};

static char AOHKState;			///< state machine

static char AOHKOnlyMe;			///< enable only my keys

static int AOHKDownKeys;		///< bitmap pressed keys
static unsigned char AOHKLastKey;	///< last scancode got

//
//	Sequences
//
static unsigned char AOHKRelease;	///< release must be send

static unsigned char AOHKStickyModifier;	///< sticky modifiers
static unsigned char AOHKModifier;	///< modifiers wanted

static unsigned char AOHKLastModifier;	///< last key modifiers
static const OHKey *AOHKLastSequence;	///< last keycode

///
///	Table: Maps input keycodes to internal keys.
///
static unsigned char AOHKConvertTable[256];

// ---------------------------------------------------

///
///	@name Modifiers
///
///	Keyboard mapping modifier. used in OHKey.Modifier to extend or change
///	the meaning of OhKey.Keycode.
///
///	- 0x00 - 0x0F modifier #SHIFT ... and keycode
///	- 0x10 0x20 0x40 free
///	- 0x80 commands
///	- 0xC0 macro index
///
///	@see OHKey.Modifier
//@{
#define SHIFT	1			///< keycode + SHIFT
#define CTL	2			///< keycode + CTRL
#define ALT	4			///< keycode + ALT
#define ALTGR	8			///< keycode + ALT-GR

#define QUAL	128			///< no keycode only qualifiers
#define STICKY	129			///< sticky qualifiers
#define QUOTE	130			///< quote state
#define RESET	131			///< reset state
#define TOGAME	132			///< enter game mode
#define TONUM	133			///< enter number mode
#define SPECIAL	134			///< enter special mode

#define MACRO	0xC0			///< macro index

//@}

///
///	@name Qualifiers
///
///	Modifier bitmap for #QUAL/#STICKY command.
///
///	@see OHKey.KeyCode
//@{
#define Q_SHFT_L	1		///< shift left
#define Q_CTRL_L	2		///< control left
#define Q_ALT_L		4		///< alt left
#define Q_GUI_L		8		///< gui (flag) left
#define Q_SHFT_R	16		///< shift right
#define Q_CTRL_R	32		///< control right
#define Q_ALT_R		64		///< alt right
#define Q_GUI_R		128		///< gui (flag) right
//@}

static char AOHKGameSendQuote;		///< send delayed quote in game mode

static unsigned AOHKGamePressed;	///< keys pressed in game mode
static unsigned AOHKGameQuotePressed;	///< quoted keys pressed in game mode

#define AOHK_TIMEOUT	(1*1000)	///< default 1s timeout
static int AOHKTimeBase = AOHK_TIMEOUT;	///< timeout in ms ticks

static unsigned long AOHKLastTick;	///< last key ms tick

//
//	LED Macros for more hardware support
//
//#define SecondStateLedOn()		///< led not used, only troubles
//#define SecondStateLedOff()		///< led not used, only troubles
//#define QuoteStateLedOn()		///< led not used, only troubles
//#define QuoteStateLedOff()		///< led not used, only troubles
//#define GameModeLedOn()		///< led not used, only troubles
//#define GameModeLedOff()		///< led not used, only troubles
#define NumberModeLedOn()		///< led not used, only troubles
#define NumberModeLedOff()		///< led not used, only troubles
#define SpecialStateLedOn()		///< led not used, only troubles
#define SpecialStateLedOff()		///< led not used, only troubles

static void AOHKEnterGameMode(void);	// forward definition
static void AOHKEnterNumberMode(void);	// forward definition
static void AOHKEnterSpecialState(void);	// forward definition

#define HASH_START	(9 * 10)	///< x# sequences, index into table
#define STAR_START	(10 * 10)	///< x* sequences, index into table
#define DOUBLE_QUOTE	(11 * 10)	///< 00 sequence, index into table
#define USR_START	(11 * 10 + 1)	///< USR sequences, index into table

///
///	Table sequences to scancodes.
///		The first value are the flags and modifiers.
///		The second value is the scancode to send.
///		Comments are the ascii output of the keyboard driver.
///
static OHKey AOHKTable[11 * 10 + 1 + 8];

///
///	Table quoted sequences to scancodes.
///		The first value are the flags and modifiers.
///		The second value is the scancode to send.
//
static OHKey AOHKQuoteTable[11 * 10 + 1 + 8];

///
///	Table super quoted sequences to scancodes.
///	If we need more codes, they can be added here.
///		The first value are the flags and modifiers.
///		The second value is the scancode to send.
///
static OHKey AOHKSuperTable[11 * 10 + 1 + 8];

///
///	Game table key to scancodes.
///		The first value are the flags and modifiers.
///		The second value is the scancode to send.
///
static OHKey AOHKGameTable[AOHK_KEY_SPECIAL + 1] = {
#ifdef DEFAULT
//	*INDENT-OFF*
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
//	*INDENT-ON*
#endif
};

///
///	Game table quoted key to scancodes.
///
static OHKey AOHKQuoteGameTable[AOHK_KEY_SPECIAL + 1] = {
#ifdef DEFAULT
//	*INDENT-OFF*
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
//	*INDENT-ON*
#endif
};

///
///	Number table key to scancodes.
///		The first value are the flags and modifiers.
///		The second value is the scancode to send.
///
static OHKey AOHKNumberTable[AOHK_KEY_SPECIAL + 1] = {
#ifdef DEFAULT
//	*INDENT-OFF*
/* * */ { 0,		KEY_KPENTER },	// ENTER
/* 1 */ { 0,		KEY_KP1	},	// 1
/* 2 */ { 0,		KEY_KP2	},	// 2
/* 3 */ { 0,		KEY_KP3	},	// 3

/* 4 */ { 0,		KEY_KP4	},	// 4
/* 5 */ { 0,		KEY_KP5	},	// 5
/* 6 */ { 0,		KEY_KP6	},	// 6

/* 7 */ { 0,		KEY_KP7	},	// 7
/* 8 */ { 0,		KEY_KP8	},	// 8
/* 9 */ { 0,		KEY_KP9	},	// 9

/* # */ { 0,		KEY_KP0},	// 0
/* . */ { 0,		KEY_KPDOT },	// .

/* % */ { 0,		KEY_KPSLASH },	// %
/* * */ { 0,		KEY_KPASTERISK},// *
/* - */ { 0,		KEY_KPMINUS },	// -
/* + */ { 0,		KEY_KPPLUS },	// +

/* ? */ { 0,		KEY_ESC	},	// ESC
/* ? */ { 0,		KEY_LEFTCTRL },	// CTRL
/* ? */ { 0,		KEY_LEFTALT },	// ALT
/* ? */ { 0,		KEY_BACKSPACE },// BSP

/*NUM*/ { RESET,	KEY_RESERVED },	// Return to normal mode
//	*INDENT-ON*
#endif
};

///
///	Macro table sequences to scancodes.
///
static OHKey AOHKMacroTable[11 * 10 + 1 + 8];

///
///	Macro table quoted sequences to scancodes.
///
static OHKey AOHKMacroQuoteTable[11 * 10 + 1 + 8];

///
///	Macro storage table.
///
///	Any of the above tables can contain macros, there are only index into
///	this table stored.
///
///	currently max. 256 different macros are supported.
///
///	It is possible to extend too 64*256 macros, using the low bits of
///	MACRO modifier.
///
static OHKey *AOHKMacros[256];

//----------------------------------------------------------------------------
//	Debug
//----------------------------------------------------------------------------

extern int DebugLevel;			///< in: debug level

///
///	Debug output function.
///
///	@param level	debug level (0: errors, 1: warnings, 2: infos: 3: ...)
///	@param fmt	printf like format string
///	@param ...	printf like arguments
///
#define Debug(level, fmt...) \
    do { if (level<DebugLevel) { printf(fmt); } } while (0)

//----------------------------------------------------------------------------
//	Send
//----------------------------------------------------------------------------

///
///	Send modifier press.
///
///	@param modifier is a bitfield of modifier
///
///	@see Q_SHFT_L Q_SHFT_R Q_CTRL_L Q_CTRL_R Q_ALT_L Q_ALT_R Q_GUI_L
///		Q_GUI_R
///
static void AOHKSendPressModifier(int modifier)
{
    if (modifier & Q_SHFT_L) {
	AOHKKeyOut(KEY_LEFTSHIFT, 1);
    }
    if (modifier & Q_SHFT_R) {
	AOHKKeyOut(KEY_RIGHTSHIFT, 1);
    }
    if (modifier & Q_CTRL_L) {
	AOHKKeyOut(KEY_LEFTCTRL, 1);
    }
    if (modifier & Q_CTRL_R) {
	AOHKKeyOut(KEY_RIGHTCTRL, 1);
    }
    if (modifier & Q_ALT_L) {
	AOHKKeyOut(KEY_LEFTALT, 1);
    }
    if (modifier & Q_ALT_R) {
	AOHKKeyOut(KEY_RIGHTALT, 1);
    }
    if (modifier & Q_GUI_L) {
	AOHKKeyOut(KEY_LEFTMETA, 1);
    }
    if (modifier & Q_GUI_R) {
	AOHKKeyOut(KEY_RIGHTMETA, 1);
    }
}

///
///	Send an internal sequence as press events to input emulation.
///
///	@param modifier is a bitfield of modifier
///	@param sequence output key definition
///
static void AOHKSendPressSequence(int modifier, const OHKey * sequence)
{
    // First all modifieres
    if (modifier) {
	AOHKSendPressModifier(modifier);
    }
    // This are special qualifiers
    if (sequence->Modifier && sequence->Modifier ^ 0x80) {
	if (sequence->Modifier & ALTGR && !(modifier & Q_ALT_R)) {
	    AOHKKeyOut(KEY_RIGHTALT, 1);
	}
	if (sequence->Modifier & ALT && !(modifier & Q_ALT_L)) {
	    AOHKKeyOut(KEY_LEFTALT, 1);
	}
	if (sequence->Modifier & CTL && !(modifier & Q_CTRL_L)) {
	    AOHKKeyOut(KEY_LEFTCTRL, 1);
	}
	if (sequence->Modifier & SHIFT && !(modifier & Q_SHFT_L)) {
	    AOHKKeyOut(KEY_LEFTSHIFT, 1);
	}
    }
    // Now the scan code
    AOHKKeyOut(sequence->KeyCode, 1);

    AOHKRelease = 1;			// Set flag release send needed
}

///
///	Send modifier release. (reverse press order)
///
///	@param modifier is a bitfield of modifier
///
///	@see Q_SHFT_L Q_SHFT_R Q_CTRL_L Q_CTRL_R Q_ALT_L Q_ALT_R Q_GUI_L
///		Q_GUI_R
///
static void AOHKSendReleaseModifier(int modifier)
{
    if (modifier & Q_GUI_R) {
	AOHKKeyOut(KEY_RIGHTMETA, 0);
    }
    if (modifier & Q_GUI_L) {
	AOHKKeyOut(KEY_LEFTMETA, 0);
    }
    if (modifier & Q_ALT_R) {
	AOHKKeyOut(KEY_RIGHTALT, 0);
    }
    if (modifier & Q_ALT_L) {
	AOHKKeyOut(KEY_LEFTALT, 0);
    }
    if (modifier & Q_CTRL_R) {
	AOHKKeyOut(KEY_RIGHTCTRL, 0);
    }
    if (modifier & Q_CTRL_L) {
	AOHKKeyOut(KEY_LEFTCTRL, 0);
    }
    if (modifier & Q_SHFT_R) {
	AOHKKeyOut(KEY_RIGHTSHIFT, 0);
    }
    if (modifier & Q_SHFT_L) {
	AOHKKeyOut(KEY_LEFTSHIFT, 0);
    }
}

///
///	Send an internal sequence as release events. (reverse order)
///
///	@param modifier is a bitfield of modifier
///	@param sequence output key definition
///
static void AOHKSendReleaseSequence(int modifier, const OHKey * sequence)
{
    // First the scan code
    AOHKKeyOut(sequence->KeyCode, 0);

    // This are special qualifiers
    if (sequence->Modifier && sequence->Modifier ^ 0x80) {
	if (sequence->Modifier & SHIFT && !(modifier & Q_SHFT_L)) {
	    AOHKKeyOut(KEY_LEFTSHIFT, 0);
	}
	if (sequence->Modifier & CTL && !(modifier & Q_CTRL_L)) {
	    AOHKKeyOut(KEY_LEFTCTRL, 0);
	}
	if (sequence->Modifier & ALT && !(modifier & Q_ALT_L)) {
	    AOHKKeyOut(KEY_LEFTALT, 0);
	}
	if (sequence->Modifier & ALTGR && !(modifier & Q_ALT_R)) {
	    AOHKKeyOut(KEY_RIGHTALT, 0);
	}
    }
    // Last all modifieres
    if (modifier) {
	AOHKSendReleaseModifier(modifier);
    }

    AOHKRelease = 0;			// release is done
}

///
///	Game mode release key.
///
///	@param key	internal key relased (AOHK_KEY_0, ...)
///
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

///
///	Game mode release all keys.
///
static void AOHKGameModeReleaseAll(void)
{
    int i;

    for (i = 0; i <= AOHK_KEY_SPECIAL; ++i) {
	AOHKGameModeRelease(i);
    }
}

//----------------------------------------------------------------------------
//	Leds
//----------------------------------------------------------------------------

///
///	Quote State led
///
static void QuoteStateLedOn(void)
{
    AOHKShowLED(0, 1);
}

///
///	Quote State led
///
static void QuoteStateLedOff(void)
{
    AOHKShowLED(0, 0);
}

///
///	Second State led
///
static void SecondStateLedOn(void)
{
    AOHKShowLED(1, 1);
}

///
///	Second State led
///
static void SecondStateLedOff(void)
{
    AOHKShowLED(1, 0);
}

///
///	Gamemode led
///
static void GameModeLedOn(void)
{
    AOHKShowLED(2, 1);
}

///
///	Gamemode led
///
static void GameModeLedOff(void)
{
    AOHKShowLED(2, 0);
}

//----------------------------------------------------------------------------

///
///	Reset to normal state.
///
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
	    Debug(0, "FIXME: release lost\n");
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
//	Helper functions
//----------------------------------------------------------------------------

///
///	Handle modifier key sequence.
///
///	First press sends press, second press sends release.
///	Next key also releases the modifier.
///
///	@param modifier is a bitfield of modifier
///
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

///
///	Handle key sequence.
///
///	@param sequence output key definition
///
///	@see RESET QUAL STICKY TOGAME TONUM SPECIAL MACRO
///
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
	case TOGAME:
	    if (sequence->KeyCode != KEY_RESERVED) {
		AOHKSendPressSequence(AOHKLastModifier =
		    AOHKModifier, AOHKLastSequence = sequence);
		AOHKModifier = AOHKStickyModifier;
	    }
	    AOHKEnterGameMode();
	    break;
	case TONUM:
	    if (sequence->KeyCode != KEY_RESERVED) {
		AOHKSendPressSequence(AOHKLastModifier =
		    AOHKModifier, AOHKLastSequence = sequence);
		AOHKModifier = AOHKStickyModifier;
	    }
	    AOHKEnterNumberMode();
	    break;
	case SPECIAL:
	    AOHKEnterSpecialState();
	    break;

	case MACRO:
	    Debug(0, "Macro %d\n", sequence->KeyCode);
	    if (1) {
		int i;
		const OHKey *macro;

		macro = AOHKMacros[sequence->KeyCode];
		for (i = 0; macro[i].KeyCode != KEY_RESERVED; ++i) {
		    AOHKSendPressSequence(AOHKModifier, macro + i);
		    AOHKSendReleaseSequence(AOHKModifier, macro + i);
		}
	    }
	    AOHKLastModifier = AOHKModifier;
	    AOHKLastSequence = sequence;
	    AOHKModifier = AOHKStickyModifier;
	    break;

	default:			// QUOTE or nothing
	    AOHKSendPressSequence(AOHKLastModifier =
		AOHKModifier, AOHKLastSequence = sequence);
	    AOHKModifier = AOHKStickyModifier;
	    break;
    }
}

//----------------------------------------------------------------------------
//	Game Mode
//----------------------------------------------------------------------------

///
///	Map to internal key symbols.
///
///	Lookup @a key in #AOHKConvertTable and return its mapping.
///	#AOHK_KEY_0, ... or -1 if not mapable.
///
///	@param key	input key scan code
///	@param down	unused, key press or release
///
///	@returns	Internal symbol for scan code, -1 if not used.
///
///	@see __aohk_internal_keys__
///
static int AOHKMapToInternal(int key, int __attribute__((unused)) down)
{
    if (key < 0 || (unsigned)key >= sizeof(AOHKConvertTable)) {
	return -1;
    }
    return AOHKConvertTable[key] == 255 ? -1 : AOHKConvertTable[key];
}

///
///	Enter game mode
///
static void AOHKEnterGameMode(void)
{
    Debug(2, "Game mode on.\n");
    AOHKReset();			// release keys, ...
    AOHKState = OHGameMode;
    GameModeLedOn();
}

///
///	Enter number mode
///
static void AOHKEnterNumberMode(void)
{
    Debug(2, "Number mode on.\n");
    AOHKReset();			// release keys, ...
    AOHKState = OHNumberMode;
    NumberModeLedOn();
}

///
///	Enter special state
///
static void AOHKEnterSpecialState(void)
{
    Debug(2, "Special state start.\n");
    AOHKState = OHSpecial;
    SpecialStateLedOn();
}

//----------------------------------------------------------------------------
//	State machine
//----------------------------------------------------------------------------

///
///	Send sequence from state machine
///
static void AOHKSendSequence(int key, const OHKey * sequence)
{
    //
    //	if quote or quote+repeat (=super) still be pressed
    //	reenter this state.
    //
    if (key != AOHK_KEY_0 && (AOHKDownKeys & (1 << AOHK_KEY_0))) {
	if (AOHKDownKeys & (1 << AOHK_KEY_HASH)) {
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

///
///	Handle the Firstkey state.
///
///	@param key	internal key pressed (#AOHK_KEY_0, ...)
///
///	@see OHFirstKey OHMacroFirstKey
///
static void AOHKFirstKey(int key)
{
    switch (key) {

	case AOHK_KEY_STAR:		// macro key
	    if (AOHKDownKeys & (1 << AOHK_KEY_HASH)) {	// game mode
		// This directions didn't work good, better MACRO+REPEAT
		AOHKEnterGameMode();
		break;
	    }
	    // FIXME: make macro -> macro configurable
	    if (AOHKState == OHMacroFirstKey) {
		AOHKEnterNumberMode();
	    } else {
		AOHKState = OHMacroFirstKey;
	    }
	    break;

	case AOHK_KEY_0:		// enter quote state
	    if (AOHKDownKeys & (1 << AOHK_KEY_HASH)) {	// super quote
		Debug(1, "super quote.\n");
		AOHKState = OHSuperFirstKey;
		QuoteStateLedOn();
		break;
	    }
	    if (AOHKState == OHMacroFirstKey) {
		Debug(1, "macro quote\n");
		AOHKState = OHMacroQuoteFirstKey;
	    } else {
		AOHKState = OHQuoteFirstKey;
	    }
	    QuoteStateLedOn();
	    break;

	case AOHK_KEY_SPECIAL:		// enter special state
	    AOHKEnterSpecialState();
	    break;

	case AOHK_KEY_HASH:		// repeat last sequence
	    // MACRO -> REPEAT
	    if (AOHKState == OHMacroFirstKey) {
		AOHKEnterGameMode();
		break;
	    }
	    if (AOHKDownKeys & (1 << AOHK_KEY_STAR)) {	// game mode
		AOHKEnterGameMode();
		break;
	    }
	    if (AOHKLastSequence) {	// repeat full sequence
		AOHKSendPressSequence(AOHKLastModifier, AOHKLastSequence);
	    } else if (AOHKLastModifier) {	// or only modifier
		AOHKDoModifier(AOHKLastModifier);
	    }
	    return;

	case AOHK_KEY_USR_1:
	case AOHK_KEY_USR_2:
	case AOHK_KEY_USR_3:
	case AOHK_KEY_USR_4:
	case AOHK_KEY_USR_5:
	case AOHK_KEY_USR_6:
	case AOHK_KEY_USR_7:
	case AOHK_KEY_USR_8:
	    AOHKSendSequence(key,
		&AOHKTable[USR_START + key - AOHK_KEY_USR_1]);
	    return;

	default:
	    // repeat key still pressed and any number key
	    if (AOHKDownKeys & (1 << AOHK_KEY_HASH)) {	// cursor mode?
		Debug(1, "cursor mode.\n");
		AOHKSendSequence(key,
		    &AOHKTable[HASH_START + key - AOHK_KEY_0]);
		return;
	    }
	    AOHKLastKey = key;
	    if (AOHKState == OHMacroFirstKey) {
		AOHKState = OHMacroSecondKey;
	    } else {
		if (AOHKState != OHFirstKey) {
		    Debug(0, "internal error.\n");
		}
		AOHKState = OHSecondKey;
	    }
	    SecondStateLedOn();
	    break;
    }
    AOHKTimeout = AOHKTimeBase;
}

///
///	Handle the Super/Quote Firstkey state.
///
///	@param key	internal key pressed (#AOHK_KEY_0, ...)
///
///	@see enum __aohk_internal_keys__
///
///	@see OHQuoteFirstKey OHSuperFirstKey OHQuoteMacroFirstKey
///
static void AOHKQuoteFirstKey(int key)
{
    switch (key) {
	case AOHK_KEY_STAR:		// macro key
	    if (AOHKDownKeys & (1 << AOHK_KEY_0)) {
		// macro quote
		Debug(2, "quote macro\n");
		AOHKState = OHMacroQuoteFirstKey;
		break;
	    }
	    if (AOHKState == OHSuperFirstKey) {
		// super quote macro
		AOHKSendSequence(key, &AOHKSuperTable[STAR_START]);
	    } else if (AOHKState == OHMacroQuoteFirstKey) {
		// macro quote macro
		AOHKSendSequence(key, &AOHKMacroTable[STAR_START]);
	    } else {
		// quote macro
		AOHKSendSequence(key, &AOHKTable[STAR_START]);
	    }
	    break;

	case AOHK_KEY_0:		// double quote extra key
	    if (AOHKState == OHSuperFirstKey) {
		// super quote quote
		AOHKSendSequence(key, &AOHKSuperTable[DOUBLE_QUOTE]);
	    } else if (AOHKState == OHMacroQuoteFirstKey) {
		// macro quote quote
		AOHKSendSequence(key, &AOHKMacroTable[DOUBLE_QUOTE]);
	    } else {
		// quote quote
		AOHKSendSequence(key, &AOHKTable[DOUBLE_QUOTE]);
	    }
	    break;

	case AOHK_KEY_SPECIAL:		// enter special state
	    AOHKEnterSpecialState();
	    return;

	case AOHK_KEY_HASH:		// quoted repeat extra key
	    if (AOHKDownKeys & (1 << AOHK_KEY_0)) {	// super quote
		// Use this works good
		Debug(2, "super quote.\n");
		AOHKState = OHSuperFirstKey;
		QuoteStateLedOn();
		break;
	    }
	    if (AOHKState == OHSuperFirstKey) {
		// super quote repeat
		AOHKSendSequence(key, &AOHKSuperTable[HASH_START]);
	    } else if (AOHKState == OHMacroQuoteFirstKey) {
		// macro quote repeat
		AOHKSendSequence(key, &AOHKMacroTable[HASH_START]);
	    } else {
		// quote repeat
		AOHKSendSequence(key, &AOHKTable[HASH_START]);
	    }
	    break;

	case AOHK_KEY_USR_1:
	case AOHK_KEY_USR_2:
	case AOHK_KEY_USR_3:
	case AOHK_KEY_USR_4:
	case AOHK_KEY_USR_5:
	case AOHK_KEY_USR_6:
	case AOHK_KEY_USR_7:
	case AOHK_KEY_USR_8:
	    if (AOHKState == OHSuperFirstKey) {
		// super quote
		AOHKSendSequence(key,
		    &AOHKSuperTable[USR_START + key - AOHK_KEY_USR_1]);
	    } else if (AOHKState == OHMacroQuoteFirstKey) {
		// macro quote
		AOHKSendSequence(key,
		    &AOHKMacroQuoteTable[USR_START + key - AOHK_KEY_USR_1]);
	    } else {
		// quote
		AOHKSendSequence(key,
		    &AOHKQuoteTable[USR_START + key - AOHK_KEY_USR_1]);
	    }
	    break;

	default:
	    AOHKLastKey = key;
	    AOHKState++;
	    SecondStateLedOn();
	    break;
    }
}

///
///	Handle the Normal/Super/Quote SecondKey state.
///
///	@param key	internal key pressed (#AOHK_KEY_0, ...)
///
///	@see enum __aohk_internal_keys__
///
static void AOHKSecondKey(int key)
{
    const OHKey *sequence;
    int n;

    //
    //	Every not supported key, does a soft reset.
    //
    if (key == AOHK_KEY_SPECIAL || (AOHK_KEY_USR_1 <= key && key <= AOHK_KEY_USR_8)) {	// oops
	AOHKReset();
	return;
    }
    // AOHK_KEY_0 ok

    if (key == AOHK_KEY_HASH) {		// second repeat 9 extra keys
	n = HASH_START + AOHKLastKey - AOHK_KEY_0;
    } else if (key == AOHK_KEY_STAR) {	// second macro 9 extra keys
	n = STAR_START + AOHKLastKey - AOHK_KEY_0;
    } else {				// normal key sequence
	n = (AOHKLastKey - AOHK_KEY_1) * 10 + key - AOHK_KEY_0;
    }

    switch (AOHKState) {
	case OHSecondKey:
	    sequence = &AOHKTable[n];
	    break;
	case OHQuoteSecondKey:
	    sequence = &AOHKQuoteTable[n];
	    break;
	case OHMacroSecondKey:
	    sequence = &AOHKMacroTable[n];
	    break;
	case OHMacroQuoteSecondKey:
	    sequence = &AOHKMacroQuoteTable[n];
	    break;
	case OHSuperSecondKey:
	default:
	    sequence = &AOHKSuperTable[n];
	    break;
    }

    // This allows pressing first key and quote together.
    if (sequence->Modifier == QUOTE) {
	Debug(2, "Quote as second key\n");
	// FIXME: macro ..
	AOHKState = OHQuoteSecondKey;
	QuoteStateLedOn();
	return;
    }
    AOHKSendSequence(key, sequence);
}

///
///	Handle the game mode state.
///
///	Game mode:	For games keys are direct mapped.
///
///	@param key	internal key pressed (#AOHK_KEY_0, ...)
///
///	@see enum __aohk_internal_keys__
///
static void AOHKGameMode(int key)
{
    const OHKey *sequence;

    Debug(4, "Gamemode key %d.\n", key);

    // FIXME: QUAL shouldn't work correct

    AOHKGameSendQuote = 0;
    sequence = AOHKLastKey ? &AOHKQuoteGameTable[key] : &AOHKGameTable[key];

    //
    //	Reset is used to return to normal mode.
    //
    if (sequence->Modifier == RESET) {
	Debug(3, "Game mode off.\n");
	AOHKGameModeReleaseAll();
	AOHKState = OHFirstKey;
	GameModeLedOff();
	return;
    }
    //
    //	Holding down QUOTE shifts to second table.
    //
    if (sequence->Modifier == QUOTE) {
	Debug(3, "Game mode quote.\n");
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

///
///	Handle the number mode state.
///
///	Number mode:	For number inputs, key are direct mapped.
///
///	@param key	internal key pressed (#AOHK_KEY_0, ...)
///
///	@see enum __aohk_internal_keys__
///
static void AOHKNumberMode(int key)
{
    const OHKey *sequence;

    Debug(3, "Numbermode key %d.\n", key);

    sequence = &AOHKNumberTable[key];

    //
    //	Reset is used to return to normal mode.
    //
    if (sequence->Modifier == RESET) {
	Debug(2, "Number mode off.\n");
	// Release all still pressed keys.
	for (key = 0; key <= AOHK_KEY_SPECIAL; ++key) {
	    if (AOHKGamePressed & (1 << key)) {
		sequence = &AOHKNumberTable[key];
		AOHKSendReleaseSequence(0, sequence);
	    }
	}
	AOHKGamePressed = 0;
	AOHKState = OHFirstKey;
	NumberModeLedOff();
	return;
    }

    AOHKDoSequence(sequence);
    AOHKGamePressed |= (1 << key);
}

///
///	Handle the special state.
///
///	The special state, is our command mode.
///
///	@param key	internal key pressed (#AOHK_KEY_0, ...)
///
///	@see enum __aohk_internal_keys__
///
static void AOHKSpecialMode(int key)
{
    SpecialStateLedOff();
    AOHKReset();

    Debug(3, "Special Key %d.\n", key);
    switch (key) {
	case AOHK_KEY_0:		// reset to known state
	    Debug(2, "Reset\n");
	    return;

	case AOHK_KEY_1:		// enable only me mode
	    AOHKOnlyMe ^= 1;
	    Debug(2, "Toggle only me mode %s.\n", AOHKOnlyMe ? "on" : "off");
	    return;

	case AOHK_KEY_2:		// double timeout
	    AOHKTimeBase <<= 1;
	    AOHKTimeBase |= 1;
	    AOHKTimeout = AOHKTimeBase;
	    Debug(2, "Double timeout %d.\n", AOHKTimeBase);
	    return;

	case AOHK_KEY_3:		// half timeout
	    AOHKTimeBase >>= 1;
	    AOHKTimeBase |= 1;
	    AOHKTimeout = AOHKTimeBase;
	    Debug(2, "Half timeout %d.\n", AOHKTimeBase);
	    return;

	case AOHK_KEY_4:		// Version
	    if (1) {
		int i;

		char version[] = {
		    KEY_A, KEY_O, KEY_H, KEY_K, KEY_SPACE,
		    KEY_V, KEY_0, KEY_DOT, KEY_9, KEY_0, KEY_RESERVED
		};
		for (i = 0; version[i]; ++i) {
		    AOHKKeyOut(version[i], 1);
		    AOHKKeyOut(version[i], 0);
		}
	    }
	    return;

	case AOHK_KEY_5:		// exit
	    AOHKReset();
	    AOHKExit = 1;
	    return;

	case AOHK_KEY_6:		// game mode
	    Debug(2, "Game mode on.\n");
	    AOHKEnterGameMode();
	    return;

	case AOHK_KEY_7:		// number mode
	    Debug(2, "Number mode on.\n");
	    AOHKEnterNumberMode();
	    return;

	case AOHK_KEY_8:		// debug
	    Debug(2, "Debug now %d.\n", ++DebugLevel);
	    return;

	case AOHK_KEY_9:		// turn it off hard
	    AOHKState = OHHardOff;
	    Debug(2, "Turned off.\n");
	    return;

	case AOHK_KEY_SPECIAL:		// turn it off soft
	    AOHKState = OHSoftOff;
	    Debug(2, "Soft turned off.\n");
	    return;
    }
    Debug(1, "unsupported special %d.\n", key);
}

///
///	Check current off state.
///
///	@returns true if off. (-1 hard, 1 soft)
///
int AOHKCheckOffState(void)
{
    if (AOHKState == OHHardOff) {
	return -1;
    }
    if (AOHKState == OHSoftOff) {
	return 1;
    }
    return 0;
}

///
///	OneHand Statemachine.
///
///	@param timestamp	ms timestamp of event
///	@param symbol		internal key symbol
///	@param down		True key is pressed, false key is released
///
///	@returns true if key wasn't used.
///
int AOHKFeedSymbol(unsigned long timestamp, int symbol, int down)
{
    //
    //	Completly turned off
    //
    if (AOHKState == OHHardOff) {
	return -1;
    }
    //
    //	Turned soft off
    //
    if (AOHKState == OHSoftOff) {
	//
	//  Next special key reenables us
	//  down! otherwise we get the release of the disable press
	//
	if (down && symbol == AOHK_KEY_SPECIAL) {
	    Debug(1, "Reenable.\n");
	    AOHKState = OHFirstKey;
	    AOHKDownKeys = 0;
	    return 0;
	}
	return -1;
    }
    //
    //	Timeout with timestamps
    //
    if (AOHKLastTick + AOHKTimeBase < timestamp) {
	Debug(5, "Timeout %lu %lu\n", AOHKLastTick, timestamp);
	AOHKFeedTimeout(timestamp - AOHKLastTick);
    }
    AOHKLastTick = timestamp;

    //
    //	Disabled keys.
    //
    if (symbol == AOHK_KEY_NOP) {
	return 0;			// ignore them, no operation
    }
    //
    //	Handling of unsupported input keys.
    //
    if (symbol == -1) {
	if (AOHKState != OHGameMode && AOHKState != OHNumberMode) {
	    AOHKReset();		// any unsupported key reset us
	}
	return -1;
    }
    //
    //	Test for repeat and handle release
    //
    if (!down) {			// release
	//
	//     Game mode, release and press aren't ordered.
	//
	if (AOHKState == OHGameMode) {
	    //
	    //	Game mode, quote delayed to release.
	    //
	    if (AOHKGameSendQuote) {
		AOHKSendPressSequence(AOHKLastModifier, AOHKLastSequence);
		AOHKSendReleaseSequence(AOHKLastModifier, AOHKLastSequence);
		AOHKGameSendQuote = 0;
	    }
	    AOHKGameModeRelease(symbol);
	    //
	    //	   Number mode, release and press aren't ordered.
	    //
	} else if (AOHKState == OHNumberMode) {
	    if (AOHKGamePressed & (1 << symbol)) {
		AOHKGamePressed ^= 1 << symbol;
		AOHKSendReleaseSequence(0, &AOHKNumberTable[symbol]);
	    } else {
		// Happens on release of start sequence.
		Debug(3, "oops key %d was not pressed\n", symbol);
	    }
	} else {
	    //
	    //	Need to send a release sequence.
	    //
	    // FIXME: if (AOHKState == AOHKFirstKey && AOHKRelease)
	    if (AOHKRelease) {
		if (AOHKLastSequence) {
		    AOHKSendReleaseSequence(AOHKLastModifier,
			AOHKLastSequence);
		} else if (AOHK_KEY_USR_1 <= symbol
		    && symbol <= AOHK_KEY_USR_8) {
		    // FIXME: hold USR than press a sequence,
		    // FIXME: than release USR is not supported!
		    if (AOHKLastModifier) {
			AOHKSendReleaseModifier(AOHKLastModifier);
			AOHKModifier = AOHKStickyModifier;
			AOHKRelease = 0;
		    }
		} else {
		    Debug(3, "Release ignored for only modifier!\n");
		    goto ignore;
		}
		if (AOHKRelease) {
		    Debug(0, "Release not done!\n");
		}
	    }
	}
      ignore:
	AOHKDownKeys &= ~(1 << symbol);
	return 0;
    } else if (AOHKDownKeys & (1 << symbol)) {	// repeating
	//
	//	internal key repeating, ignore
	//	(note: 1 repeated does nothing. 11 repeated does someting!)
	//	QUOTE is repeated in game mode, is this good?
	//	QUAL modifiers aren't repeated
	//
	if (AOHKLastSequence && (AOHKState == OHGameMode
		|| AOHKState == OHFirstKey || AOHKState == OHNumberMode)) {
	    AOHKSendPressSequence(AOHKLastModifier, AOHKLastSequence);
	}
	return 0;
    }
    AOHKDownKeys |= (1 << symbol);

    //
    //	Convert internal code into scancodes (only down events!)
    //	The fat state machine
    //
    switch (AOHKState) {

	case OHFirstKey:
	case OHMacroFirstKey:
	    AOHKFirstKey(symbol);
	    break;

	case OHSuperFirstKey:
	case OHQuoteFirstKey:
	case OHMacroQuoteFirstKey:
	    AOHKQuoteFirstKey(symbol);
	    break;

	case OHSecondKey:
	case OHQuoteSecondKey:
	case OHSuperSecondKey:
	case OHMacroSecondKey:
	case OHMacroQuoteSecondKey:
	    AOHKSecondKey(symbol);
	    break;

	case OHGameMode:
	    AOHKGameMode(symbol);
	    break;

	case OHNumberMode:
	    AOHKNumberMode(symbol);
	    break;

	case OHSpecial:
	    AOHKSpecialMode(symbol);
	    break;

	default:
	    Debug(0, "Unkown state %d reached\n", AOHKState);
	    break;
    }

    return 0;
}

///
///	OneHand Statemachine.
///
///	@param timestamp	ms timestamp of event
///	@param inkey		input keycode @see /usr/include/linux/input.h
///	@param down		True key is pressed, false key is released
///
///	@todo
///		Split input to internal conversion and state machine.
///		Needed for devices/inputs which generate AOHK_KEY direct.
///
void AOHKFeedKey(unsigned long timestamp, int inkey, int down)
{
    int symbol;

    //
    //	Completly turned off
    //
    if (AOHKState == OHHardOff) {
	AOHKKeyOut(inkey, down);
	return;
    }

    Debug(6, "Keyin 0x%02X=%d %s\n", inkey, inkey, down ? "down" : "up");

    symbol = AOHKMapToInternal(inkey, down);
    if (AOHKFeedSymbol(timestamp, symbol, down)) {
	if (symbol == -1) {
	    Debug(5, "Unsupported key %d=%#02x of state %d.\n", inkey, inkey,
		AOHKState);
	}
	AOHKKeyOut(inkey, down);
    }
}

///
///	Feed Timeouts.
///
///	Can be called seperate to update, LEDS.
///
///	@param which	Timeout happened,
///
void AOHKFeedTimeout(int which)
{
    //
    //	Timeout -> reset to intial state
    //
    if (AOHKState != OHGameMode && AOHKState != OHNumberMode
	&& AOHKState != OHSoftOff && AOHKState != OHHardOff) {
	// Long time: total reset
	if (which >= AOHKTimeBase * 10) {
	    Debug(3, "Timeout long %d\n", which);
	    AOHKReset();
	    AOHKDownKeys = 0;
	    AOHKTimeout = 0;
	    return;
	}
	if (AOHKState != OHFirstKey) {
	    Debug(3, "Timeout short %d\n", which);
	    // FIXME: Check if leds are on!
	    SecondStateLedOff();
	    QuoteStateLedOff();

	    // Short time: only reset state
	    AOHKState = OHFirstKey;
	}
	AOHKTimeout = 10 * AOHKTimeBase;
    }
}

///
///	Reset convert table
///
static void AOHKResetConvertTable(void)
{
    size_t idx;

    for (idx = 0; idx < sizeof(AOHKConvertTable); ++idx) {
	AOHKConvertTable[idx] = 255;
    }
}

///
///	Reset mapping tables
///
///	@see AOHKTable AOHKQuoteTable AOHKSuperTable
///
static void AOHKResetMappingTable(void)
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
    for (idx = 0; idx < sizeof(AOHKNumberTable) / sizeof(*AOHKNumberTable);
	++idx) {
	AOHKNumberTable[idx].Modifier = RESET;
	AOHKNumberTable[idx].KeyCode = KEY_RESERVED;
    }
}

///
///	Reset macro tables
///
static void AOHKResetMacroTable(void)
{
    size_t idx;

    for (idx = 0; idx < sizeof(AOHKMacroTable) / sizeof(*AOHKMacroTable);
	++idx) {
	AOHKMacroTable[idx].Modifier = RESET;
	AOHKMacroTable[idx].KeyCode = KEY_RESERVED;
    }

    for (idx = 0;
	idx < sizeof(AOHKMacroQuoteTable) / sizeof(*AOHKMacroQuoteTable);
	++idx) {

	AOHKMacroQuoteTable[idx].Modifier = RESET;
	AOHKMacroQuoteTable[idx].KeyCode = KEY_RESERVED;
    }
}

///
///	Setup table which converts input keycodes to internal key symbols.
///
///	Clears all previous entries of #AOHKConvertTable.
///
///	@param table	Table with pairs input internal keys.
///
void AOHKSetupConvertTable(const int *table)
{
    int idx;

    Debug(3, "Setup table %p\n", table);

    AOHKResetConvertTable();

    while ((idx = *table++)) {
	if (idx > 0 && (unsigned)idx < sizeof(AOHKConvertTable)) {
	    AOHKConvertTable[idx] = *table;
	}
	++table;
    }
}

//----------------------------------------------------------------------------
//	Macro
//----------------------------------------------------------------------------

///
///	Add macro.
///	Insert a string of #OHKey into first free slot of macro table
///	#AOHKMacros.
///
///	@param macro	string of output sequences
///
static void AddMacro(OHKey * macro)
{
    int i;
    unsigned u;

    //	Length of macro
    for (i = 0; i < 255; ++i) {
	if (macro[i].KeyCode == KEY_RESERVED) {
	    break;
	}
    }
    //
    //	Find free macro slot
    //
    for (u = 0; u < sizeof(AOHKMacros) / sizeof(*AOHKMacros); ++u) {
	if (!AOHKMacros[u]) {
	    AOHKMacros[u] = malloc(i * sizeof(OHKey));
	    memcpy(AOHKMacros[u], macro, i * sizeof(OHKey));
	    return;
	}
    }
    Debug(0, "No space for more macros\n");
}

//----------------------------------------------------------------------------
//	Save + Load
//----------------------------------------------------------------------------

//	*INDENT-OFF*

///
///	Key Scancode - string (US Layout used)
///
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
    "Scale",
    "KP_Comma",
    "Hanguel",
    "Hanja",
    "Yen",
    "LeftMeta",
    "RightMeta",
    "Menu",
};

///
///	Alias names. Alternative names for scan codes also supported.
///
///	@see AOHKKey2String
///
static const struct {
    const char* Name;			///< alias name
    unsigned char Key;			///< linux scan code
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

///
///	Internal key - string
///
static const char* AOHKInternal2String[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "#", "*",
    "USR1", "USR2", "USR3", "USR4", "USR5", "USR6", "USR7", "USR8",
    "SPECIAL", "NOP"
};

//	*INDENT-ON*

///
///	Save convert table.
///
///	@param fp	output file stream
///	@param t	input to intern convert table
///
static void AOHKSaveConvertTable(FILE * fp, const unsigned char *t)
{
    size_t i;

    fprintf(fp, "\n//\tConverts input keys to internal symbols\nconvert:\n");
    for (i = 0; i < sizeof(AOHKConvertTable)
	&& i < sizeof(AOHKKey2String) / sizeof(*AOHKKey2String); ++i) {
	if (t[i] != 255) {
	    fprintf(fp, "%-10s\t-> %s\n", AOHKKey2String[i],
		AOHKInternal2String[t[i]]);
	}
    }
}

///
///	Save sequence.
///
///	@param fp	output file stream
///	@param s	output key sequence
///
static void AOHKSaveSequence(FILE * fp, const unsigned char *s)
{
    switch (*s) {			// modifier code
	case RESET:
	    fprintf(fp, "RESET");
	    break;
	case QUOTE:
	    fprintf(fp, "QUOTE");
	    break;
	case TOGAME:
	    fprintf(fp, "TOGAME");
	    if (s[1] != KEY_RESERVED) {
		fprintf(fp, " %s", AOHKKey2String[s[1]]);
	    }
	    break;
	case TONUM:
	    fprintf(fp, "TONUM");
	    if (s[1] != KEY_RESERVED) {
		fprintf(fp, " %s", AOHKKey2String[s[1]]);
	    }
	    break;
	case SPECIAL:
	    fprintf(fp, "SPECIAL");
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
	    break;
	case MACRO:
	    if (1) {
		int i;
		const OHKey *macro;

		macro = AOHKMacros[s[1]];
		for (i = 0; macro[i].KeyCode != KEY_RESERVED; ++i) {
		    if (i) {
			fprintf(fp, " ");
		    }
		    AOHKSaveSequence(fp, (unsigned char *)&macro[i]);
		}
	    }
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
	    fprintf(fp, "%s", AOHKKey2String[s[1]]);
	    break;
    }
}

///
///	Save sequence table.
///
static void AOHKSaveMapping(FILE * fp, const char *prefix,
    const unsigned char *t, int n)
{
    int i;

    for (i = 0; i < n; i += 2) {
	//
	//	Print internal sequence code
	//
	fprintf(fp, "%s", prefix);
	if (i == DOUBLE_QUOTE * 2) {
	    fprintf(fp, "00\t-> ");
	} else if (i >= USR_START * 2) {
	    fprintf(fp, "USR%d\t-> ", (i >> 1) - USR_START + 1);
	} else if (i >= STAR_START * 2) {
	    fprintf(fp, "%d*\t-> ", (i >> 1) % 10);
	} else if (i >= HASH_START * 2) {
	    fprintf(fp, "%d#\t-> ", (i >> 1) % 10);
	} else {
	    fprintf(fp, "%d%d\t-> ", (i >> 1) / 10 + 1, (i >> 1) % 10);
	}

	//
	//	Print output sequence key
	//
	AOHKSaveSequence(fp, t + i);
	fprintf(fp, "\n");
    }
}

///
///	Save game table
///
static void AOHKSaveGameTable(FILE * fp, const char *prefix,
    const unsigned char *t, int n)
{
    int i;

    for (i = 0; i < n; i += 2) {
	fprintf(fp, "%s%s\t-> ", prefix, AOHKInternal2String[i / 2]);
	AOHKSaveSequence(fp, t + i);
	fprintf(fp, "\n");
    }
}

///
///	Save internals tables in a nice format.
///
///	@param file	File name, - for stdout.
///
void AOHKSaveTable(const char *file)
{
    FILE *fp;

    if (!strcmp(file, "-")) {		// stdout
	fp = stdout;
    } else if (!(fp = fopen(file, "w+"))) {
	Debug(0, "Can't open save file '%s'\n", file);
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
    fprintf(fp, "//\tgame mode *# key prefix\n");
    AOHKSaveGameTable(fp, "*#", (unsigned char *)AOHKGameTable,
	sizeof(AOHKGameTable));
    fprintf(fp, "//\tquote game mode 0*# key prefix\n");
    AOHKSaveGameTable(fp, "0*#", (unsigned char *)AOHKQuoteGameTable,
	sizeof(AOHKQuoteGameTable));
    fprintf(fp, "//\tnumber mode ** key prefix\n");
    AOHKSaveGameTable(fp, "**", (unsigned char *)AOHKNumberTable,
	sizeof(AOHKNumberTable));

    fprintf(fp, "//\tmacros * key prefix\nmacro:\n");
    AOHKSaveMapping(fp, "*", (unsigned char *)AOHKMacroTable,
	sizeof(AOHKMacroTable));

    fprintf(fp, "//\tquoted macros *0 key prefix\n");
    AOHKSaveMapping(fp, "*0", (unsigned char *)AOHKMacroQuoteTable,
	sizeof(AOHKMacroQuoteTable));

    if (strcmp(file, "-")) {		// !stdout
	fclose(fp);
    }
}

///
///	Junk at end of line
///
static void AOHKIsJunk(int linenr, const char *s)
{
    for (; *s && isspace(*s); ++s) {
    }
    if (*s) {
	Debug(0, "%d: Junk '%s' at end of line\n", linenr, s);
    }
}

///
///	Convert a string to key code.
///
static int AOHKString2Key(const char *line, size_t l)
{
    size_t i;

    //
    //	First lookup alias names.
    //
    for (i = 0; i < sizeof(AOHKKeyAlias) / sizeof(*AOHKKeyAlias); ++i) {
	if (l == strlen(AOHKKeyAlias[i].Name)
	    && !strncasecmp(line, AOHKKeyAlias[i].Name, l)) {
	    return AOHKKeyAlias[i].Key;
	}
    }
    //
    //	Not found, lookup normal names.
    //
    for (i = 0; i < sizeof(AOHKKey2String) / sizeof(*AOHKKey2String); ++i) {
	if (l == strlen(AOHKKey2String[i])
	    && !strncasecmp(line, AOHKKey2String[i], l)) {
	    return i;
	}
    }
    return KEY_RESERVED;
}

///
///	Convert a string to internal code.
///
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

///
///	Parse convert line.
///
static void AOHKParseConvert(char *line)
{
    char *s;
    size_t l;
    int key;
    int internal;

    //
    //	Get input key code
    //
    for (s = line; *s && !isspace(*s); ++s) {
    }
    l = s - line;
    key = AOHKString2Key(line, l);
    if (key == KEY_RESERVED) {		// Still not found giving up.
	Debug(0, "Key '%.*s' not found\n", (int)l, line);
	return;
    }
    //
    //	    Skip white spaces
    //
    for (; *s && isspace(*s); ++s) {
    }
    //
    //	    Need ->
    //
    if (strncmp(s, "->", 2)) {
	Debug(0, "'->' expected not '%s'\n", s);
	return;
    }
    s += 2;
    //
    //	    Skip white spaces
    //
    for (; *s && isspace(*s); ++s) {
    }

    //
    //	Get internal key name
    //
    for (line = s; *s && !isspace(*s); ++s) {
    }
    l = s - line;

    //
    //	Lookup internal names.
    //
    internal = AOHKString2Internal(line, l);
    if (internal == -1) {
	Debug(0, "Key '%.*s' not found\n", (int)l, line);
	return;
    }

    Debug(4, "Key %d -> %d\n", key, internal);

    if (key > 0 && (unsigned)key < sizeof(AOHKConvertTable)) {
	AOHKConvertTable[key] = internal;
    } else {
	Debug(0, "Key %d out of range\n", key);
    }
}

///
///	Parse sequence '   ->  ...'
///
///	@param linenr	current line number for errors
///	@param line	pointer into current line
///	@param[out] out internal key sequence generated from line text
///
static void AOHKParseOutput(int linenr, char *line, OHKey * out)
{
    char *s;
    size_t l;
    int i;
    int key;
    int modifier;

    //
    //	    Skip white spaces
    //
    for (s = line; *s && isspace(*s); ++s) {
    }
    //
    //	    Need ->
    //
    if (strncmp(s, "->", 2)) {
	Debug(0, "'->' expected not '%s'\n", s);
	return;
    }
    s += 2;
    modifier = 0;
    key = KEY_RESERVED;

    //
    //	Parse all keys.
    //
  next:
    //
    //	    Skip white spaces
    //
    for (; *s && isspace(*s); ++s) {
    }

    //
    //	Get internal key name
    //
    for (line = s; *s && !isspace(*s); ++s) {
    }
    l = s - line;

    //
    //	Special commands FIXME: not if modifier or qual
    //
    if (l == sizeof("qual") - 1
	&& !strncasecmp(line, "qual", sizeof("qual") - 1)) {
	if (modifier) {
	    Debug(1, "%d: multiple modifier\n", linenr);
	}
	modifier = QUAL;
	goto next;
    } else if (l == sizeof("sticky") - 1
	&& !strncasecmp(line, "sticky", sizeof("sticky") - 1)) {
	if (modifier) {
	    Debug(1, "%d: multiple modifier\n", linenr);
	}
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
	if (modifier) {
	    Debug(1, "%d: multiple modifier\n", linenr);
	}
	modifier = TOGAME;
	goto next;
    } else if (l == sizeof("tonum") - 1
	&& !strncasecmp(line, "tonum", sizeof("tonum") - 1)) {
	if (modifier) {
	    Debug(1, "%d: multiple modifier\n", linenr);
	}
	modifier = TONUM;
	goto next;
    } else if (l == sizeof("special") - 1
	&& !strncasecmp(line, "special", sizeof("special") - 1)) {
	modifier = SPECIAL;
    } else if (l == sizeof("reserved") - 1
	&& !strncasecmp(line, "reserved", sizeof("reserved") - 1)) {
	modifier = 0;
    } else if (l) {
	i = AOHKString2Key(line, l);
	if (i == KEY_RESERVED) {	// Still not found giving up.
	    Debug(0, "Key '%.*s' not found\n", (int)l, line);
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
		    Debug(0, "Key '%s' not found\n", line);
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
    Debug(4, "Found modifier %d, key %d\n", modifier, key);
    if (out) {
	out->Modifier = modifier;
	out->KeyCode = key;
    }
    AOHKIsJunk(linenr, s);
}

///
///	Parse mapping line.
///
///	[internal key sequence] -> [output key sequence]
///
///	@param linenr	current line number for errors
///	@param line	pointer into current line
///
static void AOHKParseMapping(int linenr, char *line)
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
    //	Get input key code
    //
    for (s = line; *s && !isspace(*s); ++s) {
    }
    l = s - line;

    // Quoted game mode '0*#'
    if (line[0] == '0' && line[1] == '*' && line[2] == '#') {
	internal = AOHKString2Internal(line + 3, l - 3);
	if (internal == -1) {
	    Debug(0, "Key '%s' not found\n", line);
	    return;
	}
	Debug(4, "Quoted game mode: %d\n", internal);
	AOHKParseOutput(linenr, s, AOHKQuoteGameTable + internal);
	return;
    }
    // Game mode '*#' internal key name
    if (line[0] == '*' && line[1] == '#') {
	internal = AOHKString2Internal(line + 2, l - 2);
	if (internal == -1) {
	    Debug(0, "Key '%s' not found\n", line);
	    return;
	}
	Debug(4, "Game mode: %d\n", internal);
	AOHKParseOutput(linenr, s, AOHKGameTable + internal);
	return;
    }
    // Number mode '**' internal key name
    if (line[0] == '*' && line[1] == '*') {
	internal = AOHKString2Internal(line + 2, l - 2);
	if (internal == -1) {
	    Debug(0, "Key '%s' not found\n", line);
	    return;
	}
	Debug(4, "Number mode: %d\n", internal);
	AOHKParseOutput(linenr, s, AOHKNumberTable + internal);
	return;
    }
    // Macro key '*'
    if (line[0] == '*') {
	Debug(4, "Macro: ");
	macro = 1;
	++line;
	--l;
    }
    // Super Quote
    if (line[0] == '0' && line[1] == '#') {
	Debug(4, "Super quote ");
	super = 1;
	line += 2;
	l -= 2;
	// Quote
    } else if (line[0] == '0') {
	Debug(4, "Quote ");
	quote = 1;
	line += 1;
	l -= 1;
    }

    if (!l) {				// only 0#
	// FIXME: shouldn't be correct
	super = 0;
	internal = HASH_START;
	goto parseon;
    }
    if (l == 1) {
	if (quote && line[0] == '0') {	// 00
	    quote = 0;
	    internal = DOUBLE_QUOTE;
	    goto parseon;
	}
	if (quote && line[0] == '*') {	// 0*
	    quote = 0;
	    internal = STAR_START;
	    goto parseon;
	}
    }
    if (l == 2) {
	if (line[0] == '0' && line[1] == '#') {
	    internal = HASH_START;
	} else if (line[0] == '0' && line[1] == '*') {
	    internal = STAR_START;
	} else if (line[0] == '0' && line[1] == '0') {
	    internal = DOUBLE_QUOTE;
	} else if (line[0] >= '1' && line[0] <= '9' && line[1] >= '0'
	    && line[1] <= '9') {
	    internal = (line[0] - '1') * 10 + line[1] - '0';
	} else if (line[0] >= '1' && line[0] <= '9' && line[1] == '#') {
	    internal = HASH_START + line[0] - '0';
	} else if (line[0] >= '1' && line[0] <= '9' && line[1] == '*') {
	    internal = STAR_START + line[0] - '0';
	} else {
	    Debug(0, "%d: Illegal internal key '%s'\n", linenr, line);
	    return;
	}
	goto parseon;
    }

    if (l == 4 && line[0] == 'U' && line[1] == 'S' && line[2] == 'R') {
	if (line[3] < '1' || line[3] > '8') {
	    Debug(0, "Key '%s' not found\n", line);
	    return;
	}
	internal = USR_START + line[3] - '1';
	goto parseon;
    }

    Debug(0, "%d: Unsupported internal key sequence: %s\n", linenr, line);
    return;

  parseon:
    Debug(4, "Key %d\n", internal);
    if (macro && quote) {
	AOHKParseOutput(linenr, s, AOHKMacroQuoteTable + internal);
    } else if (macro && super) {
	Debug(0, "Macro + super not supported\n");
    } else if (macro) {
	AOHKParseOutput(linenr, s, AOHKMacroTable + internal);
    } else if (super) {
	AOHKParseOutput(linenr, s, AOHKSuperTable + internal);
    } else if (quote) {
	AOHKParseOutput(linenr, s, AOHKQuoteTable + internal);
    } else {
	AOHKParseOutput(linenr, s, AOHKTable + internal);
    }
}

///
///	Load key mapping.
///
///	@param file	File name, - for stdin.
///
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
	Debug(0, "Can't open load file '%s'\n", file);
	return;
    }

    linenr = 0;
    state = Nothing;
    while (fgets(buf, sizeof(buf), fp)) {
	++linenr;
	//
	//	Skip leading white spaces
	//
	for (line = buf; *line && isspace(*line); ++line) {
	}
	if (!*line) {			// Empty line
	    continue;
	}
	//
	//	Remove comments // to end of line
	//
	if ((s = strstr(line, "//"))) {
	    *s = '\0';
	}
	//
	//	Remove trailing \n
	//
	if ((s = strchr(line, '\n'))) {
	    *s = '\0';
	}
	/*
	   for (s = line; *s; ++s) {
	   if (*s == '/' && s[1] == '/') {
	   *s = '\0';
	   break;
	   }
	   }
	 */
	if (!*line) {			// Empty line
	    continue;
	}
	Debug(5, "Parse '%s'\n", line);
	if (!strncasecmp(line, "convert:", sizeof("convert:") - 1)) {
	    Debug(5, "'%s'\n", line);
	    state = Convert;
	    AOHKResetConvertTable();
	    AOHKIsJunk(linenr, line + sizeof("convert:") - 1);
	    continue;
	}
	if (!strncasecmp(line, "mapping:", sizeof("mapping:") - 1)) {
	    Debug(5, "'%s'\n", line);
	    state = Mapping;
	    AOHKResetMappingTable();
	    AOHKIsJunk(linenr, line + sizeof("mapping:") - 1);
	    continue;
	}
	if (!strncasecmp(line, "macro:", sizeof("macro:") - 1)) {
	    Debug(5, "'%s'\n", line);
	    state = Macro;
	    AOHKResetMacroTable();
	    AOHKIsJunk(linenr, line + sizeof("macro:") - 1);
	    continue;
	}
	switch (state) {
	    case Nothing:
		Debug(0, "%d: Need convert: or mapping: or macro: first\n",
		    linenr);
		break;
	    case Convert:
		AOHKParseConvert(line);
		break;
	    case Mapping:
	    case Macro:
		AOHKParseMapping(linenr, line);
		break;
	}
    }

    if (!linenr) {
	Debug(1, "Empty file '%s'\n", file);
    }

    if (strcmp(file, "-")) {		// !stdin
	fclose(fp);
    }
}

//----------------------------------------------------------------------------
//	Default Keymaps.
//----------------------------------------------------------------------------

///
///	American keyboard layout default mapping.
///
static const OHKey AOHKUsTable[11 * 10 + 1 + 8] = {
//	*INDENT-OFF*
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

#if 0
/*##*/ { 0,		KEY_RESERVED },	// Not possible! # is already repeat
/**#*/ { 0,		TOGAME },	// ENTER GAME MODE
#endif

/*0**/ { RESET,		KEY_RESERVED },	// (FREE)
/*1**/ { RESET,		KEY_RESERVED },	// (FREE)
/*2**/ { RESET,		KEY_RESERVED },	// (FREE)
/*3**/ { RESET,		KEY_RESERVED },	// (FREE)
/*4**/ { RESET,		KEY_RESERVED },	// (FREE)
/*5**/ { RESET,		KEY_RESERVED },	// (FREE)
/*6**/ { RESET,		KEY_RESERVED },	// (FREE)
/*7**/ { RESET,		KEY_RESERVED },	// (FREE)
/*8**/ { RESET,		KEY_RESERVED },	// (FREE)
/*9**/ { RESET,		KEY_RESERVED },	// (FREE)

#if 0
/*#**/ { 0,		KEY_RESERVED },	// Not possible! # is already repeat
/****/ { 0,		TONUM },	// ENTER NUMBER MODE
#endif

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
//	*INDENT-ON*
};

///
///	American keyboard layout default quote mapping.
///
static const OHKey AOHKUsQuoteTable[11 * 10 + 1 + 8] = {
//	*INDENT-OFF*
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
/*053*/ { RESET,	KEY_RESERVED },	// (FREE)
/*054*/ { SHIFT,	KEY_SPACE },	// SHIFT Space
/*055*/ { 0,		KEY_F5	},	// F5
/*056*/ { SHIFT,	KEY_TAB	},	// SHIFT Tabulator
/*057*/ { QUAL,		Q_GUI_R },	// Gui_R
/*058*/ { SHIFT,	KEY_LEFTBRACE },// { (QUOTE [, doubled)
/*059*/ { RESET,	KEY_RESERVED },	// (FREE)
/*060*/ { RESET,	KEY_RESERVED },	// RESET
/*061*/ { SHIFT,	KEY_E	},	// NO: ¤
/*062*/ { SHIFT,	KEY_B	},	// B
/*063*/ { SHIFT,	KEY_C	},	// NO: ¢
/*064*/ { 0,		KEY_KPASTERISK },// KP-*
/*065*/ { SHIFT,	KEY_O	},	// O
/*066*/ { 0,		KEY_F6	},	// F6
/*067*/ { 0,		KEY_KPPLUS },	// KP-+
/*068*/ { SHIFT,	KEY_U	},	// U
/*069*/ { RESET,	KEY_RESERVED },	// (FREE)
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
/*094*/ { RESET,	KEY_RESERVED },	// (FREE)
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

/*00**/ { 0,		KEY_RESERVED },	// Not possible! 00 is 0
/*01**/ { RESET,	KEY_RESERVED },	// (FREE)
/*02**/ { RESET,	KEY_RESERVED },	// (FREE)
/*03**/ { RESET,	KEY_RESERVED },	// (FREE)
/*04**/ { RESET,	KEY_RESERVED },	// (FREE)
/*05**/ { RESET,	KEY_RESERVED },	// (FREE)
/*06**/ { RESET,	KEY_RESERVED },	// (FREE)
/*07**/ { RESET,	KEY_RESERVED },	// (FREE)
/*08**/ { RESET,	KEY_RESERVED },	// (FREE)
/*09**/ { RESET,	KEY_RESERVED },	// (FREE)

/*000*/ { 0,		KEY_RESERVED },	// Not possible! 00 is 0

/*0USR1*/ { QUAL,	Q_SHFT_R },	// Shift_R
/*0USR2*/ { QUAL,	Q_CTRL_R },	// Ctrl_R
/*0USR3*/ { QUAL,	Q_ALT_R },	// Alt_R
/*0USR4*/ { QUAL,	Q_GUI_R },	// Gui_R
/*0USR5*/ { QUAL,	Q_SHFT_L },	// Shift_L
/*0USR6*/ { QUAL,	Q_CTRL_L },	// Ctrl_L
/*0USR7*/ { QUAL,	Q_ALT_L },	// Alt_L
/*0USR8*/ { QUAL,	Q_GUI_L },	// Gui_L
//	*INDENT-ON*
};

///
///	German keyboard layout default mapping.
///
static const OHKey AOHKDeTable[11 * 10 + 1 + 8] = {
//	*INDENT-OFF*
/*10*/ { QUOTE,		KEY_RESERVED },	// QUOTE
/*11*/ { 0,		KEY_1	},	// 1
/*12*/ { 0,		KEY_I	},	// i
/*13*/ { 0,		KEY_W	},	// w
/*14*/ { ALTGR,		KEY_RIGHTBRACE },// ~
/*15*/ { 0,		KEY_R	},	// r
/*16*/ { 0,		KEY_V	},	// v
/*17*/ { 0,		KEY_APOSTROPHE },// ä
/*18*/ { 0,		KEY_D	},	// d
//*19*/ { RESET,		KEY_RESERVED },	// (FREE)
/*19*/ { MACRO,		0 },		// macro test
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
// 9 * 10
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
// 10 * 10
/*0**/ { TONUM,		KEY_KP0 },	// (FREE)
/*1**/ { TONUM,		KEY_KP1 },	// (FREE)
/*2**/ { TONUM,		KEY_KP2 },	// (FREE)
/*3**/ { TONUM,		KEY_KP3 },	// (FREE)
/*4**/ { TONUM,		KEY_KP4 },	// (FREE)
/*5**/ { TONUM,		KEY_KP5 },	// (FREE)
/*6**/ { TONUM,		KEY_KP6 },	// (FREE)
/*7**/ { TONUM,		KEY_KP7 },	// (FREE)
/*8**/ { TONUM,		KEY_KP8 },	// (FREE)
/*9**/ { TONUM,		KEY_KP9 },	// (FREE)
// 11 * 10
/*00*/ { 0,		KEY_0	},	// 0
// 11 * 10 + 1
/*USR1*/ { QUAL,	Q_SHFT_L },	// Shift_L
/*USR2*/ { QUAL,	Q_CTRL_L },	// Ctrl_L
/*USR3*/ { QUAL,	Q_ALT_L },	// Alt_L
/*USR4*/ { QUAL,	Q_GUI_L },	// Gui_L
/*USR5*/ { QUAL,	Q_SHFT_R },	// Shift_R
/*USR6*/ { QUAL,	Q_CTRL_R },	// Ctrl_R
/*USR7*/ { QUAL,	Q_ALT_R },	// Alt_R
/*USR8*/ { QUAL,	Q_GUI_R },	// Gui_R

// FIXME: Should I move game mode table to here?
//	*INDENT-ON*
};

///
///	German keyboard layout default quote mapping.
///
static const OHKey AOHKDeQuoteTable[11 * 10 + 1 + 8] = {
//	*INDENT-OFF*
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
/*053*/ { RESET,	KEY_RESERVED },	// (FREE)
/*054*/ { SHIFT,	KEY_SPACE },	// SHIFT Space
/*055*/ { 0,		KEY_F5	},	// F5
/*056*/ { SHIFT,	KEY_TAB	},	// SHIFT Tabulator
/*057*/ { QUAL,		Q_GUI_R },	// Gui_R
/*058*/ { ALTGR,	KEY_6	},	// { (QUOTE [, doubled)
/*059*/ { RESET,	KEY_RESERVED },	// (FREE)
/*060*/ { RESET,	KEY_RESERVED },	// RESET
/*061*/ { ALTGR,	KEY_E	},	// ¤
/*062*/ { SHIFT,	KEY_B	},	// B
/*063*/ { ALTGR,	KEY_C	},	// ¢
/*064*/ { 0,		KEY_KPASTERISK },// KP-*
/*065*/ { SHIFT,	KEY_O	},	// O
/*066*/ { 0,		KEY_F6	},	// F6
/*067*/ { 0,		KEY_KPPLUS },	// KP-+
/*068*/ { SHIFT,	KEY_U	},	// U
/*069*/ { RESET,	KEY_RESERVED },	// (FREE)
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
/*094*/ { RESET,	KEY_RESERVED },	// (FREE)
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

/*00**/ { 0,		KEY_RESERVED },	// Not possible! 00 is 0
/*01**/ { RESET,	KEY_RESERVED },	// (FREE)
/*02**/ { RESET,	KEY_RESERVED },	// (FREE)
/*03**/ { RESET,	KEY_RESERVED },	// (FREE)
/*04**/ { RESET,	KEY_RESERVED },	// (FREE)
/*05**/ { RESET,	KEY_RESERVED },	// (FREE)
/*06**/ { RESET,	KEY_RESERVED },	// (FREE)
/*07**/ { RESET,	KEY_RESERVED },	// (FREE)
/*08**/ { RESET,	KEY_RESERVED },	// (FREE)
/*09**/ { RESET,	KEY_RESERVED },	// (FREE)

/*000*/ { 0,		KEY_RESERVED },	// Not possible! 00 is 0

/*0USR1*/ { QUAL,	Q_SHFT_R },	// Shift_R
/*0USR2*/ { QUAL,	Q_CTRL_R },	// Ctrl_R
/*0USR3*/ { QUAL,	Q_ALT_R },	// Alt_R
/*0USR4*/ { QUAL,	Q_GUI_R },	// Gui_R
/*0USR5*/ { QUAL,	Q_SHFT_L },	// Shift_L
/*0USR6*/ { QUAL,	Q_CTRL_L },	// Ctrl_L
/*0USR7*/ { QUAL,	Q_ALT_L },	// Alt_L
/*0USR8*/ { QUAL,	Q_GUI_L },	// Gui_L
//	*INDENT-ON*
};

///
///	Setup compiled keyboard mappings.
///
///	@param lang	two character iso language code
///
void AOHKSetLanguage(const char *lang)
{
    unsigned i;

    Debug(2, "Set Language '%s'\n", lang);
    if (!strcmp("de", lang)) {
	memcpy(AOHKTable, AOHKDeTable, sizeof(AOHKTable));
	memcpy(AOHKQuoteTable, AOHKDeQuoteTable, sizeof(AOHKQuoteTable));
    } else if (!strcmp("us", lang)) {
	memcpy(AOHKTable, AOHKUsTable, sizeof(AOHKTable));
	memcpy(AOHKQuoteTable, AOHKUsQuoteTable, sizeof(AOHKQuoteTable));
    } else {
	Debug(0, "Language '%s' isn't suported\n", lang);
	return;
    }

    if (1) {
	OHKey version[] = {
	    {0, KEY_A}
	    ,
	    {0, KEY_O}
	    ,
	    {0, KEY_H}
	    ,
	    {0, KEY_K}
	    ,
	    {0, KEY_SPACE}
	    ,
	    {0, KEY_V}
	    ,
	    {0, KEY_0}
	    ,
	    {0, KEY_DOT}
	    ,
	    {0, KEY_9}
	    ,
	    {0, KEY_0}
	    ,
	    {0, KEY_RESERVED}
	    ,
	};
	//AOHKString2Macro("aohk v0.06");

	AddMacro(version);
    }
    //
    //	Convert Normal table into macro table. Adding Ctrl
    //
    for (i = 0; i < sizeof(AOHKTable) / sizeof(*AOHKTable); ++i) {
	int mod;

	switch (AOHKTable[i].Modifier) {
	    case RESET:
	    case QUOTE:
	    case QUAL:
	    case STICKY:
	    case TOGAME:
	    case TONUM:
	    case SPECIAL:
	    case MACRO:
		mod = AOHKTable[i].Modifier;
		break;
	    default:
		mod = AOHKTable[i].Modifier ^ CTL;
		break;
	}
	AOHKMacroTable[i].Modifier = mod;
	AOHKMacroTable[i].KeyCode = AOHKTable[i].KeyCode;
    }
    //
    //	Convert Quote table into quote macro table. Adding Ctrl
    //
    for (i = 0; i < sizeof(AOHKQuoteTable) / sizeof(*AOHKQuoteTable); ++i) {
	int mod;

	switch (AOHKQuoteTable[i].Modifier) {
	    case RESET:
	    case QUOTE:
	    case QUAL:
	    case STICKY:
	    case TOGAME:
	    case TONUM:
	    case SPECIAL:
	    case MACRO:
		mod = AOHKQuoteTable[i].Modifier;
		break;
	    default:
		mod = AOHKQuoteTable[i].Modifier ^ CTL;
		break;
	}
	AOHKMacroQuoteTable[i].Modifier = mod;
	AOHKMacroQuoteTable[i].KeyCode = AOHKQuoteTable[i].KeyCode;
    }
    //
    //	Convert Normal table into super quote table. Adding Alt
    //
    for (i = 0; i < sizeof(AOHKTable) / sizeof(*AOHKTable); ++i) {
	int mod;

	switch (AOHKTable[i].Modifier) {
	    case RESET:
	    case QUOTE:
	    case QUAL:
	    case STICKY:
	    case TOGAME:
	    case TONUM:
	    case SPECIAL:
	    case MACRO:
		mod = AOHKTable[i].Modifier;
		break;
	    default:
		mod = AOHKTable[i].Modifier ^ ALT;
		break;
	}
	AOHKSuperTable[i].Modifier = mod;
	AOHKSuperTable[i].KeyCode = AOHKTable[i].KeyCode;
    }
}

/// @}
