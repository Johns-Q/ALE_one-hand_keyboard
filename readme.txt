//
/**@name readme.txt	-	Readme intro.	*/
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


This is a userland driver for the ALE one-hand keyboard.

Requirement:
------------

Linux Kernel 2.6

Compiled with uinput.
	Enable it here:
	Device Drivers  --->
	Input device support  --->
	[*] Miscellaneous devices  --->
	<*>   User level driver support
Input driver of your keyboard hid / usb / bluetooth.

Supported input devices:
-----------------------
Nostromo(TM) SpeedPad n52
Saitek's Pro Gamer Command Unit
PS/2 PC Keyboard

How to build:
------------

Just run make and start aohk-daemon.

make && ./aohk-daemon

How to use:
-----------

To load your language mapping use: ./aohk-daemon de.default.map

To use only one input device use:

./aohk-daemon -d n52	# Nostrome n52
./aohk-daemon -d pc102	# 102 PC Keyboard
./aohk-daemon -d keypad	# Cherry Keypad G84-4700

Further documentation:
---------------------

Read aohk.txt for more informations
