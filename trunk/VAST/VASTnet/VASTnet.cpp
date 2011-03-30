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

#include "VASTnet.h"
#include "net_ace.h"
#include "net_emu.h"

namespace Vast
{   

    extern char VON_MESSAGE[][20];
    extern char VAST_MESSAGE[][20];

    using namespace std;

    VASTnet::VASTnet (VAST_NetModel model, unsigned short port, int steps_persec)
        : _model (model),
          _is_public (true), 
          _timeout_IDrequest (0),
          _timeout_cleanup (0)
    {        

        // create network manager given the network model and start it
        if (_model == VAST_NET_EMULATED)
            _manager = new net_emu (steps_persec);

        else if (_model == VAST_NET_ACE)
            _manager = new net_ace (port);

        _recvmsg = NULL;
        _recvmsg_socket = NULL;

        _time_adjust = 0;

        resetTransmissionSize ();
    }

    VASTnet::~VASTnet ()
    {
        // make sure everything's stopped & released
        _manager->stop ();

        if (_model == VAST_NET_EMULATED)
            delete ((net_emu *)_manager);
#ifndef ACE_DISABLED
        else if (_model == VAST_NET_ACE)
            delete ((net_ace *)_manager);
#endif

        // de-allocate message buffers
        if (_recvmsg)
        {
            delete _recvmsg;
            _recvmsg = NULL;
        }

        // _half_queue
        std::map<id_t, HALF_VMSG *>::iterator it = _half_queue.begin ();
        for (; it != _half_queue.end (); it++)
            delete it->second;
        _half_queue.clear ();

        // _full_queue
        std::multimap<byte_t, FULL_VMSG *>::iterator it2 = _full_queue.begin ();
        for (; it2 != _full_queue.end (); it2++)
            delete it2->second;
        _full_queue.clear ();

        // _socket_queue
        for (size_t i=0; i < _socket_queue.size (); i++)
            delete _socket_queue[i];
        _socket_queue.clear ();

        // TCP & UDP send buffers
        std::map<id_t, VASTBuffer *>::iterator it3 = _sendbuf_TCP.begin ();
        for (; it3 != _sendbuf_TCP.end (); it3++)
            delete it3->second;
        _sendbuf_TCP.clear ();

        std::map<id_t, VASTBuffer *>::iterator it4 = _sendbuf_UDP.begin ();
        for (; it4 != _sendbuf_UDP.end (); it4++)
            delete it4->second;
        _sendbuf_UDP.clear ();

    }

    // 
    // init & close functions
    //
    void 
    VASTnet::start ()
    {
        _manager->start ();
    }

    void 
    VASTnet::stop ()
    {
        _manager->stop ();
    }


    //
    // message transmission methods
    //

    // store hostID -> Address mapping        
    void
    VASTnet::storeMapping (const Addr &addr)
    {
        if (addr.host_id == NET_ID_UNASSIGNED)
        {
            printf ("VASTnet::storeMapping (): address stored has no valid hostID\n");
            return;
        }

        // check for information consistency (for debug)
        map<id_t, Addr>::iterator it = _id2addr.find (addr.host_id);
        if (it != _id2addr.end () && it->second != addr)
        {            
            // NOTE: this can occur normally due to two cases:
            //       1) when an actual listen port replaces a detected port (will always be 0)
            //       2) when a connection is re-established and a previous mapping exists (the new connection will have a detected port of 0)
            // we should flag a warning for any other cases
            if (it->second.publicIP.port != 0 && addr.publicIP.port != 0)
                printf ("VASTnet::storeMapping (): existing address and new address mismatch.\n");
        }

        // store local copy of the mapping
        _id2addr[addr.host_id] = addr;
    }

    // send a message to some targets, 
    // will queue in _sendbuf_TCP or _sendbuf_UDP depending on reliability until flush () is called
    // returns number of bytes queued
    size_t
    VASTnet::sendMessage (id_t target, Message &msg, bool reliable, VASTHeaderType type)
    {
        if (_manager->isActive () == false)
            return 0;
        
#ifdef DEBUG_DETAIL
        printf ("[%d] VASTnet::sendMessage to: %d msgtype: %d (%s) size: %d\n", _id, target, msg.msgtype, (msg.msgtype < 30 ? (msg.msgtype >= 10 ? VAST_MESSAGE[msg.msgtype-10] : VON_MESSAGE[msg.msgtype]) : "MESSAGE"), msg.size);
#endif
        // put default from field
        if (msg.from == 0)
            msg.from = _manager->getID ();

        // if it's a local message, store to receive queue directly
        if (target == _manager->getID ())
        {
            Message *newmsg = new Message (msg);

            // reset the message so that it can be properly decoded
            newmsg->reset ();

            // NOTE: the message will be de-allocated by the message processor
            storeVASTMessage (_manager->getID (), newmsg);

            return msg.size;
        }

        // make sure we have a connection, or establish one if not
        if (validateConnection (target) == false)
            return 0;
        
        // get the TCP or UDP queue, create one if necessary
        std::map<id_t, VASTBuffer *> &send_buf = (reliable ? _sendbuf_TCP : _sendbuf_UDP);        

        if (send_buf.find (target) == send_buf.end ())
            send_buf[target] = new VASTBuffer ();
    
        VASTBuffer *buf = send_buf[target];

        // NOTE: 'header' is used by the receiving host to read message from network stream
        VASTHeader header;
        //header.start = 42;
        //header.end = '\n';
        header.start = 10;
        header.end = 5;
        header.type = type;
        header.msg_size = msg.serialize (NULL);
        
        // prepare bytestring with header & serialized message
        buf->add ((char *)&header, sizeof (VASTHeader));
        buf->add (&msg);

        // collect download transmission stat
        updateTransmissionStat (target, msg.msgtype, msg.size + sizeof (VASTHeader), 1);

        return header.msg_size;
    }

    // obtain next message in queue
    // return pointer to Message, or NULL for no more message        
    Message* 
    VASTnet::receiveMessage (id_t &fromhost)
    {                   
        if (_manager->isActive () == false)
            return NULL;

        // if no time is left in current timeslot, then return immediately
        // TODO: how will this work under simulation?
        if (TimeMonitor::instance ()->available () == 0)
        {
            printf ("VASTnet::receiveMessage (): no time available for processing\n");
            return NULL;
        }

        // de-allocate memory for previous mesage
        if (_recvmsg != NULL)
        {
            delete _recvmsg;
            _recvmsg = NULL;
        }                       

        if (_full_queue.size () == 0)
            return NULL;

        // obtain next available message from queue
        FULL_VMSG *nextmsg = _full_queue.begin ()->second;
        _full_queue.erase (_full_queue.begin ());
       
        // TODO: recvtime not used?
        _recvmsg = nextmsg->getMessage ();
        fromhost = nextmsg->fromhost;

        // update time for the connection
        //_id2time[fromhost] = _manager->getTimestamp ();

        delete nextmsg;

        updateTransmissionStat (fromhost, _recvmsg->msgtype, sizeof (VASTHeader) + _recvmsg->size, 2);
        
        return _recvmsg;
    }

    // send out all pending messages to each host
    // return number of bytes sent
    size_t
    VASTnet::flush (bool compress)
    {        
		if (compress == true)
		{
            // syhu NOTE: currently compression in simulation mode is disabled
            //            somehow it should be a generic feature at the Network.cpp level
            //            so both emulated & real network can benefit from compression
            
            /*
			for (std::map<Vast::id_t, VASTBuffer *>::iterator it = _all_msg_buf.begin (); it != _all_msg_buf.end (); it++)
			{
				size_t after_def_size = 0;
				size_t bufsize = it->second->size;

				unsigned char *source_buf = new unsigned char[bufsize];
				memcpy (source_buf, it->second->data, bufsize);

				// to deflate the buffered messages
                after_def_size = _compressor.compress (source_buf, (unsigned char *)&_def_buf, bufsize);
				delete[] source_buf;

				// passing the deflated length to emu bridge and record the length
				//_bridge.pass_def_data (_id, it->first, after_def_size);
                //count_compressed_message ((net_emu *) _bridge->getNetworkInterface (it->first), after_def_size);
			}
            */
        }        

        size_t flush_size = 0;     // number of total bytes sent this time
        
        // check if there are any pending TCP queues
        std::map<id_t, VASTBuffer *>::iterator it;
        for (it = _sendbuf_TCP.begin (); it != _sendbuf_TCP.end (); it++)
        {
            id_t target = it->first;        
            VASTBuffer *buf = it->second;

            // check if there's something to send
            // TODO: remove empty buffers after some time
            if (buf->size > 0)
                flush_size += _manager->send (target, buf->data, buf->size);            

            // clear the buffer whether the message is sent or not
            buf->clear ();
        }

        // check if there are any pending UDP queues
        for (it = _sendbuf_UDP.begin (); it != _sendbuf_UDP.end (); it++)
        {
            id_t target     = it->first;        
            VASTBuffer *buf = it->second;

            // check if there's something to send
            if (buf->size > 0 && _id2addr.find (target) != _id2addr.end ())
            {
                Addr &addr = _id2addr[target];
                flush_size += _manager->send (target, buf->data, buf->size, &addr);
            }
                
            // clear buffer whether we've sent sucessfully or not
            buf->clear ();
        }

        // call cleanup every once in a while
        timestamp_t now = _manager->getTimestamp (); 
        if (now > _timeout_cleanup)
        {
            _timeout_cleanup = now + (TIMEOUT_CONNECTION_CLEANUP * _manager->getTimestampPerSecond ());
            cleanConnections ();
        }

        // TODO: collect upload transmission stat for non-local messages here?

        return flush_size;
    }

    // the general rule is we'll read until linefeed (LF) '\n' is met
    // there are two cases: 
    //
    //      1) VAST messages
    //              there's a 4-byte VAST message header right before the LF
    //              in such case, the header is read, and "message size" extracted
    //              the handler will keep reading "message size". 
    //              The "VASTHeader + message content" is considered a single message & stored
    //
    //      2) Socket message
    //              if whatever before the LF cannot be identified as a VASTHeader
    //              then everything before the LF is considered a single message & stored
    //

    // process all incoming messages (convert to VASTMessage or socket format)
    // return the # of messages processed
    int 
    VASTnet::process ()
    {
        int msg_processed = 0;
        NetSocketMsg *curr_msg = NULL;
        VASTHeader header;

        std::map<id_t, HALF_VMSG *>::iterator it;
        HALF_VMSG *hq;

        // NOTE: content in each curr_msg will be de-allocated at end of loop
        // TODO: more efficient way to store socket message? (no need to de-allocate)
        while ((curr_msg = _manager->receive ()) != NULL)
        {
            // first check for disconnection message
            if (curr_msg->size == 0)
            {
                Message *msg = new Message (DISCONNECT);
                msg->from = curr_msg->fromhost;
                storeVASTMessage (curr_msg->fromhost, msg);
                continue;
            }

            msg_processed++;
            hq = NULL;

            // if this message comes from an existing VAST host, copy message directly
            // NOTE that header will be copied as well
            it = _half_queue.find (curr_msg->fromhost);
            if (it != _half_queue.end ())
                hq = it->second;

            // otherwise, determine if this is a VAST message            
            else if (curr_msg->size > sizeof (VASTHeader))
            {
                memcpy (&header, curr_msg->msg, sizeof (VASTHeader));

                // check for VAST Header, 010101 in front, and '\n' at end
                //if (header.start == 42 && header.end == '\n')

                // check for VAST Header, 1010 in front (10) 0101 in back (5)
                if (header.start == 10 && header.end == 5)
                {
                    hq = new HALF_VMSG (header);
                    _half_queue[curr_msg->fromhost] = hq;
                }
            }

            // if half queue pointer not found, it's a socket message
            if (hq == NULL)
            {
                storeSocketMessage (curr_msg);
                continue;
            }

            // determine number of bytes to append (including VASTHeader)
            size_t full_size = sizeof (VASTHeader) + hq->header.msg_size;
            size_t size_toadd = full_size - hq->received;

            // truncate size to add this time, if received message is not enough
            if (size_toadd > curr_msg->size)
                size_toadd = curr_msg->size;

            // append current message to half queue
            hq->buf->add (curr_msg->msg, size_toadd);
            hq->received += size_toadd;

            // if the VAST message is completed, store to VAST message queue
            if (hq->received == full_size)
            {
                // NOTE: processVASTMessage will make a copy of the message
                this->processVASTMessage (hq->header, hq->buf->data + sizeof (VASTHeader), curr_msg->fromhost);

                // remove content from half queue (buffer will deleted as well)
                // TODO: try not to allocate/de-alloate too often
                delete hq;
                _half_queue.erase (curr_msg->fromhost);
            }

            // store unused message back to incoming message queue
            // NOTE: the message is stored at the beginning of queue
            if (curr_msg->size > size_toadd)
                _manager->msg_received (curr_msg->fromhost, (curr_msg->msg + size_toadd), curr_msg->size - size_toadd, curr_msg->recvtime, true);
        }

        return msg_processed;
    }

    // open a new TCP socket, in string format "IP:port"
    id_t 
    VASTnet::openSocket (IPaddr &ip_port, bool is_secure)
    {
        // if we're not yet initialized, deny internally
        if (isJoined () == false)
            return NET_ID_UNASSIGNED;

        // get unique ID from network layer, to represent this socket
        id_t socket_id = getUniqueID ();
        if (socket_id == NET_ID_UNASSIGNED)
            return NET_ID_UNASSIGNED;

        // make connection (and wait for response)
        Addr addr;        
        addr.publicIP = ip_port;
        addr.host_id  = socket_id;

        this->storeMapping (addr);

        // NOTE: we don't need to worry about cleanConnections () disconnect this connection, 
        //       as this socket will not have any time record
        // TODO: perhaps release socket_id if connection fails?
        if (_manager->connect (socket_id, addr.publicIP.host, addr.publicIP.port, is_secure) == false)
            return NET_ID_UNASSIGNED;
               
        return socket_id;
    }

    // close a TCP socket
    bool 
    VASTnet::closeSocket (id_t socket)
    {        
        return _manager->disconnect (socket);
    }

    // send a message to a socket
    // NOTE: we send directly without queueing
    bool 
    VASTnet::sendSocket (id_t socket, const char *msg, size_t size)
    {        
        return (_manager->send (socket, msg, size) > 0);
    }

    // receive a message from socket, if any
    char *
    VASTnet::receiveSocket (id_t &socket, size_t &size)
    {
        // remove previous message, if any
        if (_recvmsg_socket)
        {
            delete _recvmsg_socket;
            _recvmsg_socket = NULL;
        }

        // no more messages
        if (_socket_queue.size () == 0)
            return NULL;

        // get first available message
        _recvmsg_socket = _socket_queue[0];
        _socket_queue.erase (_socket_queue.begin ());

        socket = _recvmsg_socket->fromhost;
        size = _recvmsg_socket->size;

        // should go through each socket, or make this into a complete callback
        return _recvmsg_socket->msg;
    }

    //
    // info query methods (may require platform-dependent calls)
    //

    timestamp_t 
    VASTnet::getTimestamp ()
    {
        return (_manager->getTimestamp () + _time_adjust);
    }

    // get IP address from host name
    const char *
    VASTnet::getIPFromHost (const char *hostname)
    {
        return _manager->getIPFromHost (hostname);
    }


    // 
    // state query methods
    //

    // whether we have joined the overlay successfully and obtained a HostID
    bool
    VASTnet::isJoined ()
    {
        if (_manager->isActive () == false)
            return false;

        timestamp_t now = this->getTimestamp ();

        // if HostID is not yet obtained, and our network is up
        if (_manager->getID () == NET_ID_UNASSIGNED)
        {
            // check if we're considered as an entry point, if so we can determine HostID by self
            if (_entries.size () == 0)
            {
                Addr addr = _manager->getAddress ();
                id_t id = _manager->resolveHostID (&addr.publicIP);

                // note we need to use registerHostID to modify id instead of directly
                registerHostID (id, true);
            }

            // otherwise, start to contact entry points to obtain ID
            else if (now >= _timeout_IDrequest)
            {
                _timeout_IDrequest = now + (TIMEOUT_ID_REQUEST * this->getTimestampPerSecond ());

                // randomly pick an entry point
                int i = (rand () % _entries.size ());

                id_t target = _manager->resolveHostID (&_entries[i]);
                Addr addr (target, &_entries[i]);

                storeMapping (addr);

                //
                // send out ID request message & also detect whether we've public IP
                //

                // determine our self ID / store to 'addr' temporariliy (for net_emu)
                Addr self_addr = _manager->getAddress ();
                id_t id = self_addr.host_id;

                // create & send ID request message, consists of
                //   1) determined hostID and 2) detected IP of the host
                Message msg (0);
                msg.priority = 0;
                msg.store (id);
                msg.store (self_addr.publicIP);
          
                sendMessage (target, msg, true, ID_REQUEST);
                printf ("VASTnet::isJoined () sending ID_REQUEST to gateway [%llu]\n", target);
            }
        }

        // TODO: also should check if network is still connected (plugged)
        return (_manager->getID () != NET_ID_UNASSIGNED);
    }

    // return whether this host has public IP or not
    bool 
    VASTnet::isPublic ()
    {
        return _is_public;
    }

    // if an id is an entry point on the overlay
    bool 
    VASTnet::isEntryPoint (id_t id)
    {
        return ((0x0FFFF & id) == NET_ID_RELAY);
    }

    // if is connected with the node
    bool 
    VASTnet::isConnected (id_t id)
    { 
        return _manager->isConnected (id);
    }

    //
    // tools 
    //

    // check the validity of an IP address, modify it if necessary
    // (for example, translate "127.0.0.1" to actual IP)
    bool 
    VASTnet::
    validateIPAddress (IPaddr &addr)
    {
        // if address is localhost (127.0.0.1), replace with my detected IP 
        if (addr.host == 0 || addr.host == 2130706433)
            addr.host = getHostAddress ().publicIP.host;

        // TODO: perform other actual checks

        return true;
    }

    // check if a target is connected, and attempt to connect if not
    bool 
    VASTnet::validateConnection (id_t host_id)
    {
        // if it's message to self or already connected
        if (_manager->getID () == host_id || _manager->isConnected (host_id) == true)
            return true;

        // resolve address of host
        std::map<id_t, Addr>::iterator it = _id2addr.find (host_id);
        if (it == _id2addr.end ())
            return false;

        IPaddr &remote_addr = it->second.publicIP;

        // otherwise try to initiate connection & send handshake message
        if (_manager->connect (host_id, remote_addr.host, remote_addr.port) == false)
            return false;
        
        sendHandshake (host_id);

        return true;
    }

    // perform ticking at logical clock (only useful in simulated network)
    void
    VASTnet::tickLogicalClock ()
    {
        _manager->tickLogicalClock ();        
    }

    //
    // getters & setters
    //

    // set how many timestamps should local value be adjusted to be consistent with a master clock
    void 
    VASTnet::setTimestampAdjustment (int adjustment)
    {
        // NOTE: the adjustment is always incrementally applied, as new adjustment
        //       is applied 
        _time_adjust += adjustment;

        printf ("\nVASTnet::setTimestampAdjustment () adjusts %d milliseconds (after adjusting %d)\n\n", _time_adjust, adjustment);
    }

    // set bandwidth limitation to this network interface (limit is in Kilo bytes / second)
    void 
    VASTnet::setBandwidthLimit (bandwidth_t type, size_t limit)
    {
        // TODO: currently empty
    }

    // get how many timestamps (as returned by getTimestamp) is in a second 
    timestamp_t
    VASTnet::getTimestampPerSecond ()
    {
        return _manager->getTimestampPerSecond ();
    }
       
    // get a list of currently active connections' remote id and IP addresses
    // NOTE: only those with stored addresses will return
    std::map<id_t, Addr> &
    VASTnet::getConnections ()
    {
        static std::map<id_t, Addr> conn_list;
        conn_list.clear ();

        std::map<id_t, Addr>::iterator it = _id2addr.begin ();

        for (; it != _id2addr.end (); it ++)
        {
            if (_manager->isConnected (it->first))
                conn_list[it->first] = it->second;
        }

        return conn_list;
    }

    // add entry points to respository
    void 
    VASTnet::addEntries (std::vector<IPaddr> &entries)
    {
        // process and check the validity of each entry point
        // NOTE: the proper entry format is "hostname:port" or "IP:port" 
        for (size_t i=0; i < entries.size (); i++)
        {
            if (validateIPAddress (entries[i]) == true)            
                _entries.push_back (entries[i]);

            /*
            IPaddr addr;

            // TODO: convert any hostname to numeric IP

            // if the entry translates to a valid IP with port number
            if (IPaddr::parseIP (addr, entries[i]) == 0)            
                _entries.push_back (addr);      
            else
                printf ("VASTnet::VASTnet () invalid entry point: %s\n", entries[i].c_str ());
            */
        }
    }
        
    // get a list of entry points on the overlay network
    std::vector<IPaddr> &
    VASTnet::getEntries ()
    {
        return _entries;
    }

    // obtain the address for 'id', returns a empty (id = 0) address if not found
    Addr &
    VASTnet::getAddress (id_t id)
    {
        static Addr null_addr;

        if (_id2addr.find (id) != _id2addr.end ())        
            return _id2addr[id];
        else
        {
            printf ("VASTnet::getAddress (): address not found for [%llu]\n", id);
            return null_addr;        
        }
    }

    // get the IP address of current host machine
    Addr &
    VASTnet::getHostAddress ()
    {
        return _manager->getAddress ();
    }

    // obtain a NodeID
    id_t 
    VASTnet::getUniqueID (int id_group)
    {
        // check if we're relay and allow to give ID
        if (isEntryPoint (_manager->getID ()) == false)
            return NET_ID_UNASSIGNED;

        // id_group cannot be out of range (currently only 4)
        if (id_group < 0 || id_group >= 4)
            return NET_ID_UNASSIGNED;
        
        if (_IDcounter.find (id_group) == _IDcounter.end ())
            _IDcounter[id_group] = NET_ID_RELAY+1;

        // if all available numbers are assigned
        if (_IDcounter[id_group] <= 0 || _IDcounter[id_group] >= 16384)
            return NET_ID_UNASSIGNED;

        else
        {
            Addr &addr = _manager->getAddress ();

            // if we're a relay with public IP
            id_t id = ((id_t)addr.publicIP.host << 32) | ((id_t)addr.publicIP.port << 16) | ((id_t)id_group << 14) | _IDcounter[id_group]++;
            return id;
        }
    }

    // get hostID for myself
    id_t 
    VASTnet::getHostID ()
    {
        return _manager->getID ();
    }

    //
    // stat collection functions
    //

    // obtain the tranmission size by message type, default is to return all types
    size_t 
    VASTnet::getSendSize (msgtype_t msgtype)
    {
        if (msgtype == 0)
            return _sendsize;
        if (_type2sendsize.find (msgtype) != _type2sendsize.end ())
            return _type2sendsize [msgtype];
       
        return 0;
    }
    
    size_t 
    VASTnet::getReceiveSize (msgtype_t msgtype)
    {
        if (msgtype == 0)
            return _recvsize;
        if (_type2recvsize.find (msgtype) != _type2recvsize.end ())
            return _type2recvsize [msgtype];

        return 0;
    }

    // zero out send / recv size records
    void   
    VASTnet::resetTransmissionSize ()
    {
        _sendsize = _recvsize = 0;
        _type2sendsize.clear ();
        _type2recvsize.clear ();
    }

    // record which other IDs belong to the same host
    void 
    VASTnet::recordLocalTarget (id_t target)
    {
        _local_targets[target] = true;
    }

    //
    //  message processing methods
    //    

    // process an incoming raw VAST message (given its header)
    // whatever bytestring received is processed here in its entirety
    // returns NET_ID_UNASSIGNED for serious error
    bool
    VASTnet::processVASTMessage (VASTHeader &header, const char *p, id_t remote_id)
    {
        // convert byte string to Message object here
        Message *msg = new Message (0);
        
        if (msg->deserialize (p, header.msg_size) == 0)
        {
            printf ("VASTnet::processVASTMessage () deserialize message fail, from [%llu], size: %lu\n", remote_id, header.msg_size);
            delete msg;
            return false;
        }

        //printf ("header.type=%d\n", (int)header.type);

        switch (header.type)
        {
        // from a joining host to gateway
        case ID_REQUEST:
            {                    
                // get actual detected IP for remote host
                IPaddr actual_IP;
                _manager->getRemoteAddress (remote_id, actual_IP);

                // obtain or assign new ID
                id_t new_id = processIDRequest (*msg, actual_IP);
                
                // if new ID is accepted or assigned successfully
                if (new_id != NET_ID_UNASSIGNED)
                {
                    // switch ID mapping, because the remote host's actual ID could differ from what we locally determine
                    _manager->switchID (remote_id, new_id);

                    // send back reply to ID request
                    // NOTE: we need to use the new ID now, as the id to connection mapping has changed
                    printf ("ID_ASSIGN remote host [%llu]\n", new_id);
                    sendMessage (new_id, *msg, true, ID_ASSIGN);
                }
                else
                    printf ("processVASTMessage (): BUG new id [%llu] differs from detected id [%llu]\n", new_id, remote_id);
            }            
            break;

        // from gateway to joining host
        case ID_ASSIGN:
            {
                processIDAssignment (*msg);
            }
            break;

        // from initiating host to accepting host
        case HANDSHAKE:                        
            {        
                // process handshake message                
                id_t id = processHandshake (*msg);
                
                // register this connection with its ID if available, or terminate otherwise
                if (id == NET_ID_UNASSIGNED)
                    printf ("VASTnet::processVASTMessage () cannot determine remote host's ID from handshake\n");
                else
                    _manager->switchID (remote_id, id);
                    //register_conn (remote_id, handler);
            }            
            break;

        // regular messages after handshake is successful
        case REGULAR:
            // TODO: check with net_manager for successful handshake first
            {
                // return first so Message object would not be de-allocated, unless store was unsuccessful        
                storeVASTMessage (remote_id, msg);

                return true;
            }
            break;

        default:
            printf ("VASTnet::processVASTMessage () unreconginzed message\n");
            break;
        }

        // delete temporary message
        delete msg;

        return true;
    }

    // the basic rule is: if remote host has public IP, then use its decleared ID,
    // otherwise, assign one based on detected IP & port
    id_t
    VASTnet::processIDRequest (Message &msg, IPaddr &actualIP)
    {
        // extract the ID request message, consists of
        //   1) determined hostID and 2) detected IP
        id_t    id;
        IPaddr  detectedIP;
        
        // extract..
        msg.extract (id);
        msg.extract (detectedIP);

        // debug msg
        char ip[40], ip2[40];
        detectedIP.getString (ip);
        actualIP.getString (ip2);
        
        printf ("[%llu] ID request from: [%llu] (%s) actual address (%s)\n", _manager->getID (), id, ip, ip2); 
        
        // whether the the actual & detected IP match determines if remote host is public
        // NOTE: we do no compare port as the remote host's listen port can be different from 
        //       the port used in this communication
        bool is_public = (actualIP.host == detectedIP.host);

        // if the remote host does not have have public IP, assign one
        if (!is_public)      
        {
            // use the actual IP/port pair to define the unique ID
            // NOTE: that the remote port used may not be the default bind port, as it's rather arbitrary
            // BUG: potential bug, the same 'port' if used by different hosts behind a NAT, will generate the same ID
            id = _manager->resolveHostID (&actualIP);
        }

        if (id == NET_ID_UNASSIGNED)
            printf ("VASTnet::processIDRequest () we cannot assign new ID to remote host any more\n");
        else
        {
            // prepare reply message, store the new ID & is_public flag
            msg.clear (0);
            msg.priority = 0;
            msg.store (id);
            msg.store ((char *)&is_public, sizeof (bool));
        }

        // return the ID accepted or assigned
        return id;
    }

    // store an incoming assignment of my ID
    bool
    VASTnet::processIDAssignment (Message &msg)
    {
        if (msg.size != (sizeof (id_t) + sizeof (bool)))
            return false;

        id_t id;
        bool is_public;

        msg.extract (id);
        msg.extract ((char *)&is_public, sizeof (bool));

        // store my obtained ID
        registerHostID (id, is_public);

        return true;
    }

    // send a handshake message to a newly established connection
    void 
    VASTnet::sendHandshake (id_t target)
    {
        id_t my_id = _manager->getID ();

        // if we do not yet have our ID, should request ID first
        if (my_id == NET_ID_UNASSIGNED)
            return;

        // prepare & send the handshake message, currently just my ID
        Message msg (0);
        msg.priority = 0;
        msg.store (my_id);

        // TODO: add authentication message
        sendMessage (target, msg, true, HANDSHAKE);
    }

    // decode the remote host's ID or assign one if not available
    // TODO: perform authentication of remote host
    id_t
    VASTnet::processHandshake (Message &msg)
    {
        // TODO: perform authentication first?
        id_t remote_id;
        msg.extract (remote_id);
        return remote_id; 
    }

    // store an incoming VAST message 
    bool 
    VASTnet::storeVASTMessage (id_t fromhost, Message *msg)
    {
        if (_manager->isActive () == false)
        {
            delete msg;
            return false;
        }

        // check for UDP message, if so, replace 'fromhost'
        if (fromhost == NET_ID_UNASSIGNED)
            fromhost = msg->from;

        // store the message into priority queue in a FULL_VMSG structure
        FULL_VMSG *vastmsg = new FULL_VMSG (fromhost, msg, _manager->getTimestamp ());

        //store (storemsg);
        _full_queue.insert (std::multimap<byte_t, FULL_VMSG *>::value_type (msg->priority, vastmsg));
        
        return true;
    }

    // store unprocessed message to buffer for later processing
    bool 
    VASTnet::storeSocketMessage (const NetSocketMsg *msg)
    {
        if (_manager->isActive () == false)
            return false;
        
        _socket_queue.push_back (new NetSocketMsg (*msg));

        return true;
    }


    void 
    VASTnet::
    registerHostID (id_t my_id, bool is_public)    
    {
        // we avoid double assignment (for now)
        if (_manager->getID () != NET_ID_UNASSIGNED)
            return;

        // record my obtained ID
        _manager->setID (my_id);

        _id2addr[my_id] = _manager->getAddress ();
        _is_public      = is_public;       
    }

    // remove a single connection cleanly
    bool 
    VASTnet::removeConnection (id_t target)
    {
        bool removed = _manager->disconnect (target);

        // also remove the send buffer to this host
        // NOTE: connection record may not exist for buffer (TODO: shouldn't happen logically)
        if (_sendbuf_TCP.find (target) != _sendbuf_TCP.end ())
        {
            delete _sendbuf_TCP[target];
            _sendbuf_TCP.erase (target);
            removed = true;
        }

        if (_sendbuf_UDP.find (target) != _sendbuf_UDP.end ())
        {
            delete _sendbuf_UDP[target];
            _sendbuf_UDP.erase (target);
            removed = true;
        }
        
        // TODO: at some point should clean up id2host mappings

        return removed;
    }    

    // periodic cleanup of inactive connections
    void 
    VASTnet::cleanConnections ()
    {
        timestamp_t now = getTimestamp ();
        std::vector<Vast::id_t> remove_list;

        timestamp_t timeout = (TIMEOUT_REMOVE_CONNECTION * this->getTimestampPerSecond ());

        std::map<id_t, VASTBuffer *>::iterator it = _sendbuf_TCP.begin ();

        // go through existing TCP connections and remove inactive ones
        for (; it != _sendbuf_TCP.end (); it++)
        {
            timestamp_t lasttime = _manager->getLastTime (it->first);

            // NOTE: for connections that do not record time (such as socket-only), no timeout will occur
            if (lasttime == 0 || (now - lasttime) < timeout)
                continue;

            if (_manager->isConnected (it->first))
                continue;

            remove_list.push_back (it->first);                
        }

        if (remove_list.size () > 0)
        {
#ifdef DEBUG_DETAIL
            printf ("VASTnet::cleanConnections () removing timeout connections: ");
#endif
        
            for (size_t i=0; i < remove_list.size (); i++)
            {
#ifdef DEBUG_DETAIL
                printf ("[%llu] ", remove_list[i]);
#endif
                removeConnection (remove_list[i]);                                
            }
#ifdef DEBUG_DETAIL        
            printf ("\n");
#endif
        }
    }

    // update send/recv size statistics
    // type 1: send, type 2: receive
    void 
    VASTnet::updateTransmissionStat (id_t target, msgtype_t msgtype, size_t size, int type)
    {       
        // skip send / receive from the same host
        if (target == _manager->getID () || _local_targets.find (target) != _local_targets.end ())
            return;

        // record send stat
        if (type == 1)
        {
            _sendsize += size;

            if (_type2sendsize.find (msgtype) == _type2sendsize.end ())
                _type2sendsize [msgtype] = 0;
            _type2sendsize[msgtype] += size;
        }
        
        // record receive stat
        else if (type == 2)
        {
            _recvsize += size;

            if (_type2recvsize.find (msgtype) == _type2recvsize.end ())
                _type2recvsize [msgtype] = 0;
            _type2recvsize[msgtype] += size;
        }
    }

    // obtain the port portion of the ID
    id_t 
    VASTnet::resolvePort (id_t host_id)
    {
        //id_t port = (host_id & 0x00000000FFFF0000);
        //id_t tail = (host_id & 0x000000000000FFFF);
        return ((host_id & 0x00000000FFFF0000) >> 16);
    }

} // namespace Vast

