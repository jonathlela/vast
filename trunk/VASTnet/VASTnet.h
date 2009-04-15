/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2009 Shun-Yun Hu (syhu@yahoo.com)
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
 * VASTnet.h -- generic network layer (virtual class)
 *   
 *  
 */

#ifndef _VAST_Network_H
#define _VAST_Network_H

// whether to build with the ACE library 
// (which supports real network transmission)
#define ACE_DISABLED                0

#include "VASTTypes.h"
#include "VASTBuffer.h"
#include "net_msg.h"
#include <map>
#include <vector>

#define NET_ID_UNASSIGNED   (0)     // default ID for yet assigned ID
#define NET_ID_GATEWAY      (1)     // default ID for gateway node
#define NET_ID_RESERVED    (10)     // reserved ID range for private IDs

// 
// NOTE: private ID begins with the largest allowable unsigned integer, then each subseqent private ID is one less the previous
//       also, the number of assignable private ID is the same as NET_ID_RESERVED
//       private IDs are usd when an incoming connection does not present a valid self ID,
//       then the receiver can use this private ID to talk with the connection initiator
#define NET_ID_PRIVATEBEGIN   ((unsigned)((unsigned)0-(unsigned)1))
#define NET_ID_PRIVATENEXT(c) ((unsigned)((unsigned)(c)-(unsigned)1))
#define NET_ID_ISPRIVATE(c)   ((c)<=NET_ID_PRIVATEBEGIN && (c)>=(NET_ID_PRIVATEBEGIN-(unsigned)(NET_ID_RESERVED)))

#define NET_MSGSTATE_UNIMPLEMENTED  (0)
#define NET_MSGSTATE_UNKNOWN        (1)
#define NET_MSGSTATE_PENDING        (2)
#define NET_MSGSTATE_SENDING        (3)

// Return values for errors
#define NET_ERR_ERROR              (-1)
#define NET_ERR_BUFFERFULL         (-2)

// TODO: should consider if physical timestamp is used, or force using time-step as units?
// time-steps to remove 
#define TIMEOUT_REMOVE_CONNECTION   (50)


namespace Vast {

    // common message types
    typedef enum VASTnetMessage
    {
        DISCONNECT = 0,         // disconnection without action: leaving overlay or no longer overlapped        
    };

    class EXPORT VASTnet
    {
    public:
        // IP & port of gateway required to initialize Network layer
        VASTnet ()
            : _id (NET_ID_UNASSIGNED), _active (false), _sendsize (0), _recvsize (0), _recvmsg (0,0)
        {
        }

        virtual ~VASTnet ();

        // starting and stopping the network service
        virtual void start ();
        virtual void stop ();

        // replace unique ID for current VASTnet instance
        virtual void registerID (id_t my_id) = 0;

        // get current physical timestamp
        virtual timestamp_t getTimestamp () = 0;

        // send messages to some nodes
        // returns number of bytes sent
        size_t sendMessage (id_t target, Message &msg, bool reliable = true);

        // obtain next message in queue
        // return pointer to Message, or NULL for no more message        
        Message* receiveMessage (id_t &from, timestamp_t &senttime);

        // send out all pending messages to each host
        // return number of bytes sent
        size_t flush (bool compress = false);

        // notify a target node's network layer of nodeID -> Address mapping
        // target can be self
        bool notifyMapping (id_t target, std::map<id_t, Addr> &mapping);

        // get if is connected with the node
        bool isConnected (id_t id);  

        // get a list of currently active connections' remote id and IP addresses
        std::map<id_t, Addr> &getConnections ();

        // get a specific address by id
        Addr &getAddress (id_t id);

        // obtain the tranmission size by message type, default is to return all types
        size_t getSendSize (const msgtype_t msgtype = 0);
        size_t getReceiveSize (const msgtype_t msgtype = 0);
     
    protected:

        // connect or disconnect a remote node (should check for redundency)
        // returns (-1) for error, (0) for success        
        virtual int     connect (id_t target) = 0;
        virtual int     disconnect (id_t target) = 0;

        // send an outgoing message to a remote host
        // return the number of bytes sent
        virtual size_t send (id_t target, char const *msg, size_t size, bool reliable = true) = 0;

        // receive an incoming message
        // return success or not
        virtual bool receive (netmsg **msg) = 0;

        // store an incoming message to net_msg
        virtual int storeMessage (id_t from, char const *msg, size_t len, timestamp_t time) = 0;

        // periodic cleanup of inactive connections
        void cleanConnections ();

        // update send/recv size statistics
        // type: 1 = send, type: 2 = receive
        void updateTransmissionStat (msgtype_t msgtype, size_t total_size, int type);

        // unique id for the VAST node using this network interface
        id_t                            _id;

        // whether the current interface is working
        bool                            _active;

        // map of active connections        
        std::map<id_t, void *>          _id2conn;

        // map from nodeIDs to IP addresses
        std::map<id_t, Addr>            _id2addr;

        // map from nodeIDs to last time the connection was accessed
        // used to determine connection cleanup / removals
        // TODO: combine the connection/address/time mapping?
        std::map<id_t, timestamp_t>     _id2time;
        
        // buffer for incoming/outgoing messages    
        // TODO: combine the TCP & UDP buffers?
        std::map<id_t, VASTBuffer *>    _sendbuf_TCP;        
        std::map<id_t, VASTBuffer *>    _sendbuf_UDP;
        Message                         _recvmsg;
        //VASTBuffer                      _recvbuf;
        
        // accumulated send/receive size
        size_t _sendsize, _recvsize;

        // accumulated send/receive size stored by type
        std::map<msgtype_t, size_t>     _type2sendsize, 
                                        _type2recvsize;        
    };

} // end namespace Vast

#endif // VAST_NETWORK_H
