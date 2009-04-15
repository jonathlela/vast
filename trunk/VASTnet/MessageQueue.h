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
 *  MessageQueue.h -- a queue to hook message handlers for processing incoming messages
 *                    also allows sending messages to a given target
 *
 *   
 */

#ifndef _VAST_MessageQueue_H
#define _VAST_MessageQueue_H

#include "VASTnet.h"
#include "MessageHandler.h"
#include "IDMapper.h"
#include <map>

// the most significant 30 bits are the host's NodeID
#define EXTRACT_HOST_ID(x) ((0xFFFFFFFF - 3) & x)

using namespace std;

namespace Vast 
{
    class EXPORT MessageQueue
    {
    public:
        MessageQueue (VASTnet *net)
            :_net (net)
        {
            _net->start ();
        }

        ~MessageQueue ()
        {            
            _net->stop ();
        }

        // process all currently received messages (invoking previously registered handlers)
        // return the number of messsages processed
        int processMessages ();

        // store message handler for a particular group of messages (specified by group_id)
        // return false if handler already exists
        bool registerHandler (MessageHandler *handler);

        // unregister an existing handler
        bool unregisterHandler (MessageHandler *handler);

        // functions related to network interface
        //Addr &getNetworkAddress (id_t id);

        void registerID (id_t my_id);

        // send a message to some target nodes         
        int sendMessage (Message &msg, bool is_reliable = true);

    private:
        VASTnet *                       _net;           // network interface to send/recv messages
        map<id_t, MessageHandler *>     _handlers;      // map from <group_id, handler_id> pair to handler        
           
        // map from handler's ID to physical host's nodeID (representing a network end-point)
        map<id_t, id_t>                 _id2host;           
        IDMapper                        _idmapper;
    };

} // end namespace Vast

#endif
