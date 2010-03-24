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
#include "VSOPeer.h"


using namespace std;

namespace Vast
{

    class VASTMatcher : public MessageHandler, public VONNetwork, public VSOPolicy
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

        // 
        // GUI helper functions 
        //

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

        // get the # of ticks in each second;
        int getTickPerSecond ();

        //
        // VSOPolicy
        //

        // obtain the center of loads
        //bool getLoadCenter (Position &center); 

        // whether the current node can be a spare node for load balancing
        bool isCandidate ();

        // obtain the ID of the gateway node
        id_t getGatewayID ();

        // answer a request for objects
        // returns # of successful transfer
        int copyObject (id_t target, vector<id_t> &obj_list, bool is_transfer, bool update_only);

        // remove an obsolete unowned object
        bool removeObject (id_t obj_id);

        // handle the event of a new VSO node's successful join
        bool peerJoined ();

        // handle the event of a new VSO node's successful join
        bool peerMoved ();

        //
        // Subscription maintain functions
        //  
            
        // create a new subscriber instance at this VASTMatcher
        bool addSubscription (Subscription &sub, bool is_owner);

        // remove the subscriber instance on this VASTMatcher
        bool removeSubscription (id_t sub_no);

        // update a subscription content
        bool updateSubscription (id_t sub_no, Area &new_aoi, timestamp_t sendtime);

        // send a full subscription info to a neighboring matcher
        // returns # of successful transfers
        int transferSubscription (id_t target, vector<id_t> &sub_list, bool notify_client, bool update_only);

        // update the neighbor list for each subscriber
        void refreshSubscriptionNeighbors ();

        //
        // Matcher maintain functions
        //

        // update the list of neighboring matchers
        void refreshMatcherList ();

        // check to call additional matchers for load sharing
        void checkOverload ();
        
        // tell clients updates of their neighbors (changes in other nodes subscribing at same layer)
        void notifyClients (); 

        //
        // helper functions
        //

        // TODO: factor these into an independent / generic class?
        // obtain a list of hostIDs for enclosing neighbors
        bool getEnclosingNeighbors (vector<id_t> &list);

        // whether is particular ID is the gateway node
        inline bool isGateway (id_t id);

        Node                _self;          // information regarding current node
        NodeState           _state;         // current state
        
        Addr                _gateway;       // info about the gateway server

        VSOPeer *           _VSOpeer;       // interface as a participant in a VON

        map<id_t, Node>     _neighbors;     // list of neighboring matchers
                                                                                                            
        map<id_t, Subscription> _subscriptions; // a list of subscribers managed by this matcher
                                                // searchable by the hostID of the subscriber

        int                 _overload_limit;    // # of subscriptions considered overload

        int                 _tick;          // number of logical steps
        
        //
        // stat collection
        //

        StatType        _stat_sub;          // statistics on actual subscriptions at this matcher
                                            
	};

} // namespace Vast

#endif
