/*
 * VAST, a scalable peer-to-peer network for virtual environments
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
 *  peer_logic.h -- interface for handling logics of a peer
 *
 *  ver 0.1 (2006/07/18)
 *   
 */

#ifndef VASTATE_PEER_LOGIC_H
#define VASTATE_PEER_LOGIC_H

#include "shared.h"

namespace VAST 
{  
    class peer_logic
    {
    public:
        virtual ~peer_logic () {}

        // callback - any app-specific message sent to peer
        virtual void msg_received (char *msg, size_t size) = 0;

        // callback - learn about the addition/removal of objects
        virtual void obj_discovered (object *obj, bool is_self = false) = 0;
        virtual void obj_deleted (object *obj) = 0;
        
        // callback - learn about state changes of known AOI objects
        virtual void state_updated (id_t obj_id, int index, void *value, int length, version_t version) = 0;
        virtual void pos_changed (id_t obj_id, Position &newpos, version_t version) = 0;  
        
        // store access to peer class
        virtual void register_interface (void *) = 0;
    };
    
} // end namespace VAST

#endif // #define VASTATE_PEER_LOGIC_H

