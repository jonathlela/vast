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
 *  VASTVerse.h -- VAST factory (to create actual instance of VAST)
 *
 *  history:    2005/04/14      ver 0.1
 *              2009/04/28      refactored for SPS-based VAST
 */


#ifndef VASTVERSE_H
#define VASTVERSE_H

#include "Config.h"
#include "VAST.h"           // provides spatial publish / subscribe (SPS)
#include "Topology.h"       // provides physical coordinate, IP address, and public IP


namespace Vast
{
    typedef enum 
    {
        VAST_NET_EMULATED = 1,
        VAST_NET_EMULATED_BL,
        VAST_NET_ACE
    } VAST_NetModel;

    struct VASTPara_Net
    {
        VAST_NetModel   model;          // network model
        Addr            gateway;        // IP address to gateway
        bool            is_gateway;     // whether current node is gateway
		bool			is_relay;
        unsigned short  port;           // default port to use   
        Position        phys_coord;     // physical coordinate (optional)
        int             peer_limit;     // max number of peers hosted on a relay
        int             relay_limit;    // max number of relays each node maintains
        int             conn_limit;     // connection limit
        int             step_persec;   // step/ sec (network layer)
        size_t          send_quota;     // upload quota (bandwidth limit)
        size_t          recv_quota;     // download quota (bandwidth limit)        
    };

    struct VASTPara_Sim
    {
        int     loss_rate;      // packet loss rate
        int     fail_rate;      // node fail rate
        bool    with_latency;   // latency among nodes' connection
    };

    // the main factory to create VAST nodes and join the overlay
    //
    // NOTE that VASTVerse creates some internal message handlers, including:
    //
    //          1) ID generator                 local ID: 0
    //          2) Topology finder              local ID: 1
    //          3) VAST node (client & relay)   local ID: 2
    //
    //      so other message handlers have to use getUniqueID () to obtain
    //      a globally unique handlerID first before creating the message handler
    //      and hook to an existing handler (such as VAST)
    //
    
    // TODO: need to cleanup current implementation (too ugly for hiding internal classes from user)

    class EXPORT VASTVerse
    {
    public:
        VASTVerse (VASTPara_Net *netpara, VASTPara_Sim *simpara);
        ~VASTVerse ();

        // whether we're now ready to create clients
        bool isLogined ();

        // obtain my virtual coordinate in the physical Internet
        // return NULL if not yet ready
        //Position *getPhysicalCoordinate ();

        // obtain topology class
        Topology *getTopology ();

        // create & destroy a VAST client
        VAST *   createClient ();
        bool     destroyClient (VAST *node);

        // obtain & destory a Voronoi object
        Voronoi *createVoronoi ();
        bool     destroyVoronoi (Voronoi *v);

        // advance one time-step for all nodes to process messages  
        // NOTE: time_budget right now is ignored
        void     tick (int time_budget = 0);

        // stop operations on this node
        void     pause ();

        // resume operations on this node
        void     resume ();

        // obtain the tranmission size by message type, default is to return all types
        size_t getSendSize (const msgtype_t msgtype = 0);
        size_t getReceiveSize (const msgtype_t msgtype = 0);

        // record nodeID on the same host
        void recordLocalTarget (id_t target);

    private:

        VASTPara_Net        _netpara;
        VASTPara_Sim        _simpara;
        void               *_pointers;
        bool                _logined;
    };

} // end namespace Vast

#endif // VASTVerse_h
