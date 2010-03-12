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
 *  VONPeer.h -- a logical node that performs Voronoi-based Overlay Network (VON) functions 
 *               main functions include: join, move, getNeighbors 
 *
 *               NOTE that we assume the following:
 *                  - each VON node is uniquely identified by an external ID
 *                  - each VON node can be associated with a physically reachable address specified in Addr
 *
 *  history 2009/04/16      adopted from the Manager.h class since previous design
 *          2009/06/05      extracted from Peer.h in SPS design into independent VON class 
 *
 */



#ifndef _VAST_VONPeer_H
#define _VAST_VONPeer_H

#include "Config.h"
#include "VASTTypes.h"
#include "Voronoi.h"
#include "VONNetwork.h"

using namespace std;

namespace Vast
{

// NOTE that we assume roughly 10 ticks per second
#define TICK_PER_SECOND     (10)

#define MAX_DROP_COUNT      (2  * TICK_PER_SECOND) // # of ticks to disconnect a non-overlapped neighbor
#define MAX_TIMELY_PERIOD   (60 * TICK_PER_SECOND) // # of ticks to be considered as still active
    
// NOTE: a way to estimate the proper buffer:
//       average speed * 3 hops (from neighbor detection to discovery) * 2 (nodes heading towards directly)
//       = 5 * 3 * 2 = 30

#define AOI_DETECTION_BUFFER      (32)      // detection buffer around AOI
#define NONOVERLAP_MULTIPLIER     (1.25)    // multiplier for aoi buffer for determining non-overlap

// TODO: should check for requested nodes only, otherwise may create too much traffic

#define CHECK_EN_ONLY_                  // a boundary neighbor checks only its ENs as opposed to all AOI neighbors during neighbor discovery                                    
#define CHECK_REQNODE_ONLY_             // check neighbor discovery only for those who have asked

    typedef enum 
    {
        NEIGHBOR_OVERLAPPED = 1,
        NEIGHBOR_ENCLOSED
    } NeighborStates;



    // WARNING: VON messages currently should not exceed VON_MAX_MSG defined in Config.h
    //          otherwise there may be ID collisons with other handlers that use VONpeer
    //          internally (e.g., VASTClient in VAST or Arbitrator in VASTATE)
    typedef enum
    {
        VON_DISCONNECT = 0, // VON's disconnect
        VON_QUERY,          // VON's query, to find an acceptor that can take in a joining node
        VON_HELLO,          // VON's hello, to let a newly learned node to be mutually aware
        VON_HELLO_R,        // VON's hello response
        VON_EN,             // VON's enclosing neighbor inquiry (to see if my knowledge of EN is complete)
        VON_MOVE,           // VON's move, to notify AOI neighbors of new/current position
        VON_MOVE_F,         // VON's move, full notification with AOI
        VON_MOVE_B,         // VON's move for boundary neighbors
        VON_MOVE_FB,        // VON's move for boundary neighbors with full notification on AOI
        VON_BYE,            // VON's disconnecting a remote node
        VON_NODE            // discovery of new nodes 
    } VON_Message;



    // 
    // This class joins a node as "VONPeer", which allows the user client
    // to execute VON commands: move, getNeighbors
    // 
    class EXPORT VONPeer
    {

    public:

        VONPeer (id_t id, VONNetwork *net, length_t aoi_buffer = AOI_DETECTION_BUFFER);        
        ~VONPeer ();                        

        // join & leave the overlay 
        void        join (Area &aoi, Node *gateway = NULL);
        void        leave (bool notify_neighbors = true);
                        
        // move a subscription area to a new position
        // returns actual position in case the position is already taken
        Area &      move (Area &aoi, timestamp_t sendtime = 0);

        // whether we have joined the VON        
        bool        isJoined ();

        // get info for a particular neighbor
        // returns NULL if neighbor doesn't exist
        Node *      getNeighbor (id_t id);

        // obtain a list of subscribers with an area
        vector<Node *>& getNeighbors ();

        // get the current node's information
        Node *      getSelf ();

        // obtain access to Voronoi class (usually for drawing purpose)
        Voronoi *   getVoronoi ();

        // obtain states on which neighbors have changed
        map<id_t, NeighborUpdateStatus>& getUpdateStatus ();
   
        // process incoming messages
        void tick ();

        // returns whether the message was successfully handled
        bool handleMessage (Message &in_msg);

private:

        //
        //  process functions
        //

        // re-send current position once in a while to keep neighbors interested in me
        void sendKeepAlive ();

        // notify new neighbors with HELLO message
        void contactNewNeighbors ();

        // perform neighbor discovery check
        void checkNeighborDiscovery ();

        // check consistency of enclosing neighbors
        void checkConsistency (id_t skip_id = 0);

        // check to disconnect neighbors no longer in view
        // returns number of neighbors removed        
        int removeNonOverlapped ();

        //
        // neighbor management inside Voronoi map
        //

        // insert a new node, will connect to the node if not already connected
        bool insertNode (Node &node);
        bool deleteNode (id_t id);
        bool updateNode (Node &node);

        //
        // helper functions
        //

        // send node info to a particular node
        void sendNodes (id_t target, vector<id_t> &list, bool reliable = false);

        // send self info & a list of IDs to a particular node
        void sendHello (id_t target, vector<id_t> &id_list);

        // send a particular node its perceived enclosing neighbors
        void sendEN (id_t target); 

        // neighbor type determination helpers
        inline bool isNeighbor (id_t id);
        inline Position &isOverlapped (Position &pos);

        // is a particular neighbor within AOI
        inline bool isAOINeighbor (id_t id, Node &neighbor, length_t buffer = 0);

        // whether a neighbor is either 1) AOI neighbor of another or 2) enclosing neighbors to each other
        inline bool isRelevantNeighbor (Node &node1, Node &node2, length_t buffer = 0);

        // whether a neighbor has stayed alive with regular updates
        inline bool isTimelyNeighbor (id_t id, timestamp_t period = MAX_TIMELY_PERIOD);

        // check if a node is self
        inline bool isSelf (id_t id);

        VONNetwork         *_net;                       // pointer to network interface
        Node                _self;                      // info about my self
        NodeState           _state;                     // state of joining
        vector<Node *>      _neighbors;                 // a list of currently connected neighboring managers
        
        Voronoi            *_Voronoi;                   // a Voronoi diagram for keeping AOI neighbors                
                 
        map<id_t, Node>     _id2node;                   // mapping from id to basic node info                        
        map<id_t, map<id_t, int> *> _neighbor_states;   // neighbors' knowledge of my neighbors (for neighbor discovery check)        
        map<id_t, Node>     _new_neighbors;             // nodes worth considering to connect        
        map<id_t, Node>     _potential_neighbors;       // nodes that 
        map<id_t, bool>     _req_nodes;                 // nodes requesting for neighbor discovery check        
                
        map<id_t, NeighborUpdateStatus> _updateStatus;  // status about whether a neighbor is inserted/deleted/updated
                                                        //  1: inserted, 2: deleted, 3: updated

        length_t            _aoi_buffer;                // additional buffersize for checking relevant AOI neighbors

        // internal statistics
        Ratio               _NEIGHBOR_Message;          // stats for NodeMessages received
        map<id_t, int>      _count_drop;                // counter for disconnecting a remote node
        size_t              _tick_count;                // counter for how many ticks have occurred
    };

} // namespace Vast

#endif
