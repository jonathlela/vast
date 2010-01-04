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
        here are some current assumptions about handler creation order (2009/04/23)
                ID		        0
                topology		1
                VAST            2                
                FLoD            3
                
                VASTATE         ...
                VAST relay...

 */


#ifndef _VAST_ID_Generator_H
#define _VAST_ID_Generator_H

#include "VASTTypes.h"
#include "MessageHandler.h"


const int TIMEOUT_ID_REQUEST = 10;   // number of "ticks" before resending request
using namespace std;

namespace Vast
{

    typedef enum 
    {
        ID_REQUEST = 1,                 // query for ID
        ID_REPLY                        // response for ID request
    } ID_Message;

    class IDGenerator : public MessageHandler 
    {
    public:
        // NOTE that we assume the handlerID for ID Generator is hostID + 0
        //      that is, we assume IDGenerator is the first handler (with localID = 0)
        IDGenerator (Addr &gateway, bool is_gateway = false);

        ~IDGenerator ()
        {
        }

        // obtain a unique ID, 
        // return unique ID or NET_ID_UNASSIGNED if not yet received from gateway
        id_t getID ();

        //
        // msghandler methods
        //

        // returns whether the message has been handled successfully       
        bool handleMessage (Message &in_msg);

    private:

        inline bool is_gateway ()
        {
            return (_id_count != 0);
        }

        id_t    _id;
        Addr    _gateway;               // address for gateway 
        id_t    _id_count;              // counter for current id assignment, used only by gateway
        int     _requestTimeout;        // timeout for re-sending a ID request

    };

} // end namespace Vast

#endif // VASTID_BASE_H
