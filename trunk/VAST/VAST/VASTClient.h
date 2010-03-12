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
 *  VASTClient.h     Implementation class for the VAST interface. 
 *              Acts as both a Client at regular peers and VASTClient at super-peers
 *              
 *    history:  2009/04/28  separated from VAST.h
 */

#ifndef _VAST_CLIENT_H
#define _VAST_CLIENT_H

#include "Config.h"
#include "VASTTypes.h"
#include "VAST.h"
#include "MessageHandler.h"
#include "VASTRelay.h"
#include <vector>


const int TIMEOUT_QUERY = 30;        // number of "ticks" before resending join request
const int TIMEOUT_JOIN  = 30;        // number of "ticks" before resending join request

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
    //      Also, a VASTClient (the same one) serves as:
    //            1) a Client at a regular peer, 
    //            2) a Matcher at a super-peer
    //
    class VASTClient : public VAST
    {

    public:

		//
		// main VAST methods
		//

        // send int the relay_id of which we can notify potential publisher
        VASTClient (VASTRelay *relay);
        ~VASTClient ();
        
		/**
			join the overlay 

			@param  pos     physical coordinate of the joining client node
		*/
        // specify a joining position (physical coordinate?)
        bool        join (const IPaddr &gateway);

        // quit the overlay
        void        leave ();
        
		// specify a subscription area for point or area publications 
        // returns a unique subscription number that represents subscribed area
        bool        subscribe (Area &area, layer_t layer);

        // send a message to all subscribers within a publication area
        bool        publish (Area &area, layer_t layer, Message &message);
            
        // move a subscription area to a new position
        // returns actual AOI in case the position is already taken, or NULL of subID is invalid
        // 'update_only' indicates if move request should not be actually sent (avoid redundent MOVEMENT request when used with VASTATE)
        Area *      move (id_t subID, Area &aoi, bool update_only = false);

        // send a custom message to a particular node
        bool        send (Message &message);

        // obtain a list of subscribers with an area
        vector<Node *>& list (Area *area = NULL);

        // obtain a list of physically closest hosts
        vector<Node *>& getPhysicalNeighbors ();

        // obtain a list of logically closest hosts (a subset of relay-level nodes returned by list ())
        vector<Node *>& getLogicalNeighbors ();        

        // get a message from the network queue
        Message *   receive ();
    
        // get current statistics about this node (a NULL-terminated string)
        char * getStat (bool clear = false);
        
        // get the current node's information
        Node * getSelf ();

        // whether the current node is joined
        bool isJoined ();

        // whether the current node is listening for publications, returns subscription ID
        // 0 indicates no subscription
        id_t isSubscribing ();

        // if I'm a relay
        inline bool isRelay ();

        // whether I have public IP
        bool hasPublicIP ();

        // obtain access to Voronoi class (usually for drawing purpose)
        // returns NULL if matcher does not exist on this node
        //Voronoi *getVoronoi ();

        // get message latencies, currently supports PUBLISH & MOVE types
        StatType *getMessageLatency (msgtype_t msgtype);

        // notify the network layer of nodeID -> Address mapping        
        bool notifyAddressMapping (id_t node_id, Addr &addr);

    private:

        // perform initialization tasks for this handler (optional)
        // NOTE that all internal variables (such as handler_no) have been set at this point
        void initHandler ();

        // main handler for various incoming messages
        // returns whether the message was successfully handled
        bool handleMessage (Message &in_msg);

        // perform routine tasks after all messages have been handled 
        //  (i.e., check for reply from requests sent)
        void postHandling ();

        // store a message to the local queue to be retrieved by receive ()
        void storeMessage (Message &msg);

        // make one latency record
        void recordLatency (msgtype_t msgtype, timestamp_t sendtime);

        // variables used by VASTClient component
        Node                _self;          // information regarding current node
        NodeState           _state;         // state of joining                
        vector<Node *>      _neighbors;     // list of current AOI neighbors
        vector<Node *>      _physicals;     // list of physical neighbors
        vector<Node *>      _logicals;      // list of logical neighbors

        //id_t                _relay_id;    // the relay I currently use (could be myself)
        id_t                _matcher_id;    // hostID for interest matcher
        VASTRelay          *_relay;         // pointer to VASTRelay (to obtain relayID)        

        Subscription        _sub;           // my subscription 
        //map<id_t, Subscription> _subscriptions;  // list of subscriptions 

        
        // storage for incoming messages        
        vector<Message *>   _msglist;   // record for incoming messages
        VASTBuffer          _recv_buf;  // a receive buffer for incoming messages
        Message *           _lastmsg;   // last message received from network (to be deleted)
                                        // TODO: a better way for it?
       
        // stats
        map<msgtype_t, StatType>  _latency; // latencies for different message types 


    };

} // end namespace Vast

#endif // VAST_CLIENT_H
