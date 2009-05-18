//

/**@name aohk.h	-	ALE OneHand Keyboard headerfile.	*/
//
//	Copyright (c) 2007 by Lutz Sammer.  All Rights Reserved.
//
//	Contributor(s):
//
//	This file is part of ALE one-hand keyboard
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; only version 2 of the License.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	$Id$
////////////////////////////////////////////////////////////////////////////

//
//	Internal used keys.
//
enum
{
    OH_QUOTE,				// keypad 0
    OH_KEY_1,				// keypad 1
    OH_KEY_2,				// keypad 2
    OH_KEY_3,				// keypad 3
    OH_KEY_4,				// keypad 4
    OH_KEY_5,				// keypad 5
    OH_KEY_6,				// keypad 6
    OH_KEY_7,				// keypad 7
    OH_KEY_8,				// keypad 8
    OH_KEY_9,				// keypad 9
    OH_REPEAT,				// keypad ENTER

    OH_MACRO,				// keypad .

    OH_USR_1,				// keypad %
    OH_USR_2,				// keypad *
    OH_USR_3,				// keypad -
    OH_USR_4,				// keypad +

    OH_USR_5,				// keypad esc (future or special number pad)
    OH_USR_6,				// keypad ctrl
    OH_USR_7,				// keypad alt
    OH_USR_8,				// keypad bs

    OH_SPECIAL,				// keypad NUM-LOCK

    OH_NOP,				// no function
};

extern int Timeout;			// out: timeout used
extern int Exit;			// out: exit

extern void KeyOut(int, int);		// out: key code, press
extern void ShowLED(int, int);		// out: show led

extern void AOHKFeedKey(unsigned long, int, int);
extern void AOHKFeedTimeout(int);
extern void AOHKSetupConvertTable(const int *);

extern void AOHKSaveTable(const char *);
extern void AOHKLoadTable(const char *);

extern void AOHKSetLanguage(const char *);
