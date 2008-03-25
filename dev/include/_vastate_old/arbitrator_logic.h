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
 *  arbitrator_logic.h -- interface for handling the logics for an arbitrator
 *
 *  ver 0.1 (2006/07/18)
 *   
 */

#ifndef VASTATE_ARBITRATOR_LOGIC_H
#define VASTATE_ARBITRATOR_LOGIC_H

#include "shared.h"

namespace VAST 
{  

    class arbitrator_logic
    {
    public:
        
        // callback - for authenaticing a newly joined peer
        virtual bool join_requested (id_t from_id, char *data, int sizes) = 0;

        // callback - for processing a query returned by the shared storage
        //virtual bool query_returned (query_id_t query_id, char *data, int size) = 0;

        // callback -  for receiving a remote event (arbitrator only)
        virtual bool event_received (id_t from_id, event &e) = 0;
        //virtual bool event_process  (id_t from_id, event &e) = 0;
        
        // callback - by remote arbitrator to create or initialize objects
        virtual void obj_created    (object *obj, void *ref, size_t size) = 0;
        virtual void obj_discovered (object *obj) = 0;
        virtual void obj_deleted    (object *obj) = 0;
        
        // callback - by remote arbitrator to notify their object states have changed
        virtual void state_updated (id_t obj_id, int index, void *value, int length, version_t version) = 0;
        virtual void pos_changed (id_t obj_id, Position &newpos, version_t version) = 0;        
        
        // callback -  to learn about creation of avatar objects
        //virtual void peer_entered (Node &peer) = 0;
        //virtual void peer_left (Node &peer) = 0;    
        
        // callback - to trigger any arbitrator logic (not provoked by events)
        virtual void tick () = 0; 

        // store access to arbitrator class
        // NOTE: the app-specific implementation of arbitrator_logic must
        //       store a pointer to an 'arbitrator' object as private variable
        virtual void register_interface (void *) = 0;         

        // store access to storage class
        // NOTE: the app-specific implementation of arbitrator_logic must
        //       store a pointer to an 'storage' object as private variable
        //virtual void register_storage (void *) = 0;
    };

} // end namespace VAST

#endif // #define VASTATE_ARBITRATOR_LOGIC_H