/*
 * VAST, a scalable agent-to-Agent network for virtual environments
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
 *  VASTATEImpl.h -- VASTATE implementation
 *
 *  ver 0.1 (2006/07/15)
 *   
 */

#ifndef VASTATE_IMPL_H
#define VASTATE_IMPL_H

#include "VASTATE.h"

#include <map>

// TODO: reliable/unreliable flag for each attribute
//       hidden Object for user position
//using namespace std;

#define MAX_JOIN_COUNTDOWN      10

namespace Vast 
{        
    class VASTATE_impl : public VASTATE
    {
    public:
        
    	VASTATE_impl (vastverse *vastworld, Addr &gatewayIP, const system_parameter_t & sp);
    	
        ~VASTATE_impl () 
        {
            // de-allocate
            int i;
            for (i=0; i<(int)_arbitrators.size (); i++)
                delete _arbitrators[i];
            for (i=0; i<(int)_agents.size (); i++)
                delete _agents[i];
        }
        
        // process messages in queue
        int processMessages ();
    
        // create an initial server
        bool createServer (vector<ArbitratorLogic *> &alogics, 
                            vector<StorageLogic *> &slogics,
                            int dim_x, int dim_y, int n_vagents);

        // create a Agent entity
        Agent *createAgent (AgentLogic *logic, Node &agent_info, int capacity);

        // create an arbitrator
        Arbitrator *createArbitrator (id_t parent, 
                                       ArbitratorLogic *alogic, StorageLogic *slogic,
                                       Node &arb_info, bool is_gateway = false);

        // close down a agent
        void destroyAgent (Agent *p);

        // close down an arbitrator
        void destroyArbitrator (id_t id);

        // query for Arbitrator promotion / demotion requests
        multimap<int, Node>& getRequests ();

        // clean Arbitrator promotion / demotion requests
        bool cleanRequests ();


    private:
        
        Position generate_virpos (int dim_x, int dim_y, int num, int total);

        // check if a Agent or Arbitrator has joined successfully
        // returns the id used for joining
        id_t can_join (msghandler *handler);
                
        vastverse * _vastworld;
        Addr        _gateway;
       
        vector<Arbitrator *>    _arbitrators;        
        vector<Agent *>          _agents;

        // mapping between a node & its ID generator
        map<msghandler *, vastid *>     _node2id;
        map<msghandler *, Node>         _node2pos;
        map<msghandler *, int>          _node2count;

        // requests for Arbitrator promotion/demotion
        // int = 1: promotion, 2: demotion
        multimap<int, Node>             _arb_requests;
    };

} // end namespace Vast

#endif // VASTATE_IMPL_H


