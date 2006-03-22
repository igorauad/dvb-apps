/*
 * libdvbca - interface onto raw CA devices
 *
 * Copyright (C) 2006 Andrew de Quincey (adq_dvb@lidskialf.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <linux/dvb/ca.h>
#include "dvbca.h"


int dvbca_open(int adapter, int cadevice)
{
	char filename[PATH_MAX+1];
	int fd;

	sprintf(filename, "/dev/dvb/adapter%i/ca%i", adapter, cadevice);
	if ((fd = open(filename, O_RDWR)) < 0) {
		// if that failed, try a flat /dev structure
		sprintf(filename, "/dev/dvb%i.ca%i", adapter, cadevice);
		fd = open(filename, O_RDWR);
	}

	return fd;
}

int dvbca_reset(int fd)
{
	return ioctl(fd, CA_RESET);
}

int dvbca_get_interface_type(int fd)
{
	ca_slot_info_t info;

	memset(&info, 0, sizeof(info));
	if (ioctl(fd, CA_GET_SLOT_INFO, &info))
		return -1;

	if (info.type & CA_CI_LINK)
		return DVBCA_INTERFACE_LINK;
	if (info.type & CA_CI)
		return DVBCA_INTERFACE_HLCI;

	return -1;
}

int dvbca_get_cam_state(int fd)
{
	ca_slot_info_t info;

	memset(&info, 0, sizeof(info));
	if (ioctl(fd, CA_GET_SLOT_INFO, &info))
		return -1;

	if (info.flags == 0)
		return DVBCA_CAMSTATE_MISSING;
	if (info.flags & CA_CI_MODULE_READY)
		return DVBCA_CAMSTATE_READY;
	if (info.flags & CA_CI_MODULE_PRESENT)
		return DVBCA_CAMSTATE_INITIALISING;

	return -1;
}

int dvbca_link_write(int fd, uint8_t connection_id,
		     uint8_t *data, uint16_t data_length)
{
	uint8_t *buf = malloc(data_length + 2);
	if (buf == NULL)
		return -1;

	buf[0] = 0;
	buf[1] = connection_id;
	memcpy(buf+2, data, data_length);

	int result = write(fd, buf, data_length+2);
	free(buf);
	return result;
}

int dvbca_link_writev(int fd, uint8_t connection_id,
		      struct iovec *vector, int count)
{
	uint32_t data_length = 0;
	int i;

	// allocate buffer space
	for(i=0; i< count; i++) {
		data_length += vector[i].iov_len;
	}
	uint8_t *buf = malloc(data_length + 2);
	if (buf == NULL)
		return -1;

	// merge IOVs
	uint32_t pos = 2;
	for(i=0; i< count; i++) {
		memcpy(buf+pos, vector[i].iov_base, vector[i].iov_len);
		pos += vector[i].iov_len;
	}

	// the header
	buf[0] = 0;
	buf[1] = connection_id;

	// write it
	int result = write(fd, buf, data_length+2);
	free(buf);
	return result;
}

int dvbca_link_read(int fd, uint8_t *connection_id,
		     uint8_t *data, uint16_t data_length)
{
	int size;

	uint8_t *buf = malloc(data_length + 2);
	if (buf == NULL)
		return -1;

	if ((size = read(fd, buf, data_length+2)) < 2)
		return -1;

	if (buf[0] != 0)
		return -1;
	*connection_id = buf[1];
	memcpy(data, buf+2, size-2);
	free(buf);

	return size - 2;
}
