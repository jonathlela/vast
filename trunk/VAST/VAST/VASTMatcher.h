/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2010 Shun-Yun Hu  (syhu@yahoo.com)
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
 *  VASTMatcher.h  
 *
 *  keep publication & subscription records, and match/find potential subscribers for a 
 *  given point or area publication
 *
 *  history 2010/02/09      starts 1st implementation
 *                          separates the server-like tasks in the original VASTNode (now VASTClient) into this new class
 */



#ifndef _VAST_VASTMatcher_H
#define _VAST_VASTMatcher_H

#include "Config.h"
#include "VASTTypes.h"
#include "MessageHandler.h"
#include "VAST.h"
#include "Voronoi.h"
#include "VONPeer.h"

using namespace std;

// load balancing settings
#define MATCHER_MOVEMENT_FRACTION (0.1f)    // fraction of remaining distance to move for arbitrators

#define TIMEOUT_OVERLOAD_REQUEST  (3)       // seconds to re-send a overload help request


namespace Vast
{


    class VASTMatcher : public MessageHandler, public VONNetwork
    {

    public:

        VASTMatcher (int overload_limit);
        ~VASTMatcher ();
        
        // join the Matcher overlay at a logical location for a given world
        bool    join (const Position &pos);

        // leave the Matcher overlay
        bool    leave ();

        // get the current node's information
        Node *  getSelf ();

        // whether the current node is joined (find & connect to closest relay)
        bool    isJoined ();
               
        // set the gateway node for this world
        bool    setGateway (const IPaddr &gatewayIP);

        // whether I'm a gateway node
        bool    isGateway ();

        // obtain a list of peers hosted on this relay
        //map<id_t, VONPeer *>& getPeers ();

        // get # of peers hosted at this relay, returns NULL for no record
        //StatType *getPeerStat ();

        // 
        // GUI helper functions 
        //

        // get a particular peer's info
        //Node *getPeer (id_t peer_id);

        // get the neighbors for a particular peer
        // returns NULL if the peer does not exist
        //vector<Node *> *getPeerNeighbors (id_t peer_id);

        // obtain access to Voronoi class (usually for drawing purpose)
        // returns NULL if the peer does not exist
        Voronoi *getVoronoi ();

    private:

        // perform initialization tasks for this handler (optional)
        // NOTE that all internal variables (such as handler_no) have been set at this point
        void initHandler ();

        // returns whether the message was successfully handled
        bool handleMessage (Message &in_msg);

        // performs tasks after all messages are handled
        void postHandling ();


        //
        // VONNetwork
        //

        // send messages to some target nodes
        // returns number of bytes sent
        size_t sendVONMessage (Message &msg, bool is_reliable = true, vector<id_t> *failed_targets = NULL);

        // obtain next message in queue
        // return pointer to Message, or NULL for no more message
        Message* receiveVONMessage (timestamp_t &senttime);

        // notify the network layer of nodeID -> Address mapping        
        bool notifyAddressMapping (id_t node_id, Addr &addr);

        // get the IP address of current host machine
        Addr &getHostAddress ();

        // get current physical timestamp
        timestamp_t getTimestamp ();


        //
        // Subscription maintain functions
        //  
            
        // create a new subscriber instance at this VASTMatcher
        bool addSubscription (Subscription &sub);

        // remove the subscriber instance on this VASTMatcher
        bool removeSubscription (id_t sub_no);

        // update the neighbor list for each subscriber
        void updateSubscriptionNeighbors ();

        //
        // Matcher maintain functions
        //

        // whether the matcher has properly joined the VON network
        void checkMatcherJoin ();

        // change position of matcher for load balancing purpose
        void moveMatcher ();

        // update the list of neighboring matchers
        void updateMatchers ();

        // check to call additional matchers for load sharing
        void checkOverload ();

        // let current node know of overload/underload status
        void notifyLoading (int status);
        
        // check to see if subscriptions have migrated 
        int transferOwnership ();

        // tell clients updates of their neighbors (changes in other nodes subscribing at same layer)
        void notifyClients (); 

        // obtain the center of currently subscribing clients
        bool getSubscriptionCenter (Position &sub_center);

        Node                _self;          // information regarding current node
        NodeState           _state;         // current state
        Node                _newpos;        // new AOI & position to be updated

        Addr                _gateway;       // info about the gateway server

        VONPeer *           _VONpeer;       // interface as a participant in a VON

        map<id_t, Subscription> _subscriptions; // a list of subscribers managed by this matcher
                                                // searchable by the hostID of the subscriber

        // TODO: record these counters in other cleaner ways?
        int                 _overload_limit;    // maximum # of subscriptions at a matcher

        // counters
        int                 _load_counter;          // counter for # of timesteps overloaded (positive) or underloaded (negative)
        //int                 _overload_requests;     // counter for # of times a OVERLOAD_M request is sent

        int                 _tick;          // number of logical steps

        //
        // stat collection
        //

        StatType        _stat_sub;          // statistics on actual subscriptions at this matcher
                                            
	};

} // namespace Vast

#endif
