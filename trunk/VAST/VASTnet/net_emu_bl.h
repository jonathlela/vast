/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007 Shao-Chen Chang (cscxcs at gmail.com)
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
 * net_emu_bl.h -- network layer implementation for emulation
 *                 bandwidth limited version (revolution 2)
 *  
 */

#ifndef VAST_NET_EMU_BL_H
#define VAST_NET_EMU_BL_H

#include "VASTTypes.h"

#include "net_emu.h"
#include "net_emubridge_bl.h"
#include "net_msg.h"
#include <math.h> // min ()
#include <map>
#include <vector>


// Send buffer size (NETBL_SENDBUFFER_MULTIPLIER * peer upload bandwidth)
#define NETBL_SENDBUFFER_MULTIPLIER (2)
// Send buffer minimumu size (send buffer will not smaller than this size 
//                            in case peer has a small upload bandwidth)
#define NETBL_SENDBUFFER_MIN        (4096)

namespace Vast {

    class net_emu_bl : public net_emu
    {
    public:
        net_emu_bl (net_emubridge_bl &b)
            : net_emu (b), _bridgebl (b), _last_actqueue_id (0), _last_msgid (0)
        {
        }

        ~net_emu_bl ()
        {
        }

        // assignment to shut VC compiler warning
        net_emu_bl &operator=(const net_emu_bl& c)
        { 
            _bridgebl = c._bridgebl;
            return *this;
        }

        // calculate total packet size (add header size)
        inline static size_t packetsize (size_t payload_size)
        {
            //return ((payload_size) + sizeof (msgtype_t) + sizeof (timestamp_t));
			return payload_size;
        }

        //
        // inherent methods from class 'network'
        //

        // connect or disconnect a remote node (should check for redundency)
        // returns (-1) for error, (0) for success
        int disconnect (id_t target);

        // send an outgoing message to a remote host
        // return the number of bytes sent
        size_t send (id_t target, char const *msg, size_t size, bool reliable = true);

		// send out all pending messages to each host
		// return number of bytes sent
		//size_t flush (bool compress = false);
		size_t clearQueue ();

        // bandwidth limitation settings
        void setBandwidthLimit (bandwidth_t type, size_t limit)
        {
            switch (type)
            {
            case BW_UPLOAD:
                _bridgebl.node_upcap[_id] = limit;
                break;
            case BW_DOWNLOAD:
                _bridgebl.node_downcap[_id] = limit;
                break;
            default:
                ;
            }
        }
        
        //
        // emulator-specific methods
        //

        void remoteDisconnect (Vast::id_t remote_id);

    protected:

        // a shared object among net_emu class to discover class pointer via unique id
        net_emubridge_bl &              _bridgebl;

        // the size can be transmitted for the specified destination, how much size of this message I have sent at previous step
        std::map<id_t, size_t>          _sendspace;

        // total packet size of a specified queue in sendqueue
        std::map<id_t, size_t>          _sendqueue_size;

        // map from message ID to its state for messages in send queue
        std::map<netmsg*, int>          _msg2state;

        // id for last active sendqueue
        id_t                            _last_actqueue_id;

        // queue for outgoing messages (a mapping from destination to message queue)
        std::map<id_t, netmsg *>        _sendqueue;		

        // last message ID used, used for allocating new message IDs
        id_t                            _last_msgid;
    };

} // end namespace Vast

#endif // VAST_NET_EMU_BL_H
