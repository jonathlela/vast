/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2010  Shun-Yun Hu (syhu@ieee.org)
 *                          Shao-Chen Chang (cscxcs at gmail.com)
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
 * net_emubridge_bl.h -- virtual bridege between various net_emu_bl classes
 *                       for discovering pointers of other net_emu
 *                       to send messages
 *                       bandwidth limited version (revolution 2)
 */

#ifndef VAST_NET_EMUBRIDGE_BL_H
#define VAST_NET_EMUBRIDGE_BL_H

#include "VASTTypes.h"
#include <map>
#include "net_emubridge.h"

namespace Vast {

    class net_emubridge_bl : public net_emubridge
    {
    public:
        net_emubridge_bl (int loss_rate, int fail_rate, int seed , size_t net_spc, timestamp_t init_time = 1, size_t upload_quota = 0, size_t download_quota = 0)
            : net_emubridge (loss_rate, fail_rate, seed, net_spc, init_time), default_upcap (upload_quota), default_downcap (download_quota)
        {
        }

        ~net_emubridge_bl ()
        {
        }

        id_t obtain_id (void *pointer);

        void releaseHostID (id_t id);

        // Return remaining send/recv quota for the node #node_id
        size_t get_quota (bandwidth_t type, id_t node_id);

        // Decrease quota spent after some success allocation or transmission
        bool spend_quota (bandwidth_t type, id_t node_id, size_t spent_quota);

        // Set default quota of node
        bool set_default_quota (id_t node_id, bandwidth_t type, size_t quota);

		// advance timestamp
        void tick (int tickvalue = 1);

        // Default upload/download capacity for every node
        size_t default_upcap, default_downcap;

        // Special capacity for specified node(s) (has higher priority then default_capacity)
        std::map<id_t, size_t> node_upcap, node_downcap;


    private:
        // Quota for send/recv for each node registered in net_emubridge
        std::map<id_t, size_t> _upload_quota, _download_quota;
    };
        
} // end namespace Vast

#endif // VAST_NET_EMUBRIDGE_BL_H
