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
 * net_emu_bl.h -- network implementation interface (virtual class)
 *                 bandwidth limited version
 *  
 */

#ifndef VAST_NET_EMU_BL_H
#define VAST_NET_EMU_BL_H

#include "typedef.h"
#include "network.h"
#include "net_msg.h"
#include "net_emubridge_bl.h"
#include "vastbuf.h"
#include <map>
#include <vector>

// Send buffer size (NETBL_SENDBUFFER_MULTIPLIER * peer upload bandwidth)
#define NETBL_SENDBUFFER_MULTIPLIER (2)
// Send buffer minimumu size (send buffer will not smaller than this size 
//                            in case peer has a small upload bandwidth)
#define NETBL_SENDBUFFER_MIN        (4096)

namespace VAST {

    class net_emu_bl : public network
    {
    public:
        net_emu_bl (net_emubridge_bl &b)
            :_bridge (b), _last_actqueue_id (0), _active (false)
        {
            // obtain a temporary id first
            _id = _bridge.obtain_id (this);

            //printf ("tempid: %d\n", _id);
        }

        ~net_emu_bl ()
        {
            // remove from bridge so that others can't find me
            _bridge.release_id (_id);

            // free up message queue
            netmsg * msg;
            std::multimap<timestamp_t, netmsg *>::iterator it = _msgqueue.begin ();
            for (; it != _msgqueue.end (); it++)
            {
                while (it->second != NULL)
                {
                    it->second = it->second->getnext (&msg);
                    delete msg;
                }
            }
        }

        //
        // inherent methods from class 'network'
        //

        void register_id (VAST::id_t my_id);

        void start ()
        {
            _active = true;
        }

        void stop ()
        {
            _active = false;
            
            // remove all existing connections
            std::vector<VAST::id_t> remove_list;
            std::map<VAST::id_t, Addr>::iterator it = _id2addr.begin ();
            for (; it != _id2addr.end (); it++)
                remove_list.push_back (it->first);

            for (unsigned int i=0; i<remove_list.size (); i++)
                disconnect (remove_list[i]);            
        }

        // connect or disconnect a remote node (should check for redundency)
        // returns (-1) for error, (0) for success
        int connect (VAST::id_t target, Addr addr);
        int connect (id_t target);
        int connect (Addr & addr);
        int disconnect (VAST::id_t target);

        Addr &getaddr (VAST::id_t id)
        {
            static Addr s_addr;
            if (is_connected (id))
                return _id2addr[id];

            return s_addr;
        }

        // get a list of currently active connections' remote id and IP addresses
        std::map<VAST::id_t, Addr> &getconn ()
        {
            return _id2addr;
        }

        // send message to a node
        int sendmsg (VAST::id_t target, msgtype_t msgtype, char const *msg, size_t len, bool reliable = true, bool buffered = false);
        
        // obtain next message in queue before a given timestamp
        // returns size of message, or -1 for no more message
        int recvmsg (VAST::id_t &from, msgtype_t &msgtype, timestamp_t &recvtime, char **msg);

        // send out all pending reliable message in a single packet to each target
        int flush (bool compress = false);

        // notify ip mapper to create a series of mapping from src_id to every one in the list map_to
        int notify_id_mapper (id_t src_id, const std::vector<id_t> & map_to);

        // get current physical timestamp
        timestamp_t get_curr_timestamp ()
        {
            return _bridge.get_curr_timestamp ();
        }

        // bandwidth limitation settings
        void set_bandwidth_limit (bandwidth_type_t type, size_t limit)
        {
            switch (type)
            {
            case BW_UPLOAD:
                _bridge.node_upcap[_id] = limit;
                break;
            case BW_DOWNLOAD:
                _bridge.node_downcap[_id] = limit;
                break;
            default:
                ;
            }
        }
        
        //
        // emulator-specific methods
        //
        int  storemsg (VAST::id_t from, msgtype_t msgtype, char const *msg, size_t len, timestamp_t time);
        bool remote_connect (VAST::id_t remote_id, Addr const &addr);
        void remote_disconnect (VAST::id_t remote_id);

        // get active connections
        //const std::vector<id_t> & get_active_connections ();

        // query state of message ID is msgid
        int query_msg (VAST::id_t msgid)
        {
            if (_msg2state.find (msgid) != _msg2state.end ())
                return _msg2state[msgid];

            return NET_MSGSTATE_UNKNOWN;
        }

        // get if is connected with the node
        inline bool is_connected (VAST::id_t id)
        {
            if (_id2addr.find (id) == _id2addr.end ())
                return false;

            return true;
        }

    private:

        // unique id for the vast class that uses this network interface
        VAST::id_t                           _id;

        // mapping from node ids to addresses
        std::map<VAST::id_t, Addr>           _id2addr;        

        // a shared object among net_emu class 
        // to discover class pointer via unique id
        net_emubridge_bl &                   _bridge;

        // the size can be transmitted for the specified destination
        std::map<VAST::id_t, size_t>         _sendspace;

        // total packet size of a specified queue in sendqueue
        std::map<VAST::id_t, size_t>         _sendqueue_size;

        // map from message ID to its state for messages in send queue
        std::map<VAST::id_t, int>            _msg2state;

        // id for last active sendqueue
        VAST::id_t                           _last_actqueue_id;

        // queue for outgoing messages (a mapping from destination to message queue)
        std::map<VAST::id_t, netmsg *>       _sendqueue;

        // queue for incoming messages
        std::multimap<timestamp_t, netmsg *> _msgqueue;

        // last timestamp
        //timestamp_t                 _curr_time;

        // network characteristics
        bool                        _active;

        // buffer for sending/receiving messages
        vastbuf                     _recv_buf;
    };

} // end namespace VAST

#endif // VAST_NET_EMU_BL_H
