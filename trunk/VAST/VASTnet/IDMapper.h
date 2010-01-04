/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2009  Shun-Yun Hu (syhu @ yahoo.com) 
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
 *  IDMapper.h -- maintains mapping between node IDs and physical IP addresses
 *                mainly used by VASTnet ()
 *
 *  history:    2009/04/13  init
 *
 *   
 */

#ifndef _VAST_IDMapper_H
#define _VAST_IDMapper_H

#include "VASTnet.h"

namespace Vast 
{        
    class IDMapper
    {
    
    public:
        IDMapper ()
        {
        }

        ~IDMapper ()
        {
        }

        void clear () {}

    private:


    };

} // end namespace Vast

#endif // VAST_IDMapper_H
