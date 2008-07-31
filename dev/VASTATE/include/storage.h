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
 *  storage.h -- VASTATE storage class (provides query for any type of data)
 *
 *  ver 0.1 (2006/10/03)
 *   
 */

#ifndef VASTATE_STORAGE_H
#define VASTATE_STORAGE_H

#include "shared.h"
#include "storage_logic.h"

namespace VAST 
{      

    class storage : public msghandler
    {
    public:
        storage ()
        {
        }
        
        virtual ~storage () 
        {
        }

        //
        // storage interface
        //
        
        // sends a query with a given 'type' number, returns a unique id for this query
        virtual query_id_t query (char *query, size_t size) = 0;

        // sends the results of a query back to the requester
        virtual bool respond (query_id_t query_id, char *reply, size_t size) = 0;

        // process messages sent by vastnode
        virtual bool handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size) = 0;

        // do things after messages are all handled
        virtual void post_processmsg () = 0;

        // pass in a base ID to generate all future query_id's for this storage
        virtual bool init_id (id_t id) = 0;

    };
    
} // end namespace VAST

#endif // #define VASTATE_STORAGE_H

