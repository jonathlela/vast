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

    VASTnet::~VASTnet ()
    {
        // make sure everything's stopped & released
        stop ();
    }

    void 
    VASTnet::start ()
    {
        //_active = true;
        _addr.host_id = NET_ID_UNASSIGNED;
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
            disconnect (list[i]);

        _id2conn.clear ();        

        // NOTE: when _active turns false, the listening thread for messages will start to terminate
        //_active = false;

        // clean up the sendbufs
        // NOTE: important to also 'clear' the buffers to avoid double delete
        std::map<id_t, VASTBuffer *>::iterator it;
        
        for (it = _sendbuf_TCP.begin (); it != _sendbuf_TCP.end (); it++)
            delete it->second;
        _sendbuf_TCP.clear ();

        for (it = _sendbuf_UDP.begin (); it != _sendbuf_UDP.end (); it++)
            delete it->second;
        _sendbuf_UDP.clear ();
        
        
        // clear up current messages received
        /*
        while (_curr_msg != NULL || receive (&_curr_msg) != false)
        {
            // de-allocate memory
            netmsg *temp;
            _curr_msg = _curr_msg->getnext (&temp);
            delete temp;
        }
        */

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
    registerID (id_t my_id)
    {
        _id = my_id;  
        _addr.host_id = _id;
        _id2addr[_id] = _addr;

        // disconnect all existing connections (as they may have previous mapping)
        // TODO: a more efficient method?
        vector<id_t> list;
        for (std::map<id_t, void *>::iterator it = _id2conn.begin (); it != _id2conn.end (); it++)
            list.push_back (it->first);

        for (unsigned int i=0; i < list.size (); i++)
            disconnect (list[i]);
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
    VASTnet::sendMessage (id_t target, Message &msg, bool reliable)
    {
        if (_active == false)
            return 0;

        // collect download transmission stat
        //size_t total_size = sizeof (size_t) + sizeof (id_t) + sizeof (timestamp_t) + qmsg->msg->size;
        updateTransmissionStat (target, msg.msgtype, msg.size, 1);

        timestamp_t sent_time = getTimestamp ();

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
            /*
            _sendbuf_local.clear ();
            _sendbuf_local.add (&msg);

            netmsg *localmsg = new netmsg (_id, _sendbuf_local.data, _sendbuf_local.size, sent_time, NULL, NULL);

            if (_curr_msg == NULL)
                _curr_msg = localmsg;
            else
                _curr_msg->append (localmsg, sent_time);

            return _sendbuf_local.size;
            */

            QMSG *newmsg = new QMSG;

            newmsg->fromhost = _id;
            newmsg->senttime = sent_time;
            newmsg->msg      = new Message (msg);
            
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

        // NOTE: the total_size is used by the receiving host to read message from network stream
        size_t total_size = sizeof (id_t) + sizeof (timestamp_t) + msg.serialize (NULL);
        
        // prepare bytestring with id, timestamp & serialized message  
        // NOTE the first three items (total_size, id, sent_time) will be extracted by the receiver's network layer
        // TODO: the packing & unpacking are not symmetric (i.e., not handled by the opposite respective functions)
        // TODO: id is sent now because for net_ace, a receiving host may not know the remote node's host_id
        //       but sending id everytime is wasteful, handshake ID just once? (what about UDP packets?)
        //       or perhaps id handshake should be done at a lower level (in front of msg packets)
        buf->add (&total_size, sizeof (size_t));
        buf->add (&_id, sizeof (id_t));
        buf->add (&sent_time, sizeof (timestamp_t));
        buf->add (&msg);

        return total_size;
    }

    // obtain next message in queue
    // return pointer to Message, or NULL for no more message        
    Message* 
    VASTnet::receiveMessage (id_t &fromhost, timestamp_t &senttime)
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
        senttime = nextmsg->senttime;

        delete nextmsg;

        updateTransmissionStat (_recvmsg->from, _recvmsg->msgtype, _recvmsg->size, 2); 
        
        return _recvmsg;
    }

    // store an incoming message for processing by message handlers
    // return the number of bytes stored
    size_t
    VASTnet::storeRawMessage (id_t fromhost, char const *msg, size_t len, timestamp_t senttime, timestamp_t recvtime)
    {                 
        if (_active == false)
            return 0;

        // transform into a Message and store to QMSG structure
        QMSG *storemsg = new QMSG;

        storemsg->msg = new Message (0);
        if (storemsg->msg->deserialize (msg) == ((size_t)(-1)))
        {
            printf ("VASTnet::storeRawMessage () deserialize message fail, from [%ld], size: %u\n", fromhost, len);
            delete storemsg->msg;
            delete storemsg;
            return 0;
        }

        storemsg->fromhost = fromhost;
        storemsg->senttime = senttime;
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

        timestamp_t time = getTimestamp ();

        // check if there are any pending TCP queues
        std::map<id_t, VASTBuffer *>::iterator it;
        for (it = _sendbuf_TCP.begin (); it != _sendbuf_TCP.end (); it++)
        {
            id_t target     = it->first;        
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
                _id2time[target] = time;
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
        if (_cleanup_counter++ > COUNTER_CONNECTION_CLEANUP)
        {
            _cleanup_counter = 0;
            //cleanConnections ();
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
    VASTnet::storeMapping (Addr &address)
    {
        // store local copy of the mapping
        _id2addr[address.host_id] = address;
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

    // set whether this host has public IP or not
    void 
    VASTnet::setPublic (bool is_public)
    {
        _is_public = is_public;
    }

    // get how many ticks exist in a second (for stat reporting)
    int 
    VASTnet::getTickPerSecond ()
    {
        return _tick_persec;
    }

    // set how many ticks exist in a second (for stat reporting)
    void 
    VASTnet::setTickPerSecond (int ticks)
    {
        printf ("VASTnet::setTickPerSecond () as %d\n", ticks);
        _tick_persec = ticks;
    }
            
    // check if a target is connected, and attempt to connect if not
    bool 
    VASTnet::validateConnection (id_t host_id)
    {
        return (isConnected (host_id) == true || connect (host_id) != (-1));
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
                printf ("VASTnet: getConnections (): Can't find IP record of id [%lu]\n", it->first);
                continue;
            }

            conn_list[it->first] = _id2addr[it->first];
        }

        return conn_list;
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
            
#ifdef DEBUG_DETAIL
        printf ("VASTnet::getAddress (): address not found for [%d]\n", (int)id);
#endif
        return null_addr;        
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
        timestamp_t curr_time = getTimestamp ();
        std::vector<Vast::id_t> remove_list;

        map<id_t, timestamp_t>::iterator it2;

        // go through existing connections and remove inactive ones
        for (map<id_t, void *>::iterator it = _id2conn.begin (); it != _id2conn.end (); it++)
        {
            it2 = _id2time.find (it->first);

            if (it2 == _id2time.end ())
            {
                // starts to record time value, as this connection could be initiated by remote host
                _id2time[it->first] = curr_time;
                continue;                
            }
            else if ((curr_time - it2->second) <= TIMEOUT_REMOVE_CONNECTION)
                continue;

            remove_list.push_back (it->first);                
        }
        for (size_t i=0; i<remove_list.size (); i++)
        {
            disconnect (remove_list[i]);
            
            // remove address
            // NOTE: disconnect would not remove id to address mapping as they could still be useful
            //_id2addr.erase (remove_list[i]);
            
            // TODO: at somepoint should clean up mappings
        }
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

