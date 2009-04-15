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

#include "IDGenerator.h"
#include "MessageQueue.h"

namespace Vast
{  
    // obtain a unique ID
    id_t 
    IDGenerator::getid ()
    {
        //static int request_counter = 0;

        // if this is a first-time join
        if (_id == NET_ID_UNASSIGNED && _requestTimeout == 0)
        {
            connect (_gateway);
            
            Message msg (ID_REQUEST);

            // NOTE, we assume gateway is the 1st handler to be registered 
            // BUG: potential bug.. 
            id_t target = COMBINE_ID (1, _gateway.id);
            sendMessage (target, msg, true, false);                          

            // start countdown
            _requestTimeout = 10;
        }

        _requestTimeout--;

        return _id;
    }

    // returns whether the message has been handled successfully
 
    bool 
    IDGenerator::handleMessage (id_t from, Message &in_msg)
    {
        switch ((ID_Message)in_msg.msgtype)
        {
        case ID_REQUEST:
            if (is_gateway () == true)
            {
                // assign a new unique id to the requester
                Message reply (ID_REPLY);
                reply.store ((char *)&_id_count, sizeof (id_t));         
                sendMessage (from, reply, true);
                
                _id_count++;
            }
            break;

        case ID_REPLY:
            if (is_gateway () == false)
            {
                // obtain unique id from gateway
                if (_id == NET_ID_UNASSIGNED && in_msg.size == sizeof (id_t))
                {
                    in_msg.extract ((char *)&_id, sizeof (id_t));                                        
                    ((MessageQueue *)_msgqueue)->registerID (_id);
                }                
                disconnect (from);
            }
            break;

        default:
            return false;

        } // end switch

        return true;
    }

} // namespace Vast

