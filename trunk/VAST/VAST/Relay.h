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
 *  Relay.h     Implementation class for the VAST interface. 
 *              Acts as both a Client at regular peers and Relay at super-peers
 *              
 *    history:  2009/04/28  separated from VAST.h
 */

#ifndef _VAST_RELAY_H
#define _VAST_RELAY_H

#include "Config.h"
#include "VASTTypes.h"
#include "VAST.h"
#include "MessageHandler.h"
#include "Voronoi.h"
#include <vector>
#include "VONPeer.h"


const int TIMEOUT_QUERY = 30;        // number of "ticks" before resending join request
const int TIMEOUT_JOIN  = 30;        // number of "ticks" before resending join request

using namespace std;

namespace Vast
{

    // NOTE: we start the message number slightly higher than VONPeer
    typedef enum 
    {
        // internal message # must begin with VON_MAX_MSG as VONpeer is used and share the MessageQueue        
        QUERY = VON_MAX_MSG,// find the closet node to a particular physical coordinate
        QUERY_REPLY,        // the closest node
        JOIN,               // connect to an existing relay to join the overlay
        JOIN_REPLY,         // a list of relays to contact
        LEAVE,              // depart from the overlay
        RELAY,              // notification of a new relay
        SUBSCRIBE,          // send subscription
        SUBSCRIBE_REPLY,    // to reply whether a node has successfully subscribed (VON node joined)
        PUBLISH,            // publish a message             
        MOVE,               // position update to normal nodes
        MOVE_F,             // full update for an AOI region
        NEIGHBOR,           // send back a list of known neighbors
        MESSAGE,            // deliver a message to a node        
    } VAST_Message;

    // definition for a subscription
    typedef struct 
    {
        id_t    sub_id;         // subscription number
        Area    aoi;            // aoi of the subscription (including a center position)
        layer_t layer;          // layer number for the subscription
        bool    active;         // whether the subscription is successful
        
    } Subscription;

    //
    // NOTE that we currently assume two things are decided
    //      outside of the VAST class:
    //
    //          1. unique ID (hostID obtained via IDGenerator, handlerID by MessageQueue)
    //          2. a coordinate representing physical location
    //      
    //      Also, a VAST node (the same one) serves as a Client at a regular peer, 
    //            but a Relay at a super-peer
    //
    class Relay : public VAST, public VONNetwork
    {

    public:

		//
		// main VAST methods
		//

        Relay (id_t host_id, int peer_limit = 0, int relay_limit = 0);
        ~Relay ();
        
		/**
			join the overlay 

			@param  pos     physical coordinate of the joining client node
		*/
        // specify a joining position (physical coordinate?)
        bool        join (Position &pos, bool as_relay = true);

        // quit the overlay
        void        leave ();
        
		// specify a subscription area for point or area publications 
        // returns a unique subscription number that represents subscribed area
        id_t        subscribe (Area &area, layer_t layer);

        // send a message to all subscribers within a publication area
        bool        publish (Area &area, layer_t layer, Message &message);
            
        // move a subscription area to a new position
        // returns actual AOI in case the position is already taken, or NULL of subNo is invalid
        // 'update_only' indicates if move request should not be actually sent (avoid redundent MOVEMENT request when used with VASTATE)
        Area *      move (id_t subNo, Area &aoi, bool update_only = false);

        // send a custom message to a particular node
        bool        send (Message &message);

        // obtain a list of subscribers with an area
        vector<Node *>& list (Area *area = NULL);

        // get a message from the network queue
        Message *   receive ();
    
        // get current statistics about this node (a NULL-terminated string)
        char * getStat (bool clear = false);
        
        // get the current node's information
        Node * getSelf ();

        // obtain a list of peers hosted on this relay
        map<id_t, VONPeer *>& getPeers ();

        // whether the current node is joined
        bool isJoined ();

        // whether the current node is listening for publications
        bool isSubscribing (id_t sub_no);

        // if myself is a relay
        inline bool isRelay ();

        // whether I have public IP
        bool hasPublicIP ();

        // 
        // GUI helper functions 
        //

        // get a particular peer's info
        Node *getPeer (id_t peer_id);

        // get the neighbors for a particular peer
        // returns NULL if the peer does not exist
        vector<Node *> *getPeerNeighbors (id_t peer_id);

        // obtain access to Voronoi class (usually for drawing purpose)
        // returns NULL if the peer does not exist
        Voronoi *getVoronoi (id_t peer_id);

        // get message latencies, currently supports PUBLISH & MOVE types
        StatType *getMessageLatency (msgtype_t msgtype);

        // get # of peers hosted at this relay, returns NULL for no record
        StatType *getPeerStat ();

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

        // find the relay closest to a position, which can be myself
        // returns the relay's Node info
        Node * closestRelay (Position &pos);
               
        // connect to the closest, currently known relay
        bool joinRelay (bool as_relay, Node *relay = NULL);

        // add a newly learned node as a Relay
        void addRelay (Node &relay);

        // remove a relay no longer valid
        void removeRelay (id_t id);

        // remove non-useful relays
        void cleanupRelays ();

        // check if relay re-joining is successful, or attempt to join the next available 
        void checkRelayRejoin ();

        // create a new Peer instance at this Relay
        bool addPeer (id_t fromhost, id_t sub_no, Area &area, layer_t layer);

        // remove the Peer instance on this Relay
        bool removePeer (id_t sub_no);

        // update the states of Peer (whether they've successfully joined VON and starts listening)
        void updatePeerStates ();

        // store a message to the local queue to be retrieved by receive ()
        void storeMessage (Message &msg);

        // make one latency record
        void recordLatency (msgtype_t msgtype, timestamp_t sendtime);

        // variables used by Client component
        Node                _self;      // information regarding current node
        NodeState           _state;     // state of joining                
        vector<Node *>      _neighbors; // list of current AOI neighbors
        id_t                _relay_id;  // the relay I currently use (could be myself)

        map<id_t, Subscription> _sub_list; // list of subscriptions 

        // variables used by Relay component
        map<id_t,VONPeer *> _peers;     // pointers to peers hosted on this relay (might include my own peer)
        map<id_t,NodeState> _peer_state;// the join state of peers hosted on this relay
        map<id_t, id_t>     _peer2host; // host_IDs of the peers hosted on this relay
        multimap<double, Node *> _relays;    // pointers to known relays (neighbors or otherwise), listed / queried by distance
        size_t              _relaylimit;// # of relays this node keeps

        size_t              _peerlimit; // number of peers I can accomodate, could vary depending on load
        map<id_t, Node>     _accepted;  // list of accepted clients to this relay

        vector<Message *>   _msglist;   // record for incoming messages
        VASTBuffer          _recv_buf;  // a receive buffer for incoming messages
        Message *           _lastmsg;   // last message received from network (to be deleted)
                                        // TODO: a better way for it?

        // counter for timeout
        int                 _query_timeout; // timeout for querying the initial relay
        int                 _join_timeout;  // timeout for joining a relay

        // stats
        map<msgtype_t, StatType>  _latency; // latencies for different message types 
        StatType            _peerstat;      // stats for peers hosted on this relay

    };

} // end namespace Vast

#endif // VAST_RELAY_H
