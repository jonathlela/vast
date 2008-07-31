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
 *  peer.h -- VASTATE peer class (handles everything related to a peer)
 *
 *  ver 0.1 (2006/07/18)
 *   
 */

#ifndef VASTATE_PEER_H
#define VASTATE_PEER_H

#include "shared.h"
#include "peer_logic.h"

namespace VAST 
{  
    class peer : public msghandler
    {
    public:
        peer ()
        {
        }
        
        virtual ~peer () 
        {
        }

        //
        // peer interface
        //
        
        // process messages (send new object states to neighbors)
        virtual int process_msg () = 0;

        // join network
        virtual bool    join (id_t id, Position &pt, aoi_t radius, char *auth, size_t size, Addr *entrance) = 0;
        
        // quit network
        virtual void    leave (bool notify) = 0;
        
        // AOI related functions
        virtual void    set_aoi (aoi_t radius) = 0;
        virtual aoi_t   get_aoi () = 0;
               
        virtual event *create_event () = 0;

        // send an event to the current managing arbitrator
        virtual bool send_event (event *e) = 0;

        // obtain any request to promote as arbitrator
        virtual bool is_promoted (Node &info) = 0;

        //
        //  msghandler methods
        //

        // handle messages sent by vastnode
        virtual bool handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size) = 0;

        // do things after messages are all handled
        virtual void post_processmsg () = 0;


        //
        // accessors
        //

        virtual Node &get_self () = 0;
        virtual bool is_joined () = 0;
        virtual char *to_string () = 0;

    };
    
} // end namespace VAST

#endif // #define VASTATE_PEER_H

