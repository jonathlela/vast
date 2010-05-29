/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2008 Shun-Yun Hu (syhu@yahoo.com)
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

namespace Vast
{   
    extern char VON_MESSAGE[][20];
    extern char VAST_MESSAGE[][20];

    using namespace std;

    VASTnet::VASTnet ()
        : _id (NET_ID_UNASSIGNED), 
          _active (false), 
          _binded (false),
          _is_public (true), 
          _timeout_IDrequest (0),
          _timeout_cleanup (0),
          _recvmsg (NULL),           
          _sec2timestamp (0)
    {        
        resetTransmissionSize ();
    }        

    VASTnet::~VASTnet ()
    {
        // make sure everything's stopped & released
        stop ();
    }

    void 
    VASTnet::start ()
    {
        //_active = true;
        //_addr.host_id = NET_ID_UNASSIGNED;
    }

    void 
    VASTnet::stop ()
    {
        if (_active == false)
            return;

        printf ("VASTnet::stop () for node %d\n", (int)_id);

        // close all active connections
        // NOTE: we still need the message receiving thread to be running 
        //       (_active cannot be set false yet, otherwise the connection during validateConnection () will fail)
        //       TODO: kind of weird..
        std::map<id_t, void *>::iterator it2;
        vector<id_t> list;
        for (it2 = _id2conn.begin (); it2 != _id2conn.end (); it2++)
            list.push_back (it2->first);

        for (size_t i=0; i < list.size (); i++)
            removeConnection (list[i]);
        
        cleanConnections ();

        // NOTE: when _active turns false, the listening thread for messages will start to terminate
        //_active = false;

        // clean up the sendbufs
        // NOTE: number of buffers may be larger than active connections, as remote host could disconnect me        
        // NOTE: important to also 'clear' the buffers to avoid double delete
        /*
        std::map<id_t, VASTBuffer *>::iterator it;
        
        for (it = _sendbuf_TCP.begin (); it != _sendbuf_TCP.end (); it++)
            delete it->second;
        
        for (it = _sendbuf_UDP.begin (); it != _sendbuf_UDP.end (); it++)
            delete it->second;

        _sendbuf_TCP.clear ();
        _sendbuf_UDP.clear ();

        */
                        
        // clear up current messages received
        for (std::multimap<byte_t, QMSG *>::iterator it = _msgqueue.begin (); it != _msgqueue.end (); it++)
        {
            delete it->second->msg;
            delete it->second;
        }

        _msgqueue.clear ();

        if (_recvmsg != NULL)
        {
            delete _recvmsg;
            _recvmsg = NULL;
        }
    }

    void 
    VASTnet::
    registerHostID (id_t my_id, bool is_public)    
    {
        // we avoid double assignment (for now)
        if (_id != NET_ID_UNASSIGNED)
            return;
                
        // record my obtained ID
        _id = my_id;  
        _addr.host_id = _id;
        _id2addr[_id] = _addr;

        _is_public    = is_public;
        
        // below is probably not necessary as the connection stores only the remote node's HostID

        /*
        // disconnect all existing connections (as they may have previous mapping)
        // TODO: a more efficient method?
        
        vector<id_t> list;
        for (std::map<id_t, void *>::iterator it = _id2conn.begin (); it != _id2conn.end (); it++)
            list.push_back (it->first);

        for (size_t i=0; i < list.size (); i++)
            disconnect (list[i]);
        */
    }

    // whether we have joined the overlay successfully and obtained a HostID
    bool
    VASTnet::isJoined ()
    {
        if (_binded == false)
            return false;

        timestamp_t now = this->getTimestamp ();

        // if HostID is not yet obtained, and our network is up
        if (_id == NET_ID_UNASSIGNED)
        {
            // check if we're considered as an entry point, if so we can determine HostID by self
            if (_entries.size () == 0)
            {
                id_t id = resolveHostID (&_addr.publicIP);

                // note we need to use registerHostID to modify _id instead of directly
                registerHostID (id, true);
            }

            // otherwise, start to contact entry points to obtain ID
            else if (now >= _timeout_IDrequest)
            {
                _timeout_IDrequest = now + (TIMEOUT_ID_REQUEST * this->getTimestampPerSecond ());

                // randomly pick an entry point
                int i = (rand () % _entries.size ());

                id_t target = resolveHostID (&_entries[i]);
                Addr addr (target, &_entries[i]);

                storeMapping (addr);

                //
                // send out ID request message & also detect whether we've public IP
                //
                // TODO: cleaner way?

                // determine our self ID / store to 'addr' temporariliy (for net_emu)
                id_t id = resolveHostID (&_addr.publicIP);             
                _addr.host_id = id;

                // create & send ID request message, consists of
                //   1) determined hostID and 2) detected IP of the host
                Message msg (0);
                msg.priority = 0;
                msg.store (id);
                msg.store (getHostAddress ().publicIP);
          
                sendMessage (target, msg, true, ID_REQUEST);
                printf ("VASTnet::isJoined () sending ID_REQUEST to gateway [%llu]\n", target);

                /*
                // create & send ID request message, consists of
                //   1) NET_ID_UNASSIGNED  2) determined hostID and 3) detected IP
                const size_t total_size = sizeof (id_t) + sizeof (id_t) + sizeof (unsigned long);
                char msg[sizeof (size_t) + total_size];

                char *p = msg;

                memcpy (p, (char *)&total_size, sizeof (size_t));       p += sizeof (size_t);
                memcpy (p, (char *)&_id, sizeof (id_t));                p += sizeof (id_t);
                memcpy (p, (char *)&id, sizeof (id_t));                 p += sizeof (id_t);
                memcpy (p, (char *)&host, sizeof (unsigned long));

                validateConnection (target);
                send (target, msg, sizeof (size_t) + total_size);
                */
            }
        }

        return (_id != NET_ID_UNASSIGNED);
    }

    // get the IP address of current host machine
    Addr &
    VASTnet::getHostAddress ()
    {
        return _addr;
    }

    // send a message to some targets, 
    // will queue in _sendbuf_TCP or _sendbuf_UDP depending on reliability until flush () is called
    // returns number of bytes sent
    size_t
    VASTnet::sendMessage (id_t target, Message &msg, bool reliable, VASTHeaderType type)
    {
        if (_active == false)
            return 0;

        // collect download transmission stat
        //size_t total_size = sizeof (size_t) + sizeof (id_t) + msg.size;
        //updateTransmissionStat (target, msg.msgtype, total_size, 1);
        updateTransmissionStat (target, msg.msgtype, msg.size, 1);
        
#ifdef DEBUG_DETAIL
        printf ("[%d] VASTnet::sendMessage to: %d msgtype: %d (%s) size: %d\n", _id, target, msg.msgtype, (msg.msgtype < 30 ? (msg.msgtype >= 10 ? VAST_MESSAGE[msg.msgtype-10] : VON_MESSAGE[msg.msgtype]) : "MESSAGE"), msg.size);
#endif
        // put default from field
        if (msg.from == 0)
            msg.from = _id;

        // TODO: more efficient way for self message?
        // if it's a local message, store to receive queue directly
        if (target == _id)
        {
            QMSG *newmsg = new QMSG;

            newmsg->fromhost = _id;
            newmsg->msg      = new Message (msg);
            newmsg->recvtime = 0;                   // set 0 so it'll be processed immediately

            // reset the message so that it can be properly decoded
            newmsg->msg->reset ();

            store (newmsg);

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
        header.type = type;
        header.msg_size = msg.serialize (NULL);
        
        // prepare bytestring with id, timestamp & serialized message  
        // NOTE the first three items (total_size, id, sent_time) will be extracted by the receiver's network layer
        // TODO: id is sent now because for net_ace, a receiving host may not know the remote node's host_id
        //       but sending id constantly is wasteful, handshake ID just once? (what about UDP packets?)
        //       or perhaps id handshake should be done at a lower level (in front of msg packets)
        buf->add ((char *)&header, sizeof (VASTHeader));
        //buf->add (&_id, sizeof (id_t));
        buf->add (&msg);

        return header.msg_size;
    }

    // obtain next message in queue
    // return pointer to Message, or NULL for no more message        
    Message* 
    VASTnet::receiveMessage (id_t &fromhost)
    {                   
        if (_active == false)
            return NULL;

        // de-allocate memory for previous mesage
        if (_recvmsg != NULL)
        {
            delete _recvmsg;
            _recvmsg = NULL;
        }

        QMSG *nextmsg;

        // obtain next available message from queue
        if ((nextmsg = receive ()) == NULL)
            return NULL;
       
        _recvmsg = nextmsg->msg;
        fromhost = nextmsg->fromhost;

        // update time for the connection
        _id2time[fromhost] = this->getTimestamp ();

        delete nextmsg;

        updateTransmissionStat (_recvmsg->from, _recvmsg->msgtype, _recvmsg->size, 2); 
        
        return _recvmsg;
    }

    
    // process an incoming raw message (decode its header)
    // whatever bytestring received is processed here in its entirety
    // returns NET_ID_UNASSIGNED for serious error (connection will be terminated)
    id_t
    VASTnet::processRawMessage (VASTHeader &header, const char *p, id_t remote_id, IPaddr *actual_IP, void *handler)
    {
        // convert byte string to Message object here
        Message *msg = new Message (0);
        
        if (msg->deserialize (p, header.msg_size) == 0)
        {
            printf ("VASTnet::processRawMessage () deserialize message fail, from [%llu], size: %lu\n", remote_id, header.msg_size);
            delete msg;
            return NET_ID_UNASSIGNED;
        }

        switch (header.type)
        {
        case ID_REQUEST:
            {
                if (remote_id != NET_ID_UNASSIGNED)
                {
                    printf ("VASTnet::processRawMessage () remote host already has an ID [%llu] ignore new requests\n", remote_id);
                }
                else
                {
                    // assign
                    remote_id = processIDRequest (*msg, actual_IP);       
                
                    // if new ID is accepted or assigned successfully
                    if (remote_id != NET_ID_UNASSIGNED)
                    {
                        // register connection
                        register_conn (remote_id, handler);
                
                        // send back reply to ID request
                        sendMessage (remote_id, *msg, true, ID_ASSIGN);
                    } 
                }
            }            
            break;

        case ID_ASSIGN:
            {
                processIDAssignment (*msg);
            }
            break;

        case HANDSHAKE:            
            if (remote_id == NET_ID_UNASSIGNED)
            {        
                // process handshake message                
                remote_id = processHandshake (*msg);
                
                // register this connection with its ID if available, or terminate otherwise
                if (remote_id == NET_ID_UNASSIGNED)
                    printf ("VASTnet::processRawMessage () cannot determine remote host's ID from handshake\n");
                else
                    register_conn (remote_id, handler);
            }            
            break;

        case REGULAR:
            // regular message must come after successful handshake
            if (remote_id != NET_ID_UNASSIGNED) 
            {
                // return first so Message object would not be de-allocated
                // NOTE: that if 0 is returned, the connection may be disconnected from here.. 
                storeRawMessage (remote_id, msg);                           

                return remote_id;
            }
            break;

        default:
            printf ("VASTnet::processRawMessage () unreconginzed message\n");
            break;
        }

        // delete temporary message
        delete msg;

        return remote_id;
    }


    // store an incoming message for processing by message handlers
    // return the number of bytes stored
    size_t
    VASTnet::storeRawMessage (id_t fromhost, Message *msg)
    {                 
        if (_active == false)
            return 0;

        // transform into a Message and store to QMSG structure
        QMSG *storemsg = new QMSG;

        storemsg->msg = msg;
        storemsg->fromhost = fromhost;
        storemsg->recvtime = getTimestamp ();
        size_t stored_size = storemsg->msg->size;

        store (storemsg);
        
        // NOTE: it's important to copy the size out first, as the stored QMSG could
        //       be read & de-allocated by another thread in real network
        return stored_size;
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
                //count_compressed_message ((net_emu *) _bridge.getNetworkInterface (it->first), after_def_size);
			}
            */
        }        

        size_t flush_size = 0;     // number of total bytes sent this time

        timestamp_t now = getTimestamp ();

        // check if there are any pending TCP queues
        std::map<id_t, VASTBuffer *>::iterator it;
        for (it = _sendbuf_TCP.begin (); it != _sendbuf_TCP.end (); it++)
        {
            id_t target = it->first;        
            VASTBuffer *buf = it->second;

            // check if there's something to send
            if (buf->size == 0)
                continue;

            // check whether the connection exists, fail if not           
            //if (validateConnection (target) == true)
            if (isConnected (target))
            {               
                flush_size += send (target, buf->data, buf->size, true);
                        
                // update the last accessed time for this connection 
                // note UDP connections aren't updated as there is no connection to remove
                _id2time[target] = now;
            }

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
                flush_size += send (target, buf->data, buf->size, false);

            // clear buffer whether we've sent sucessful or not
            buf->clear ();
        }

        // call cleanup every once in a while
        if (now > _timeout_cleanup)
        {
            _timeout_cleanup = now + (TIMEOUT_CONNECTION_CLEANUP * this->getTimestampPerSecond ());
            cleanConnections ();
        }

        // clear pending messages in queue
        // the reason why size is being collected here is because in bandwidth-limited mode, the actual bytes sent 
        // can only be known here
        flush_size += clearQueue ();

        // collect upload transmission stat for non-local messages
        // TODO: collect stats for different msgtypes? use msgtype=1 for now
        
/*
		if (this->_addr.host_id == 1)
		{
			printf("Host ID: [1] Flush Size: %ul \n", flush_size);
		}
*/

        return flush_size;
    }

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
            // NOTE: this may be normal as the port of an address can differ betweeen 
            //       what others see (if they receive a connection previously) vs. 
            //       what we actually use to connect to outside
            printf ("VASTnet::storeMapping (): existing address and new address mismatch.\n");
        }

        // store local copy of the mapping
        _id2addr[addr.host_id] = addr;
    }

    // if is connected with the node
    bool 
    VASTnet::isConnected (id_t id)
    { 
        return (_id2conn.find (id) != _id2conn.end ());
    }

    // return whether this host has public IP or not
    bool 
    VASTnet::isPublic ()
    {
        return _is_public;
    }

    /*
    // get how many ticks exist in a second (for stat reporting)
    int 
    VASTnet::getTickPerSecond ()
    {
        return _tick_persec;
    }
    */

    // set how many ticks exist in a second (for stat reporting)
    void 
    VASTnet::setTimestampPerSecond (int timestamps)
    {
        if (_sec2timestamp == 0)
            _sec2timestamp = timestamps;

        printf ("VASTnet::setTimestampPerSecond () as %d\n", (int)_sec2timestamp);
    }

    // get how many timestamps (as returned by getTimestamp) is in a second 
    timestamp_t
    VASTnet::getTimestampPerSecond ()
    {
        return _sec2timestamp;
    }
       
    // check if a target is connected, and attempt to connect if not
    bool 
    VASTnet::validateConnection (id_t host_id)
    {
        // if it's message to self or already connected
        if (_id == host_id || isConnected (host_id) == true)
            return true;

        // otherwise try to initiate connection & send handshake message
        if (connect (host_id) == (-1))
            return false;
        
        sendHandshake (host_id);

        return true;        
    }

    // get a list of currently active connections' remote id and IP addresses
    std::map<Vast::id_t, Addr> &
    VASTnet::getConnections ()
    {
        static std::map<Vast::id_t, Addr> conn_list;
        conn_list.clear ();

        for (std::map<Vast::id_t, void *>::iterator it = _id2conn.begin (); it != _id2conn.end (); it ++)
        {
            if (_id2addr.find (it->first) == _id2addr.end ())
            {
                // a potential bug (connection should have address)
                printf ("VASTnet: getConnections (): Can't find IP record of id [%llu]\n", it->first);
                continue;
            }

            conn_list[it->first] = _id2addr[it->first];
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

    /* simple version without error check
    // get a list of currently active connections' remote id and IP addresses
    std::map<id_t, Addr> &
    net_ace::getConnections ()
    {
        return _id2addr;
    }
    */

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

    // obtain a HostID based on public IP + port for entry point hosts
    //                    or on public IP + port + number for non-entry hosts;    
    id_t 
    VASTnet::resolveHostID (const IPaddr *addr)
    {
        // check self
        if (addr == NULL)
            //return _id;
            return NET_ID_UNASSIGNED;
       
        // if we're a relay with public IP
        return ((id_t)addr->host << 32) | ((id_t)addr->port << 16) | NET_ID_RELAY;
    }

    // obtain the assigned number from a HostID;
    id_t 
    VASTnet::resolveAssignedID (id_t host_id)
    {
        // last 16 bits are assigned ID + ID group (2 bits)
        //return (host_id & (0xFFFF >> 2));
        return VAST_EXTRACT_ASSIGNED_ID (host_id);
    }

    // obtain the port portion of the ID
    id_t 
    VASTnet::resolvePort (id_t host_id)
    {
        //id_t port = (host_id & 0x00000000FFFF0000);
        //id_t tail = (host_id & 0x000000000000FFFF);
        return ((host_id & 0x00000000FFFF0000) >> 16);
    }

    // obtain a NodeID
    id_t 
    VASTnet::getUniqueID (int id_group)
    {
        // check if we're relay and allow to give ID
        if (isEntryPoint (_id) == false)
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
            // if we're a relay with public IP
            id_t id = ((id_t)_addr.publicIP.host << 32) | ((id_t)_addr.publicIP.port << 16) | ((id_t)id_group << 14) | _IDcounter[id_group]++;
            return id;
        }
    }

    // get hostID for myself
    id_t 
    VASTnet::getHostID ()
    {
        return _id;
    }


    // if an id is an entry point on the overlay
    bool 
    VASTnet::isEntryPoint (id_t id)
    {
        return ((0x0FFFF & id) == NET_ID_RELAY);
    }

    // extract the ID group from an ID
    int 
    VASTnet::extractIDGroup (id_t id)
    {
        return (int)(0x03 & (id >> 14));
    }

    // extract the host IP from an ID
    int 
    VASTnet::extractHost (id_t id)
    {
        return ((id >> 32) & 0xFFFFFFFF);
    }

    id_t
    VASTnet::processIDRequest (Message &msg, IPaddr *actualIP)
    {
        // parameter check, actual IP must exist in order to process the request
        if (actualIP == NULL)
        {
            printf ("VASTnet::processIDRequest () actualIP invalid\n");
            return NET_ID_UNASSIGNED;
        }

        // extract the ID request message, consists of
        //   1) determined hostID and 2) detected IP
        id_t    id;
        IPaddr  detectedIP;
        
        // extract..
        msg.extract (id);
        msg.extract (detectedIP);

        //id_t port = VASTnet::resolvePort (id);

        // debug msg
        //printf ("[%lld] ID request from: %lld (%s:%d) actual address (%s:%d)\n", _host. addr.host_id, IP_sent, addr.publicIP.port, IP_actual, actual_addr.publicIP.port);
        
        // we assume the remote host has public IP
        // if actual IP is provided, we perform a more accurate check
        bool is_public = (actualIP->host == detectedIP.host);

        // if the remote host does not have have public IP, assign one
        if (!is_public)      
        {
            //id = getUniqueID ();
            // use the actual IP/port pair to define the unique ID
            id = resolveHostID (actualIP);

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
    id_t
    VASTnet::processIDAssignment (Message &msg)
    {
        if (msg.size != (sizeof (id_t) + sizeof (bool)))
            return NET_ID_UNASSIGNED;

        id_t id;
        bool is_public;

        msg.extract (id);
        msg.extract ((char *)&is_public, sizeof (bool));

        // store my obtained ID
        registerHostID (id, is_public);

        return id;
    }

    // send a handshake message to a newly established connection
    void 
    VASTnet::sendHandshake (id_t target)
    {
        // if we do not yet have our ID, should request ID first
        if (_id == NET_ID_UNASSIGNED)
            return;

        // prepare & send the handshake message, currently just my ID
        Message msg (0);
        msg.priority = 0;
        msg.store (_id);

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

    // periodic cleanup of inactive connections
    void 
    VASTnet::cleanConnections ()
    {
        timestamp_t now = getTimestamp ();
        std::vector<Vast::id_t> remove_list;

        timestamp_t timeout = (TIMEOUT_REMOVE_CONNECTION * this->getTimestampPerSecond ());

        // go through existing connections and remove inactive ones
        for (map<id_t, void *>::iterator it = _id2conn.begin (); it != _id2conn.end (); it++)
        {
            if ((now - _id2time[it->first]) < timeout)
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

        // TODO: cleaner way?
        // remove send buffers no longer assocated with valid connections
        std::map<id_t, VASTBuffer *>::iterator it;
        remove_list.clear ();        
        for (it = _sendbuf_TCP.begin (); it != _sendbuf_TCP.end (); it++)
        {
            if (isConnected (it->first) == false)
            {
                delete it->second;
                remove_list.push_back (it->first);
            }
        }

        for (size_t i=0; i < remove_list.size (); i++)
            _sendbuf_TCP.erase (remove_list[i]);
        
        remove_list.clear ();
        for (it = _sendbuf_UDP.begin (); it != _sendbuf_UDP.end (); it++)
        {
            if (isConnected (it->first) == false)
            {
                delete it->second;
                remove_list.push_back (it->first);
            }
        }

        for (size_t i=0; i < remove_list.size (); i++)
            _sendbuf_UDP.erase (remove_list[i]);
    }

    // remove a single connection cleanly
    bool 
    VASTnet::removeConnection (id_t target)
    {
        if (_id2conn.find (target) == _id2conn.end ())
            return false;

        disconnect (target);

        // also remove the send buffer to this host
        if (_sendbuf_TCP.find (target) != _sendbuf_TCP.end ())
        {
            delete _sendbuf_TCP[target];
            _sendbuf_TCP.erase (target);
        }

        if (_sendbuf_UDP.find (target) != _sendbuf_UDP.end ())
        {
            delete _sendbuf_UDP[target];
            _sendbuf_UDP.erase (target);
        }

        // remove address
        // NOTE: disconnect should not remove id to address mapping as they could still be useful
        //_id2addr.erase (remove_list[i]);
        
        // TODO: at somepoint should clean up id2host mappings

        return true;
    }


    // update send/recv size statistics
    // type 1: send, type 2: receive
    void 
    VASTnet::updateTransmissionStat (id_t target, msgtype_t msgtype, size_t size, int type)
    {       
        // skip send / receive from the same host
        if (target == _id || _local_targets.find (target) != _local_targets.end ())
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

} // namespace Vast

