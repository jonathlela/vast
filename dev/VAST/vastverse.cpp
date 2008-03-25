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

#include "vastverse.h"

#include <vector>
#include <map>


// implementation specific includes
#include "vast_dc.h"
//#include "vast_fo.h"
#include "vast_mc.h"
#include "vastid_base.h"
#include "net_emu.h"
#include "net_emu_bl.h"
#include "vor_SF.h"

#ifndef ACE_DISABLED
#include "net_ace.h"
#endif

namespace VAST
{  
    net_emubridge *g_bridge = NULL;
    
    std::vector<vast *>        g_vastnodes;            // a list of nodes created
    std::map<msghandler *, vastid *> g_vastids;            // a list of ID generator

    vastverse::
    vastverse (int model, int netmode, int connlimit, int loss_rate, int fail_rate, size_t send_quota, size_t recv_quota)
        :_vast_model (model), _net_model (netmode), _connlimit(connlimit)
    {
        // create a netbridge
        if (netmode == VAST_NET_EMULATED)
        {
            // initalize rand generator (for node fail stimulation)
            srand ((unsigned int)time (NULL));

            g_bridge = new net_emubridge (loss_rate, fail_rate, rand ());
        }
        else if (netmode == VAST_NET_EMULATED_BL)
        {
            // initalize rand generator (for node fail stimulation)
            srand ((unsigned int)time (NULL));

            g_bridge = new net_emubridge_bl (loss_rate, fail_rate, rand (), 1, send_quota, recv_quota);
        }
    }

    vastverse::~vastverse ()
    {
        if (g_bridge != NULL)
        {
            if (_net_model == VAST_NET_EMULATED_BL)
                delete (net_emubridge_bl *) g_bridge;
            else
                delete g_bridge;
        }
    }

    // obtain the unique ID for a particular node
    vastid * 
    vastverse::create_id (msghandler *node, bool is_gateway, Addr &gateway)
    {
        if (g_vastids.find (node) == g_vastids.end ())
        {
            // create ID generator
            g_vastids[node] = new vastid_base (node->getnet (), gateway, is_gateway);

            // chain it to the node so that it'll process message when the vastnode ticks
            node->chain (g_vastids[node]);
        }

        return g_vastids[node];
    }

    void 
    vastverse::destroy_id (vastid *id)
    {
        std::map<msghandler *, vastid *>::iterator it = g_vastids.begin ();

        for (; it != g_vastids.end (); it++)
            if (it->second == id)
            {
                // unchain it as message handler
                it->first->unchain (id);

                delete (vastid_base *)it->second;
                g_vastids.erase (it);
                return;
            }
    }

    vast *
    vastverse::
    create_node (unsigned int port, aoi_t detect_buffer)
    {
        network *net = NULL;
        vast    *vnode = NULL;

        if ((net = create_net (port)) != NULL)
        {
            if (_vast_model == VAST_MODEL_DIRECT)
                vnode = new vast_dc (net, detect_buffer, _connlimit);
            //else if (_vast_model == VAST_MODEL_FORWARD)    
            //    vnode = new vast_fo (net, detect_buffer, _connlimit);
			else if (_vast_model == VAST_MODEL_MULTICAST)
			    vnode = new vast_mc (net, detect_buffer);
        }

        if (vnode != NULL)
            g_vastnodes.push_back (vnode);

        return vnode;
    }

    void 
    vastverse::
    destroy_node (vast *node)
    {
        // release memory
        if (node != NULL)
        {       
            // remove from internal node list
            for (unsigned int i=0; i<g_vastnodes.size (); ++i)
            {
                if (g_vastnodes[i] == node)
                {
                    g_vastnodes.erase (g_vastnodes.begin () + i);
                    break;
                }
            }

            if (g_vastids.find (node) != g_vastids.end ())
            {
                delete g_vastids[node];
                g_vastids.erase (node);
            }

            network *net = node->getnet ();

            // NOTE: must delete vastnode before deleting the network object
            //       as it still needs to access the network
            if (_net_model == VAST_MODEL_DIRECT)
                delete (vast_dc *)node;
            //else if (_net_model == VAST_MODEL_FORWARD)
            //    delete (vast_fo *)node;
            else if (_net_model == VAST_MODEL_MULTICAST)
                delete (vast_mc *)node;

            destroy_net (net);
        }
    }
    
    network *
    vastverse::create_net (unsigned int port)
    {
        network *net = NULL;
        if (_net_model == VAST_NET_EMULATED)
            net = new net_emu (*g_bridge);
        else if (_net_model == VAST_NET_EMULATED_BL)
            net = new net_emu_bl (* (net_emubridge_bl *) g_bridge);
#ifndef ACE_DISABLED
        else if (_net_model == VAST_NET_ACE)
            net = new net_ace (port);
#endif
        return net;
    }

    bool 
    vastverse::destroy_net (network *net)
    {
        if (net == NULL)
            return false;
        else if (_net_model == VAST_NET_EMULATED)
            delete (net_emu *) net;
        else if (_net_model == VAST_NET_EMULATED_BL)
            delete (net_emu_bl *) net;
#ifndef ACE_DISABLED
        else if (_net_model == VAST_NET_ACE)
            delete (net_ace *) net;
#endif
        else
            return false;

        return true;
    }
    
    voronoi *
    vastverse::create_voronoi ()
    {
        return new vor_SF ();
    }

    bool 
    vastverse::destroy_voronoi (voronoi *v)
    {
        delete (vor_SF *)v;
        return true;
    }

    /*
    net_emubridge *
    vastverse::get_netbridge ()
    {
        return g_bridge;
    }
    */

    // obtain send & receive of particular nodes
    /*
    size_t 
    vastverse::sendsize (id_t node)
    {
        return g_bridge->sendsize (node);
    }
        
    size_t 
    vastverse::recvsize (id_t node)
    {
        return g_bridge->recvsize (node);
    }
    */

	// added by yuli ======================================
    /*
	size_t vastverse::send_def_size (id_t node)
	{
		return g_bridge->send_def_size (node);
	}

	size_t vastverse::recv_def_size (id_t node)
	{
		return g_bridge->recv_def_size (node);
	}
    */
	//=====================================================

	void
    vastverse::tick ()
    {
        if (g_bridge != NULL)
            g_bridge->tick ();
    }

    /*
    void
    vastverse::tick (bool advance_time)
    {
        for (unsigned int i=0; i<g_vastnodes.size (); ++i)
            g_vastnodes[i]->tick (advance_time);
    }
    */

} // end namespace VAST

