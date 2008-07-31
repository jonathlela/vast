
/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2008 Shao-Jhen Chang (cscxcs at gmail.com)
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
 *  State Management for VAST
 *  gateway_impl.h - gateway class - gateway for system enterance
 *
 */

/*
 *  class structure:
 *  gateway    <--- gateway_impl
 *  msghandler <-|
 *
 *  msghandler from network model of vast
 *
 */

#ifndef _VASTATE_GATEWAYIMPL_H
#define _VASTATE_GATEWAYIMPL_H

#include "precompile.h" // precompile header of VASTATE
#include "vastate_typedef.h"
#include "shared.h"
#include "gateway.h"

namespace VAST {

#define GATEWAY_DEFAULT_POSITION (VAST::Position(0, 0))

    // pre-declaration for friend class declare
    class vastate_impl;

    class gateway_impl : public gateway, public msghandler
    {
    public:
        // startup gateway
        bool start (const Addr & listen_addr);

        // stop gateway
        bool stop ();

        // members derived from msghandler
        // returns whether the message has been handled successfully
        bool handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size);

        // do things after messages are all handled
        void post_processmsg ();

    protected:
        // to enforce con/destruction through vastory
        friend class vastate_impl;
        gateway_impl (id_t my_id, 
                      VAST::vastverse & v, VAST::network *net, const system_parameter_t & sp);
        ~gateway_impl ();

    private:
        // input parameters
        // reference of vastworld to create vastid/vnode/net
        VAST::vastverse             & _vastworld;
        // network interface
        VAST::network               * _net;
        // system parameters
        system_parameter_t            _sp;
        // listening address
        VAST::Addr                    _listen_addr;

        // variable members
        // node id
        VASTATE::id_t                 _id;
        // overlay network 
        VAST::vast                  * _overlay;

    };

} /* namespace VAST */

#endif /* _VASTATE_GATEWAYIMPL_H */

