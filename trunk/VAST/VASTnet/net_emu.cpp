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

#include "net_emu.h"

namespace Vast
{   
    // static member variables
    //Compressor net_emu::_compressor;

    // currently we assume there's only one globally accessible netbridge (for simu only)
    // TODO: once created, it will not be released until the program ends,
    //       a better mechanism?
    static net_emubridge *g_bridge     = NULL;
    int                   g_bridge_ref = 0;            // reference count for the bridge

    net_emu::net_emu (timestamp_t sec2timestamp)
    {
        // initialize rand generator (for node fail simulation, NOTE: same seed is used to produce exactly same results)
        //srand ((unsigned int)time (NULL));
        srand (0);

        // create the netbridge used by simulations, if needed
        if (g_bridge == NULL)
        {
            // create a shared net-bridge (used in simulation to locate other simulated nodes)
            // NOTE: g_bridge may be shared across different VASTVerse instances            
            //g_bridge = new net_emubridge (_simpara.loss_rate, _simpara.fail_rate, 1, _simpara.step_persec, 1);
            g_bridge = new net_emubridge (0, 0, 1, (size_t)sec2timestamp, 1);
        }

        g_bridge_ref++;

        // NOTE: we mimic what happens with net_ace, where the IP/port is first obtained
        //       but unique ID is yet assigned (need to query gateway)

        // obtain a temporary id first        
        id_t id = g_bridge->obtain_id (this);

        // store artificial IP:port for this host (127.0.0.1:1037 + id - 1)
        _self_addr.setPublic (2130706433, (uint16_t)(1037 + id - 1));

        // self-determine preliminary hostID first
        _self_addr.host_id = this->resolveHostID (&_self_addr.publicIP);
        
        // make sure the bridge stores proper unique ID
        g_bridge->replaceHostID (id, _self_addr.host_id);

        // set the conversion rate between seconds and timestamp unit
        // for net_emu it's the same as tick_persec
        _sec2timestamp = sec2timestamp;
    }

    net_emu::~net_emu ()
    {
        // remove from bridge so that others can't find me
        g_bridge->releaseHostID (_id);

        g_bridge_ref--;

        // only delete the bridge if no other VASTVerse's using it
        if (g_bridge != NULL && g_bridge_ref == 0)
        {
            delete g_bridge;
            g_bridge = NULL;
        }
    }

    void 
    net_emu::start ()
    {
        _active = true;
        net_manager::start ();          
    }

    void 
    net_emu::stop ()
    {
        net_manager::stop ();
		g_bridge->releaseHostID (_self_addr.host_id);
        _active = false;
    }

    // get current physical timestamp
    timestamp_t 
    net_emu::getTimestamp ()
    {
        if (g_bridge)
            return g_bridge->getTimestamp ();
        else
            return 0;
    }

    // get IP address from host name
    const char *
    net_emu::getIPFromHost (const char *hostname)
    {
        return "127.0.0.1";
    }

    // obtain the IP / port of a remotely connected host
    // returns NULL if not available
    bool 
    net_emu::getRemoteAddress (id_t host_id, IPaddr &addr)
    {
        if (g_bridge)
        {
            net_emu *receiver = (net_emu *)g_bridge->getNetworkInterface (host_id);

            if (receiver == NULL)
                return false;

            addr = receiver->getAddress ().publicIP;
            return true;
        }

        return false;
    }

    // Note:
    //      when connecting to an address with connect(Addr& addr), 
    //      id is NET_ID_UNASSIGNED:
    //          means connecting to an outside node, 
    //          so the network layer should allocate a temp & private id and return 
    //          an addr structure 
    //      any non-private id: 
    //          connects to the node, and register addr.id to the address
    bool
    net_emu::
    connect (id_t target, unsigned int host, unsigned short port, bool is_secure)
    {
        if (_active == false)
            return false;

        // avoid self-connection & re-connection
        if (target == _id)
            return false;

        // notify remote node of the connection
        net_emu *receiver = (net_emu *)g_bridge->getNetworkInterface (target);

        if (receiver == NULL)
            return false;

        // create a dummy connection record
        this->socket_connected (target, NULL, is_secure);

        // notify remote host of connection
        receiver->remoteConnect (_self_addr.host_id, _self_addr);
        
        return true;
    }

    bool
    net_emu::
    disconnect (Vast::id_t target)
    {
        // check if connection exists
        if (_id2conn.find (target) == _id2conn.end ())
            return false;

#ifdef DEBUG_DETAIL
        printf ("[%lu] disconnection success\n", target);
#endif

        // do a remote disconnect
        net_emu *receiver = (net_emu *)g_bridge->getNetworkInterface (target);
        
        if (receiver == NULL)
            return false;

        this->socket_disconnected (target);
        receiver->remoteDisconnect (_id);  

        return true;
    }

    // send an outgoing message to a remote host
    // return the number of bytes sent
    size_t 
    net_emu::send (id_t target, char const *msg, size_t size, const Addr *addr)
    {        
        if (_active == false || isConnected (target) == false)
            return 0;

        // find the receiver network record
        net_emu *receiver = (net_emu *)g_bridge->getNetworkInterface (target);

        if (receiver == NULL)
            return 0;

        bool reliable = (addr == NULL);

        // create the message receive time, do not send if dropped (arrival time is -1)
        timestamp_t recvtime;

        if ((recvtime = g_bridge->getArrivalTime (_id, target, size, reliable)) == (timestamp_t)(-1))
            return 0;
        
        // TODO: try not to dual allocate message size?  
        //receiver->msg_received (_id, msg, size, recvtime);
        receiver->msg_received (_self_addr.host_id, msg, size, recvtime);

        // update last access time for connection
        if (_id2conn.find (target) != _id2conn.end ())
            _id2conn[target].lasttime = this->getTimestamp ();

        /*
        // loop through the bytestring, as this msg could contain several 
        // distinct messages to different target logical nodes
        // this is to simulate message de-multiplex in real network
        char *p = (char *)msg;
        VASTHeader header;

        while (p < (msg + size))
        {            
            memcpy (&header, p, sizeof (VASTHeader));
            p += sizeof (VASTHeader);

            // TODO: record recvtime?
            // NOTE: my actual IP is sent for processing, so that public/private IP check will pass
            receiver->processVASTMessage (header, p, _id, &_addr.publicIP);
            p += header.msg_size;
        }
        */

        return size;
    }

    // receive an incoming message
    // return pointer to next NetSocketMsg structure or NULL for no more message
    NetSocketMsg *
    net_emu::receive ()
    {
        // clear last received message
        if (_recvmsg != NULL)
        {            
            delete _recvmsg;
            _recvmsg = NULL;
        }

        // return if no more message in queue 
        // or all messages are beyond current logical time
  
        if (_recv_queue.size () == 0)
            return NULL;

        // obtain current time
        timestamp_t now = getTimestamp ();

        NetSocketMsg *msg;
        
        // go through all messages to find the first message ready to be processed
        // NOTE: if we do not do logical time check, then it'll be possible
        // for a node to process messages from another node that had sent
        // a message earlier in the same time-step (this makes 0 latency). 

        for (size_t i=0; i < _recv_queue.size (); i++)
        {
            msg = _recv_queue[i];
            
            if (msg->recvtime == 0 || now > msg->recvtime)
            {
                _recvmsg = msg;
                _recv_queue.erase (_recv_queue.begin () + i);
                return _recvmsg;
            }
        }

        /*
        for (multimap<byte_t, QMSG *>::iterator it = _msgqueue.begin (); it != _msgqueue.end (); it++)
        {
            qmsg = it->second;
            if (qmsg->recvtime == 0 || now > qmsg->recvtime)
            {
                _msgqueue.erase (it);

                return qmsg;
            }
        }
        */

        return NULL;
    }

    // change the ID for a remote host
    bool 
    net_emu::switchID (id_t prevID, id_t newID)
    {
        // new ID already in use
        if (_id2conn.find (newID) != _id2conn.end () || _id2conn.find (prevID) == _id2conn.end ())
        {
            printf ("[%llu] net_emu::switchID () old ID not found or new ID already exists\n");
            return false;
        }

        // copy to new ID
        _id2conn[newID] = _id2conn[prevID];

        // erase old ID mapping
        _id2conn.erase (prevID);

        return true;
    }

    // perform a tick of the logical clock 
    void 
    net_emu::tickLogicalClock ()
    {
        if (g_bridge)
            g_bridge->tick ();
    }

    // store a message into priority queue
    // returns success or not
    bool 
    net_emu::msg_received (id_t fromhost, const char *message, size_t size, timestamp_t recvtime, bool in_front)
    {        
        NetSocketMsg *msg = new NetSocketMsg;

        msg->fromhost = fromhost;
        msg->recvtime = recvtime;

        if (size > 0)
        {
            msg->msg = new char[size];
            memcpy (msg->msg, message, size);
        }
        msg->size = size;

        if (recvtime == 0)
            msg->recvtime = this->getTimestamp ();

        // we store message according to message priority
        _recv_queue.push_back (msg);

        // update last access time of the connection
        std::map<id_t, ConnectInfo>::iterator it = _id2conn.find (fromhost);
        if (it != _id2conn.end ())
        {
            if (msg->recvtime > it->second.lasttime)
                it->second.lasttime = msg->recvtime; 
        }

        return true;
    }

    // methods to keep track of active connections (associate ID with connection stream)
    // returns NET_ID_UNASSIGNED if failed
    bool
    net_emu::socket_connected (id_t id, void *stream, bool is_secure)
    {
        if (_id2conn.find (id) != _id2conn.end ())
            return false;

        ConnectInfo info (stream, getTimestamp (), is_secure);
        
        // make a record of connection
        _id2conn.insert (std::map<id_t, ConnectInfo>::value_type (id, info)); 

        return true;
    }

    bool
    net_emu::socket_disconnected (id_t id)
    {
        // cut connection
        map<id_t, ConnectInfo>::iterator it = _id2conn.find (id); 
        
        // error connection doesn't exist
        if (it == _id2conn.end ())
            return false;
      
        _id2conn.erase (it);

        // store a NULL message to indicate disconnection to message queue
        this->msg_received (id, NULL, 0);

        return true;
    }

    // remote host has connected to me
    bool
    net_emu::
    remoteConnect (Vast::id_t remote_id, const Addr &addr)
    {
        if (_active == false)
            return false;

        // store dummy (NULL) stream connection, note: by default assuming non-secure
        this->socket_connected (remote_id, NULL, false);
        
        // TODO: move this to VASTnet
        //storeMapping (addr);

        return true;
    }
    
    // remote host has disconnected me
    void 
    net_emu::
    remoteDisconnect (Vast::id_t remote_id)
    {
        // NOTE: this will cause a DISCONNECT message be stored in local message queue
        this->socket_disconnected (remote_id);

        // send a DISCONNECT notification
        //Message *msg = new Message (DISCONNECT);
        //storeRawMessage (remote_id, msg);
    }

} // end namespace Vast

