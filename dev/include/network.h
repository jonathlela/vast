/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2007 Shun-Yun Hu (syhu@yahoo.com)
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
 * network.h -- network layer header (virtual class)
 *   
 *  
 */

#ifndef VAST_NETWORK_H
#define VAST_NETWORK_H

#include "typedef.h"
#include <map>
#include <vector>

#define NET_ID_PRESERVED    (10)

#define NET_ID_UNASSIGNED   (0)
#define NET_ID_GATEWAY      (1)

#define NET_ID_PRIVATEBEGIN   ((unsigned)((unsigned)0-(unsigned)1))
#define NET_ID_PRIVATENEXT(c) ((unsigned)((unsigned)(c)-(unsigned)1))
#define NET_ID_ISPRIVATE(c)   ((c)<=NET_ID_PRIVATEBEGIN&&(c)>=(NET_ID_PRIVATEBEGIN-(unsigned)(NET_ID_PRESERVED)))

#define NET_MSGSTATE_UNIMPLEMENT  (0)
#define NET_MSGSTATE_UNKNOWN      (1)
#define NET_MSGSTATE_PENDING      (2)
#define NET_MSGSTATE_SENDING      (3)

// Return value for 
#define VAST_ERR_ERROR          (-1)
#define VAST_ERR_BUFFERISFULL   (-2)

namespace VAST {

    class network
    {
    public:
        // IP & port of gateway required to initialize network layer
        network ()
            : _sendsize (0), _recvsize (0), _sendsize_comprd (0), _recvsize_comprd (0), _last_msgid (0)
        {
        }

        virtual ~network ()
        {
        }

        virtual void register_id (id_t my_id) = 0;

        virtual void start () = 0;
        virtual void stop ()  = 0;

        // connect or disconnect a remote node (should check for redundency)
        // returns (-1) for error, (0) for success
        // Notice::
        //    while connecting to an address by connect(Addr& addr) calling, if addr.id is
        //        NET_ID_UNASSIGNED: means connects to a outside node, network layer should allocating a
        //                           temp&private id and return by addr structure
        //        any non-private id: connects to the node, and register addr.id to the address
        //virtual int connect (id_t target, Addr addr) { puts ("network: connect (): Not supported.\n"); return 0; }
        virtual int connect (id_t target) = 0;
        virtual int connect (Addr & addr) = 0;
        virtual int disconnect (id_t target) = 0;

        // get a specific address by id
        virtual Addr &getaddr (id_t id) = 0;

        // get a list of currently active connections' remote id and IP addresses
        virtual std::map<id_t, Addr> &getconn () = 0;

        // send message to a node
        // returns the size of message transmitted, -1 for error
        virtual int sendmsg (id_t target, msgtype_t msgtype, char const *msg, size_t len, bool reliable = true, bool buffered = false) = 0;

        // obtain next message in queue
        // returns size of message, or -1 for no more message
        virtual int recvmsg (id_t &from, msgtype_t &msgtype, timestamp_t &recvtime, char **msg) = 0;

        // send out all pending reliable messages in a single packet to each target
        // returns number of bytes sent
        virtual int flush (bool compress = false) = 0;

        // notify id mapper to create mappings from src_id to each one in the list map_to
        // to indicate src_id knows IP address(es) of nodes in the list
        virtual int notify_id_mapper (id_t src_id, const std::vector<id_t> & map_to) = 0;

        // get current physical timestamp
        virtual timestamp_t get_curr_timestamp () = 0;

        // bandwidth limitation settings
        virtual void set_bandwidth_limit (bandwidth_type_t type, size_t limit) {}

        // statistics related methods
        size_t sendsize ()
        {
            return _sendsize;
        }

        size_t recvsize ()
        {
            return _recvsize;
        }

        size_t sendsize_bytype (const msgtype_t msgtype)
        {
            if (_type2sendsize.find (msgtype) != _type2sendsize.end ())
                return _type2sendsize [msgtype];

            return 0;
        }

        size_t recvsize_bytype (const msgtype_t msgtype)
        {
            if (_type2recvsize.find (msgtype) != _type2recvsize.end ())
                return _type2recvsize [msgtype];

            return 0;
        }

        size_t sendsize_compressed ()
        {
            return _sendsize_comprd;
        }

        size_t recvsize_compressed ()
        {
            return _recvsize_comprd;
        }

        // get active connections
        /*
        virtual const std::vector<id_t> & get_active_connections ()
        {
            // return a empty vector by default
            static std::vector<id_t> conns;

            return conns;
        }
        */

        // get ID of last sent message
        virtual id_t get_last_msgid ()
        {
            return _last_msgid;
        }

        // query state of message ID is msgid
        virtual int query_msg (id_t msgid)
        {
            return 0;
        }

        // get if is connected with the node
        virtual bool is_connected (id_t id) = 0;

        // calculate total packet size (add header size)
        inline static size_t sizeof_packetize (size_t payload_size)
        {
            return ((payload_size) + sizeof (msgtype_t) + sizeof (timestamp_t));
        }

    protected:
        // update send/recv size statistics
        void count_message (network *receiver, msgtype_t msgtype, size_t total_size)
        {
            // fail to count with NULL receiver pointer
            if (receiver == NULL)
            {
                printf ("[][network] count_message: get NULL receiver pointer.\n");
                return;
            }
            
            // avoid count lookback messages
            if (receiver == this)
                return;

            _sendsize += total_size;
            if (_type2sendsize.find (msgtype) == _type2sendsize.end ())
                _type2sendsize [msgtype] = 0;
            _type2sendsize[msgtype] += total_size;

            receiver->_recvsize += total_size;
            if (receiver->_type2recvsize.find (msgtype) == receiver->_type2recvsize.end ())
                receiver->_type2recvsize [msgtype] = 0;
            receiver->_type2sendsize[msgtype] += total_size;
        }

        void count_compressed_message (network *receiver, size_t length)
        {
            // fail to count with NULL receiver pointer
            if (receiver == NULL)
            {
                printf ("[][network] count_compressed_message: get NULL receiver pointer.\n");
                return;
            }
            
            // avoid count lookback messages
            if (receiver == this)
                return;

    		_sendsize_comprd += length;
			receiver->_recvsize_comprd += length;
        }

        // accumulated send/receive size
        size_t _sendsize, _recvsize;
        // accumulated send/receive size stored by type
        std::map<msgtype_t, size_t> _type2sendsize, _type2recvsize;
        // compressed accmulated send/receive size
		size_t _sendsize_comprd, _recvsize_comprd;
        
        // last message ID used, used for allocating new message IDs
        id_t _last_msgid;
    };

} // end namespace VAST

#endif // VAST_NETWORK_H
