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

//
//	Internal used keys.

#include "aohk.h"

static char AOHKeyboardOff;              // my keyboard disabled

//
//
//	OneHand Statemachine.
//
void AOHKFeedKey(int key, int pressed)
{
    if (AOHKeyboardOff) {		// Completly turned off
	KeyOut(key, pressed);
    }
    KeyOut(key, pressed);
}
