/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2010  Shun-Yun Hu (syhu@ieee.org)
 *                          Shao-Chen Chang (cscxcs at gmail.com)
 *
 * This library is free software; you scan redistribute it and/or
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
 * net_emubridge_bl.cpp -- virtual bridege implementation between various net_emu_bl classes
 *                         bandwidth limited version (revolution 2)
 */

#include "net_emubridge_bl.h"

namespace Vast
{
    id_t 
    net_emubridge_bl::
    obtain_id (void *pointer)
    {
        id_t new_id = net_emubridge::obtain_id (pointer);

        _upload_quota [new_id] = default_upcap;
        _download_quota [new_id] = default_downcap;

        return new_id;
    }

    void 
    net_emubridge_bl::
    releaseHostID (id_t id)
    {
        net_emubridge::releaseHostID (id);

        // clear up bandwidth limitation related variables
        _upload_quota.erase (id);
        _download_quota.erase (id);
        node_upcap.erase (id);
        node_downcap.erase (id);
    }

    size_t
    net_emubridge_bl::
    get_quota (bandwidth_t type, id_t node_id)
    {
        std::map<id_t, size_t> * target_quota = NULL;

        if (type == BW_UPLOAD)
            target_quota = & _upload_quota;
        else if (type == BW_DOWNLOAD)
            target_quota = & _download_quota;

        if (target_quota == NULL)
            return 0;

        if (target_quota->find (node_id) != target_quota->end ())
            return target_quota->operator [] (node_id);
        else
            return 0;
    }

    bool
    net_emubridge_bl::
    spend_quota (bandwidth_t type, id_t node_id, size_t spent_quota)
    {
        // get correct map of quota
        std::map<id_t, size_t> * target_quota = NULL;

        if (type == BW_UPLOAD)
            target_quota = & _upload_quota;
        else if (type == BW_DOWNLOAD)
            target_quota = & _download_quota;

        if (target_quota == NULL)
            return false;

        std::map<id_t, size_t> & tq = *target_quota;

        if (tq.find (node_id) == tq.end ()
            || tq[node_id] < spent_quota)
            return false;

        tq[node_id] -= spent_quota;
        return true;
    }

    bool 
    net_emubridge_bl::
    set_default_quota (id_t node_id, bandwidth_t type, size_t quota)
    {
        std::map<id_t, size_t> * target_quota = NULL;

        if (type == BW_UPLOAD)
            target_quota = & node_upcap;
        else if (type == BW_DOWNLOAD)
            target_quota = & node_downcap;

        if (target_quota == NULL)
            return false;

        target_quota->operator [] (node_id) = quota;

        return true;
    }

    void 
    net_emubridge_bl::
    tick (int tickvalue)
    {
        net_emubridge::tick (tickvalue);

        // reset send/recv quota for this step
        // for each node registered in _id2ptr list
        std::map<id_t, void *>::iterator it = _id2ptr.begin ();
        for (; it != _id2ptr.end (); it ++)
        {
            id_t nodeid = it->first;

            if (node_upcap.find (nodeid) != node_upcap.end ())
                _upload_quota [nodeid] = node_upcap [nodeid];
            else
                _upload_quota [nodeid] = default_upcap;

            if (node_downcap.find (nodeid) != node_downcap.end ())
                _download_quota [nodeid] = node_downcap [nodeid];
            else
                _download_quota [nodeid] = default_downcap;
        }
    }
} // end namespace Vast

