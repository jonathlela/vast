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
#include "net_manager.h"     // for keeping sockets
#include <map>
#include <vector>

#define GATEWAY_DEFAULT_PORT    (1037)          // default port for gateway

// grouping for locally-generated ID
#define ID_GROUP_VON_VAST       1
//#define ID_GROUP_VON_VASTATE    2

// # of elapsed seconds before removing a connection
#define TIMEOUT_REMOVE_CONNECTION   (15)

// # of seconds before a connection cleanup is called (don't want to do it too often)
#define TIMEOUT_CONNECTION_CLEANUP  (1)

// # of seconds before an ID request is sent
#define TIMEOUT_ID_REQUEST          (5)

namespace Vast {

    // currently supported VASTnet implementions 
    typedef enum 
    {
        VAST_NET_EMULATED = 1,      // simulaion layer
        //VAST_NET_EMULATED_BL,     // simulaton layer with bandwidth limitation
        VAST_NET_ACE                // real network layer using ACE
    } VAST_NetModel;

    // common message types
    typedef enum
    {
        DISCONNECT = 0,         // disconnection without action: leaving overlay or no longer overlapped
    } VASTnetMessage;
         
    /*
    // header used by all VAST messages
    typedef struct 
    {
        uint32_t start    : 6;     // start marker (should be number 42: indicates 101010)
        uint32_t type     : 2;     // type of message (0: ID request; 1: ID assignment; 3: handshake; 4: regular) 
        uint32_t msg_size : 24;    // size of message (max: 16,777,216 bytes)
        uint32_t end      : 8;     // end marker, linefeed (LF) '\n'
    } VASTHeader;
    */

    // header used by all VAST messages
    typedef struct 
    {
        uint32_t start    : 4;     // start marker (number 10: indicates 1010)
        uint32_t type     : 2;     // type of message (0: ID request; 1: ID assignment; 3: handshake; 4: regular) 
        uint32_t msg_size : 22;    // size of message (max: 4,194,304 bytes)
        uint32_t end      : 4;     // end marker (number 5: 0101)
    } VASTHeader;

    // simple structure for a partially received VAST message
    class HALF_VMSG
    {
    public:

        // we store both header & message in the buffer
        HALF_VMSG (VASTHeader &h) 
        {
            header = h;
            this->buf = new VASTBuffer (sizeof (VASTHeader) + header.msg_size);
            received = 0;
        }

        ~HALF_VMSG ()
        {
            if (buf)
                delete buf;
        }

        VASTHeader  header;         // VAST header
        size_t      received;       // bytes already received
        VASTBuffer  *buf;           // content of the message (including VASTHeader)
    };

    // simple structure for a queued message
    class FULL_VMSG
    {
    public:

        FULL_VMSG (id_t from, Message *msg, timestamp_t time) 
        {
            fromhost = from;
            recvtime = time;
            this->_msg = msg;
        }

        ~FULL_VMSG ()
        {
            if (_msg)
                delete _msg;
        }

        // return what's pointed by msg
        Message *getMessage ()
        {
            Message *temp = this->_msg;
            this->_msg = NULL;
            return temp;
        }

        id_t        fromhost;
        timestamp_t recvtime;

    private:
        Message     *_msg;
    };

    // common message types
    typedef enum
    {
        ID_REQUEST = 0,     // requesting a new ID & public IP detection
        ID_ASSIGN,          // assigning a new ID
        HANDSHAKE,          // handshake message (notify my hostID)
        REGULAR             // regular message 

    } VASTHeaderType;

    // definition of main VAST network functions
    class EXPORT VASTnet
    {
    public:

        VASTnet (VAST_NetModel model, unsigned short port, int steps_persec);
        ~VASTnet ();

        // 
        // init & close functions
        //
        void start ();
        void stop ();

        //
        // message transmission methods
        //

        // store hostID -> Address mapping        
        void storeMapping (const Addr &address);

        // send messages to some nodes, note that everything in msg will be preserved (from, data, targets)
        // returns number of bytes sent
        size_t sendMessage (id_t target, Message &msg, bool reliable = true, VASTHeaderType type = REGULAR);

        // obtain next message in queue
        // return pointer to Message, or NULL for no more message
        Message* receiveMessage (id_t &fromhost);

        // send out all pending messages to each host
        // returns number of bytes sent
        size_t flush (bool compress = false);

        // process all incoming messages (convert to VASTMessage or raw format)
        // return the # of messages processed
        int process ();

        //
        //  socket communication methods
        //

        // open a new TCP socket
        id_t openSocket (IPaddr &ip_port, bool is_secure = false);

        // close a TCP socket
        bool closeSocket (id_t socket);

        // send a message to a socket
        bool sendSocket (id_t socket, const char *msg, size_t size);

        // receive a message from socket, if any
        // returns the message in byte array, and the socket_id, message size, NULL for no messages
        // NOTE: the returned data is valid until the next call to receiveSocket
        char *receiveSocket (id_t &socket, size_t &size);

        //
        // info query methods (may require platform-dependent calls)
        //

        // get current physical timestamp (in millisecond since program starts)
        // NOTE: using uint32_t as timestamp means valid timestamp last until 54 days
        //       it also means that timestamps on different hosts are not comparable (for ordering purpose)
        timestamp_t getTimestamp ();

        // get IP address from host name
        const char *getIPFromHost (const char *hostname);

        // 
        // state query methods
        //

        // whether we have joined the overlay successfully and obtained a hostID
        bool isJoined ();
       
        // return whether this host has public IP or not
        bool isPublic ();

        // if I'm an entry point on the overlay
        bool isEntryPoint (id_t id);

        // check if a certain node is connected
        bool isConnected (id_t id);

        //
        // tools 
        //

        // check the validity of an IP address, modify it if necessary
        // (for example, translate "127.0.0.1" to actual IP)
        bool validateIPAddress (IPaddr &addr);

        // check if a target is connected as a VAST message channel, 
        // and attempt to connect if not (send handshake afterwards)
        bool validateConnection (id_t id);

        // perform ticking at logical clock (only useful in simulated network)
        void tickLogicalClock ();

        //
        // getters & setters
        //

        // set how many timestamps should local value be adjusted to be consistent with a master clock
        void setTimestampAdjustment (int adjustment);

        // set bandwidth limitation to this network interface (limit is in Kilo bytes / second)
        void setBandwidthLimit (bandwidth_t type, size_t limit);

        // get how many timestamps (as returned by getTimestamp) is in a second 
        timestamp_t getTimestampPerSecond ();

        // get a list of currently active connections' remote id and IP addresses
        std::map<id_t, Addr> &getConnections ();

        // add entry points to respository
        void addEntries (std::vector<IPaddr> &entries);

        // get a list of entry points on the overlay network
        std::vector<IPaddr> &getEntries ();

        // get a specific address by id
        Addr &getAddress (id_t id);

        // get the IP address of current host machine
        // TODO: combine with with getAddress ()?
        Addr &getHostAddress ();

        // obtain a unique NodeID, given one of the ID groups
        id_t getUniqueID (int id_group = 0);

        // get hostID for myself
        id_t getHostID ();

        //
        // stat collection functions
        //

        // obtain the tranmission size by message type, default is to return all types
        size_t getSendSize (msgtype_t msgtype = 0);
        size_t getReceiveSize (msgtype_t msgtype = 0);
        
        // zero out send / recv size records
        void resetTransmissionSize ();

        // record which other IDs belong to the same host
        // TODO: added due to the two networks / host design in VASTATE
        //       a cleaner way?
        void recordLocalTarget (id_t target);

        //
        // Static Methods
        //

        // obtain the port portion of the ID
        // TODO: remove this so VASTnet's interface is cleaner?
        static id_t resolvePort (id_t host_id);

    protected:

        //
        //  message processing methods
        //

        // process an incoming raw message (decode its header)        
        bool processVASTMessage (VASTHeader &header, const char *msg, id_t remote_id);

        // accept or assign ID for newly joined remote host (depend on whether public IP exists or not)
        // also send back the remote host's ID if it's newly assigned
        id_t processIDRequest (Message &msg, IPaddr &actualIP);

        // store an incoming assignment of my ID
        // returns the newly assigned ID
        bool processIDAssignment (Message &msg);

        // send a handshake message to a newly established connection
        void sendHandshake (id_t target);

        // decode the remote host's ID or assign one if not available
        id_t processHandshake (Message &msg);

        // store an incoming VAST message, 'msg' is assumed will not be de-allocated after the call
        bool storeVASTMessage (id_t fromhost, Message *msg);

        // store unprocessed message to buffer for later processing
        bool storeSocketMessage (const NetSocketMsg *msg);

        // record my HostID
        void registerHostID (id_t my_id, bool is_public);


        // 
        // maintaineance functions
        //

        // remove a single connection cleanly
        bool removeConnection (id_t target);

        // periodic cleanup of inactive connections
        void cleanConnections ();

        // update send/recv size statistics
        // type: 1 = send, type: 2 = receive
        void updateTransmissionStat (id_t target, msgtype_t msgtype, size_t total_size, int type);

        // 
        // member variables
        //

        // network model we use
        VAST_NetModel                   _model;

        // the actual socket manager we use
        net_manager                    *_manager;

        // whether this host has public IP or not
        bool                            _is_public;             

        // entry points for joining the overlay network
        std::vector<IPaddr>             _entries;

        // map from hostIDs to IP addresses
        std::map<id_t, Addr>            _id2addr;

        // time adjustment
        int                             _time_adjust;
   
        // timeouts
        timestamp_t                     _timeout_IDrequest; // timeout for sending ID request
        timestamp_t                     _timeout_cleanup;   // # of ticks before cleanupConnections is called
        
        // counter for assigning unique ID
        std::map<int, id_t>             _IDcounter;         // counter for assigning IDs by this host   

        // buffer for incoming/outgoing messages
        // incoming queues
        VASTBuffer                          _buf;               // generic buffer for receiving messages
        std::map<id_t, HALF_VMSG *>         _half_queue;        // queue for storing partial incoming messages
        std::multimap<byte_t, FULL_VMSG *>  _full_queue;        // priority queue for (complete) incoming VAST messages
        std::vector<NetSocketMsg *>         _socket_queue;      // queue for incoming socket messages
        Message *                           _recvmsg;           // received VAST message
        NetSocketMsg *                      _recvmsg_socket;    // received socket message

        // TODO: combine the TCP & UDP buffers?
        // outgoing queues
        std::map<id_t, VASTBuffer *>        _sendbuf_TCP;
        std::map<id_t, VASTBuffer *>        _sendbuf_UDP;


        // 
        // stat collection
        //

        // accumulated send/receive size
        size_t _sendsize, _recvsize;

        // accumulated send/receive size stored by type
        std::map<msgtype_t, size_t>     _type2sendsize,
                                        _type2recvsize;

        std::map<id_t, bool>            _local_targets;     // send/receive targets on the same host
    };

} // end namespace Vast

#endif // VAST_NETWORK_H
