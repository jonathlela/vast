/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007 Shun-Yun Hu (syhu@yahoo.com)
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
 *  vastate_impl.h -- VASTATE implementation
 *
 *  ver 0.1 (2006/07/15)
 *   
 */

#ifndef VASTATE_IMPL_H
#define VASTATE_IMPL_H

#include "vastate.h"

#include <map>

// TODO: reliable/unreliable flag for each attribute
//       hidden object for user position
//using namespace std;

#define MAX_JOIN_COUNTDOWN      10

namespace VAST 
{        
    class vastate_impl : public vastate
    {
    public:
        
    	vastate_impl (vastverse *vastworld, Addr &gatewayIP, const system_parameter_t & sp);
    	
        ~vastate_impl () 
        {
            // de-allocate
            int i;
            for (i=0; i<(int)_arbitrators.size (); i++)
                delete _arbitrators[i];
            for (i=0; i<(int)_peers.size (); i++)
                delete _peers[i];
        }
        
        // process messages in queue
        int process_msg ();
    
        // create an initial server
        bool create_server (vector<arbitrator_logic *> &alogics, 
                            vector<storage_logic *> &slogics,
                            int dim_x, int dim_y, int n_vpeers);

        // create a peer entity
        peer *create_peer (peer_logic *logic, Node &peer_info, int capacity);

        // create an arbitrator
        arbitrator *create_arbitrator (id_t parent, 
                                       arbitrator_logic *alogic, storage_logic *slogic,
                                       Node &arb_info, bool is_gateway = false);

        // close down a peer
        void destroy_peer (peer *p);

        // close down an arbitrator
        void destroy_arbitrator (id_t id);

        // query for arbitrator promotion / demotion requests
        multimap<int, Node>& get_requests ();

        // clean arbitrator promotion / demotion requests
        bool clean_requests ();


    private:
        
        Position generate_virpos (int dim_x, int dim_y, int num, int total);

        // check if a peer or arbitrator has joined successfully
        // returns the id used for joining
        id_t can_join (msghandler *handler);
                
        vastverse * _vastworld;
        Addr        _gateway;
       
        vector<arbitrator *>    _arbitrators;        
        vector<peer *>          _peers;

        // mapping between a node & its ID generator
        map<msghandler *, vastid *>     _node2id;
        map<msghandler *, Node>         _node2pos;
        map<msghandler *, int>          _node2count;

        // requests for arbitrator promotion/demotion
        // int = 1: promotion, 2: demotion
        multimap<int, Node>             _arb_requests;
    };

} // end namespace VAST

#endif // VASTATE_IMPL_H


