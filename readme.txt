
	@file readme.txt	-	Readme intro.

	Copyright (c) 2007,2009 by Lutz Sammer.  All Rights Reserved.

	Contributor(s):

	This file is part of ALE one-hand keyboard

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; only version 2 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	$Id$
----------------------------------------------------------------------------

This is an userland driver for the ALE one-hand keyboard.

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

Read access to /dev/input/event*
Write access to /dev/input/uinput

Supported input devices:
-----------------------
PS/2 PC Keyboard
Nostromo(TM) SpeedPad n52
Saitek's Pro Gamer Command Unit
Apple wireless keyboard
Apple aluminium wireless keyboard
Cherry USB keypad
Fujitsu Siemens USB keypad

any other usb/bluetooth HID device

How to build:
------------

Just run make and start aohkd.

make && ./aohkd

How to use:
-----------

To load your language mapping use:

	./aohkd pc102numpad.map de.default.map

    or for builtin language support

	./aohkd -l de

To use only one input device use:

	./aohkd -d n52		# Nostrome n52
	./aohkd -d pc102	# 102 PC Keyboard
	./aohkd -d keypad	# Cherry Keypad G84-4700

To use a not builtin device:

	./aohkd -v 0x046A -p 0x014A pc102leftside.map us.default.map

	-v 0x046A		is the vendor id
	-p 0x014A		is the product id
	pc102leftside.map	is the input key mapping
	us.default.map		is the output key mapping

Further documentation:
---------------------

Read aohk.txt for more informations
