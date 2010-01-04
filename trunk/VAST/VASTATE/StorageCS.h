/*
 * VAST, a scalable Agent-to-Agent network for virtual environments
 * Copyright (C) 2007 Shun-Yun Hu (syhu@yahoo.com)
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*
 *  Storage.h -- VASTATE Storage class (client-server implementation)
 *
 *  ver 0.1 (2006/10/03)
 *   
 */

#ifndef VASTATE_STORAGE_CS_H
#define VASTATE_STORAGE_CS_H

#include "Storage.h"
#include "VASTnet.h"
#include "VASTTypes.h"

namespace Vast 
{     
    class StorageCS : public Storage
    {
    public:
        StorageCS (StorageLogic *logic, id_t id)      
            :Storage (id), _logic (logic), _query_id_count (0)
        {
            // make sure StorageLogic has access to 'StorageCS' to send back data
            _logic->register_Storage (this);
        }
        
        virtual ~StorageCS () 
        {
        }

        //
        // Storage interface
        //
        
        // sends a query with a given 'type' number, returns a unique id for this query
        query_id_t query (char *query, size_t size);

        // sends the results of a query back to the requester
        bool respond (query_id_t query_id, char *reply, size_t size);

        // process messages sent by vastnode
        bool handleMessage (Message &in_msg);

        // pass in a base ID to generate all future query_id's for this Storage
        bool init_id (id_t id);

    private:
        StorageLogic *_logic;

        // mapping between a query_id and which Arbitrator had sent this query
        // used when sending reply back to the Arbitrator via respond ()
        map<query_id_t, id_t> _querymap;

        // counters
        query_id_t _query_id_count;                // internal counter for query id

        // buffers
        char _buf[VASTATE_BUFSIZ];

        // debug
        errout  _eo;
        char    _str[VASTATE_BUFSIZ];

    };
    
} // end namespace Vast

#endif // #define VASTATE_STORAGE_CS_H

