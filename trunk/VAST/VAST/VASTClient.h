/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2011 Shun-Yun Hu (syhu@ieee.org)
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
 *  VASTClient.h    Implementation class for the VAST interface. 
 *              
 *      history:    2009/04/28  separated from VAST.h
 */

#ifndef _VAST_CLIENT_H
#define _VAST_CLIENT_H

#include "Config.h"
#include "VASTTypes.h"
#include "VAST.h"
#include "MessageHandler.h"
#include "VASTRelay.h"
#include <vector>

// NOTE: remove ghost should be at least twice as much as keepalive

const int TIMEOUT_JOIN          = (5);        // # of seconds before re-attempting to subscribe 
const int TIMEOUT_SUBSCRIBE     = (5);        // # of seconds before re-attempting to subscribe 
const int TIMEOUT_REMOVE_GHOST  = (5);        // # of seconds before removing ghost objects at clients
//const int TIMEOUT_KEEP_ALIVE   = (2);        // # of seconds before re-sending our own position

using namespace std;

namespace Vast
{

    //
    // NOTE that we currently assume two things are decided external to VAST class:
    //
    //          1. unique ID
    //          2. a coordinate representing physical location      
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

        // join the overlay (so we can perform subscription)
        bool        join (const IPaddr &gateway, world_t worldID = VAST_DEFAULT_WORLD_ID);

        // quit the overlay
        void        leave ();
        
		// specify a subscription area for point or area publications 
        bool        subscribe (Area &area, layer_t layer);

        // send a message to all subscribers within a publication area
        bool        publish (Area &area, layer_t layer, Message &message);
            
        // move a subscription area to a new position
        // returns actual AOI in case the position is already taken, or NULL of subID is invalid
        // 'update_only' indicates if move request should not be actually sent (avoid redundent MOVEMENT request when used with VASTATE)
        Area *      move (id_t subID, Area &aoi, bool update_only = false);

        // send a custom message to a particular node
        // returns the number of successful send targets
        int        send (Message &message, vector<id_t> *failed = NULL, bool direct = false);

        // obtain a list of subscribers at the same layer with an area
        vector<Node *>& list (Area *area = NULL);

        // obtain a list of physically closest hosts
        vector<Node *>& getPhysicalNeighbors ();

        // obtain a list of logically closest hosts (a subset of relay-level nodes returned by list ())
        vector<Node *>& getLogicalNeighbors ();   

        // get a message from the network queue
        Message *   receive ();

        // report some message to gateway (to be processed or recorded)
        bool        reportGateway (Message &message);

        // report some message to origin matcher
        bool        reportOrigin (Message &message);

        // get current statistics about this node (a NULL-terminated string)
        char * getStat (bool clear = false);
        
        // get the current node's information
        Node * getSelf ();

        // whether the current node is joined
        bool isJoined ();

        // whether the current node is listening for publications, 
        // returns subscription ID, 0 indicates no subscription
        id_t getSubscriptionID ();

        // get the world_id I'm currently joining
        world_t getWorldID ();

        // if I'm a relay
        inline int isRelay ();

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

        //
        // neighbor handling methods
        //

        bool addNeighbor (Node &neighbor, timestamp_t now);
        bool removeNeighbor (id_t id);
        bool requestNeighbor (id_t id);

        //
        // fault tolerance mechanism
        //

        // matcher message with error checking, default priority is 1
        bool sendMatcherMessage (Message &msg, byte_t priority = 0, vector<id_t> *failed = NULL);

        // gateway message with error checking, priority is 1, msggroup must specify
        bool sendGatewayMessage (Message &msg, byte_t msggroup);

        // deal with matcher disconnection or non-update
        void handleMatcherDisconnect ();

        // re-update our position every once in a while
        //void sendKeepAlive ();

        // remove ghost subscribers (those no longer updating)
        void removeGhosts ();

        // make one latency record
        void recordLatency (msgtype_t msgtype, timestamp_t sendtime);

        // variables used by VASTClient component
        Node                _self;          // information regarding current node
        NodeState           _state;         // state of joining                
        vector<Node *>      _neighbors;     // list of current AOI neighbors
        vector<Node *>      _physicals;     // list of physical neighbors
        vector<Node *>      _logicals;      // list of logical neighbors
        
        id_t                _matcher_id;    // hostID for interest matcher
        id_t                _closest_id;    // hostID for the closest neighbor matcher
        VASTRelay          *_relay;         // pointer to VASTRelay (to obtain relayID)        

        Subscription        _sub;           // my subscription 
        world_t             _world_id;      // my current worldID

        // timeouts
        timestamp_t         _next_periodic;     // next timestamp to perform tasks
        timestamp_t         _timeout_join;      // timeout for re-attempt to join
        timestamp_t         _timeout_subscribe; // timeout for re-attempt to subscribe        
        map<id_t, timestamp_t> _last_update;    // last update time for a particular neighbor

        Addr                _gateway;       // info about the gateway server
        
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
