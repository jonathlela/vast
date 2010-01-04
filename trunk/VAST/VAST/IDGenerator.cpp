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
    /*
    // obtain a unique ID
    // NOTE we have use some strong assumptions here about the
    //      the hostID of gateway being NET_ID_GATEWAY (=1) and
    //      the handlerID's localID is 0 (i.e., IDGenerator is always the 1st handler)
    //
    //
    */

    // NOTE that we assume the handlerID for ID Generator is hostID + 0
    //      that is, we assume IDGenerator is the first handler (with localID = 0)

    IDGenerator::IDGenerator (Addr &gateway, bool is_gateway)
        :MessageHandler (MSG_GROUP_ID_GENERATOR), _id (NET_ID_UNASSIGNED), _gateway (gateway), _id_count (0), _requestTimeout (0)
    {   
        // we assume the gateway's host ID is the first one
        _gateway.host_id = NET_ID_GATEWAY;

        // assume the ID assignment starts after the gateway's ID
        _id_count = (is_gateway ? NET_ID_GATEWAY + 1: 0);

        // NOTE we cannot use _msgqueue here yet as it's uninitialized
    }

    id_t 
    IDGenerator::getID ()
    {
        // if this is a first-time join
        if (_id == NET_ID_UNASSIGNED && _requestTimeout == 0)
        {               
            // if gateway then return directly
            if (_id_count != 0)
            {   
                _id = NET_ID_GATEWAY;
                ((MessageQueue *)_msgqueue)->registerHostID (_id);
                return _id;
            }

            // otherwise send request            
            Message msg (ID_REQUEST);
            msg.priority = 0;
            msg.store (_net->getHostAddress ());
            msg.addTarget (_gateway.host_id);

            // save gateway's ID to IP mapping
            notifyMapping (_gateway.host_id, &_gateway);
            sendMessage (msg);

            // start countdown
            _requestTimeout = TIMEOUT_ID_REQUEST;
        }

        _requestTimeout--;

        return _id;
    }

    // returns whether the message has been handled successfully 
    bool 
    IDGenerator::handleMessage (Message &in_msg)
    {
        switch ((ID_Message)in_msg.msgtype)
        {
        case ID_REQUEST:
            if (is_gateway () == true)
            {
                // extract IP & verify 
                Addr addr;
                in_msg.extract (addr);

                // obtain the actual address assocated with this message
                //Addr &actual_addr = _net->getAddress (addr.host_id);
                Addr &actual_addr = _net->getAddress (in_msg.from);

                char IP_sent[16], IP_actual[16];
                addr.publicIP.getString (IP_sent);
                actual_addr.publicIP.getString (IP_actual);
                                               
                printf ("ID requested from: %ld (%s:%d) actual address (%s:%d)\n", addr.host_id, IP_sent, addr.publicIP.port, IP_actual, actual_addr.publicIP.port);
                
                id_t new_id = UNIQUE_ID(_id_count, 0);
                
                // assign a new unique id to the requester
                bool is_public = (addr.publicIP.host == actual_addr.publicIP.host);
                Message msg (ID_REPLY); 
                msg.priority = 0;
                msg.store (new_id);
                msg.store ((char *)&is_public, sizeof (bool));
                msg.addTarget (in_msg.from);

                sendMessage (msg);
                _id_count++;
            }
            break;

        case ID_REPLY:
            if (is_gateway () == false)
            {
                // obtain unique id from gateway
                if (_id == NET_ID_UNASSIGNED && (in_msg.size == (sizeof (id_t) + sizeof (bool))))
                {
                    in_msg.extract (_id);                    
                    ((MessageQueue *)_msgqueue)->registerHostID (_id);
                    printf ("got my ID [%ld]\n", _id);

                    bool is_public;
                    in_msg.extract ((char *)&is_public, sizeof (bool));

                    _net->setPublic (is_public);

                    // disconnect from gateway, so that future connections to gateway can use the new ID
                    // otherwise gateway might have difficulty to send back to the new ID
                    //_net->disconnect (NET_ID_GATEWAY);

                }
            }
            break;

        default:
            return false;

        } // end switch

        return true;
    }

} // namespace Vast

