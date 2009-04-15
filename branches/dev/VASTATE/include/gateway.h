
/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2008 Shao-Chen Chang (cscxcs at gmail.com)
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
 *  gateway.h - gateway interface class - gateway for system enterance
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

#ifndef _VASTATE_IGATEWAY_H
#define _VASTATE_IGATEWAY_H

#include "vastate_typedef.h"

namespace VAST {

    class gateway : public identifiable
    {
    public:
        // startup gateway
        virtual bool start (const Addr & listen_addr) = 0;

        // stop gateway
        virtual bool stop () = 0;

    protected:
        // to enforce constrution gateway from vastate
        gateway (VASTATE::id_t my_id)
            : identifiable (my_id)   {}

        virtual ~gateway ()
        {
        }
    };

}

#endif /* _VASTATE_IGATEWAY_H */

