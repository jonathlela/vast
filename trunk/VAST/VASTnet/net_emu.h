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
 *  TODO
 */

/*
 * net_emu.h -- network implementation interface (virtual class)
 *   
 *  
 */

#ifndef VAST_NET_EMU_H
#define VAST_NET_EMU_H

#include "VASTnet.h"
#include "net_emubridge.h"
#include "net_emubridge_bl.h"
#include "VASTUtil.h"

namespace Vast {

    class net_emu : public Vast::net_manager
    {
    public:
        net_emu (timestamp_t sec2timestamp);

        ~net_emu ();
        
        /*
        // assignment to shut VC compiler warning
        net_emu &operator=(const net_emu& c)
        {  
            //_msgqueue = c._msgqueue;
            //_bridge   = c._bridge;

            return *this;
        }
        */
        
        //
        // inherent methods from class 'net_manager'
        //
       
        virtual void start ();
        virtual void stop ();

        // get current physical timestamp
        timestamp_t getTimestamp ();

        // get IP address from host name
        const char *getIPFromHost (const char *hostname);

        // obtain the IP / port of a remotely connected host
        bool getRemoteAddress (id_t host_id, IPaddr &addr);

        // connect or disconnect a remote node (should check for redundency)       
        virtual bool connect (id_t target, unsigned int host, unsigned short port, bool is_secure = false);
        virtual bool disconnect (id_t target);

        // send an outgoing message to a remote host
        // return the number of bytes sent
        virtual size_t send (id_t target, char const *msg, size_t size, const Addr *addr = NULL);
        
        // receive an incoming message
        // return pointer to valid NetSocketMsg structure or NULL for no messages
        virtual NetSocketMsg *receive ();

        // change the ID for a remote host
        bool switchID (id_t prevID, id_t newID);

        // perform a tick of the logical clock 
        void tickLogicalClock ();

        // store a message into priority queue
        // returns success or not
        bool msg_received (id_t fromhost, const char *message, size_t size, timestamp_t recvtime = 0, bool in_front = false);

        bool socket_connected (id_t id, void *stream, bool is_secure);
        bool socket_disconnected (id_t id);

        //
        // emulator-specific methods
        //               
        virtual bool remoteConnect (id_t remote_id, Addr const &addr);
        virtual void remoteDisconnect (id_t remote_id);

    protected:



    };

} // end namespace Vast

#endif // VAST_NET_EMU_H
