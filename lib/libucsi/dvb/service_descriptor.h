/*
 * section and descriptor parser
 *
 * Copyright (C) 2005 Kenneth Aafloy (kenneth@linuxtv.org)
 * Copyright (C) 2005 Andrew de Quincey (adq_dvb@lidskialf.net)
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

#ifndef _UCSI_DVB_SERVICE_DESCRIPTOR
#define _UCSI_DVB_SERVICE_DESCRIPTOR 1

#include <ucsi/descriptor.h>
#include <ucsi/endianops.h>

/**
 * dvb_service_descriptor structure.
 */
struct dvb_service_descriptor {
	struct descriptor d;

	uint8_t service_type;
	uint8_t service_provider_name_length;
	/* uint8_t service_provider_name[] */
	/* struct dvb_service_descriptor_part2 part2 */
} packed;

/**
 * Second part of a dvb_service_descriptor following the variable length
 * service_provider_name field.
 */
struct dvb_service_descriptor_part2 {
	uint8_t service_name_length;
	/* uint8_t service_name[] */
} packed;

/**
 * Process a dvb_service_descriptor.
 *
 * @param d Generic descriptor pointer.
 * @return dvb_service_descriptor pointer, or NULL on error.
 */
static inline struct dvb_service_descriptor*
	dvb_service_descriptor_codec(struct descriptor* d)
{
	struct dvb_service_descriptor *p =
		(struct dvb_service_descriptor *) d;
	struct dvb_service_descriptor_part2 *p2;
	int pos = sizeof(struct dvb_service_descriptor) - 2;
	int len = d->len;

	if (pos > len)
		return NULL;

	pos += p->service_provider_name_length;

	if (pos > len)
		return NULL;

	p2 = (struct dvb_service_descriptor_part2*) ((uint8_t*) d + 2 + pos);

	pos += sizeof(struct dvb_service_descriptor_part2);

	if (pos > len)
		return NULL;

	pos += p2->service_name_length;

	if (pos != len)
		return NULL;

	return p;
}

/**
 * Accessor for the service_provider_name field of a dvb_service_descriptor.
 *
 * @param d dvb_service_descriptor pointer.
 * @return Pointer to the service_provider_name field.
 */
static inline uint8_t *
	dvb_service_descriptor_service_provider_name(struct dvb_service_descriptor *d)
{
	return (uint8_t *) d + sizeof(struct dvb_service_descriptor);
}

/**
 * Accessor for the second part of a dvb_service_descriptor.
 *
 * @param d dvb_service_descriptor pointer.
 * @return dvb_service_descriptor_part2 pointer.
 */
static inline struct dvb_service_descriptor_part2 *
	dvb_service_descriptor_part2(struct dvb_service_descriptor *d)
{
	return (struct dvb_service_descriptor_part2 *)
		((uint8_t*) d + sizeof(struct dvb_service_descriptor) +
		 d->service_provider_name_length);
}

/**
 * Accessor for the service_name field of a dvb_service_descriptor_part2.
 *
 * @param d dvb_service_descriptor_part2 pointer.
 * @return Pointer to the service_name field.
 */
static inline uint8_t *
	dvb_service_descriptor_service_name(struct dvb_service_descriptor_part2 *d)
{
	return (uint8_t *) d + sizeof(struct dvb_service_descriptor_part2);
}

#endif