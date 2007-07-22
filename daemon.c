//

/**@name daemon.c	-	Keyboard handler daemon.	*/
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
#include <linux/uinput.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <poll.h>

#include "aohk.h"

#define SYSTEM_VENDOR_ID	0x0001	// System vendor
#define PS2_KEYBOARD_ID		0x0001	// System keyboard

#define BELKIN_VENDOR_ID	0x050D	// Belkin's vendor ID
#define NOSTROMO_N50_ID		0x0805	// n50 USB ID
#define NOSTROMO_N52_ID		0x0815	// n52 USB ID

//
//      Table of supported input devices
//
struct input_device
{
    int Vendor;
    int Product;
} InputDevices[] = {
    {
//    SYSTEM_VENDOR_ID, PS2_KEYBOARD_ID}, {
    BELKIN_VENDOR_ID, NOSTROMO_N50_ID}, {
    BELKIN_VENDOR_ID, NOSTROMO_N52_ID}, {
    }
};

#define MAX_INPUTS	32
int InputFds[MAX_INPUTS];		// Inputs
int InputFdsN;				// Number of Inputs

int UInputFd;				// Output

//
//      Open input event device.
//
int OpenEvent(void)
{
    char dev[32];
    char *s;
    int i;
    int j;
    int fd;
    struct input_id info;

    strcpy(dev, "/dev/input/event");
    s = dev + sizeof("/dev/input/event") - 1;
    s[1] = '\0';
    s[2] = '\0';
    for (i = 0; i < 32; ++i) {		// 0 - 31 supported
	if (i < 10) {
	    s[0] = '0' + i;
	} else {
	    s[0] = '0' + i / 10;
	    s[1] = '0' + i % 10;
	}
	// printf("Trying '%s'\n", dev);
	if ((fd = open(dev, O_RDWR)) >= 0) {
	    // Open ok
	    // printf("Open success\n");
	    if (ioctl(fd, EVIOCGID, &info) == 0) {
		//printf("BUS: %04X Vendor: %04X Product: %04X Version: %04X\n",
		//    info.bustype, info.vendor, info.product, info.version);
		for (j = 0; InputDevices[j].Vendor; ++j) {
		    if (InputDevices[j].Vendor == info.vendor
			&& InputDevices[j].Product == info.product) {
			printf("Device found BUS: %04X Vendor: %04X Product: "
			    "%04X Version: %04X\n",
			info.bustype, info.vendor, info.product, info.version);
			// Grab the device for exclusive use
			if (ioctl(fd, EVIOCGRAB, 1) < 0) {
			    perror("ioctl(EVIOCGRAB)");
			}
			if (InputFdsN<MAX_INPUTS) {
			    InputFds[InputFdsN++] = fd;
			}
			goto found;
		    }
		}
	    } else {
		perror("ioctl(EVIOCGID)");
	    }
	    close(fd);
	} else {
	    // perror("open()");
	}
found:	;
    }

    if( !InputFdsN) {
	printf("No useable device found.\n");
	return 1;
    }

    return 0;
}

//
//      Open uinput device
//
int OpenUInput(void)
{
    int i;
    int fd;
    struct uinput_user_dev device;
    unsigned char aux;

    //  open uinput device file
    if ((fd = open("/dev/misc/uinput", O_RDWR)) < 0
	&& (fd = open("/dev/input/uinput", O_RDWR)) < 0
	&& (fd = open("/dev/uinput", O_RDWR)) < 0) {
	perror("open()");
	return fd;
    }

    memset(&device, 0, sizeof(device));

    //  sets the name of our device
    strncpy(device.name, "ALE OneHand Keyboard", UINPUT_MAX_NAME_SIZE);

    //  its bus
    device.id.bustype = BUS_USB;

    //  and vendor id/product id/version
    device.id.vendor = 0x414C;
    device.id.product = 0x4F48;
    device.id.version = 0x0001;

    //  inform that we'll generate key events
    ioctl(fd, UI_SET_EVBIT, EV_KEY);

    //  set key events we can generate (in this case, all)
    for (i = 1; i < /*KEY_MAX */ 255; i++) {
	ioctl(fd, UI_SET_KEYBIT, i);
    }

    //  write down information for creating a new device
    if (write(fd, &device, sizeof(struct uinput_user_dev)) < 0) {
	perror("write");
	close(fd);
	return -1;
    }
    //  actually creates the device
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
	perror("ioctl(UI_DEV_CREATE)");
	close(fd);
	return -1;
    }

    return fd;
}

//
//      Send keydown event
//
int UInputKeydown(int fd, int code)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));

    event.type = EV_KEY;
    event.code = code;
    event.value = 1;

    return write(fd, &event, sizeof(event));
}

//
//      Send keyup event
//
int UInputKeyup(int fd, int code)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));

    event.type = EV_KEY;
    event.code = code;
    event.value = 0;

    return write(fd, &event, sizeof(event));
}

//
//      Close UInput
//
void CloseUInput(int fd)
{
    if (ioctl(fd, UI_DEV_DESTROY) < 0) {
	perror("ioctl(UI_DEV_DESTROY)");
    }
}

//
//	Input read
//
void InputRead(int fd)
{
     struct input_event ev;

    if( read(fd, &ev, sizeof(ev))!=sizeof(ev)) {
	perror("read()");
	return;
    }
    switch (ev.type) {
	case EV_KEY:			// Key event
	    printf("Key %d %s\n", ev.code, ev.value ? "pressed" : "released");
	    // Feed into AOHK Statemachine
	    AOHKFeedKey(ev.code, ev.value);
	    break;
	case EV_SYN:			// Synchronization events
	    // Ignore
	    break;
	default:
	    printf("Event Type(%d,%d,%d) unsupported\n", ev.type, ev.code,
	    	ev.value);
	    break;
    }
}

//
//	Event Loop
//
void EventLoop(void)
{
    struct pollfd fds[MAX_INPUTS+1];
    int ret;
    int i;

    for (i=0; i<InputFdsN; ++i) {
	printf("Wait on %d\n", InputFds[i]);
	fds[i].fd = InputFds[i];
	fds[i].events = POLLIN;
	fds[i].revents = 0;
    }

    for(;;) {
	ret = poll(fds, InputFdsN, 1000*30);
	if (ret<0) {		// -1 error
	    perror("poll()");
	    return;
	}
	if (!ret) {
	    printf("Timeout\n");
	    return;
	}
	for (i=0; i<InputFdsN; ++i) {
	    if (fds[i].revents) {
		printf("Imput avail\n");
		InputRead(fds[i].fd);
		fds[i].revents = 0;
	    }
	}
    }
}

//
//	Key out
//
void KeyOut(int key, int pressed)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));

    event.type = EV_KEY;
    event.code = code;
    event.value = pressed;

    return write(UInputFd, &event, sizeof(event));
}

//
//      Main entry point.
//
int main(int argc, char **argv)
{
    int i;
    int ufd;

    if (OpenEvent()) {
	exit(-1);
    }

    ufd = OpenUInput();
    if (ufd) {
	UInputFd = ufd;
	EventLoop();

	CloseUInput(ufd);
    }
    for (i=0; i<InputFdsN; ++i) {
	close(InputFds[i]);
    }

    return 0;
}
