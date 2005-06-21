/**
 * dvbcfg_adapter configuration file support.
 *
 * Copyright (c) 2005 by Andrew de Quincey <adq_dvb@lidskialf.net>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#ifndef DVBCFG_ADAPTER_H
#define DVBCFG_ADAPTER_H

#include <dvbcfg_common.h>

/**
 * The adapters file describes each DVB adapter in a system, and indicates what source_ids
 * are receivable by each adapter. It consists of multiple lines as follows:
 *
 * <adapter_id> <source_id> ...
 *
 * <adapter_id> identifies a DVB adapter in the system. The following adapter_ids are supported:
 *
 *   DVB<dvb_type><adapter_number>.<frontend_number>
 *   ATSC<adapter_number>.<frontend_number>
 *
 *   <dvb_type> is one of 'S', 'T', or 'C'(DVBS, DVBT, and DVBC respectively)
 *   <adapter_number> is the number allocated to the DVB device by the OS (i.e. /dev/dvb/adapterX)
 *   <frontend_number> is the frontend ID on a particular DVB device (i.e. /dev/dvb/adapterX/frontendY)
 *
 * <source_id> corresponds to an entry in the dvbcfg_sources file. Multiple source_ids can
 * be specified for an adapter, indicating it can be automatically switched between them in
 * some manner (e.g. by using DISEQC for DVBS adapters).
 *
 * Comments begin with '#' - any characters after this will be ignored
 * to the end of the line.
 *
 * Examples:
 * DVBS0.0 S5E S7E
 * DVBC1.0
 * DVBT2.0 Tuk-BlackHill
 */


struct dvbcfg_adapter_entry {
        struct dvbcfg_source_id source_id;

        struct dvbcfg_adapter_entry *next;
};

/**
 * In-memory representation of a single adapter.
 */
struct dvbcfg_adapter {
        char *adapter_id;
        struct dvbcfg_adapter_entry *source_ids;

        struct dvbcfg_adapter *prev;    /* NULL=> this is the first entry */
        struct dvbcfg_adapter *next;    /* NULL=> this is the last entry */
};


/**
 * Load adapters from a config file.
 *
 * @param config_file Config filename to load.
 * @param adapters Where to put the pointer to the start of the loaded
 * adapters. If NULL, a new list will be created, if it points to an already initialised list,
 * the loaded adapters will be appended to it.
 * @return 0 on success, or nonzero error code on failure.
 */
extern int dvbcfg_adapter_load(char *config_file,
                               struct dvbcfg_adapter **adapters);

/**
 * Save adapters to a config file.
 *
 * @param config_file Config filename to save.
 * @param adapters Pointer to the list of adapters to save.
 * @return 0 on success, or nonzero error code on failure.
 */
extern int dvbcfg_adapter_save(char *config_file,
                               struct dvbcfg_adapter *adapters);

/**
 * Find the entry for a particular adapter_id.
 *
 * @param adapters Pointer to the list to search.
 * @param adapter_id adapter_id to find.
 * @return A dvbcfg_adapter structure if found, or NULL if not.
 */
extern struct dvbcfg_adapter *dvbcfg_adapter_find(struct dvbcfg_adapter
                                                  *adapters,
                                                  char *adapter_id);

/**
 * Find an adapter supporting a source_id.
 *
 * @param adapters Pointer to the list to search.
 * @param source_id source_id to find.
 * @return A dvbcfg_adapter structure if found, or NULL if not.
 */
extern struct dvbcfg_adapter *dvbcfg_adapter_find_source_id(struct dvbcfg_adapter* adapters,
                                                            struct dvbcfg_source_id* source_id);

/**
 * Does the supplied adapter support the supplied source_id?
 *
 * @param adapter Adapter to check
 * @param source_id sourec_id to check for.
 * @return 1 if it does, 0 if not.
 */
extern int dvbcfg_adapter_supports_source_id(struct dvbcfg_adapter* adapter,
                                             struct dvbcfg_source_id* source_id);

/**
 * Unlink a single adapter from a list, and free its memory.
 *
 * @param adapters The list of adapters.
 * @param tofree The adapter to free.
 */
extern void dvbcfg_adapter_free(struct dvbcfg_adapter **adapters,
                                struct dvbcfg_adapter *tofree);

/**
 * Free memory for all adapters in a list.
 *
 * @param adapters Pointer to list of adapters to free.
 */
extern void dvbcfg_adapter_free_all(struct dvbcfg_adapter *adapters);

#endif                          // DVBCFG_ADAPTER_H