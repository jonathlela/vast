
/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007 Shun-Yun Hu (syhu@yahoo.com)
 *               2008 Shao-Jhen Chang (cscxcs at gmail.com)
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
 *  VSM Implementation: vast-state (vastate)
 *  gateway_impl.h - gateway class - gateway for system enterance
 *
 *  ver. 20080520  Csc
 */

/*
 *  class structure:
 *  gateway    <--- gateway_impl
 *  msghandler <-|
 *
 *  msghandler from network model of vast
 *
 */

#include "precompile.h"
#include "gateway_impl.h"

namespace VAST {

    gateway_impl::gateway_impl 
        (id_t my_id, 
         VAST::vastverse & v, VAST::network *net, const system_parameter_t & sp)
        : gateway (my_id), _vastworld (v), _net (net), _sp (sp)
        , _overlay (NULL)
    {
    }

    gateway_impl::~gateway_impl ()
    {
        if (_overlay != NULL)
            stop ();
    }

    // startup gateway
    bool gateway_impl::start (const Addr & listen_addr)
    {
        if (_overlay != NULL)
            return true;

        if ((_overlay = _vastworld.create_node (_net, 20)) == NULL)
            return false;

        VAST::Addr la = listen_addr;
        _overlay->join (get_id (), _sp.aoi, GATEWAY_DEFAULT_POSITION, la);

        return true;
    }

    // stop gateway
    bool gateway_impl::stop ()
    {
        if (_overlay != NULL)
        {
            _vastworld.destroy_node (_overlay);
            _overlay = NULL;
        }

        return true;
    }

    // returns whether the message has been handled successfully
    bool gateway_impl::
        handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size)
    {
        return false;
    }

    // do things after messages are all handled
    void gateway_impl::
        post_processmsg ()
    {
    }

} /* namespace VAST */


