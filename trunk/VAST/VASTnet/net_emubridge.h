/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2011 Shun-Yun Hu (syhu@ieee.org)
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
#include "Vivaldi.h"
#include <map>
#include <math.h>
namespace Vast {

    class EXPORT net_emubridge 
    {

    public:

		net_emubridge (int loss_rate, int fail_rate, int seed, size_t net_spsc, timestamp_t init_time)
			:_loss_rate (loss_rate), _fail_rate (fail_rate), _time (init_time), _net_step_per_sec(net_spsc)
		{
			//srand (seed);
			_last_seed = rand ();

            _id_count = 1;
		}

        virtual ~net_emubridge ()
        {
        }

        // better way than using void pointer?
        virtual id_t obtain_id (void *netptr);
        
        virtual void replaceHostID (id_t temp_id, id_t id);
        
        virtual void releaseHostID (id_t id);
       
        void *getNetworkInterface (id_t target);

        // obtain the arrived timestamp of a packet, subject to packet loss or latency
        // (-1) indicates a failed send
        timestamp_t getArrivalTime (id_t sender, id_t receiver, size_t length, bool reliable);

        virtual void tick(int tickvalue = 1);
        
        inline timestamp_t getTimestamp ()
        {
            return _time;
        }

		inline Vivaldi* getVivaldi () const { return _vivaldi; }
		void setVivaldi (Vivaldi* val) { _vivaldi = val; }		

    protected:        

        id_t                    _id_count;  
        std::map<id_t, void *>  _id2ptr;    // pointers to other stored net_emu classes          

        int                     _last_seed;
        int                     _loss_rate; // between 0 - 100%
        int                     _fail_rate; // between 0 - 100%		
		timestamp_t             _time;

		Vivaldi*				_vivaldi;   // for latency		
		size_t                  _net_step_per_sec;
	};
        
} // end namespace Vast

#endif // VAST_NET_EMUBRIDGE_H
