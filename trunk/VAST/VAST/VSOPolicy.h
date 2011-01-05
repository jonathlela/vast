/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2011 Shun-Yun Hu  (syhu@ieee.org)
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
 *  VSOPolicy.h -- an abstract interface to be used by VSOPeer for load balancing policies
 *
 *  history 2010/03/18  1st version
 *
 */



#ifndef _VAST_VSOPolicy_H
#define _VAST_VSOPolicy_H

#include "Config.h"
#include "VASTTypes.h"
#include <map>

using namespace std;


namespace Vast
{
    class VSOPolicy
    {

    public:
        virtual ~VSOPolicy () {}

        // obtain the center of loads
        //virtual bool getLoadCenter (Position &center) = 0; 

        // whether the current node can be a spare node for load balancing
        //virtual bool isCandidate () = 0;

        // find a candidate node suitable for promotion (gateway-only)
        virtual bool findCandidate (Addr &new_node, float level) = 0;

        // obtain the ID of the gateway node
        virtual id_t getGatewayID () = 0;

        // answer object request from a neighbor node, 'is_transfer' indicates whether ownership has been transferred 
        // returns # of successful transfer
        virtual int copyObject (id_t target, vector<id_t> &obj_list, bool update_only) = 0;

        // remove an obsolete unowned object
        virtual bool removeObject (id_t obj_id) = 0;

        // objects whose ownership has transferred to a neighbor node
        virtual bool ownershipTransferred (id_t target, vector<id_t> &obj_list) = 0;

        // notify the claiming of an object as mine
        virtual bool objectClaimed (id_t obj_id) = 0;

        // handle the event of a new VSO node's successful join
        virtual bool peerJoined (id_t origin_id) = 0;

        // handle the event of the VSO node's movement
        virtual bool peerMoved () = 0;

    };

} // namespace Vast

#endif

