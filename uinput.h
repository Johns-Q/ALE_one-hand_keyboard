///
///	@file uinput.h	@brief	linux uinput modul header file
///
///	Copyright (c) 2007,2009 by Lutz Sammer.  All Rights Reserved.
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

/// @addtogroup uinput
/// @{

#define UINPUT_MAX_ABS_X 1023		///< abs x max
#define UINPUT_MAX_ABS_Y 1023		///< abs y max

//----------------------------------------------------------------------------
//	Prototypes
//----------------------------------------------------------------------------

extern int OpenUInput(const char *);	///< open uinput
extern int UInputKeydown(int, int);	///< send key down event
extern int UInputKeyup(int, int);	///< send key up event
extern int UInputAbsX(int, int);	///< send tablet x event
extern int UInputAbsY(int, int);	///< send tablet y event
extern int UInputRelX(int, int);	///< send mouse x event
extern int UInputRelY(int, int);	///< send mouse y event
extern int UInputSyn(int);		///< send syn report event
extern void CloseUInput(int);		///< close uinput

/// @}
