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
 *  vastverse.h -- VAST factory (to create actual instance of VAST)     
 *
 *  ver 0.1 (2005/04/14)
 *   
 */


#ifndef VASTVERSE_H
#define VASTVERSE_H

#include "config.h"
#include "vast.h"
#include "vastid.h"
//#include "net_emubridge.h"

#include <time.h>           // time

#define VAST_MODEL_DIRECT		1
#define VAST_MODEL_FORWARD		2
#define VAST_MODEL_MULTICAST	3

#define VAST_NET_EMULATED       1
#define VAST_NET_EMULATED_BL    2
#define VAST_NET_ACE            3

namespace VAST
{
    class EXPORT vastverse
    {
    public:
        vastverse (int model, int netmode, int connlimit, int loss_rate = 0, int fail_rate = 0, 
                   size_t send_quota = 0, size_t recv_quota = 0);
        ~vastverse ();

        // obtain the unique ID for a particular node
        vastid * create_id (msghandler *node, bool is_gateway, Addr &gateway);
        void     destroy_id (vastid *id);

        // create & destroy a vast node, given the specified world condition
        vast *   create_node (unsigned int port, aoi_t detect_buffer);
        void     destroy_node (vast *node);

        // obtain a network interface
        network *create_net (unsigned int port);
        bool     destroy_net (network *net);

        // obtain a voronoi object
        voronoi *create_voronoi ();
        bool     destroy_voronoi (voronoi *v);

        // allow all currently created nodes to advance one time-step
        //void    tick (bool advance_time = true);
        void    tick ();

        // csc 20080316: all size statistics is collected by network interface
        // obtain send & receive of particular nodes
        //size_t sendsize (id_t node);
        //size_t recvsize (id_t node);

		// added by yuli ====================================================
        // csc 20080316: all size statistics is collected by network interface
		// obtain deflated send/recv msg size of particular nodes
		//size_t send_def_size (id_t node);
		//size_t recv_def_size (id_t node);
		// ==================================================================

		//net_emubridge *get_netbridge ();

    private:
        int  _vast_model;
        int  _net_model;
        int  _connlimit;
    };

} // end namespace VAST

#endif // vastverse_h
