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

    void 
    net_emu::
    registerID (id_t my_id)
    {        
        // first replace ID used in bridge's lookup
        _bridge.registerID (_id, my_id);       

        VASTnet::registerID (my_id);
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

        // create a dummy connection record
        _id2conn[target] = 0;

        // notify remote node of the connection
        net_emu *receiver = (net_emu *)_bridge.getNetworkInterface (target);

        if (receiver == NULL)
            return (-1);

        // notify remote host of connection
        receiver->remoteConnect (_id, _id2addr[_id]);
        
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
        
        // update the connection relationship
        _id2conn.erase (target);
        //_id2addr.erase (target);
        _id2time.erase (target);

        // do a remote disconnect
        net_emu *receiver = (net_emu *)_bridge.getNetworkInterface (target);
        
        if (receiver == NULL)
            return -1;

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
        size_t      msg_size;
        id_t        fromhost;
        timestamp_t senttime;

        while (p < (msg + size))
        {            
            memcpy (&msg_size, p, sizeof (size_t));
            p += sizeof (size_t);

            msg_size -= (sizeof (id_t) + sizeof (timestamp_t));

            memcpy (&fromhost, p, sizeof (id_t));
            p += sizeof (id_t);

            memcpy (&senttime, p, sizeof (timestamp_t));
            p += sizeof (timestamp_t);

            receiver->storeRawMessage (fromhost, p, msg_size, senttime, recvtime);
            p += msg_size;
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
            if (time > qmsg->senttime)
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
    net_emu::
    store (QMSG *qmsg)
    {
        // we store message according to message priority
        _msgqueue.insert (std::multimap<byte_t, QMSG *>::value_type (qmsg->msg->priority, qmsg));        

        return true;
    }

    // remote host has connected to me
    bool
    net_emu::
    remoteConnect (Vast::id_t remote_id, const Addr &addr)
    {
        if (_active == false)
            return false;

        if (_id2conn.find (remote_id) == _id2conn.end ())
        {
            // make a record of connection
            _id2conn[remote_id] = 0;

            // check for information consistency
            if (_id2addr.find (remote_id) != _id2addr.end () &&
                _id2addr[remote_id] != addr)
                printf ("net_emu::remoteConnect (): remote address and local address remote mismatch.\n");

            _id2addr[remote_id] = addr;
        }

        return true;
    }
    
    // remote host has disconnected me
    void 
    net_emu::
    remoteDisconnect (Vast::id_t remote_id)
    {
        // cut connection
        if (_id2conn.find (remote_id) != _id2conn.end ())
            _id2conn.erase (remote_id);

        // send a DISCONNECT notification
        storeRawMessage (remote_id, 0, 0, getTimestamp (), getTimestamp ());
    }


} // end namespace Vast

