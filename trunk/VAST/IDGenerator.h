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
 *  vastid_base.h -- unique ID generator for VAST (default implementation)
 *
 *  history     2007/03/15
 *   
 */


#ifndef _VAST_ID_Generator_H
#define _VAST_ID_Generator_H

#include "VASTTypes.h"
#include "MessageHandler.h"

namespace Vast
{

    typedef enum ID_Message
    {
        ID_REQUEST = 1,                 // query for ID
        ID_REPLY                        // response for ID request
    };

    class IDGenerator : public MessageHandler 
    {
    public:
        IDGenerator (Addr &gateway, bool is_gateway = false)
            :MessageHandler (MSGGROUP_ID_VASTID), _gateway (gateway), _requestTimeout (0)
        {           
            _id = (is_gateway ? NET_ID_GATEWAY : NET_ID_UNASSIGNED);

            // assume the ID assignment starts after the gateway's ID
            _id_count = (is_gateway ? NET_ID_GATEWAY + 1 : 0);

            /* BUG/TODO: when creating the gateway's net interface, ID has to be assigned manually
                         as at this stage the IDgenerator still doesn't know its net interface
            
            // registering gateway's ID to net layer (do it here?)
            if (is_gateway)
                _net->register_id (NET_ID_GATEWAY);
            */            
        }

        ~IDGenerator ()
        {
        }

        // obtain a unique ID
        id_t getid ();

        //
        // msghandler methods
        //

        // returns whether the message has been handled successfully       
        bool handleMessage (id_t from, Message &in_msg);

    private:

        inline bool is_gateway ()
        {
            return (_id_count != 0);
        }

        id_t    _id;                    // unique ID for myself
        Addr    _gateway;               // address for gateway 
        id_t    _id_count;              // counter for current id assignment, used only by gateway
        int     _requestTimeout;        // timeout for re-sending a ID request
    };

} // end namespace Vast

#endif // VASTID_BASE_H
