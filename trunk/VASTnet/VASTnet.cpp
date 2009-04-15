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
    using namespace std;

    VASTnet::~VASTnet ()
    {
        // make sure everything's stopped & released
        stop ();
    }

    void 
    VASTnet::start ()
    {
    }

    void 
    VASTnet::stop ()
    {
        _active = false;

        // clean up the sendbufs
        std::map<id_t, VASTBuffer *>::iterator it;
        
        for (it = _sendbuf_TCP.begin (); it != _sendbuf_TCP.end (); it++)
            delete it->second;

        for (it = _sendbuf_UDP.begin (); it != _sendbuf_UDP.end (); it++)
            delete it->second;

        // close all active connections
        std::map<id_t, void *>::iterator it2;
        for (it2 = _id2conn.begin (); it2 != _id2conn.end (); it2++)
            disconnect (it2->first);
    }

    // send a message to some nodes, will queue in _sendbuf_TCP or _sendbuf_UDP 
    // depending on reliable or not, until flush () is called
    // returns number of bytes sent
    size_t
    VASTnet::sendMessage (id_t target, Message &msg, bool reliable)
    {
        if (_active == false)
            return 0;

        timestamp_t sent_time = getTimestamp ();
       
        // make sure we have a connection, or establish one if not
        if (isConnected (target) == false && connect (target) == (-1))
            return 0;
        
        // get the TCP or UDP queue, create one if necessary            
        std::map<id_t, VASTBuffer *> &send_buf = (reliable ? _sendbuf_TCP : _sendbuf_UDP);
        
        if (send_buf.find (target) == send_buf.end ())
            send_buf[target] = new VASTBuffer ();
        VASTBuffer *buf = send_buf[target];

        // NOTE: the total_size is used by the receiving host to read message from network stream
        size_t total_size = sizeof (id_t) + sizeof (timestamp_t) + msg.serialize (NULL);
        
        // prepare bytestring with id, timestamp & serialized message  
        // NOTE the first three ideas will be extracted by the receiver's network layer
        buf->add (&total_size, sizeof (size_t));
        buf->add (&_id, sizeof (id_t));
        buf->add (&sent_time, sizeof (timestamp_t));
        buf->add (&msg);
        
        // collect transmission stat
        updateTransmissionStat (msg.msgtype, total_size, 1);

        return total_size;
    }

    // obtain next message in queue
    // return pointer to Message, or NULL for no more message        
    Message* 
    VASTnet::receiveMessage (id_t &from, timestamp_t &senttime)
    {   
        netmsg *curr_msg;

        if (_active == false || receive (&curr_msg) == false)
            return NULL;
        
        // prepare the return data
        from     = curr_msg->from;
        senttime = curr_msg->time;

        _recvmsg.deserialize (curr_msg->msg);

        // collect transmission stat
        size_t total_size = sizeof (id_t) + sizeof (timestamp_t) + curr_msg->size;
        updateTransmissionStat (_recvmsg.msgtype, total_size, 2);

        // de-allocate memory
        delete curr_msg;
        return &_recvmsg;
    }

    // send out all pending messages to each host
    // return number of bytes sent
    size_t
    VASTnet::flush (bool compress)
    {
        /*
		if (compress == true)
		{
            // syhu NOTE: currently compression in simulation mode is disabled
            //            somehow it should be a generic feature at the Network.cpp level
            //            so both emulated & real network can benefit from compression
            
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

        }
        */

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

            // check for connection
            if (isConnected (target) == false && connect (target) == (-1))
                continue;

            send (target, buf->data, buf->size, true);
            flush_size += buf->size; 
            buf->clear ();            

            // update the last accessed time for this connection 
            // note UDP connections aren't updated as there is no connection to remove
            _id2time[target] = time;
        }

        // check if there are any pending UDP queues
        for (it = _sendbuf_UDP.begin (); it != _sendbuf_UDP.end (); it++)
        {
            id_t target     = it->first;        
            VASTBuffer *buf = it->second;

            // check if there's something to send
            if (buf->size == 0 || _id2addr.find (target) == _id2addr.end ())
                continue;

            send (target, buf->data, buf->size, false);
            flush_size += buf->size; 
            buf->clear ();
        }
        
        return flush_size;
    }

    // notify a certain node's network layer of nodeID -> Address mapping
    // target can be self
    bool 
    VASTnet::notifyMapping (id_t target, std::map<id_t, Addr> &mapping)
    {
        // notify self
        if (target == _id || target == NET_ID_UNASSIGNED)
        {
            map<id_t, Addr>::iterator it;
            for (it = mapping.begin (); it != mapping.end (); it++)
                _id2addr[it->first] = it->second;
        }
        // else deliver the mapping to a remote node
        // TODO: implement this:

        return true;
    }

    // if is connected with the node        
    bool 
    VASTnet::isConnected (id_t id)
    { 
        return (_id2conn.find (id) != _id2conn.end ());
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

        if (_id2addr.find (id) == _id2addr.end ())
        {
#ifdef DEBUG_DETAIL
            printf ("getAddress (): address not found for [%d]\n", (int)id);
#endif
            return null_addr;
        }
        return _id2addr[id];
    }

    // obtain the tranmission size by message type, default is to return all types
    size_t 
    VASTnet::getSendSize (const msgtype_t msgtype)
    {
        if (msgtype == 0)
            return _sendsize;
        if (_type2sendsize.find (msgtype) != _type2sendsize.end ())
            return _type2sendsize [msgtype];
       
        return 0;
    }
    
    size_t 
    VASTnet::getReceiveSize (const msgtype_t msgtype)
    {
        if (msgtype == 0)
            return _recvsize;
        if (_type2recvsize.find (msgtype) != _type2recvsize.end ())
            return _type2recvsize [msgtype];

        return 0;
    }

    // periodic cleanup of inactive connections
    void 
    VASTnet::cleanConnections ()
    {
        timestamp_t curr_time = getTimestamp ();
        std::vector<Vast::id_t> remove_list;

        // go through existing connections and remove inactive ones
        for (map<id_t, void *>::iterator it = _id2conn.begin (); it != _id2conn.end (); it++)
        {
            if (_id2time.find (it->first) == _id2time.end () ||
                _id2time[it->first] - curr_time > TIMEOUT_REMOVE_CONNECTION)
                remove_list.push_back (it->first);                
        }
        for (size_t i=0; i<remove_list.size (); i++)
            disconnect (remove_list[i]);
    }

    // update send/recv size statistics
    // type 1: send, type 2: receive
    void 
    VASTnet::updateTransmissionStat (msgtype_t msgtype, size_t size, int type)
    {
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

