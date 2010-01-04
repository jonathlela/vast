/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2009 Shun-Yun Hu (syhu@yahoo.com)
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
 *  VAST.h -- main VAST header, used by application     
 *
 *  history 2005/04/11  ver 0.1
 *          2007/01/11  ver 0.2     simplify interface   
 *			2008/08/20  ver 0.3		re-written for generic overlay
 *          2009/04/02  ver 0.4		re-defined for SPS
 */

#ifndef _VAST_H
#define _VAST_H

#include "Config.h"
#include "VASTTypes.h"
#include "Voronoi.h"
#include "MessageHandler.h"

using namespace std;

namespace Vast
{

    //
    // NOTE that we currently assume two things are decided
    //      outside of the VAST class:
    //
    //          1. unique ID (hostID obtained via IDGenerator, handlerID by MessageQueue)
    //          2. a coordinate representing physical location
    //      
    //      Also, a VAST node (the same one) serves as both a Client at a regular node
    //            but a Relay at a super-peer
    //
    class EXPORT VAST : public MessageHandler
    {

    public:

		//
		// main VAST methods
		//

        VAST ()
            :MessageHandler (MSG_GROUP_VAST_RELAY)
        {
        }
         
        //virtual ~VAST () = 0;
        
	/**
		join the overlay 

		@param  pos     physical coordinate of the joining client node
	*/
        // specify a joining position (physical coordinate?)
        virtual bool        join (Position &pos, bool as_relay = true) = 0;

        // quit the overlay
        virtual void        leave () = 0;
        
	// specify a subscription area for point or area publications 
        // returns a unique subscription number that represents subscribed area
        virtual id_t        subscribe (Area &area, layer_t layer) = 0;

        // send a message to all subscribers within a publication area
        virtual bool        publish (Area &area, layer_t layer, Message &message) = 0;
            
        // move a subscription area to a new position
        // returns actual AOI in case the position is already taken, or NULL if subscription does not exist
        virtual Area *      move (id_t subNo, Area &aoi, bool update_only = false) = 0;

        // send a custom message to a particular node
        virtual bool        send (Message &message) = 0;

        // obtain a list of subscribers with an area
        virtual vector<Node *>& list (Area *area = NULL) = 0;

        // get a message from the network queue
        virtual Message *   receive () = 0;
    
        // get current statistics about this node (a NULL-terminated string)
        virtual char * getStat (bool clear = false) = 0;
        
        // get the current node's information
        virtual Node * getSelf () = 0;

        // whether the current node is joined (part of relay mesh)
        virtual bool isJoined () = 0;

        // whether the current node is listening for publications
        virtual bool isSubscribing (id_t sub_no) = 0;

        // whether I am a relay node
        virtual bool isRelay () = 0;

        // whether I have public IP
        virtual bool hasPublicIP () = 0;

        //
        // accessor (non-essential) functions for GUI display
        //

        // get a particular peer's info
        virtual Node *getPeer (id_t peer_id) = 0;

        // get the neighbors for a particular peer
        // returns NULL if the peer does not exist
        virtual vector<Node *> *getPeerNeighbors (id_t peer_id) = 0;

        // obtain access to Voronoi class (usually for drawing purpose)
        // returns NULL if the peer does not exist
        virtual Voronoi *getVoronoi (id_t peer_id) = 0;

        // 
        // stat collectors
        //
        // TODO: remove stat from VAST interface?
        
        // get message latencies, currently support PUBLISH & MOVE
        // msgtype == 0 indicates clear up existing latency records
        virtual StatType *getMessageLatency (msgtype_t msgtype) = 0;

        // get # of peers hosted at this relay, returns NULL for no record (at non-relays)
        virtual StatType *getPeerStat () = 0;

    };

} // end namespace Vast

#endif // VAST_H
