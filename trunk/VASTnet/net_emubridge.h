/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005 Shun-Yun Hu (syhu@yahoo.com)
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
 * net_emubridge.h -- a shared class between various net_emu classes
 *                    to discover pointers to other net_emu endpoints and delay to them
 */

#ifndef VAST_NET_EMUBRIDGE_H
#define VAST_NET_EMUBRIDGE_H

#include "VASTTypes.h"
#include <map>

namespace Vast {


    class net_emubridge 
    {

    public:
        net_emubridge (int loss_rate, int fail_rate, int seed = 0, timestamp_t init_time = 1)
            :_loss_rate (loss_rate), _fail_rate (fail_rate), _time (init_time)
        {
            srand (seed);
            _last_seed = rand ();

            // starting from the largest possible id for assigning temp ids
            _id_count = (id_t)(-1);

        }

        virtual ~net_emubridge ()
        {
        }

        // better way than using void pointer?
        virtual id_t obtain_id (void *netptr);
        virtual void registerID (id_t temp_id, id_t id);
        virtual void release_id (id_t id);
       
        void *getNetworkInterface (id_t target);

        // obtain the arrived timestamp of a packet, subject to packet loss or latency
        // (-1) indicates a failed send
        timestamp_t getArrivalTime (id_t sender, id_t receiver, size_t length, bool reliable);

        virtual void       tick               (int tickvalue = 1);
        
        inline timestamp_t getTimestamp ()
        {
            return _time;
        }

    protected:        

        id_t                    _id_count;  
        std::map<id_t, void *>  _id2ptr;    // pointers to other stored net_emu classes          

        int                     _last_seed;
        int                     _loss_rate; // between 0 - 100%
        int                     _fail_rate; // between 0 - 100%

		timestamp_t             _time;
    };
        
} // end namespace Vast

#endif // VAST_NET_EMUBRIDGE_H
