/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005 Shun-Yun Hu (syhu@yahoo.com)
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

#include "net_emu.h"

namespace Vast
{   
    // static member variables
    Compressor net_emu::_compressor;

    net_emu::net_emu (net_emubridge &b)
        :_bridge (b)
    {
        // NOTE: we mimic what happens with net_ace, where the IP/port is first obtained
        //       but unique ID is yet assigned (need to query gateway)

        // obtain a temporary id first
        
        id_t id = _bridge.obtain_id (this);
        //registerHostID (id, true);

        //ACE_INET_Addr addr (1037, getIPFromHost ());
        //gateway = *VASTVerse::translateAddress (str);

        // store IP:port for this host
        _addr.setPublic (2130706433, (unsigned short)(1037 + id - 1));

        // translate our ID
        id_t new_id = VASTnet::resolveHostID (&_addr.publicIP);

        _bridge.replaceHostID (id, new_id);

        // set the conversion rate between seconds and timestamp unit
        // for net_emu it's the same as tick_persec
        _sec2timestamp = 0;
    }

    /*
    void 
    net_emu::
    registerHostID (id_t my_id)
    {        
        // first replace ID used in bridge's lookup
        _bridge.registerHostID (_id, my_id);       

        VASTnet::registerHostID (my_id);
    }
    */

    void 
    net_emu::start ()
    {
        _active = true;
        _binded = true;
        VASTnet::start ();            
    }

    void 
    net_emu::stop ()
    {
        VASTnet::stop ();
		_bridge.releaseHostID (_addr.host_id);
        _binded = false;
        _active = false;
    }

    // Note:
    //      when connecting to an address with connect(Addr& addr), 
    //      id is NET_ID_UNASSIGNED:
    //          means connecting to an outside node, 
    //          so the network layer should allocate a temp & private id and return 
    //          an addr structure 
    //      any non-private id: 
    //          connects to the node, and register addr.id to the address
    int 
    net_emu::
    connect (id_t target)
    {
        if (_active == false)
            return (-1);

        // avoid self-connection & re-connection
        if (target == _id || isConnected (target))
            return 0;

        // lookup address
        if (_id2addr.find (target) == _id2addr.end ())
        {
            printf ("net_emu::connect (): cannot find address for target:%d\n", (int)target);
            return 0;           
        }

        // notify remote node of the connection
        net_emu *receiver = (net_emu *)_bridge.getNetworkInterface (target);

        if (receiver == NULL)
            return (-1);

        // create a dummy connection record
        register_conn (target, NULL);

        // notify remote host of connection
        //receiver->remoteConnect (_id, _id2addr[_id]);        
        receiver->remoteConnect (_addr.host_id, _addr);
        
        return 0;
    }

    int 
    net_emu::
    disconnect (Vast::id_t target)
    {
        // check if connection exists
        if (_id2conn.find (target) == _id2conn.end ())
            return -1;

#ifdef DEBUG_DETAIL
        printf ("[%lu] disconnection success\n", target);
#endif

        // do a remote disconnect
        net_emu *receiver = (net_emu *)_bridge.getNetworkInterface (target);
        
        if (receiver == NULL)
            return -1;

        unregister_conn (target);

        receiver->remoteDisconnect (_id);   
        return 0;
    }

    // send an outgoing message to a remote host
    // return the number of bytes sent
    size_t 
    net_emu::send (id_t target, char const *msg, size_t size, bool reliable)
    {        
        if (_active == false || isConnected (target) == false)
            return 0;

        // find the receiver network record
        net_emu *receiver = (net_emu *)_bridge.getNetworkInterface (target);

        if (receiver == NULL)
            return 0;

        // create the message receive time, do not send if dropped (arrival time is -1)
        timestamp_t recvtime;

        if ((recvtime = _bridge.getArrivalTime (_id, target, size, reliable)) == (timestamp_t)(-1))
            return 0;

        // loop through the bytestring, as this msg could contain several 
        // distinct messages to different target logical nodes
        // this is to simulate message de-multiplex in real network
        char *p = (char *)msg;
        VASTHeader header;
        //size_t      msg_size;
        //id_t        fromhost;
        //timestamp_t senttime;

        while (p < (msg + size))
        {            
            memcpy (&header, p, sizeof (VASTHeader));
            p += sizeof (VASTHeader);

            /*
            msg_size -= (sizeof (id_t) + sizeof (timestamp_t));

            memcpy (&fromhost, p, sizeof (id_t));
            p += sizeof (id_t);

            memcpy (&senttime, p, sizeof (timestamp_t));
            p += sizeof (timestamp_t);
            */

            // TODO: record recvtime?
            receiver->processRawMessage (header, p, _id);
            p += header.msg_size;
        }

        return size;
    }

    // receive an incoming message
    // return pointer to next QMSG structure or NULL for no more message
    QMSG *
    net_emu::receive ()
    {
        // return if no more message in queue 
        // or all messages are beyond current logical time
   
        if (_msgqueue.size () == 0)
            return NULL;

        // obtain current time
        timestamp_t time = getTimestamp ();

        QMSG *qmsg;
        
        // go through all messages to find the first message ready to be processed
        // NOTE: if we do not do logical time check, then it'll be possible
        // for a node to process messages from another node that had sent
        // a message earlier in the same time-step (this makes 0 latency). 
        for (multimap<byte_t, QMSG *>::iterator it = _msgqueue.begin (); it != _msgqueue.end (); it++)
        {
            qmsg = it->second;
            if (qmsg->recvtime == 0 || time > qmsg->recvtime)
            {
                _msgqueue.erase (it);
                return qmsg;
            }
        }

        return NULL;
    }

    // store a message into priority queue
    // returns success or not
    bool 
    net_emu::store (QMSG *qmsg)
    {
        // we store message according to message priority
        _msgqueue.insert (std::multimap<byte_t, QMSG *>::value_type (qmsg->msg->priority, qmsg));        

        return true;
    }

    // methods to keep track of active connections (associate ID with connection stream)
    // returns NET_ID_UNASSIGNED if failed
    id_t 
    net_emu::register_conn (id_t id, void *stream)
    {
        if (_id2conn.find (id) != _id2conn.end ())
            return NET_ID_UNASSIGNED;
        
       // make a record of connection
       _id2conn[id] = stream;
       _id2time[id] = getTimestamp ();

       return id;        
    }

    id_t 
    net_emu::unregister_conn (id_t id)
    {
        // cut connection
        map<id_t, void *>::iterator it = _id2conn.find (id); 
        
        // error connection doesn't exist
        if (it == _id2conn.end ())
            return NET_ID_UNASSIGNED;
      
        _id2conn.erase (it);
        _id2time.erase (id);

        return id;
    }

    // remote host has connected to me
    bool
    net_emu::
    remoteConnect (Vast::id_t remote_id, const Addr &addr)
    {
        if (_active == false)
            return false;

        // store dummy (NULL) stream connection
        register_conn (remote_id, NULL);
        storeMapping (addr);

        return true;
    }
    
    // remote host has disconnected me
    void 
    net_emu::
    remoteDisconnect (Vast::id_t remote_id)
    {
        unregister_conn (remote_id);

        // send a DISCONNECT notification
        Message *msg = new Message (DISCONNECT);
        storeRawMessage (remote_id, msg);
    }


} // end namespace Vast

