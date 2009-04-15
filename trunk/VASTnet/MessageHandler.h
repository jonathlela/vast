/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2006-2007 Shun-Yun Hu (syhu@yahoo.com)
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
 *  MessageHandler.h -- interface to extend message processing functions
 *
 *  history:    2006/07/19
 *              2007/01/10  allow chaining of an already chained message handler
 *              2008/08/21  re-design to support MessageQueue class
 *   
 */

#ifndef _VAST_MessageHandler_H
#define _VAST_MessageHandler_H

#include "VASTnet.h"

using namespace std;

namespace Vast 
{        
    class EXPORT MessageHandler
    {
    
    friend class MessageQueue;

    public:
        // init by specifying the unique handler ID (= NodeID)
        MessageHandler (id_t id)
            :_id (id)
        {
        }

        virtual ~MessageHandler () {}

        // replace the current / existing unique ID
        //void registerID (id_t my_id);

        // add additional handlers to the same message queue
        bool addHandler (MessageHandler *handler);

        // remove handlers added from current handler
        bool removeHandler (MessageHandler *handler);

        // perform initialization tasks for this handler (optional)
        // NOTE that all internal variables (such as handler_no) have been set at this point
        virtual void initHandler () {}

        // returns whether the message has been handled successfully
        virtual bool handleMessage (Message &in_msg) = 0;

        // perform some tasks after all messages have been handled (default does nothing)        
        virtual void postHandling () {}

    protected:

        // functions related to network interface      
        //Addr &getNetworkAddress (id_t id);

        // send a message to some target nodes in the current group         
        int sendMessage (Message &msg, bool is_reliable);

        // used by MessageQueue only to store pointer to the MessageQueue
        // returns the unique ID for this handler
        id_t setQueue (void *msgqueue, VASTnet *net);

        id_t         _id;               // unique ID to identify this message handler in a MessageQueue
        void *       _msgqueue;         // pointer to message queue (in case to register new handlers from within)    
        VASTnet *    _net;              // pointer to network interface
    };

} // end namespace Vast

#endif // VAST_MSGHANDLER_H
