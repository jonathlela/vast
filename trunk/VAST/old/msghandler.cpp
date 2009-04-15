/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007 Shun-Yun Hu (syhu@yahoo.com)
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

#include "msghandler.h"
#include "net_msg.h"
#include "vastbuf.h"

namespace Vast
{  
netmsg      *g_msgqueue = NULL;         // queue for all app-specific PAYLOAD messages

    // send app-specific (PAYLOAD) message to a particular node
	int 
    msghandler::send (id_t target, const char* data, size_t size, bool is_reliable, bool buffered)
    {
        return _net->sendmsg (target, PAYLOAD, data, size, /*_currtime,*/ is_reliable, buffered);
    }

    // receive app-specific (PAYLOAD) message to a particular node
	int 
    msghandler::recv (id_t &from, char **buffer)
    {
        static vastbuf buf;

        if (g_msgqueue == NULL)
            return 0;

        // retrieve the next message              
        netmsg *msg;
        g_msgqueue = g_msgqueue->getnext (&msg);
    
        // copy the msg into buffer
        size_t size = msg->size;
        from = msg->from;
        buf.reserve (size);
        buf.add (msg->msg, size);       
        *buffer = buf.data;

        delete msg;
        return size;
    }

    // add another msghandler after this one
    bool 
    msghandler::chain (msghandler *handler)
    {
        if (handler == NULL || _net == NULL)
            return false;

        msghandler *curr = this;

        // if new handler is already chained, simply insert, otherwise put it to the end
        if (handler->_nexthandler == NULL)
        {                
            while (curr->_nexthandler != NULL)
                curr = curr->_nexthandler;
        }

        // insert new handler at the current handler position
        msghandler *temp = curr->_nexthandler;
        curr->_nexthandler = handler;
        handler->_nexthandler = temp;

        // make sure the next handler uses the same network object as myself
        handler->setnet (_net);

        return true;
    }

    // remove a msghandler
    bool 
    msghandler::unchain (msghandler *handler)
    {
        if (handler == NULL || _net == NULL)
            return false;

        msghandler *curr = this;

        // find the message handler
        while (curr != NULL)
        {
            if (curr->_nexthandler == handler)
            {
                // simply point to the next
                curr->_nexthandler = handler->_nexthandler;
                return true;
            }
            curr = curr->_nexthandler;
        }

        return false;
    }

    // process ALL messages in queue, (TODO: change to one at a time?)
    // returns number of messages successfully handled

    int 
    msghandler::processmsg (/*timestamp_t currtime*/)
    {
        //_currtime = currtime;

        int num_msg = 0;

        id_t        from;
        msgtype_t   msgtype;
        timestamp_t recvtime;
        char       *msg;
        int         size;

        msghandler *curr;

        while ((size = _net->recvmsg (from, msgtype, recvtime, &msg/*, currtime*/)) >= 0)
        {
            // store PAYLOAD messages
            if (msgtype == PAYLOAD)
            {
                // create a buffer
                netmsg *newnode = new netmsg (from, msg, size, msgtype, recvtime, NULL, NULL);

                // put the content into a queue
                if (g_msgqueue == NULL)
                    g_msgqueue = newnode;
                else
                    g_msgqueue->append (newnode, recvtime);

                continue;
            }

            // loop through all handlers to see if they're processed
            curr = this;
            while (curr != NULL)
            {
                if (curr->handlemsg (from, msgtype, recvtime, msg, size) == true)
                {
                    num_msg++;
                    break;
                }
                else
                    curr = curr->_nexthandler;
            }
        }                

        // call post_processmsg () for all handlers
        curr = this;
        while (curr != NULL)
        {
            curr->post_processmsg ();
            curr = curr->_nexthandler;
        }

        return num_msg;
    }


} // end namespace Vast

