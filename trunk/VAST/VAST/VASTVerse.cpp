/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2009 Shun-Yun Hu (syhu@yahoo.com)
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

#include "VASTVerse.h"

#include <vector>
#include <map>
#include <time.h>           // time

#include "MessageQueue.h"
//#include "Relay.h"
//#include "IDGenerator.h"
#include "VASTRelay.h"
#include "VASTClient.h"
#include "VASTMatcher.h"
#include "VoronoiSF.h"
#include "net_emu.h"     // TODO: cleaner way?
#include "net_emu_bl.h"     // TODO: cleaner way?

#ifndef ACE_DISABLED
#include "net_ace.h"
#endif



namespace Vast
{  
	
    // currently we assume there's only one globally accessible
    // netbridge (for simulation only)
    // TODO: once created, it will not ever be released until the program terminates
    //       a better mechanism?
    static net_emubridge *g_bridge     = NULL;
    int            g_bridge_ref = 0;            // reference count for the bridge
    //int            g_bridge_tickcount = 0;      // countdown to next tick
   
    class VASTPointer
    {
    public:
        VASTPointer ()
        {
            net         = NULL;
            msgqueue    = NULL;
            client      = NULL;
            relay       = NULL;
            matcher     = NULL;
        }

        VASTnet *       net;            // network interface
        MessageQueue *  msgqueue;       // message queue for managing mesage handlers
        VAST *          client;         // clients that enter an overlay
        VASTRelay *     relay;          // physical coordinate locator & joiner
        VASTMatcher *   matcher;        // a relay node to the network
    };

    //
    //  Internal helper functions
    //

    // TODO: better interface?
    VAST *
    createNode (MessageQueue *msgqueue, VASTPointer *node)
    {
        VASTClient *vnode;
        
        if ((vnode = new VASTClient ((VASTRelay *)node->relay)) == NULL)
            return NULL;

        node->msgqueue->registerHandler (vnode);

        return vnode;
    }

    bool 
    destroyNode (MessageQueue *msgqueue, VAST *node)
    {
        if (node == NULL || msgqueue == NULL)
            return false;

        // release memory
        if (node != NULL)
        {                   
            // unregister from message queue
            msgqueue->unregisterHandler ((VASTClient *)node);            
            delete (VASTClient *)node;
        }
        return true;
    }
    
    VASTnet *
    createNet (unsigned short port, VASTPara_Net &para, vector<IPaddr> &entries)
    {
        if (para.step_persec == 0)
        {
            printf ("VASTnet::createNet () VASTPara_Net's step_persec == 0, error\n");
            return NULL;
        }

        VASTnet *net = NULL;
        if (para.model == VAST_NET_EMULATED)
            net = new net_emu (*g_bridge);
		else if (para.model == VAST_NET_EMULATED_BL)					
			net = new net_emu_bl (*(net_emubridge_bl *)g_bridge);			
		
#ifndef ACE_DISABLED
        else if (para.model == VAST_NET_ACE)
        {
            printf ("VASTnet::createNet, creating net_ace at port: %d\n", port);
            net = new net_ace (port);
        }
#endif        

        net->addEntries (entries);
        net->setTimestampPerSecond (para.step_persec);

		// set the bandwidth limitation after the net is created
		if (para.model == VAST_NET_EMULATED_BL)
		{
			Bandwidth bw;

			bw.UPLOAD   = para.send_quota;
			bw.DOWNLOAD = para.recv_quota;				

			net->setBandwidthLimit (BW_UPLOAD,   bw.UPLOAD  / para.step_persec);
			net->setBandwidthLimit (BW_DOWNLOAD, bw.DOWNLOAD/ para.step_persec);
		}

        return net;
    }

    bool 
    destroyNet (VASTnet *net, VASTPara_Net &para)
    {
        if (net == NULL)
            return false;
        else if (para.model == VAST_NET_EMULATED)
            delete (net_emu *) net;
		else if (para.model == VAST_NET_EMULATED_BL)
			delete (net_emu_bl *) net;
#ifndef ACE_DISABLED
        else if (para.model == VAST_NET_ACE)
        {
            // force close (to terminate thread)
            ((net_ace *) net)->close (0);
            delete (net_ace *) net;
        }
#endif
        else
            return false;

        return true;
    }
    
    MessageQueue *
    createQueue (VASTnet *net)
    {
        return new MessageQueue (net);
    }

    bool
    destroyQueue (MessageQueue *queue)
    {
        if (queue == NULL)
            return false;
        
        delete queue;
        return true;
    }

    VASTVerse::
    VASTVerse (vector<IPaddr> &entries, VASTPara_Net *netpara, VASTPara_Sim *simpara)
    {
        _state = ABSENT;
        _lastsend = _lastrecv = 0;
        _next_periodic = 0;

        _logined = false;

        // make the local copy of the parameters
        _entries = entries;
        _netpara = *netpara;

        if (simpara != NULL)
            _simpara = *simpara;
        else
            // use 0 as defaults for all values
            memset (&_simpara, 0, sizeof (VASTPara_Sim)); 

        // zero all pointers
        _pointers = new VASTPointer ();
        memset (_pointers, 0, sizeof (VASTPointer));

        // initialize rand generator (for node fail simulation, NOTE: same seed is used to produce exactly same results)
        //srand ((unsigned int)time (NULL));
        srand (0);

        if (g_bridge == NULL)
        {
            // create a shared net-bridge (used in simulation to locate other simulated nodes)
            // NOTE: g_bridge may be shared across different VASTVerse instances   
            if (_netpara.model == VAST_NET_EMULATED)                            
                g_bridge = new net_emubridge (_simpara.loss_rate, _simpara.fail_rate, 1, _netpara.step_persec, 1);           
		    else if (_netpara.model == VAST_NET_EMULATED_BL)		
			    g_bridge = new net_emubridge_bl (_simpara.loss_rate, _simpara.fail_rate, 1, _netpara.step_persec);        
        }
        g_bridge_ref++;
    }

    VASTVerse::~VASTVerse ()
    {
        VASTPointer *handlers = (VASTPointer *)_pointers;

        // delete those vast_nodes not yet deleted
        if (handlers->client != NULL)
        {
            destroyNode (handlers->msgqueue, handlers->client);
            handlers->client = NULL;
        }

        if (handlers->matcher != NULL)
        {
            delete handlers->matcher;
            handlers->matcher = NULL;
        }

        if (handlers->relay != NULL)
        {
            delete handlers->relay;
            handlers->relay = NULL;
        }

        if (handlers->msgqueue != NULL)
        {
            destroyQueue (handlers->msgqueue);
            handlers->msgqueue = NULL;
        }

        if (handlers->net != NULL)
        {
            destroyNet (handlers->net, _netpara);
            handlers->net = NULL;
        }
        
        g_bridge_ref--;

        // only delete the bridge if no other VASTVerse's using it
        if (g_bridge != NULL && g_bridge_ref == 0)
        {
            delete g_bridge;
            g_bridge = NULL;
        }

        delete (VASTPointer *)_pointers;

        _logined = false;
    }

    // create & destroy a VASTNode
    bool  
    VASTVerse::createVASTNode (const IPaddr &gateway, Area &area, layer_t layer)
    {
        // right now can only create one
        if (_vastinfo.size () != 0)
            return false;

        Subscription info;
        size_t id = _vastinfo.size () + 1; 

        // store info about the VASTNode to be created
        // NOTE we store gateway's info in relay
        info.relay.publicIP = gateway; 
        info.aoi = area;
        info.layer = layer;
        info.id = id;

        _vastinfo.push_back (info);

        return true;
    }

    bool 
    VASTVerse::destroyVASTNode (VAST *node)
    {
        if (_vastinfo.size () == 0)
            return false;

        _vastinfo.clear ();
      
        return destroyClient (node);
    }

    // obtain a reference to the created VASTNode
    VAST *
    VASTVerse::getVASTNode ()
    {
        // error check
        if (_vastinfo.size () == 0)
        {
            printf ("VASTVerse::getVASTNode () attempt to get VASTNode without first calling createVASTNode ()\n");
            return NULL;
        }

        VASTPointer *handlers = (VASTPointer *)_pointers;
        Subscription &info = _vastinfo[0];

        // create the VAST node
        if (_state == ABSENT)
        {            
            if ((createClient (info.relay.publicIP)) != NULL)
            {                            
                printf ("VASTVerse::getVASTNode () client created\n");
                _state = JOINING;
            }
        }
        else if (_state == JOINING)
        {
            if (handlers->client->isJoined ())
            {
                printf ("VASTVerse::getVASTNode () subscribing ... \n");
                handlers->client->subscribe (info.aoi, info.layer);
                _state = JOINING_2;
            }
        }
        else if (_state == JOINING_2)
        {
            if (handlers->client->getSubscriptionID () != NET_ID_UNASSIGNED)
            {
                printf ("VASTVerse::getVASTNode () ID obtained, joined\n");
                _state = JOINED;
            }
        }

        if (_state == JOINED)
            return handlers->client;
        else
            return NULL;
    }

    // whether this VASTVerse is initialized to create VASTNode instances
    bool 
    VASTVerse::isLogined ()
    {
        if (_logined == true)
            return true;

        VASTPointer *handlers = (VASTPointer *)_pointers;

        Position *physcoord = NULL;     // physical coordinate obtained

        //
        // initialize various necessary functions of a VAST factory
        //

        // create the basic network layer & message queue
        if (handlers->net == NULL)
        {
            printf ("VASTVerse::isLogined () creating VASTnet...\n");

            // create network layer
            handlers->net = createNet (_netpara.port, _netpara, _entries);
            if (handlers->net == NULL)
                return false;

            printf ("VASTVerse::isLogined () creating MessageQueue...\n");
            handlers->msgqueue = createQueue (handlers->net);
        }

        // make sure the network layer is joined properly
        else if (handlers->net->isJoined () == false)
            return false;

        // after we get unique ID, obtain physical coordinate
        else if (handlers->relay == NULL)
        {
            // create the Relay node and store potential overlay entries 
            printf ("VASTVerse::isLogined () creating VASTRelay...\n");

            id_t id = handlers->net->getHostID ();

            // check if unique hostID is obtained
            // NOTE we will try to determine physical coordinate only after unique ID is gotten
            if (id != NET_ID_UNASSIGNED)
            {
                printf ("unique ID obtained [%llu]\n", id);
                printf ("if this hangs, check if physical coordinate is obtained correctly\n");

                bool hasPhysCoord = !(_netpara.phys_coord.x == 0 && _netpara.phys_coord.y == 0);
                handlers->relay = new VASTRelay (_netpara.is_relay, _netpara.client_limit, _netpara.relay_limit, hasPhysCoord ? &_netpara.phys_coord : NULL);

                handlers->msgqueue->registerHandler (handlers->relay);
            }
        }
        // NOTE: if physical coordinate is not supplied, the login process may pause here indefinitely
        else if (handlers->relay->isJoined () == false)
            return false;

        // create matcher instance (though it may not be used)
        else if (handlers->matcher == NULL)
        {             
            // relay has just been properly created, get physical coordinates
            physcoord = handlers->relay->getPhysicalCoordinate ();
            printf ("[%llu] physical coord: (%.3f, %.3f)\n", handlers->net->getHostID (), physcoord->x, physcoord->y);

            // create (idle) 'matcher' instance
            handlers->matcher = new VASTMatcher (_netpara.is_matcher, _netpara.overload_limit);
            handlers->msgqueue->registerHandler (handlers->matcher);

            _logined = true;
        }
   
        return _logined;
    }

    VAST *
    VASTVerse::createClient (const IPaddr &gateway)
    {
        if (isLogined () == false)
            return NULL;

        VASTPointer *handlers = (VASTPointer *)_pointers;

        // avoid double-creation
        if (handlers->client != NULL)
            return handlers->client;

        // make sure matcher has joined first
        if (handlers->matcher->isJoined () == false)
        {
            handlers->matcher->join (gateway);           
            return NULL;
        }

        VAST *vnode = createNode (handlers->msgqueue, (VASTPointer *)_pointers);

        // perform join for the client 
        if (vnode != NULL)
        {
            vnode->join (gateway);
            handlers->client = vnode;
        }
        
        return vnode;
    }

    bool     
    VASTVerse::destroyClient (VAST *node)
    {
        VASTPointer *handlers = (VASTPointer *)_pointers;
        bool result = destroyNode (handlers->msgqueue, node);
        handlers->client = NULL;
        return result;
    }

    /*
    Voronoi *
    VASTVerse::createVoronoi ()
    {
        return new VoronoiSF ();
    }

    bool
    VASTVerse::destroyVoronoi (Voronoi *v)
    {
        if (v == NULL)
            return false;

        delete (VoronoiSF *)v;
        return true;
    }
    */

    // advance one time-step 
	int
    VASTVerse::tick (int time_budget)
    {        
        VASTPointer *handlers = (VASTPointer *)_pointers;

        // if no time is available
        if (time_budget < 0)
            time_budget = 0;

        // perform routine procedures for each logical time-step
        if (handlers->msgqueue != NULL)
            handlers->msgqueue->tick ();
           
        // perform per-second tasks
        timestamp_t now = handlers->net->getTimestamp ();
        if (now >= _next_periodic)
        {
            _next_periodic = now + handlers->net->getTimestampPerSecond ();
        
            // record network stat for this node
            size_t size = handlers->net->getSendSize (0);            
            _sendstat.addRecord (size - _lastsend);
            _lastsend = size;
        
            size = handlers->net->getReceiveSize (0);
            _recvstat.addRecord (size - _lastrecv);            
            _lastrecv = size;
        }

        // right now there's always time available
        return time_budget;
    }

    // move logical clock forward
    void  
    VASTVerse::tickLogicalClock ()
    {
        // increase tick globally when the last node has processed its messages for this round
        // this will prevent its message be delayed one round in processing by other nodes
        // TODO: find a better way? 
        if (g_bridge != NULL)
        {
            g_bridge->tick ();
        }      
    }

    // stop operations on this node
    void     
    VASTVerse::pause ()
    {
        VASTPointer *handlers = (VASTPointer *)_pointers;
        if (handlers->net != NULL)
        {
            //handlers->net->flush ();
            handlers->net->stop ();			
        }
    }

    // resume operations on this node
    void     
    VASTVerse::resume ()
    {
        VASTPointer *handlers = (VASTPointer *)_pointers;
        if (handlers->net != NULL)
            handlers->net->start ();
    }

    // obtain access to Voronoi class (usually for drawing purpose)
    // returns NULL if matcher does not exist on this node
    Voronoi *
    VASTVerse::getMatcherVoronoi ()
    {
        if (isLogined () == false)
            return NULL;

        VASTPointer *handlers = (VASTPointer *)_pointers;

        // if there's a joined matcher on this node, then we should have a Voronoi map
        if (handlers->matcher->isJoined ())
            return handlers->matcher->getVoronoi ();

        return NULL;
    }

    // obtain the matcher's adjustable AOI radius, returns 0 if no matcher exists
    Area *
    VASTVerse::getMatcherAOI ()
    {
        if (isLogined () == false)
            return NULL;

        VASTPointer *handlers = (VASTPointer *)_pointers;

        // if there's a joined matcher on this node, then we should have a Voronoi map
        if (handlers->matcher->isJoined ())
            return handlers->matcher->getMatcherAOI ();

        return NULL;
    }

    // obtain the number of active connections at this node
    int 
    VASTVerse::getConnectionSize ()
    {
        VASTPointer *handlers = (VASTPointer *)_pointers;
        if (handlers->net != NULL)
            return (int)handlers->net->getConnections ().size ();

        return 0;
    }

    // obtain the tranmission size by message type, default is to return all types
    StatType &
    VASTVerse::getSendStat (const msgtype_t msgtype)
    {
        /*
        VASTPointer *handlers = (VASTPointer *)_pointers;
        if (handlers->net != NULL)
            return handlers->net->getSendSize (msgtype);
        return 0;
        */

        return _sendstat;
    }
    
    StatType &
    VASTVerse::getReceiveStat (const msgtype_t msgtype)
    {
        /*
        VASTPointer *handlers = (VASTPointer *)_pointers;
        if (handlers->net != NULL)
            return handlers->net->getReceiveSize (msgtype);
        return 0;
        */

        return _recvstat;
    }

    // record nodeID on the same host
    void 
    VASTVerse::recordLocalTarget (id_t target)
    {
        VASTPointer *handlers = (VASTPointer *)_pointers;
        if (handlers->net != NULL)
            return handlers->net->recordLocalTarget (target);
    }

    // translate a string-based address into Addr object
    Addr *
    VASTVerse::translateAddress (const string &str)
    {
        static Addr addr;        

        // convert any hostname to IP        
        string address = str;

        /*
        // convert a hostname to numeric IP string
        if (isalpha (str[0]))
        {            
            size_t port_pos = address.find (":");
            if (port_pos == (unsigned)(-1))
            {
                printf ("VASTVerse::translateAddress () address format incorrect, should be [numeric IP or hostname:port]\n");
                return NULL;
            }

            const char *IP = _net->getIPFromHost (address.substr (0, port_pos).c_str ());
            const char *port = address.substr (port_pos+1).c_str ();
                        
            // printout for debug purpose
            char converted[255];
            sprintf (converted, "%s:%s\0", IP, port);            
            printf ("VASTVerse::translateAddress () convert hostname into: %s\n", converted);

            // store numeric
            address = string (converted);            
        }
        */

        // determine IP from string
        if (IPaddr::parseIP (addr.publicIP, address) != 0)
        {
            printf ("VASTVerse::translateAddress () cannot resolve address: %s\n", str.c_str ());
            return NULL;
        }

        // determine hostID based on IP
        addr.host_id = VASTnet::resolveHostID (&addr.publicIP);

        return &addr;
    }

} // end namespace Vast

