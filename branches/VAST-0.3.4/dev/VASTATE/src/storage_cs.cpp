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

#include "storage_cs.h"

namespace VAST
{
    extern char VASTATE_MESSAGE[][20];

    //
    // storage interface
    //
    
    // sends a query with a given 'type' number, returns a unique id for this query
    query_id_t 
    storage_cs::query (char *query, size_t size)
    {
        // compose query message with query_id + type + msg content
        char *p = _buf;
        memcpy (p, &_query_id_count, sizeof (query_id_t));
        p += sizeof (query_id_t);

        memcpy (p, &size, sizeof(size_t));
        p += sizeof(size_t);

        memcpy (p, query, size);

        _net->sendmsg (NET_ID_GATEWAY, S_QUERY, _buf, sizeof (query_id_t) + sizeof(size_t) + size, 0);

        return _query_id_count++;
    }

    // sends the results of a query back to the requester
    bool 
    storage_cs::respond (query_id_t query_id, char *message, size_t size)
    {   
        // find out which node to send back given the query_id
        if (_querymap.find (query_id) == _querymap.end ())
            return false;

        id_t node_id = _querymap[query_id];
        _querymap.erase (query_id);

        // construct response message
        memcpy (_buf, &query_id, sizeof (query_id_t));
        char *p = _buf + sizeof (query_id_t);
        memcpy (p, &size, sizeof (size_t));
        p += sizeof (size_t);
        memcpy (p, message, size);

        // send response back to the querying arbitrator
        _net->sendmsg (node_id, S_REPLY, _buf, sizeof (query_id_t) + sizeof (size_t) + size, 0);
        return true;
    }

    bool 
    storage_cs::handlemsg (id_t from_id, msgtype_t msgtype, 
                            timestamp_t recvtime, char *msg, int size)
    {
#ifdef DEBUG_DETAIL
        if (msgtype >= 100 && msgtype < VASTATE_MESSAGE_END)
        {
            sprintf (_str, "[%lu] arb handlemsg from [%lu]: (%d)%s \n", 0 /* self->id */, from_id, msgtype, VASTATE_MESSAGE[msgtype-100]);
            _eo.output (_str);
        }
#endif

        switch (msgtype)
        {
        // process a remote query request
        case S_QUERY:           
            {
                // take message apart
                query_id_t query_id;
                memcpy (&query_id, msg, sizeof (query_id_t));

                char *p = msg + sizeof (query_id_t);
                size_t query_size;
                memcpy (&query_size, p, sizeof (size_t));
                p += sizeof (size_t);

                // size check
                if (query_size + sizeof(query_id_t) + sizeof (size_t) != (size_t) size)
                    break;

                // record mapping between query_id and the querying arbitrator
                _querymap[query_id] = from_id;

                // send the query message to app-specific callback handler
                _logic->query_received (query_id, p, query_size);
            }            
            break;

        case S_REPLY:
            {
                // take message apart
                query_id_t query_id;
                memcpy (&query_id, msg, sizeof (query_id_t));

                char *p = msg + sizeof (query_id_t);
                size_t reply_size;
                memcpy (&reply_size, p, sizeof (size_t));
                p += sizeof (size_t);

                // size check
                if (reply_size + sizeof(query_id_t) + sizeof (size_t) != (size_t) size)
                    break;

                // send the reply message to app-specific callback handler
                _logic->reply_received (query_id, p, reply_size);
            }
            break;

        default:            
            return false;       
        }

        // message successfully handled
        return true;
    }

    bool 
    storage_cs::init_id (id_t id)
    {
        if (_query_id_count != 0)
            return false;

        _query_id_count = ((query_id_t)id << 16) + 1;
        return true;
    }

} // namespace VAST


