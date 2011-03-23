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
 *  VASTVerse.h -- VAST factory (to create actual instance of VAST)
 *
 *  history:    2005/04/14      ver 0.1
 *              2009/04/28      refactored for SPS-based VAST
 *              2010/01/28      IDGenerator moved into VASTnet
 */


#ifndef VASTVERSE_H
#define VASTVERSE_H
/*
// for running a separate thread that executes tick ()
#include "ace/ACE.h"
#include "ace/OS.h"
#include "ace/Task.h"
#include "ace/Reactor.h"
#include "ace/Condition_T.h"        // ACE_Condition
*/

#include "Config.h"
#include "VAST.h"           // provides spatial publish / subscribe (SPS)
#include "VASTRelay.h"      // provides physical coordinate, IP address, and public IP
#include "VASTCallback.h"   // callback for handling incoming message at a VAST node
#include "VASTnet.h"

#define VASTVERSE_RETRY_PERIOD  (10)     // # of seconds if we're stuck in a state, revert to the previous

namespace Vast
{
    class VASTPara_Net
    {
    public:

        // set default values
        VASTPara_Net (VAST_NetModel netmodel)
        {
            this->model     = netmodel;
            is_entry        = true;
            is_relay        = true;
            is_matcher      = true;
            is_static       = false;
            port            = 0;
            client_limit    = 0;
            relay_limit     = 0;
            overload_limit  = 20;
            conn_limit      = 0;
            send_quota      = 0;
            recv_quota      = 0;
        }

        VAST_NetModel   model;          // network model
        bool            is_entry;       // whether current node is an entry point to the overlay (assigns ID, determine physical coordinates)
	    bool            is_relay;       // whether this node should join as a relay (may not succeed depend on public IP is available)
        bool            is_matcher;     // whether this node should join as a candidate matcher (need to be a relay as well)       
        bool            is_static;      // whether static partitioning is used
        unsigned short  port;           // default port to use 
        Position        phys_coord;     // default physical coordinate (optional)
        Position        matcher_coord;  // default matcher join coordinate (optional)        
        int             client_limit;   // max number of clients connectable to this relay
        int             relay_limit;    // max number of relays each node maintains
        int             overload_limit; // max number of subscriptions at each matcher
        int             conn_limit;     // connection limit
        size_t          send_quota;     // upload quota (bandwidth limit)
        size_t          recv_quota;     // download quota (bandwidth limit)        
    };

    struct VASTPara_Sim
    {
        int     step_persec;    // step/ sec (simulated network layer)
        int     loss_rate;      // packet loss rate
        int     fail_rate;      // node fail rate
        bool    with_latency;   // latency among nodes' connection
    };

    // the main factory to create VAST nodes and join the overlay
    //
    // NOTE that VASTVerse creates some internal message handlers, including:
    //
    //          1) ID generator                 local ID: 0
    //          2) VASTRelay finder              local ID: 1
    //          3) VAST node (client & relay)   local ID: 2
    //
    //      so other message handlers have to use getUniqueID () to obtain
    //      a globally unique handlerID first before creating the message handler
    //      and hook to an existing handler (such as VAST)
    //
    
    // TODO: need to cleanup current implementation (too ugly for hiding internal classes from user)
    
    // NOTE: a general rule: avoid STL objects passing across DLL boundaries
    class EXPORT VASTVerse 
    {
    friend class VASTThread;

    public:

        // specify a number of entry points (hostname / IP) to the overlay, 
        // also the network & simulation parameters
        VASTVerse (bool is_gateway, const string &GWstr, VASTPara_Net *netpara, VASTPara_Sim *simpara, VASTCallback *callback = NULL, int tick_persec = 0);
        ~VASTVerse ();
        
        // NOTE: to run a gateway-like entry point only, there's no need to call
        //       createVASTNode (), as long as isInitialized () returns success
       
        // check if we're ready to create a VASTNode
        // (all init and the creation of Relay, Matcher are ready)
        bool isInitialized ();

        // to add entry points for this VAST node (should be called before createVASTNode)
        // format is "IP:port" in string, returns the number of successfully added entries
        //int addEntries (std::vector<std::string>);            

        // create & destroy a VASTNode
        // currently only supports one per VASTVerse
        bool createVASTNode (world_t world_id, Area &area, layer_t layer);
        bool destroyVASTNode (VAST *node);

        // obtain a reference to the created VASTNode
        VAST *getVASTNode ();       

        // TODO: support this function? so clients can enter a different world
        //       without having to destroyClient?
        // bool switchWorld (IPaddr &gateway);

        // advance one time-step for all nodes to process messages  
        // input time budget for this tick in microseconds, 10^-6, specify 0 for unlimited budget
        // returns time left in microseconds, 0 for no more time, (-1) for unlimited budget
        // 'per_sec' indicates whether per-second tasks were performed 
        // NOTE: currently only (-1) would return
        int     tick (int time_budget = 0, bool *per_sec = NULL);

        // move logical clock forward (mainly for simulation purpose, but also records per-second tasks)
        void    tickLogicalClock ();

        // stop operations on this node
        void    pauseNetwork ();

        // resume operations on this node
        void    resumeNetwork ();


        //
        // socket messaging
        //

        // open a new TCP socket
        id_t openSocket (IPaddr &ip_port, bool is_secure = false);

        // close a TCP socket
        bool closeSocket (id_t socket);

        // send a message to a socket
        bool sendSocket (id_t socket, const char *msg, size_t size);

        // receive a message from socket, if any
        // returns the message in byte array, and the socket_id, message size, NULL for no messages
        // NOTE: the returned data is valid until the next call to receiveSocket
        char *receiveSocket (id_t &socket, size_t &size);

        //
        //  accessors & state check
        //

        // get current timestamp from host machine (millisecond since 1970)
        timestamp_t getTimestamp ();

        // obtain access to Voronoi class of the matcher (usually for drawing purpose)
        // returns NULL if matcher does not exist on this node
        Voronoi *getMatcherVoronoi ();

        // obtain the matcher's adjustable AOI radius, returns 0 if no matcher exists
        Area *getMatcherAOI ();

        // whether I am a matcher node
        bool isMatcher ();

        // whether I am a gateway node
        bool isGateway ();

        //
        // stat collection
        //

        // obtain the number of active connections at this node
        int getConnectionSize ();

        // obtain the tranmission size by message type, default is to return all types
        StatType &getSendStat (bool interval_only = false);
        StatType &getReceiveStat (bool interval_only = false);

        // reset stat collection for a particular interval, however, accumulated stat will not be cleared
        void    clearStat ();

        // record nodeID on the same host
        void    recordLocalTarget (id_t target);

        //
        // misc tools
        //

        // obtain gateway's IP & port
        IPaddr &getGateway ();

        // translate a string-based address into Addr object
        static Addr *translateAddress (const string &addr);

    private:

        // create & destroy a VAST client
        VAST *   createClient (const IPaddr &gateway, world_t world_id);
        bool     destroyClient (VAST *node);

        // obtain & destory a Voronoi object
        //Voronoi *createVoronoi ();
        //bool     destroyVoronoi (Voronoi *v);

        NodeState           _state;
        VASTPara_Net        _netpara;
        VASTPara_Sim        _simpara;
        void               *_pointers;      // pointers to VASTPointer
        
        int                 _timeout;       // # of ticks before we consider current state timeout

        vector<Subscription> _vastinfo;     // info about a VASTNode to be created
        vector<IPaddr>      _entries;       // entry points for the overlay
        IPaddr              _gateway;       // gateway for entering the VAST network
        bool                _GWconnected;   // whether I've connected to gateway

        // stat collection class
        timestamp_t         _next_periodic; // next timestamp when per-second task is executed
        StatType            _sendstat;      // stat on send size
        StatType            _recvstat;      // stat on recv size

        StatType            _sendstat_interval;      // stat on send size per interval
        StatType            _recvstat_interval;      // stat on recv size per interval

        StatType            _connstat;      // stat on connection size

        size_t              _lastsend;      // last accumulated send bytes
        size_t              _lastrecv;      // last accumulated recv bytes
    };

} // end namespace Vast

#endif // VASTVerse_h
