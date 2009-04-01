/*
 * input-helper.c - find and open input devices
 *
 * Copyright © 2009 Evgeni Golov <sargentd@die-welt.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <sys/ioctl.h>
#include <linux/input.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int device_open(int id) {
	char node[32];
	int fd;

	snprintf(node, 32, "/dev/input/event%d", id);
	fd = open(node,O_RDONLY);
	if (fd < 0)
		return -1;

	return fd;
}

int device_find_byphys(char *phys) {
	int fd, i, rc;
	char buf[1024];

	for (i = 0; i < 32; i++) {
		fd = device_open(i);
		if (fd > 0) {
		        rc = ioctl(fd,EVIOCGPHYS(sizeof(buf)),buf);
			if (rc >= 0 && strcmp(phys, buf) == 0) {
				close(fd);
				return i;
			}
		}
		close(fd);
	}
	return -1;
}

int device_find_byname(char *name) {
	int fd, i, rc;
	char buf[1024];

	for (i = 0; i < 32; i++) {
		fd = device_open(i);
		if (fd > 0) {
		        rc = ioctl(fd,EVIOCGNAME(sizeof(buf)),buf);
			if (rc >= 0 && strcmp(name, buf) == 0) {
				close(fd);
				return i;
			}
		}
		close(fd);
	}
	return -1;
}
