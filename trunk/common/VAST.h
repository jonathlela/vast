/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2008 Shun-Yun Hu (syhu@yahoo.com)
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
 *  vast.h -- main VAST header, used by application     
 *
 *  history 2005/04/11  ver 0.1
 *          2007/01/11  ver 0.2     simplify interface   
 *			2008/08/20  ver 0.3		re-written for generic overlay
 *          2009/04/02  ver 0.4		re-defined for SPS
 */

#ifndef _VAST_H
#define _VAST_H

#include "config.h"
#include <vector>
#include "MessageHandler.h"
#include "Manager.h"
#include "Peer.h"

using namespace std;


namespace Vast
{

    class EXPORT VAST : public MessageHandler
    {

    public:

		//
		// main VAST methods
		//

        VAST (id_t id)            
            :MessageHandler (id)
        {
            _self.id = id;
            _join_state = ABSENT;
        }
        
        ~VAST ()
        {
        }
        
		/**
			join the overlay 

			@param  id          unique ID for the joining node
		*/
        bool        join (id_t id);

        // quit the overlay
        void        leave ();
        
		// specify a subscription area for point or area publications 
        // returns a unique subscription number that represents subscribed area
        id_t        subscribe (Area &area, layer_t layer);

        // send a message to all subscribers within a publication area
        bool        publish (Area &area, layer_t layer, Message &msg);
            
        // move a subscription area to a new position
        // returns actual position in case the position is already taken
        Position &  move (id_t sub_no, Position &pos);

        // send a custom message to a particular node
        bool        send (Node &target, Message &msg);

        // get a message from the network queue
        bool        receive (Message *msg);

        // obtain a list of subscribers with an area
        vector<Node *>& list (Area &area);

        // get current statistics about this node (a NULL-terminated string)
        char * getStat (bool clear = false);
        
        // get the current node's information
        Node * getSelf ();

        // whether the current node is joined
        bool isJoined ();

        // obtain access to Voronoi class (usually for drawing purpose)
        Voronoi *getVoronoi ();

    private:

        // main handler for various incoming messages
        // returns whether the message was successfully handled
        bool handleMessage (Message &in_msg);

        // flags               
        Node                _self;                  // information regarding current node
        vector<Peer *>      _peers;                 // peer roles deployed by this node               
        NodeState           _join_state;            // state of joining
    };

} // end namespace Vast

#endif // VAST_H
