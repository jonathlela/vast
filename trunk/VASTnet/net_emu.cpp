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
    registerID (Vast::id_t my_id)
    {
        Vast::id_t temp_id = _id;

        // replace ID used in bridge's lookup
        _bridge.registerID (_id, my_id);
        _id = my_id;

        // register new id in _id2addr structure
        _id2addr[_id]    = _id2addr[temp_id];

        // check to prevent registerID from erasing currently used ID
        if (temp_id != _id)
            _id2addr.erase (temp_id);
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
            printf ("net_emu::connect (): cannot find address for target:%d\n", target);
            return 0;           
        }

        Addr &addr = _id2addr[target];

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
        _id2addr.erase (target);
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
    net_emu::send (id_t target, char const*msg, size_t size, bool reliable)
    {
        if (_active == false || isConnected (target) == false)
            return 0;

        // find the receiver network record
        net_emu *receiver = (net_emu *)_bridge.getNetworkInterface (target);

        if (receiver == NULL)
            return (-1);

        // create the message receive time, do not send if dropped (arrival time is -1)
        timestamp_t time;
       
        if (target == _id)
            time = 0;
        else if ((time = _bridge.getArrivalTime (_id, target, size, reliable)) == (timestamp_t)(-1))
            return (-1);

        return receiver->storeMessage (_id, msg, size, time);
    }

    // receive an incoming message
    // return success or not
    bool 
    net_emu::receive (netmsg **msg)
    {
        // record time for use by remoteDisconnect
        timestamp_t time = getTimestamp ();

        // return if no more message in queue 
        // or all messages are beyond current logical time
        //
        // NOTE: if we do not do logical time check, then it'll be possible
        // for a node to process messages from another node that had sent
        // a message earlier in the same time-step (this makes 0 latency). 
        
        if (_msgqueue.size () == 0 || (time > 0 && time <= _msgqueue.begin ()->second->time))
            return false;

        *msg = _msgqueue.begin ()->second;
        _msgqueue.erase (_msgqueue.begin ());

        return true;
    }

    // insert the new message as a node in the double linklist
    // NOTE: the list is sorted by timestamp when inserting
    int
    net_emu::
    storeMessage (Vast::id_t from, char const *msg, size_t len, timestamp_t time)    
    {                    
        if (_active == false)
            return 0;

        // store the new message
        netmsg *newnode = new netmsg (from, msg, len, time, NULL, NULL);
        
        // failure likely due to memory allocation problem
        if (newnode == NULL || newnode->size < 0)
            return 0;

        _msgqueue.insert (std::multimap<timestamp_t, netmsg *>::value_type (time, newnode));
        
        return len;
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
        storeMessage (remote_id, 0, 0, getTimestamp ());
    }


} // end namespace Vast

