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
#include "VASTUtil.h"

namespace Vast {

    class net_emu : public Vast::VASTnet
    {
    public:
        net_emu (net_emubridge &b)
            :_bridge (b)
        {
            // obtain a temporary id first
            _id = _bridge.obtain_id (this);
            
            Addr addr;
            _id2addr[_id] = addr;
        }

        ~net_emu ()
        {
            // remove from bridge so that others can't find me
            _bridge.release_id (_id);

            // free up message queue
            std::multimap<timestamp_t, netmsg *>::iterator it = _msgqueue.begin ();
            for (; it != _msgqueue.end (); it++)
                delete it->second;
        }

        //
        // inherent methods from class 'VASTnet'
        //
       
        virtual void start ()
        {
            _active = true;
        }

        virtual void stop ()
        {
            _active = false;
            VASTnet::stop ();
        }

        virtual void registerID (Vast::id_t my_id);

        // get current physical timestamp
        timestamp_t getTimestamp ()
        {
            return _bridge.getTimestamp ();
        }

        //
        // emulator-specific methods
        //               
        virtual bool remoteConnect (Vast::id_t remote_id, Addr const &addr);
        virtual void remoteDisconnect (Vast::id_t remote_id);

    protected:

        // connect or disconnect a remote node (should check for redundency)
        // returns (-1) for error, (0) for success        
        virtual int connect (id_t target);
        virtual int disconnect (id_t target);

        // send an outgoing message to a remote host
        // return the number of bytes sent
        virtual size_t send (id_t target, char const*msg, size_t size, bool reliable = true);
        
        // receive an incoming message
        // return success or not
        virtual bool receive (netmsg **msg);

        virtual int storeMessage (Vast::id_t from, char const *msg, size_t len, timestamp_t time);
    
        // a shared object among net_emu to discover other end-points
        net_emubridge &                         _bridge;

        // queue for incoming messages
        std::multimap<timestamp_t, netmsg *>    _msgqueue;
        
		// added by yuli ====================================================
        // Csc 20080225: change to 'static'
        //   it's just a namespace 'Compressor', so declare as static maybe wastes less memory?
        static Compressor                       _compressor;

    };

} // end namespace Vast

#endif // VAST_NET_EMU_H
