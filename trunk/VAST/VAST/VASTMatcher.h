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
#include "VSOPeer.h"

#define TIMEOUT_SUBSCRIPTION_KEEPALIVE          (1.0)   // # of seconds to send updates for my subscriptions

#define TIMEOUT_MATCHER_KEEPALIVE               (10)    // # of seconds to inform gateway of alive status
    
#define LIMIT_LIVE_MATCHERS_PER_WORLD           (5)     // # of live matcher info the gateway needs to keep (for fault tolerance)

#define SUBSCRIPTION_AOI_BUFFER                (10)     // extended AOI to avoid ghost objects

// flag to send NEIGHBOR notices via relay (slower but can test relay correctness)
// IMPORTANT NOTE: if position updates are not sent via relay, need to make sure
//                 clients contact relays periodically so relays will not timeout
//                 the connection to clients, causing clients to re-join relays
//                 check out    TIMEOUT_COORD_QUERY         (in VASTRelay.h)
//                              TIMEOUT_REMOVE_CONNECTION   (in VASTnet.h)
#define SEND_NEIGHBORS_VIA_RELAY_

using namespace std;

namespace Vast
{

    // possible states for a given matcher
    typedef enum 
    {
        CANDIDATE = 1,      // a idle matcher
        PROMOTING,          // in the process of promotion
        ACTIVE,             // active regular matcher
        ORIGIN,             // active origin matcher
    } MatcherState;

    struct MatcherInfo
    {
        MatcherState    state;      // matcher state
        world_t         world_id;   // world the matcher belongs
        Addr            addr;       // address for matcher        
        timestamp_t     time;       // last update of the info
    };

    class VASTMatcher : public MessageHandler, public VONNetwork, public VSOPolicy
    {

    public:

        // constructor, passing in whether the node can be a matcher candidate, 
        // and what's the threshold considered as overload
        // optionally a logical coordinate can be supplied as the initial join position
        VASTMatcher (bool is_matcher, int overload_limit, bool is_static = false, Position *coord = NULL);
        ~VASTMatcher ();
        
        // join the Matcher overlay for a given world (gateway)
        bool    join (const IPaddr &gatewayIP);

        // leave the Matcher overlay
        bool    leave ();

        // get the current node's information
        Node *  getSelf ();

        // whether the current node is joined (find & connect to closest relay)
        bool    isJoined ();
               
        // whether I'm a gateway node
        bool    isGateway (id_t id = 0);

        // whether I'm an active matcher (promoted by gateawy)
        bool    isActive ();

        // 
        // GUI helper functions 
        //

        // obtain access to Voronoi class (usually for drawing purpose)
        // returns NULL if the peer does not exist
        Voronoi *getVoronoi ();

        // obtain the matcher's adjustable radius (determined by VSOPeer)
        Area *getMatcherAOI ();

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

        // get the # of ticks in each second;
        timestamp_t getTimestampPerSecond ();

        //
        // VSOPolicy
        //

        // get a candidate origin matcher to use (gateway-only)
        bool findCandidate (Addr &new_origin, float level = 0);

        // obtain the ID of the gateway node
        id_t getGatewayID ();

        // answer a request for objects
        // returns # of successful transfer
        int copyObject (id_t target, vector<id_t> &obj_list, bool update_only);

        // remove an obsolete unowned object
        bool removeObject (id_t obj_id);

        // objects whose ownership has transferred to a neighbor node
        bool ownershipTransferred (id_t target, vector<id_t> &obj_list);

        // notify the claiming of an object as mine
        bool objectClaimed (id_t obj_id);

        // handle the event of a new VSO node's successful join
        bool peerJoined (id_t origin_id);

        // handle the event of the VSO node's movement
        bool peerMoved ();

        //
        // Subscription maintain functions
        //  
            
        // create a new subscriber instance at this VASTMatcher
        bool addSubscription (Subscription &sub, bool is_owner);

        // remove the subscriber instance on this VASTMatcher
        bool removeSubscription (id_t sub_no);

        // update a subscription content
        bool updateSubscription (id_t sub_no, Area &new_aoi, timestamp_t sendtime, Addr *relay = NULL, bool *is_owner = NULL);

        // send a full subscription info to a neighboring matcher
        // returns # of successful transfers
        int transferSubscription (id_t target, vector<id_t> &sub_list, bool update_only);

        // update the neighbor list for each subscriber
        void refreshSubscriptionNeighbors ();

        // check if a disconnecting host contains subscribers
        bool subscriberDisconnected (id_t host_id);

        //
        // Matcher maintain functions
        //

        // whether the current node can be a spare node for load balancing
        bool isCandidate ();

        // notify the gateway that I can be available as origin matchers
        bool notifyCandidacy ();

        // update the list of neighboring matchers
        void refreshMatcherList ();

        // check to call additional matchers for load sharing
        void checkOverload ();
        
        // re-send updates of our owned objects so they won't be deleted
        void sendKeepAlive ();

        // remove matchers that have not sent in keepalive
        void removeExpiredMatchers ();

        // tell clients updates of their neighbors (changes in other nodes subscribing at same layer)
        void notifyClients (); 

        // handle the failure of a origin matcher (gateway-only)
        void originDisconnected (id_t matcher_id);

        //
        // helper functions
        //

        // send a message to clients (optional flag to send directly)
        // returns # of targets successfully sent, optional to return failed targets
        int sendClientMessage (Message &msg, id_t client_ID = NET_ID_UNASSIGNED, vector<id_t> *failed_targets = NULL);

        // deal with unsuccessful send targets
        void removeFailedSubscribers (vector<id_t> &list);

        // initialize a new matcher (also serve to notify existing matcher of new origin)
        // returns whether send was successful
        bool initMatcher (world_t world_id, id_t origin_id, id_t target);

        // set the gateway node for this world
        bool setGateway (const IPaddr &gatewayIP);

        // check if a node is an origin matcher (gateway-only)
        bool isOriginMatcher (id_t id);

        // store a joining client to the queue of waiting for origin matcher response
        void storeRequestingClient (world_t world_id, id_t client_id);


        Node                _self;          // information regarding current node
        NodeState           _state;         // current state
        
        Addr                _gateway;       // info about the gateway server

        VSOPeer *           _VSOpeer;       // interface as a participant in a VON

        world_t             _world_id;      // which world I'm currently in (default to VAST_DEFAULT_WORLD_ID)
        id_t                _origin_id;     // id for the origin matcher
        Addr                _origin_addr;   // address for origin matcher

        map<id_t, Node *>   _neighbors;     // list of neighboring matchers, NOTE: pointers refer to data in VSOPeer, so do not need to be released upon destruction
                                                                                                            
        map<id_t, Subscription> _subscriptions; // a list of subscribers managed by this matcher
                                                // searchable by the subscription ID of the subscriber

        map<id_t, map<id_t, bool> *> _replicas; // a list of replicas of owned objects at remote neighbors, map from obj_id to host_id map

        map<id_t, id_t>     _closest;           // mapping of subscription to closest alternative matcher

        bool                _is_matcher;        // whether the node can be a matcher candidate
        bool                _is_static;         // whether the current matcher will never move
        int                 _overload_limit;    // # of subscriptions considered overload
        
        timestamp_t         _next_periodic;     // record for next time stamp to process periodic (per-second) tasks

        vector<Message *>   _queue;             // messages received but cannot yet processed before VSOpeer is joined

        int                 _matcher_keepalive; // # of seconds before reporting to gateway of keep alive status

        //
        // origin matcher management (gateway only)
        //
        // TODO: separate this into a VASTGateway class?

        world_t                         _world_counter;     // counter for assigning world IDs 
        map<world_t, id_t>              _origins;           // list of origin matchers
        map<id_t, MatcherInfo>          _matchers;          // info of candidate & currently active matchers
        map<world_t, vector<id_t> *>    _requests;          // initial requests from clients to join a given world        

        //
        // stat collection
        //

        StatType        _stat_sub;          // statistics on actual subscriptions at this matcher                                            
	};

} // namespace Vast

#endif
