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
 *                    for discovering pointers of other net_emu
 *                    to send messages
 */

#ifndef VAST_NET_EMUBRIDGE_H
#define VAST_NET_EMUBRIDGE_H

#include "typedef.h"
#include <map>

namespace VAST {

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
            //net_emubridge::_netbridge = NULL;
        }

        virtual ~net_emubridge ()
        {
        }

        // better way than using void pointer?
        virtual id_t obtain_id (void *netptr);
        virtual void register_id (id_t temp_id, id_t id);
        virtual void release_id (id_t id);
       
        void *get_netptr (id_t target);

        // obtain a timestamp a potential send should use, subject to packet loss, delay, or latency
        // (-1) indicates a failed send
        timestamp_t get_timestamp (id_t sender, id_t receiver, size_t length, bool reliable);

        // csc 20080316: network size statistics are all collected by network
        //size_t sendsize (id_t node);
        //size_t recvsize (id_t node);

        virtual void       tick               (int tickvalue = 1);
        inline timestamp_t get_curr_timestamp ()
        {
            return _time;
        }

        /*
        static net_emubridge *instance()
        {
            if (net_emubridge::_netbridge == NULL)
                net_emubridge::_netbridge = new net_emubridge ();
            return net_emubridge::_netbridge;

        }
        */
                
		// added by yuli ====================================================
        // csc 20080316: network size statistics are all collected by network
        /*
		void pass_def_data (id_t sender, id_t receiver, size_t length);
		size_t send_def_size (id_t node);
		size_t recv_def_size (id_t node);
        */
		// ==================================================================

    protected:        

        id_t                    _id_count;
        std::map<id_t, void *>  _id2ptr;

        int                     _last_seed;
        int                     _loss_rate; // between 0 - 100%
        int                     _fail_rate; // between 0 - 100%
        //int                     _latency;   // in millisecond

        //static net_emubridge *_netbridge;
        // csc 20080316: network size statistics are all collected by network
        //std::map<id_t, size_t>     _send_size;
        //std::map<id_t, size_t>     _recv_size;

		// added by yuli ===========================
        // csc 20080316: network size statistics are all collected by network
        /*
		std::map<id_t, size_t>		_send_def_size;
		std::map<id_t, size_t>		_recv_def_size;
        */
		// =========================================

		timestamp_t             _time;
    };
        
} // end namespace VAST

#endif // VAST_NET_EMUBRIDGE_H
