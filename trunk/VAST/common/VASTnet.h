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
//#define ACE_DISABLED    

#include "VASTTypes.h"
#include "VASTBuffer.h"
#include <map>
#include <vector>

#define GATEWAY_DEFAULT_PORT    (1037)    // default port for gateway

#define NET_ID_UNASSIGNED       (0)     // default ID for yet assigned ID
#define NET_ID_GATEWAY          (1)     // default ID for gateway node
#define NET_ID_RESERVED         (10)    // reserved ID range for private IDs

// TODO: force using time-step as units?

#ifdef SIMULATION_ONLY
// # of elapsed timestamps before a connection is removed
#define TIMEOUT_REMOVE_CONNECTION   (50)
#else
// # of milliseconds
#define TIMEOUT_REMOVE_CONNECTION   (5000)
#endif

// # of flush called before a connection cleanup is called (don't want to do it too often)
#define COUNTER_CONNECTION_CLEANUP  (10)

namespace Vast {

    // common message types
    typedef enum
    {
        DISCONNECT = 0,         // disconnection without action: leaving overlay or no longer overlapped
        IPMAPPING,              // mapping between msggroup and IP addresses (stored at MessageQueue layer)
    } VASTnetMessage;

    // simple structure for a queued message
    typedef struct 
    {
        id_t        fromhost;
        timestamp_t senttime;
        Message    *msg;
    } QMSG;

    class EXPORT VASTnet
    {
    public:

        VASTnet ()
            : _id (NET_ID_UNASSIGNED), _active (false), _is_public (true), _cleanup_counter (0),
              _recvmsg (NULL), _tick_persec (0)
        {
            resetTransmissionSize ();
        }        

        virtual ~VASTnet ();

        // starting and stopping the network service
        virtual void start ();
        virtual void stop ();

        // get current physical timestamp
        virtual timestamp_t getTimestamp () = 0;

        // replace unique ID for current VASTnet instance
        virtual void registerID (id_t id);

        // get IP address from host name
        virtual const char *getIPFromHost (const char *hostname) = 0;

        // set bandwidth limitation to this network interface (limit is in Kilo bytes / second)
        virtual void setBandwidthLimit (bandwidth_t type, size_t limit) {}

        // get the IP address of current host machine
        Addr &getHostAddress ();

        // send messages to some nodes, note that everything in msg will be preserved (from, data, targets)
        // returns number of bytes sent
        size_t sendMessage (id_t target, Message &msg, bool reliable = true);

        // obtain next message in queue
        // return pointer to Message, or NULL for no more message
        Message* receiveMessage (id_t &fromhost, timestamp_t &senttime);

        // store an incoming message for processing by message handlers
        // return the number of bytes stored
        size_t storeRawMessage (id_t fromhost, char const *msg, size_t len, timestamp_t senttime, timestamp_t recvtime);

        // send out all pending messages to each host
        // return number of bytes sent
        virtual size_t flush (bool compress = false);

        // store hostID -> Address mapping        
        void storeMapping (Addr &address);

        // check if connected with the node
        bool isConnected (id_t id);

        // return whether this host has public IP or not
        bool isPublic ();

        // set whether this host has public IP or not
        void setPublic (bool is_public);

        // get how many ticks exist in a second (for stat reporting)
        int getTickPerSecond ();

        // set how many ticks exist in a second (for stat reporting)
        void setTickPerSecond (int ticks);

        // check if a target is connected, and attempt to connect if not
        bool validateConnection (id_t id);

        // get a list of currently active connections' remote id and IP addresses
        std::map<id_t, Addr> &getConnections ();

        // get a specific address by id
        Addr &getAddress (id_t id);
               
        //
        // stat collection functions
        //

        // obtain the tranmission size by message type, default is to return all types
        size_t getSendSize (msgtype_t msgtype = 0);
        size_t getReceiveSize (msgtype_t msgtype = 0);
        
        // zero out send / recv size records
        void   resetTransmissionSize ();

        // record which other IDs belong to the same host
        void recordLocalTarget (id_t target);

    protected:

        // connect or disconnect a remote node (should check for redundency)
        // returns (-1) for error, (0) for success
        virtual int     connect (id_t target) = 0;
        virtual int     disconnect (id_t target) = 0;

        // send an outgoing message to a remote host
        // return the number of bytes sent
        virtual size_t send (id_t target, char const *msg, size_t size, bool reliable = true) = 0;

        // receive an incoming message
        // return pointer to next QMSG structure or NULL for no more message
        virtual QMSG *receive () = 0;

        // store a message into priority queue
        // returns success or not
        virtual bool store (QMSG *qmsg) = 0;

        // clear up send queue's content
        virtual size_t clearQueue () {return 0;}

        // periodic cleanup of inactive connections
        void cleanConnections ();

        // update send/recv size statistics
        // type: 1 = send, type: 2 = receive
        void updateTransmissionStat (id_t target, msgtype_t msgtype, size_t total_size, int type);

        // unique id for the VAST node using this network interface
        id_t                            _id;
        Addr                            _addr;

        // whether the current interface is working
        bool                            _active;

        // whether this host has public IP or not
        bool                            _is_public;             

        // map of active connections
        std::map<id_t, void *>          _id2conn;

        // map from nodeIDs to IP addresses
        std::map<id_t, Addr>            _id2addr;

        // map from nodeIDs to last time the connection was accessed
        // used to determine connection cleanup / removals
        // TODO: combine the connection/address/time mapping?
        std::map<id_t, timestamp_t>     _id2time;
        unsigned int                    _cleanup_counter;   // # of ticks before cleanupConnections is called

        // buffer for incoming/outgoing messages
        // TODO: combine the TCP & UDP buffers?
        std::map<id_t, VASTBuffer *>    _sendbuf_TCP;
        std::map<id_t, VASTBuffer *>    _sendbuf_UDP;        
        std::multimap<byte_t, QMSG *>   _msgqueue;          // queue for incoming messages
        Message                        *_recvmsg;           // next available message for processing

        //VASTBuffer                      _sendbuf_local;     // sendbuffer for local messages
        //Message                         _recvmsg;         // next available message for processing
        //netmsg *                        _curr_msg;        // the current message received
        
        // 
        // stat collection
        //
        int                             _tick_persec;       // how many ticks per second

        // accumulated send/receive size
        size_t _sendsize, _recvsize;

        // accumulated send/receive size stored by type
        std::map<msgtype_t, size_t>     _type2sendsize,
                                        _type2recvsize;

        std::map<id_t, bool>            _local_targets;     // send/receive targets on the same host
    };

} // end namespace Vast

#endif // VAST_NETWORK_H
