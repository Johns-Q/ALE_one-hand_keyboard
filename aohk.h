///
///	@file aohk.h	@brief	ALE OneHand Keyboard headerfile.
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

/// @addtogroup aohk
/// @{

///
///	Internal used keys.
///
enum __aohk_internal_keys__
{
    AOHK_KEY_0,				///< internal key 0 (quote)
    AOHK_KEY_1,				///< internal key 1
    AOHK_KEY_2,				///< internal key 2
    AOHK_KEY_3,				///< internal key 3
    AOHK_KEY_4,				///< internal key 4
    AOHK_KEY_5,				///< internal key 5
    AOHK_KEY_6,				///< internal key 6
    AOHK_KEY_7,				///< internal key 7
    AOHK_KEY_8,				///< internal key 8
    AOHK_KEY_9,				///< internal key 9
    AOHK_KEY_HASH,			///< internal key # (repeat)

    AOHK_KEY_STAR,			///< internal key * (macro)

    AOHK_KEY_USR_1,			///< internal user defined key 1
    AOHK_KEY_USR_2,			///< internal user defined key 2
    AOHK_KEY_USR_3,			///< internal user defined key 3
    AOHK_KEY_USR_4,			///< internal user defined key 4

    AOHK_KEY_USR_5,			///< internal user defined key 5
    AOHK_KEY_USR_6,			///< internal user defined key 6
    AOHK_KEY_USR_7,			///< internal user defined key 7
    AOHK_KEY_USR_8,			///< internal user defined key 8

    AOHK_KEY_SPECIAL,			///< internal key (special)

    AOHK_KEY_NOP,			///< internal key: no function
};

extern int AOHKTimeout;			///< out: timeout in ms needed
extern int AOHKExit;			///< out: exit flag

extern void AOHKKeyOut(int, int);	///< out: key code, press
extern void AOHKShowLED(int, int);	///< out: show led

    /// Check current state
extern int AOHKCheckOffState(void);

    /// Handle internal key symbol
extern int AOHKFeedSymbol(unsigned long, int, int);

    /// Handle key
extern void AOHKFeedKey(unsigned long, int, int);

    /// Handle timeout
extern void AOHKFeedTimeout(int);

    /// Set convert table
extern void AOHKSetupConvertTable(const int *);

    /// Save internal tables
extern void AOHKSaveTable(const char *);

    /// Load internal tables
extern void AOHKLoadTable(const char *);

    /// Set language, changes mapping
extern void AOHKSetLanguage(const char *);

/// @}
