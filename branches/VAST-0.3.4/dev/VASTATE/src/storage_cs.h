/*
 * VAST, a scalable peer-to-peer network for virtual environments
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
 *  storage.h -- VASTATE storage class (client-server implementation)
 *
 *  ver 0.1 (2006/10/03)
 *   
 */

#ifndef VASTATE_STORAGE_CS_H
#define VASTATE_STORAGE_CS_H

#include "storage.h"
#include "network.h"
#include "vastutil.h"

namespace VAST 
{     
    class storage_cs : public storage
    {
    public:
        storage_cs (storage_logic *logic)      
            :_logic (logic), _query_id_count (0)
        {
            // make sure storage_logic has access to 'storage_cs' to send back data
            _logic->register_storage (this);
        }
        
        virtual ~storage_cs () 
        {
        }

        //
        // storage interface
        //
        
        // sends a query with a given 'type' number, returns a unique id for this query
        query_id_t query (char *query, size_t size);

        // sends the results of a query back to the requester
        bool respond (query_id_t query_id, char *reply, size_t size);

        // process messages sent by vastnode
        bool handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size);

        // do things after messages are all handled
        void post_processmsg ()
        {
        }

        // pass in a base ID to generate all future query_id's for this storage
        bool init_id (id_t id);

    private:
        storage_logic *_logic;

        // mapping between a query_id and which arbitrator had sent this query
        // used when sending reply back to the arbitrator via respond ()
        map<query_id_t, id_t> _querymap;

        // counters
        query_id_t _query_id_count;                // internal counter for query id

        // buffers
        char _buf[VASTATE_BUFSIZ];

        // for debug purpose
        errout  _eo;
        char    _str[VASTATE_BUFSIZ];

    };
    
} // end namespace VAST

#endif // #define VASTATE_STORAGE_CS_H

