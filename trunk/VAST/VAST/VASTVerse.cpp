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
#include "IDGenerator.h"
#include "Topology.h"
#include "Relay.h"
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
    int            g_bridge_tickcount = 0;      // countdown to next tick

    
#ifdef ENABLE_LATENCY
	// for physical topology
    static Vivaldi*	   g_vivaldi = NULL;
//#define STEP_PER_SEC (5)
#endif

    class VASTPointer
    {
    public:

        VASTnet *       net;            // network interface
        MessageQueue *  msgqueue;       // message queue for managing mesage handlers
        VAST *          vastnode;       // clients that enter an overlay
        IDGenerator *   IDgen;          // unique ID generator
        Topology *      topology;       // physical coordinate locator
    };

    //
    //  Internal helper functions
    //

    // TODO: better interface?
    VAST *
    createNode (MessageQueue *msgqueue, VASTPointer *node, int peerlimit, int relaylimit)
    {
        Relay *vnode;
        
        if ((vnode = new Relay (node->IDgen->getID (), peerlimit, relaylimit)) == NULL)
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
            msgqueue->unregisterHandler ((Relay *)node);            
            delete (Relay *)node;
        }
        return true;
    }
    
    VASTnet *
    createNet (unsigned short port, VASTPara_Net &para)
    {
        VASTnet *net = NULL;
        if (para.model == VAST_NET_EMULATED)
            net = new net_emu (*g_bridge);
		else if (para.model == VAST_NET_EMULATED_BL)					
			net = new net_emu_bl(*(net_emubridge_bl *)g_bridge);			
		
#ifndef ACE_DISABLED
        else if (para.model == VAST_NET_ACE)
        {
            printf ("VASTnet::createNet, creating net_ace at port: %d\n", port);
            net = new net_ace (port);
        }
#endif
        net->setTickPerSecond (para.step_persec);

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
    VASTVerse (VASTPara_Net *netpara, VASTPara_Sim *simpara)
    {
        _logined = false;

        // make the local copy of the parameters
        _netpara = *netpara;
        if (simpara != NULL)
            _simpara = *simpara;
        else
            // use 0 as defaults for all values
            memset (&_simpara, 0, sizeof (VASTPara_Sim)); 

        // zero all pointers
        _pointers = new VASTPointer ();
        memset (_pointers, 0, sizeof (VASTPointer));

        // create a shared net-bridge (used in simulation to locate other simulated nodes)
        if (_netpara.model == VAST_NET_EMULATED)
        {
            // initialize rand generator (for node fail stimulation)
            srand ((unsigned int)time (NULL));

            // NOTE: g_bridge may be shared across different VASTVerse instances
            if (g_bridge == NULL)
                g_bridge = new net_emubridge (_simpara.loss_rate, _simpara.fail_rate, 1, _netpara.step_persec, 1);
            g_bridge_ref++;
        }
		else if (_netpara.model == VAST_NET_EMULATED_BL)
		{
			// initialize rand generator (for node fail stimulation)
			srand ((unsigned int)time (NULL));

			// NOTE: g_bridge may be shared across different VASTVerse instances
			if (g_bridge == NULL)
				g_bridge = new net_emubridge_bl (_simpara.loss_rate, _simpara.fail_rate, 1, _netpara.step_persec);
			g_bridge_ref++;
		}

#ifdef ENABLE_LATENCY
        // new Vivaldi object
		if (g_vivaldi == NULL && g_bridge != NULL)
		{
			g_vivaldi = new Vivaldi ();			
            g_bridge->setVivaldi (g_vivaldi); // for latency table
		}			
#endif // ENABLE_LATENCY_
    }

    VASTVerse::~VASTVerse ()
    {
        // delete those vast_nodes not yet deleted
        if (((VASTPointer *)_pointers)->vastnode != NULL)
        {
            destroyNode (((VASTPointer *)_pointers)->msgqueue, ((VASTPointer *)_pointers)->vastnode);
            ((VASTPointer *)_pointers)->vastnode = NULL;
        }

        if (((VASTPointer *)_pointers)->topology != NULL)
        {
            delete ((VASTPointer *)_pointers)->topology;
            ((VASTPointer *)_pointers)->topology = NULL;
        }

        if (((VASTPointer *)_pointers)->IDgen != NULL)
        {
            delete ((VASTPointer *)_pointers)->IDgen;
            ((VASTPointer *)_pointers)->IDgen = NULL;
        }

        if (((VASTPointer *)_pointers)->msgqueue != NULL)
        {
            destroyQueue (((VASTPointer *)_pointers)->msgqueue);
            ((VASTPointer *)_pointers)->msgqueue = NULL;
        }

        if (((VASTPointer *)_pointers)->net != NULL)
        {
            destroyNet (((VASTPointer *)_pointers)->net, _netpara);
            ((VASTPointer *)_pointers)->net = NULL;
        }
        
        g_bridge_ref--;

        // only delete the bridge if no other VASTVerse's using it
        if (g_bridge != NULL && g_bridge_ref == 0)
        {
            delete g_bridge;
            g_bridge = NULL;
        }

#ifdef ENABLE_LATENCY
		if (g_vivaldi != NULL)
		{
			//delete g_vivaldi;
		}
#endif

        delete (VASTPointer *)_pointers;

        _logined = false;
    }

    // whether this VASTVerse has properly authenticated and gotten unique ID
    bool 
    VASTVerse::isLogined ()
    {       
        if (_logined == true)
            return true;

        VASTPointer *handlers = (VASTPointer *)_pointers;

        // we have not yet initialized
        if (handlers->IDgen == NULL)
        {
            printf ("VASTVerse::isLogined () creating IDgenerator...\n");

            // create message queue & IDGenerator for getting unique ID
            handlers->net = createNet (_netpara.port, _netpara);
            if (handlers->net == NULL)
                return false;

            // if gateway is localhost (127.0.0.1), replace with my detected IP 
            if (_netpara.gateway.publicIP.host == 0 || 
                _netpara.gateway.publicIP.host == 2130706433)
            {
                _netpara.gateway.publicIP = handlers->net->getHostAddress ().publicIP;
            }

			// set the bandwidth limitation after the net is created
			if (_netpara.model == VAST_NET_EMULATED_BL)
			{
				Bandwidth bw;
#ifdef ENABLE_LATENCY
				if (_simpara.send_quota == 0 || _simpara.recv_quota == 0)
				{
					if (g_vivaldi != NULL)
					{
						g_vivaldi->get_bandwidth (bw);										
						while((_netpara.is_gateway || _netpara.is_gateway)
							&& bw.UPLOAD < 153600)
						{
							g_vivaldi->get_bandwidth (bw);	
						}
					}
				}
				else
				{
					bw.UPLOAD   = _simpara.send_quota;
					bw.DOWNLOAD = _simpara.recv_quota;				
				}

#else
				bw.UPLOAD   = _netpara.send_quota;
				bw.DOWNLOAD = _netpara.recv_quota;				
#endif
				handlers->net->setBandwidthLimit (BW_UPLOAD,   bw.UPLOAD  / _netpara.step_persec);
				handlers->net->setBandwidthLimit (BW_DOWNLOAD, bw.DOWNLOAD/ _netpara.step_persec);

			}

            handlers->msgqueue = createQueue (handlers->net);
        
            handlers->IDgen    = new IDGenerator (_netpara.gateway, _netpara.is_gateway);       
            handlers->msgqueue->registerHandler (handlers->IDgen);               
            handlers->IDgen->getID ();
        }
        // after we get unique ID, obtain physical coordinate
        else if (handlers->topology == NULL)
        {
            id_t id = handlers->IDgen->getID ();

            // check if unique hostID is obtained
            // NOTE we will try to determine physical coordinate only after unique ID is gotten
            if (id != NET_ID_UNASSIGNED)
            {
                printf ("VASTVerse::isLogined () unique ID obtained [%ld], creating Topology...\n", id);
                printf ("if this hangs, check if physical coordinate is obtained correctly\n");

                // gateway's physical coordinate is set at the origin
                if (_netpara.is_gateway)
                {
                    Position origin (0, 0, 0);
                    handlers->topology = new Topology (id, &origin);
                }
                else
                {
                    bool hasPhysCoord = !(_netpara.phys_coord.x == 0 && _netpara.phys_coord.y == 0);
                    handlers->topology = new Topology (id, hasPhysCoord ? &_netpara.phys_coord : NULL);
                }

                handlers->msgqueue->registerHandler (handlers->topology);
            }
        }
        // NOTE: if physical coordinate is not supplied, the login process may pause here indefinitely
        else if (handlers->topology != NULL && handlers->topology->getPhysicalCoordinate () != NULL)
        {
            Position *physcoord = handlers->topology->getPhysicalCoordinate ();
            printf ("VASTVerse::isLogined () Topology obtained [%ld] (%.3f, %.3f)\n", handlers->IDgen->getID (), physcoord->x, physcoord->y);
            _logined = true;
        }
                
        return _logined;
    }

    // obtain topology class, 
    // NOTE that Topology is returnable / usable only after we've properly joined 
    //           (i.e. gotten unique ID *and* also the physical coordinate)
    Topology *
    VASTVerse::getTopology ()
    {
        // we need to obtain unique ID first
        if (isLogined () == false)
            return NULL;
        else
            return ((VASTPointer *)_pointers)->topology;
    }

    VAST *
    VASTVerse::createClient ()
    {
        if (isLogined () == false)
            return NULL;
                
        return createNode (((VASTPointer *)_pointers)->msgqueue, (VASTPointer *)_pointers, _netpara.peer_limit, _netpara.relay_limit);
    }

    bool     
    VASTVerse::destroyClient (VAST *node)
    {
        return destroyNode (((VASTPointer *)_pointers)->msgqueue, node);
    }

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

    // advance one time-step 
	void
    VASTVerse::tick ()
    {        
        // perform routine procedures for each logical time-step
        if (((VASTPointer *)_pointers)->msgqueue != NULL)
            ((VASTPointer *)_pointers)->msgqueue->tick ();
           
        // increase tick globally when the last node has processed its messages for this round
        // this will prevent its message be delayed one round in processing by other nodes
        // TODO: find a better way? 
        if (g_bridge != NULL)
        {
            //printf ("tickcount: %d, ref: %d\n", g_bridge_tickcount, g_bridge_ref);
            if (++g_bridge_tickcount == g_bridge_ref)
            {                
                g_bridge_tickcount = 0;
                g_bridge->tick ();
            }
        }
    }

    // stop operations on this node
    void     
    VASTVerse::pause ()
    {
        if (((VASTPointer *)_pointers)->net != NULL)
        {
            ((VASTPointer *)_pointers)->net->flush ();
            ((VASTPointer *)_pointers)->net->stop ();			
        }
    }

    // resume operations on this node
    void     
    VASTVerse::resume ()
    {
        if (((VASTPointer *)_pointers)->net != NULL)
            ((VASTPointer *)_pointers)->net->start ();
    }

    // obtain the tranmission size by message type, default is to return all types
    size_t 
    VASTVerse::getSendSize (const msgtype_t msgtype)
    {
        if (((VASTPointer *)_pointers)->net != NULL)
            return ((VASTPointer *)_pointers)->net->getSendSize (msgtype);
        return 0;
    }

    size_t 
    VASTVerse::getReceiveSize (const msgtype_t msgtype)
    {
        if (((VASTPointer *)_pointers)->net != NULL)
            return ((VASTPointer *)_pointers)->net->getReceiveSize (msgtype);
        return 0;
    }

    // record nodeID on the same host
    void 
    VASTVerse::recordLocalTarget (id_t target)
    {
        if (((VASTPointer *)_pointers)->net != NULL)
            return ((VASTPointer *)_pointers)->net->recordLocalTarget (target);
    }

} // end namespace Vast

