///
///	@file uinput.c	@brief	linux uinput modul
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

///	@defgroup uinput The uinput module.
///
///	Collection of uinput routines.
///

#include <linux/input.h>
#include <linux/uinput.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "uinput.h"

///
///	Open uinput device
///
///	@param name	name of the uinput device
///
///	@returns uinput file descriptor, -1 if failure.
///
int OpenUInput(const char *name)
{
    int i;
    int fd;
    char buf[64];
    struct uinput_user_dev device;

    //	open uinput device file
    if ((fd = open("/dev/input/uinput", O_WRONLY)) < 0
	&& (fd = open("/dev/misc/uinput", O_WRONLY)) < 0
	&& (fd = open("/dev/uinput", O_WRONLY)) < 0) {
	perror("open()");
	return fd;
    }

    memset(&device, 0, sizeof(device));

    //	sets the name of our device
    strncpy(device.name, name, UINPUT_MAX_NAME_SIZE);

    //	its bus
    device.id.bustype = BUS_USB;

    //	and vendor id/product id/version
    device.id.vendor = 0x414C;
    device.id.product = 0x4F48;
    device.id.version = 0x0001;

    device.absmin[ABS_X] = 0;
    device.absmin[ABS_Y] = 0;
    device.absmax[ABS_X] = UINPUT_MAX_ABS_X;
    device.absmax[ABS_Y] = UINPUT_MAX_ABS_Y;

    device.absfuzz[ABS_X] = 4;
    device.absfuzz[ABS_Y] = 4;
    device.absflat[ABS_X] = 2;
    device.absflat[ABS_Y] = 2;

    //	mouse emulation
    //
    //	inform that we'll generate relative axis events
    ioctl(fd, UI_SET_EVBIT, EV_REL);
    ioctl(fd, UI_SET_EVBIT, EV_SYN);
    ioctl(fd, UI_SET_RELBIT, REL_X);
    ioctl(fd, UI_SET_RELBIT, REL_Y);

#if 1
    //	joystick/touchpad emulation
    //
    //	inform that we'll generate absolute axis events
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    ioctl(fd, UI_SET_ABSBIT, ABS_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_Y);
    //ioctl(fd, UI_SET_ABSBIT, ABS_PRESSURE);

    ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
#endif

    //	inform that we'll generate key events
    ioctl(fd, UI_SET_EVBIT, EV_KEY);

    //	set key events we can generate (in this case, all)
    for (i = 1; i < /*KEY_MAX */ 255; i++) {
	ioctl(fd, UI_SET_KEYBIT, i);
    }

    ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(fd, UI_SET_KEYBIT, BTN_MIDDLE);

    //	write down information for creating a new device
    if (write(fd, &device, sizeof(struct uinput_user_dev)) < 0) {
	perror("write");
	close(fd);
	return -1;
    }
    //	set phys cookie
    snprintf(buf, sizeof(buf), "aohkd-%04x:%04x", device.id.vendor,
	device.id.product);
    ioctl(fd, UI_SET_PHYS, buf);

    //	actually creates the device
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
	perror("ioctl(UI_DEV_CREATE)");
	close(fd);
	return -1;
    }

    return fd;
}

///
///	Send keydown event
///
///	@param fd	uinput file descriptor
///	@param code	scancode
///
///	@returns -1 if failure
///
///	@see /usr/include/linux/input.h for possible scancodes.
///
int UInputKeydown(int fd, int code)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));

    event.type = EV_KEY;
    event.code = code;
    event.value = 1;

    return write(fd, &event, sizeof(event));
}

///
///	Send keyup event
///
///	@param fd	uinput file descriptor
///	@param code	scancode
///
///	@returns -1 if failure
///
///	@see /usr/include/linux/input.h for possible scancodes.
///
int UInputKeyup(int fd, int code)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));

    event.type = EV_KEY;
    event.code = code;
    //event.value = 0;

    return write(fd, &event, sizeof(event));
}

///
///	Send absolute x event
///
///	@param fd	uinput file descriptor
///	@param x	absolute x coordinate
///
///	@returns -1 if failure
///
int UInputAbsX(int fd, int x)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));

    event.type = EV_ABS;
    event.code = ABS_X;
    event.value = x;

    return write(fd, &event, sizeof(event));
}

///
///	Send absolute y event
///
///	@param fd	uinput file descriptor
///	@param y	absolute y coordinate
///
///	@returns -1 if failure
///
int UInputAbsY(int fd, int y)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));

    event.type = EV_ABS;
    event.code = ABS_Y;
    event.value = y;

    return write(fd, &event, sizeof(event));
}

///
///	Send relative x event
///
///	@param fd	uinput file descriptor
///	@param x	relative x coordinate
///
///	@returns -1 if failure
///
int UInputRelX(int fd, int x)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));

    event.type = EV_REL;
    event.code = REL_X;
    event.value = x;

    return write(fd, &event, sizeof(event));
}

///
///	Send relative y event
///
///	@param fd	uinput file descriptor
///	@param y	relative y coordinate
///
///	@returns -1 if failure
///
int UInputRelY(int fd, int y)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));

    event.type = EV_REL;
    event.code = REL_Y;
    event.value = y;

    return write(fd, &event, sizeof(event));
}

///
///	Send syn event
///
///	@param fd	uinput file descriptor
///
///	@returns -1 if failure
///
int UInputSyn(int fd)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));
    //gettimeofday(&event.time, NULL);

    event.type = EV_SYN;
    event.code = SYN_REPORT;
    // event.value = 0;

    return write(fd, &event, sizeof(event));
}

///
///	Close UInput
///
///	@param fd	uinput file descriptor
///
void CloseUInput(int fd)
{
    if (ioctl(fd, UI_DEV_DESTROY) < 0) {
	perror("ioctl(UI_DEV_DESTROY)");
    }
}
