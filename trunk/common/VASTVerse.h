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
#include "VAST.h"
#include "MessageQueue.h"
//#include "vastid.h"

#include <time.h>           // time

//#define VAST_MODEL_DIRECT		1
//#define VAST_MODEL_FORWARD		2

#define VAST_NET_EMULATED       1
#define VAST_NET_EMULATED_BL    2
#define VAST_NET_ACE            3

namespace Vast
{
    class EXPORT VASTVerse
    {
    public:
        VASTVerse (int netmode, int connlimit, int loss_rate = 0, int fail_rate = 0, 
                   size_t send_quota = 0, size_t recv_quota = 0);
        ~VASTVerse ();

        // obtain the unique ID for a particular node
        //vastid * create_id (bool is_gateway, Addr &gateway);
        //void     destroy_id (vastid *id);

        // create & destroy a vast node, given the specified world condition        
        VAST *   createNode (MessageQueue *msgqueue);
        void     destroyNode (MessageQueue *msgqueue, VAST *node);

        // obtain a network interface
        VASTnet *createNet (unsigned int port);
        bool     destroyNet (Vast::Network *net);

        // obtain message queue
        MessageQueue *createQueue (Vast::Network *net);
        bool     destroyQueue (MessageQueue *queue);

        // obtain a voronoi object
        Voronoi *createVoronoi ();
        bool     destroyVoronoi (voronoi *v);

        // allow all currently created nodes to advance one time-step
        void     tick ();

    private:
        //int  _vast_model;
        int  _net_model;
        int  _connlimit;
    };

} // end namespace Vast

#endif // vastverse_h
