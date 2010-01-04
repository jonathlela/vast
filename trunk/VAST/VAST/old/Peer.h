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



#ifndef _VAST_Peer_H
#define _VAST_Peer_H

#include "Config.h"
#include "VASTTypes.h"
#include "Voronoi.h"
#include "VONNetwork.h"
#include "VONPeer.h"

using namespace std;

namespace Vast
{

    // 
    // This class allows a node to join as "VONPeer", which allows the user client
    // to execute publish / subscribe tasks, and lives within a super-peer "Relay"
    // 
    class Peer : public MessageHandler, public VONNetwork, public VONPeer
    {

    public:

        Peer (id_t handlerID, id_t sub_no, id_t fromhost)
            :MessageHandler (handlerID), VONPeer (sub_no, this)
        {
        }

        ~Peer ();                        

        /*
        // join & leave the overlay 
        void        join (Area &aoi, layer_t layer, Node *gateway = NULL);

        // move a subscription area to a new position
        // returns actual position in case the position is already taken
        Area &      move (Area &aoi);

        // get the current node's information
        Node *      getSelf ();

        // get info for a particular neighbor
        // returns NULL if neighbor doesn't exist
        Node *      getNeighbor (id_t id);

        // obtain a list of subscribers with an area
        vector<Node *>& getNeighbors ();

        // obtain access to Voronoi class (usually for drawing purpose)
        Voronoi *   getVoronoi ();

        // obtain states on which neighbors have changed
        map<id_t, NeighborUpdateStatus>& getUpdateStatus ();
        */

        //
        // VONNetwork
        //

        // send messages to some target nodes
        // returns number of bytes sent
        size_t sendMessage (Message &msg, bool is_reliable = true);

        // obtain next message in queue
        // return pointer to Message, or NULL for no more message
        Message* receiveMessage (timestamp_t &senttime);

        // notify the network layer of nodeID -> Address mapping        
        bool notifyMapping (id_t node_id, Addr &addr);

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

    };

} // namespace Vast

#endif