/*
 * VAST, a scalable agent-to-Agent network for virtual environments
 * Copyright (C) 2007 Shun-Yun Hu (syhu@yahoo.com)
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

#include "VASTory.h"
#include "VASTATEImpl.h"

namespace Vast
{
    VASTATE *
    vastory::create (vastverse *vastworld, Addr &gatewayIP, const system_parameter_t & sp)
    {
        return new VASTATE_impl (vastworld, gatewayIP, sp);
    }

    bool 
    vastory::destroy (VASTATE *v)
    {
        delete v;
        return true;
    }
    
} // namespace Vast