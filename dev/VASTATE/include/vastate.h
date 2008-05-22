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
 *  vastate.h -- VASTATE - State Management for VAST
 *
 *  ver 0.1 (2006/07/15)
 *   
 */

#ifndef VASTATE_H
#define VASTATE_H

#include "shared.h"
#include "vworld.h"
#include "gateway.h"
#include "arbitrator.h"
#include "peer.h"
#include "storage.h"

#include <map>

// TODO: reliable/unreliable flag for each attribute
//       hidden object for user position
//using namespace std;

namespace VAST 
{        
    class vastate
    {
    public:
        // Constants
        const static int STATE_INIT     = 0;
        const static int STATE_STARTING = 1;
        const static int STATE_STARTED  = 2;
        const static int STATE_CAN_JOIN = 2;

    public:
        
        vastate (const system_parameter_t & sp)
            : sysparm (sp)
        {            
        }
    	
        virtual ~vastate () 
        {
        }

        // start the node, get id
        virtual bool start (bool is_gateway = false) = 0;

        // stop the node, disconnects all rolls
        virtual bool stop () = 0;

        // process messages in queue
        virtual int process_message () = 0;
    
        // create an initial server, if a server, must be called before any create of peers/arbitrators
        virtual gateway *create_gateway () = 0;

        // create an initial server
        /*
        virtual bool create_server (vector<arbitrator_logic *>  &alogics, 
                                    vector<storage_logic *>     &slogics, 
                                    int dim_x, int dim_y, int n_vpeers) = 0;
        */

        // create a peer entity
        virtual peer *create_peer (peer_logic *logic, Node &peer_info, int capacity) = 0;

        // create an arbitrator
        virtual arbitrator *create_arbitrator (id_t parent, 
                                               arbitrator_logic *alogic, 
                                               storage_logic    *slogic, 
                                               Node &arb_info, 
                                               bool is_gateway = false) = 0;                       

        // close down a peer
        virtual void destroy_peer (peer *p) = 0;

        // close down an arbitrator
        virtual void destroy_arbitrator (id_t id) = 0;

        // query for arbitrator promotion / demotion requests
        // first parameter: 1: promotion, 2: demotion
        virtual multimap<int, Node>& get_requests () = 0;

        // clean arbitrator promotion / demotion requests
        virtual bool clean_requests () = 0;

        // system parameter
        system_parameter_t sysparm;
    };

} // end namespace VAST

#endif // VASTATE_H
