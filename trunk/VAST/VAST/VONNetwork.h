/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2011 Shun-Yun Hu  (syhu@ieee.org)
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
 *  VONNetwork.h -- an abstract interface to be used by VONPeer for all network functions
 *
 *  history 2009/06/05  init
 *
 */



#ifndef _VAST_VONNetwork_H
#define _VAST_VONNetwork_H

#include "Config.h"
#include "VASTTypes.h"
#include <map>

using namespace std;


namespace Vast
{
    class VONNetwork
    {

    public:
        virtual ~VONNetwork () {}

        // send messages to some target nodes
        // returns number of bytes sent
        virtual size_t sendVONMessage (Message &msg, bool is_reliable = true, vector<id_t> *failed_targets = NULL) = 0;

        // obtain next message in queue
        // return pointer to Message, or NULL for no more message
        virtual Message* receiveVONMessage (timestamp_t &senttime) = 0;

        // notify the network layer of nodeID -> Address mapping        
        virtual bool notifyAddressMapping (id_t node_id, Addr &addr) = 0;

        // get the IP address of current host machine
        virtual Addr &getHostAddress () = 0;

        // get current physical timestamp
        virtual timestamp_t getTimestamp () = 0;

        // get how many timestamps exist in a second (for timeout-related tasks)
        virtual timestamp_t getTimestampPerSecond () = 0;

    };

} // namespace Vast

#endif

