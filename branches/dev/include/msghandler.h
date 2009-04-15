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
 *  msghandler.h -- interface to extend message processing functions
 *
 *  history:    2006/07/19
 *              2007/01/10  allow chaining of an already chained message handler
 *   
 */

#ifndef VAST_MSGHANDLER_H
#define VAST_MSGHANDLER_H

#include "network.h"

#define TIMEOUT_COUNTER             10      // how many time-steps should elapse before we retry a network action

namespace VAST 
{        
    class EXPORT msghandler 
    {
    public:
        msghandler ()
        {
            //_currtime = 0;
            _net = NULL;
            _nexthandler = NULL;
        }      

        virtual ~msghandler () {}
        
        // store network layer so that the logic in processmsg may send message to network
        void setnet(network *netlayer)
        {
            _net = netlayer;
        }

        network *       
        getnet ()
        {
            return _net;
        }

        // send app-specific (PAYLOAD) message to a particular node
		int send (id_t target, const char* data, size_t size, bool is_reliable, bool buffered = false);

        // receive app-specific (PAYLOAD) message to a particular node
		int recv (id_t &from, char **buffer);
        
        // add another msghandler after this one
        bool chain (msghandler *handler);

        // remove a msghandler
        bool unchain (msghandler *handler);
        
    protected:

        // returns whether the message has been handled successfully
        virtual bool handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size) = 0;

        // do things after messages are all handled
        virtual void post_processmsg () = 0;

        // process ALL messages in queue, (TODO: change to one at a time?)
        // returns number of messages successfully handled
        int processmsg (/*timestamp_t currtime*/);

        network     *_net;
        msghandler  *_nexthandler;
        //timestamp_t  _currtime;

    };

} // end namespace VAST

#endif // VAST_MSGHANDLER_H
