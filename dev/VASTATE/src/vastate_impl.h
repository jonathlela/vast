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
    class vastate_impl : public vastate, public VAST::msghandler
    {
    public:
        vastate_impl (vastverse *vastworld, const Addr &gatewayIP, const system_parameter_t & sp);
    	
        ~vastate_impl ()
        {
            stop ();

            // de-allocate
            int i;
            for (i=0; i<(int)_arbitrators.size (); i++)
                delete _arbitrators[i];
            for (i=0; i<(int)_peers.size (); i++)
                delete _peers[i];
        }

        // return status of the vastate node is started
        inline 
        int is_started ()
        {
            return _started;
        }

        // start the node, get id
        bool start (bool is_gateway);

        // stop the node, disconnects all rolls
        bool stop ();

        // return node id
        id_t get_id ()
        {
            if (_ider != NULL)
                return _ider->getid ();
            else
                return NET_ID_UNASSIGNED;
        }

        // process messages in queue
        int process_message ();

        // derived from msghandler
        // returns whether the message has been handled successfully
        bool handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size);

        // do things after messages are all handled
        void post_processmsg ();
    
        // create an initial server, if a server, must be called before any create of peers/arbitrators
        gateway *create_gateway ();

        // create an initial server
        /*
        bool create_server (vector<arbitrator_logic *> &alogics, 
                            vector<storage_logic *> &slogics,
                            int dim_x, int dim_y, int n_vpeers);
        */

        // create a peer entity
        peer *create_peer (peer_logic *logic, Node &peer_info, int capacity);

        // create an arbitrator
        arbitrator *create_arbitrator (id_t parent, 
                                       arbitrator_logic *alogic, storage_logic *slogic,
                                       Node &arb_info, bool is_gateway = false);

        // close down the gateway
        void destroy_gateway (gateway *g);

        // close down a peer
        void destroy_peer (peer *p);

        // close down an arbitrator
        void destroy_arbitrator (id_t id);

        // query for arbitrator promotion / demotion requests
        multimap<int, Node>& get_requests ();

        // clean arbitrator promotion / demotion requests
        bool clean_requests ();

    private:
        // starting state, ref to vastate::STATE_* constants
        int _started;

        // pointer of vastworld to create vastid/vnode/net
        vastverse * _vastworld;

        // address of gateway
        bool        _is_gateway;
        Addr        _gateway_addr;

        // id fetcher
        vastid    * _ider;

        gateway *               _gateway;
        vector<arbitrator *>    _arbitrators;
        vector<peer *>          _peers;

        //// old codes ////////
        Position generate_virpos (int dim_x, int dim_y, int num, int total);

        // check if a peer or arbitrator has joined successfully
        // returns the id used for joining
        id_t can_join (msghandler *handler);

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


