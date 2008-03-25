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

#include "vastid_base.h"

namespace VAST
{  
    // obtain a unique ID
    id_t 
    vastid_base::getid ()
    {
        //static int request_counter = 0;

        // if this is a first-time join
        if (_id == NET_ID_UNASSIGNED && _request_counter == 0)
        {
            _net->connect (NET_ID_GATEWAY, _gateway);
            _net->sendmsg (NET_ID_GATEWAY, ID, 0, 0);//, 0);

            // we'll make a countdown
            _request_counter = 10;
        }

        // see if we get any response from gateway
        //processmsg (0);

        _request_counter--;

        return _id;
    }

    // returns whether the message has been handled successfully
    bool 
    vastid_base::handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size)
    {
        switch ((VAST_Message)msgtype)
        {        
        case ID:
            if (is_gateway () == false)
            {
                // obtain unique id from gateway
                if (_id == NET_ID_UNASSIGNED && size == sizeof (id_t))
                {
                    memcpy (&_id, (void *)msg, sizeof (id_t));

                    _net->register_id (_id);
                    
                    /*
                    // adjust time with the server version
                    timestamp_t server_time;
                    memcpy (&server_time, msg + sizeof(id_t), sizeof(timestamp_t));

                    printf ("VAST: %4d [%3d] got my id: %d sync time with server: %d to %d\n", _time, (int)_id, (int)_id, _time, server_time);
                    _time = server_time;
                    */
                }
                
                _net->disconnect (from_id);
            }
            else
            {
                // assign a new unique id to the requester
                char buf[10];
                memcpy (buf, &_id_count, sizeof (id_t));
                //memcpy (buf + sizeof (id_t), &_time, sizeof (timestamp_t));

                //_net->sendmsg (from_id, ID, (char *)buf, sizeof(id_t) + sizeof(timestamp_t), _time);
                _net->sendmsg (from_id, ID, (char *)buf, sizeof(id_t), true);
                _id_count++;
            }
            break;

        default:
            return false;

        } // end switch

        return true;
    }


} // namespace VAST

