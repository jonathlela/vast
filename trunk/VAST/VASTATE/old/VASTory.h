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

/*
 *  VASTory.h -- factory class for VASTORY
 *
 *  ver 0.1 (2006/07/22)
 *   
 */

#ifndef VASTORY_H
#define VASTORY_H

#include "VASTATE.h"

namespace Vast 
{        
    class EXPORT vastory
    {
    public:
        
        vastory ()
        {            
        }
    	
        ~vastory () 
        {
        }

        VASTATE *create (vastverse *vastworld, Addr &gatewayIP, const system_parameter_t & sp);
        bool destroy (VASTATE *v);
    };

} // end namespace Vast

#endif // VASTORY_H


