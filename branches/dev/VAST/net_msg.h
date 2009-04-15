/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005 Shun-Yun Hu (syhu@yahoo.com)
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
 * net_msg.h -- network message class
 *   
 *  
 */

#ifndef VAST_NET_MSG_H
#define VAST_NET_MSG_H

#include "typedef.h"
//#include "network.h"
#include <map>

namespace VAST {

    class netmsg
    {
    public:
        netmsg (id_t src, char const *m, size_t s, msgtype_t t, timestamp_t ts, netmsg *p = NULL, netmsg *n = NULL, id_t id = 0)
            :from(src), msg(NULL), msgtype(t), time(ts), size(s), msg_id (id)
        {
            if (p == NULL)
                p = this;
            if (n == NULL)
                n = this;

            prev = p;
            next = n;
            
            // joker: BCB does not allow allocating size of 0
            if (size > 0)
            {
                if ((msg = (char *)malloc (size)) == NULL)
                {
                    //ERROR_MSG ("memory allocation in netmsg\n");
                    printf ("ERROR: memory allocation in netmsg\n");
                    msg = NULL;
                    size = -1;
                    return;
                }                
                memcpy (msg, m, size);
            }            
        }
        
        ~netmsg ()
        {
            if (msg != NULL)
                free (msg);
        }

        // Q: allow insertion of earlier time?
        void append (netmsg *newnode, timestamp_t time)
        {
            // find the place to insert to keep timestamp ordering
            netmsg *start = this;
            netmsg *prv   = this->prev;
            netmsg *nxt   = this;
            
            // search from the end of queue
            while (prv != start && prv->time > time)
            {
                nxt = prv;
                prv = prv->prev;
            } 
            
            // update links for newnode
            newnode->prev = prv;
            newnode->next = nxt;
            
            // update links in prev and next nodes
            nxt->prev = newnode;
            prv->next = newnode;         
        }

        netmsg *getnext (netmsg **nextnode)
        {   
            *nextnode = this;
            netmsg *nxt;

            // update pointers in my prev & next neighbors
            if (this->next != this)
                this->next->prev = this->prev;
            if (this->prev != this)
                this->prev->next = this->next;

            // update the pointer of message queue
            if (this->next == this && this->prev == this)
                nxt = NULL;
            else
                nxt = this->next;            
                        
            return nxt;
        }
        
        id_t        from;
        char        *msg;
        msgtype_t   msgtype;
        timestamp_t time;
        size_t      size;
        id_t        msg_id;
        netmsg      *prev;
        netmsg      *next;
    };

    
} // end namespace VAST

#endif // VAST_NET_MSG_H


