/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2009 Shun-Yun Hu  (syhu@yahoo.com)
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
 *  VSOPeer.h -- a logical node that performs VON-based Self-organizing Overlay (VSO) functions 
 *               main functions include: notifyLoading and self adjustment of area
 *          
 *               this is a sub-class of VONPeer
 *
 *               NOTE that we assume the following:
 *                  - each VON node is uniquely identified by an external ID
 *                  - each VON node can be associated with a physically reachable address specified in Addr
 *
 *  history 2010/03/18      adopted from ArbitratorImpl.h in VASTATE
 *
 */



#ifndef _VAST_VSOPeer_H
#define _VAST_VSOPeer_H

#include "Config.h"
#include "VASTTypes.h"
#include "VONPeer.h"
#include "VSOPolicy.h"

// load balancing settings
#define VSO_MOVEMENT_FRACTION           (0.1f)  // fraction of remaining distance to move for nodes
#define VSO_TIMEOUT_OVERLOAD_REQUEST    (3)     // seconds to re-send a overload help request
#define VSO_INSERTION_TRIGGER           (5)     // # of matcher movement requests before an insertion should be requested

using namespace std;

namespace Vast
{

    // WARNING: VON messages currently should not exceed VON_MAX_MSG defined in Config.h
    //          otherwise there may be ID collisons with other handlers that use VONpeer
    //          internally (e.g., VASTClient in VAST or Arbitrator in VASTATE)
    typedef enum
    {
        VSO_DISCONNECT = 0,     // VSO's disconnect
        VSO_CANDIDATE = 11,     // notify gateway of a candidate node    
        VSO_PROMOTE,            // promotes a candidate to functional
        VSO_INSERT,             // overload request for insertion
        VSO_MOVE,               // overload request for movement
        
    } VSO_Message;

    // 
    // This class joins a node as "VONPeer", which allows the user client
    // to execute VON commands: move, getNeighbors
    // 
    class EXPORT VSOPeer : public VONPeer
    {

    public:

        VSOPeer (id_t id, VONNetwork *net, VSOPolicy *policy, length_t aoi_buffer = AOI_DETECTION_BUFFER);        
        ~VSOPeer ();                        
       
        // returns whether the message was successfully handled
        bool handleMessage (Message &in_msg);

        // current node overloaded, call for help
        // note that this will be called continously until the situation improves
        void notifyLoading (float level);

        // notify the gateway that I can be available to join
        bool notifyCandidacy ();

    private:

        // change the center position in response to overload signals
        void moveCenter ();

        // get a new node that can be inserted
        bool findCandidate (float level, Node &new_node);

        // check whether a new node position is legal
        bool isLegalPosition (const Position &pos, bool include_self);

        // helper function to identify if a node's gateway
        inline bool isGateway (id_t id)
        {
            return (_policy->getGatewayID () == id);
        }
        
        VSOPolicy *         _policy;            // various load balancing policies

        Node                _newpos;            // new AOI & position to be updated due to load balancing

        
        // server data (Gateway node)       
        vector<Node>        _candidates;          // list of potential nodes      
        map<id_t, Node>     _promote_requests;    // requesting nodes' timestamp of promotion and position, index is the promoted node
        
        // counters for current node
        int                 _load_counter;      // counter for # of timesteps overloaded (positive) or underloaded (negative)
        int                 _overload_count;    // counter for # of times a OVERLOAD_M request is sent
    };

} // namespace Vast

#endif
