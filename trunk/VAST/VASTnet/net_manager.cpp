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

#include "net_manager.h"

namespace Vast
{   

    using namespace std;

    void 
    net_manager::start ()
    {
        // we begin with unassigned ID
        _id = NET_ID_UNASSIGNED;
        _recvmsg = NULL;
    }

    void 
    net_manager::stop ()
    {
        if (_active == false)
            return;

        printf ("net_manager::stop () for node [%llu]\n", _id);

        // close all active connections
        // NOTE: we still need the message receiving thread to be running 
        //       (_active cannot be set false yet, otherwise potential call to validateConnection () may fail & crash)
        //       TODO: kind of weird..
        vector<id_t> list;
        for (std::map<id_t, ConnectInfo>::iterator it = _id2conn.begin (); it != _id2conn.end (); it++)
            list.push_back (it->first);

        for (size_t i=0; i < list.size (); i++)
            disconnect (list[i]);
        
        //cleanConnections ();
                        
        // clear up incoming queue
        for (size_t i=0; i < _recv_queue.size (); i++)
            delete _recv_queue[i];
        _recv_queue.clear ();

        // clear received message buffer
        if (_recvmsg != NULL)
        {
            delete _recvmsg;
            _recvmsg = NULL;
        }
    }

    // see if the network interface is active
    bool 
    net_manager::isActive ()
    {
        return _active;
    }

    // check if a certain host is connected
    bool 
    net_manager::isConnected (id_t target)
    {
        return (_id2conn.find (target) != _id2conn.end ());
    }

    // set my ID, return the previous ID
    id_t 
    net_manager::setID (id_t my_id)
    {
        id_t prev_id = _id;

        _id = my_id;
        _self_addr.host_id = _id;

        return prev_id;
    }

    // obtain my ID
    id_t 
    net_manager::getID ()
    {
        return _id;
    }

    // obtain my address
    Addr &
    net_manager::getAddress ()
    {
        return _self_addr;
    }

    // get how many timestamps (as returned by getTimestamp) is in a second 
    timestamp_t
    net_manager::getTimestampPerSecond ()
    {
        return _sec2timestamp;
    }

    // get last access time of a connection
    timestamp_t 
    net_manager::getLastTime (id_t id)
    {
        std::map<id_t, ConnectInfo>::iterator it = _id2conn.find (id);
        if (it != _id2conn.end ())
            return it->second.lasttime;
        else
            return 0;
    }

    // 
    // static methods (tools for external classes)
    //

    // obtain a HostID based on public IP + port for entry point hosts
    //                    or on public IP + port + number for non-entry hosts;    
    id_t 
    net_manager::resolveHostID (const IPaddr *addr)
    {
        // check self
        if (addr == NULL)
            return NET_ID_UNASSIGNED;
       
        // if we're a relay with public IP
        return ((id_t)addr->host << 32) | ((id_t)addr->port << 16) | NET_ID_RELAY;
    }

    // obtain the assigned number from a HostID;
    id_t 
    net_manager::resolveAssignedID (id_t host_id)
    {
        // last 16 bits are assigned ID + ID group (2 bits)
        //return (host_id & (0xFFFF >> 2));
        return VAST_EXTRACT_ASSIGNED_ID (host_id);
    }

    // extract the ID group from an ID
    int 
    net_manager::extractIDGroup (id_t id)
    {
        return (int)(0x03 & (id >> 14));
    }

    // extract the host IP from an ID
    int 
    net_manager::extractHost (id_t id)
    {
        return ((id >> 32) & 0xFFFFFFFF);
    }

} // namespace Vast

