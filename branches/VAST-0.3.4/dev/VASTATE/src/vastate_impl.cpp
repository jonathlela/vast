/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007 Shun-Yun Hu (syhu@yahoo.com)
 *               2008 Shao-Chen Chang (cscxcs at gmail.com)
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

#include "vastate_impl.h"
#include "arbitrator_impl.h"
#include "peer_impl.h"
#include "storage_cs.h"

namespace VAST
{
    vastate_impl::vastate_impl (vastverse *vastworld, Addr &gatewayIP, const system_parameter_t & sp)
        : vastate (sp), _vastworld(vastworld), _gateway(gatewayIP), _gateway_node (NULL)
    {
        // test data
        /*
  
        attributes_impl a;

        a.add (true);
        a.add ((int)32767);
        a.add (112343.14159269777f);       string s("abcdefg ");
        a.add (s);      vec3_t c; c.x = 382.573829f; c.y = 2349.2431f; c.z = 5382.34234f;
        a.add (c);
        a.add (true);
        //a.add (19);
        
        bool   v1 = false;
        int    v2 = 777;
        float  v3 = 0.0;
        string v4 = "abc good";
        vec3_t v5;

        a.get (0, v1);        printf ("bool: %d\n", v1);        
        a.get (1, v2);        printf ("int: %d\n", v2);        
        a.get (2, v3);        printf ("float: %f\n", v3);
        a.get (3, v4);        printf ("str: (%s)\n", v4.c_str ());        
        a.get (4, v5);        printf ("vec3: (%f, %f, %f)\n", v5.x, v5.y, v5.z);        
        a.get (5, v1);        printf ("bool: %d\n", v1);       
        //a.get (6, v2);        printf ("int: %d\n", v2); 
                
        
        char buf[VASTATE_BUFSIZ];
        char *p = buf;
        int size = a.pack_all (&p);
        
        string str;
        string str1;
        string str2;
        
        for (int k=0; k<size; k++)
        {
            str1 += buf[k];
            str2 += buf[k];
        }
        str += str1;
        str += str2;

        printf ("packsize: %d string size: %d\n", size, str.size());
        */
        
        /*
        printf ("\ntransferring to another attribute (pack size: %d)\n\n", size);
        
        attributes_impl b;   
        
        b.unpack (buf);
        
        b.get (0, v1);        printf ("bool: %d\n", v1);        
        b.get (1, v2);        printf ("int: %d\n", v2);        
        b.get (2, v3);        printf ("float: %f\n", v3);
        b.get (3, v4);        printf ("str: (%s)\n", v4.c_str ());        
        b.get (4, v5);        printf ("vec3: (%f, %f, %f)\n", v5.x, v5.y, v5.z);        
        b.get (5, v1);        printf ("bool: %d\n", v1);
        //b.get (6, v2);        printf ("int: %d\n", v2); 
        
        printf ("\nresetting values in a\n\n");
        
        a.reset_dirty();

        a.add (19);

        a.set (0, false);
        a.set (1, (int)512);
        a.set (2, (float)3.14159269f);  
        string s2("abcdefg   ");
        a.set (3, s2);               
        vec3_t c2; c2.x = 5.0; c2.y = 6.0; c2.z = 7.0;
        //a.set (4, c2);
        a.set (5, false);
        
        a.get (0, v1);        printf ("bool: %d\n", v1);        
        a.get (1, v2);        printf ("int: %d\n", v2);        
        a.get (2, v3);        printf ("float: %f\n", v3);
        a.get (3, v4);        printf ("str: (%s)\n", v4.c_str ());        
        a.get (4, v5);        printf ("vec3: (%f, %f, %f)\n", v5.x, v5.y, v5.z);        
        a.get (5, v1);        printf ("bool: %d\n", v1);       
        a.get (6, v2);        printf ("int: %d\n", v2);                 
        
        p = buf;
        size = a.pack_dirty (&p);
        //size = a.pack_all (&p);
        printf ("\npacking to b again, size: %d\n\n", size);
        
        b.unpack (buf);
        
        int vv;
        b.get (0, v1);        printf ("bool: %d\n", v1);        
        b.get (1, vv);        printf ("int: %d\n", vv);        
        b.get (2, v3);        printf ("float: %f\n", v3);
        b.get (3, v4);        printf ("str: (%s)\n", v4.c_str ());        
        b.get (4, v5);        printf ("vec3: (%f, %f, %f)\n", v5.x, v5.y, v5.z);        
        b.get (5, v1);        printf ("bool: %d\n", v1);       
        b.get (6, v2);        printf ("int: %d\n", v2);  
        
        printf ("\n\n");
*/        
    }

    int      
    vastate_impl::process_msg ()
    {
        //
        // NOTE: it's important to let arbitrators process their message first
        //       this will ensure peer & arbitrator logical clocks are in sync
        //

        unsigned int i;

        // allow arbitrators I manage to process 
        for (i=0; i<_arbitrators.size (); ++i)
            _arbitrators[i]->process_msg ();

        // allow peers I manage to process 
        for (i=0; i<_peers.size (); ++i)
            _peers[i]->process_msg ();

        if (_gateway_node != NULL)
            _gateway_node->tick ();

        //
        // check for arbitrator promotion/demotion
        //
        Node info;
        for (i=0; i<_arbitrators.size (); ++i)
        {
            if (_arbitrators[i]->is_demoted (info) == true)
                _arb_requests.insert (multimap<int, Node>::value_type (2, info));
        }

        for (i=0; i<_peers.size (); ++i)
        {
            if (_peers[i]->is_promoted (info) == true)
                _arb_requests.insert (multimap<int, Node>::value_type (1, info));
        }

        // check for arbitrator join (after IDs are obtained)
        id_t id;        
        arbitrator *a;
        peer *p;
        vector<void *> remove_list;

        for (map<void *, bool>::iterator it = _wait2join.begin (); it != _wait2join.end (); it ++)
        {
            // for arbitrator
            if (std::find (_arbitrators.begin (), _arbitrators.end (), it->first) != _arbitrators.end ())
            {
                a = (arbitrator *) it->first;
                // note that join will be called only once every MAX_JOIN_COUNTDOWN timesteps
                if (a->is_joined () == false && (id = can_join (a)) != NET_ID_UNASSIGNED)
                    a->join (id, _node2pos[a].pos, _gateway);
                // remove from wait 2 join list if joined successfully
                else if (a->is_joined () == true)
                    remove_list.push_back (it->first);
            }

            // for peers
            else if (std::find (_peers.begin (), _peers.end (), it->first) != _peers.end ())
            {
                p = (peer *) it->first;

                arbitrator_impl *arb_paired = (arbitrator_impl *)
                        ((_peerarb_pair.find (p) == _peerarb_pair.end ()) ? NULL : _peerarb_pair[p]);
                // note that join will be called only once every MAX_JOIN_COUNTDOWN timesteps
                if (p->is_joined () == false && (id = can_join (p)) != NET_ID_UNASSIGNED
                    && (arb_paired == NULL || arb_paired->is_joined ())
                    )
                {
                    // TODO: we assume no authentication data for now
                    // '== Addr ()' meaning gets NULL Address
                    if (arb_paired == NULL || arb_paired->getnet ()->getaddr (arb_paired->self->id) == Addr ())
                        p->join (id, _node2pos[p].pos, _node2pos[p].aoi, 0, 0, NULL);
                    else
                        p->join (id, _node2pos[p].pos, _node2pos[p].aoi, 0, 0, & (arb_paired->getnet ()->getaddr (arb_paired->self->id)));

                    // check and inform ,if exists, my child arbitrator of my id
                    if (_peerarb_pair.find (p) != _peerarb_pair.end ())
                    {
                        _peerarb_pair[p]->set_parent (id);
                    }
                }
                // remove from wait 2 join list if joined successfully
                else if (p->is_joined () == true)
                    remove_list.push_back (it->first);
            }
        }

        for (vector<void *>::iterator it = remove_list.begin (); it != remove_list.end (); it ++)
            _wait2join.erase (*it);

        /*
        for (i=0; i<_arbitrators.size (); ++i)
        {
            a = _arbitrators[i];
            // note that join will be called only once every MAX_JOIN_COUNTDOWN timesteps
            if (a->is_joined () == false && (id = can_join (a)) != NET_ID_UNASSIGNED)
                a->join (id, _node2pos[a].pos);
        }
        
        // check for peer join (after IDs are obtained)
        for (i=0; i<_peers.size (); ++i)
        {
            p = _peers[i];

            arbitrator_impl *arb_paired = (arbitrator_impl *)
                    ((_peerarb_pair.find (p) == _peerarb_pair.end ()) ? NULL : _peerarb_pair[p]);
            // note that join will be called only once every MAX_JOIN_COUNTDOWN timesteps
            if (p->is_joined () == false && (id = can_join (p)) != NET_ID_UNASSIGNED
                && (arb_paired == NULL || arb_paired->is_joined ())
                )
            {
                // TODO: we assume no authentication data for now
                // '== Addr ()' meaning gets NULL Address
                if (arb_paired == NULL || arb_paired->getnet ()->getaddr (arb_paired->self->id) == Addr ())
                    p->join (id, _node2pos[p].pos, _node2pos[p].aoi, 0, 0, NULL);
                else
                    p->join (id, _node2pos[p].pos, _node2pos[p].aoi, 0, 0, & (arb_paired->getnet ()->getaddr (arb_paired->self->id)));

                // check and inform ,if exists, my child arbitrator of my id
                if (_peerarb_pair.find (p) != _peerarb_pair.end ())
                {
                    _peerarb_pair[p]->set_parent (id);
                }
            }
        }
        */

        return 0;
    }

    // check if a peer or arbitrator has joined successfully
    id_t
    vastate_impl::can_join (msghandler *a)
    {
        id_t id;
        if ((id = _node2id[a]->getid ()) != NET_ID_UNASSIGNED)
        {
            if (_node2count[a] == 0)
            {
                _node2count[a] = MAX_JOIN_COUNTDOWN;
                return id;
            }
            else
            {
                _node2count[a]--;
                return NET_ID_UNASSIGNED;
            }
        }
        return NET_ID_UNASSIGNED;
    }


    // creating a gateway node only has vnode
    void
    vastate_impl::create_gateway ()
    {            
        vast *vnode     = _vastworld->create_node (_gateway.publicIP.port, 0);
        vastid *idgen   = _vastworld->create_id (vnode, true, _gateway);
        
        _gateway_node = vnode;
        _node2id[vnode] = idgen;

        // send a request for ID
        idgen->getid ();
    }


    // create an initial server
    bool 
    vastate_impl::create_server (vector<arbitrator_logic *> &alogics, 
                                 vector<storage_logic *>    &slogics, 
                                 int dim_x, int dim_y, int n_vpeers)
    {
        if (alogics.size () != (size_t) n_vpeers || slogics.size () != (size_t) n_vpeers)
            return false;

        // calculate default AOI based on virtual peers density
        //aoi_t default_aoi = (aoi_t)((dim_x / sqrt ((double)n_vpeers)) * 1.5);
        //printf ("default_aoi = %d\n", default_aoi);
        
        //
        // create arbitrators (1st one is the server node)
        //
        for (int i=0; i<n_vpeers; i++)
        {
            Node n;

            // TODO: id generation/assignment
            n.id    = i+1;
            // BUG: put gateway to out of map if only one virtual peers 
            if (n_vpeers == 1)
                n.pos   = Position (-dim_x, -dim_y);
            else
                n.pos   = generate_virpos (dim_x, dim_y, i, n_vpeers);            
            create_arbitrator (NET_ID_GATEWAY, alogics[i], slogics[i], n, (i == 0 ? true : false));
        }

        return true;
    }

    std::pair<peer*, arbitrator*>
    vastate_impl::create_peerarb_pair (peer_logic *logic, Node &peer_info, int capacity, 
                                arbitrator_logic *alogic, storage_logic *slogic)
    {
        std::pair<peer*, arbitrator*> ret;
        ret.first = create_peer (logic, peer_info, capacity);
        ret.second = create_arbitrator (NET_ID_UNASSIGNED, alogic, slogic, peer_info, false);

        // keep the record for later informing arbitrator of its parent
        _peerarb_pair[ret.first] = ret.second;

        return ret;
    }

    peer *
    vastate_impl::create_peer (peer_logic *logic, Node &peer_info, int capacity)
    {
        peer *p = new peer_impl (logic, _vastworld->create_net (_gateway.publicIP.port), capacity, _gateway);

        vastid *idgen = _vastworld->create_id (p, false, _gateway);

        // send a request for ID
        idgen->getid ();
        _node2id[p] = idgen;
        _node2pos[p] = peer_info;
        _node2count[p] = 0;

        _peers.push_back (p);
        _wait2join[p] = true;
        return p;
    }

    arbitrator *
    vastate_impl::create_arbitrator (id_t parent, arbitrator_logic *alogic, storage_logic *slogic, Node &arb_info, bool is_gateway, bool is_aggregator)
    {            
        vast *vnode     = _vastworld->create_node (_gateway.publicIP.port, 0);
        storage *s      = new storage_cs (slogic);
        arbitrator *a   = new arbitrator_impl (parent, alogic, vnode, s, is_gateway, _gateway, _vastworld, &sysparm, is_aggregator);

        vastid *idgen   = _vastworld->create_id (vnode, is_gateway, _gateway);
        
        // chain up event handlers, so the ordering is vastnode -> vastid -> arbitrator -> storage
        // NOTE: in the current design,
        //       storage must be chained AFTER arbitrator is chained to vastnode
        vnode->chain (a);
        a->chain (s);

        /*
        // send out initial id query 
        // BUG: will enter infinite loop if ID assignment node is managyed by another VASTATE
        //      during simulation
        id_t id;
        while ((id = idgen->get_id ()) == NET_ID_UNASSIGNED)
        {
            // keep processing messages for all nodes until getting my ID
            process_msg ();

            // sleep a little (under real-network)
        }        

        a->join (id, arb_info.pos);
        */

        // store myself as the first in a list of known arbitrators
        _arbitrators.push_back (a);        

        // send a request for ID
        idgen->getid ();
        _node2id[a]    = idgen;
        _node2pos[a]   = arb_info;
        _node2count[a] = 0;
        _wait2join[a]  = true;

        return a;
    }

    // close down a peer
    void
    vastate_impl::destroy_peer (peer *p)
    {
        // find the particular peer
        for (unsigned int i=0; i<_peers.size (); ++i)
        {
            if (_peers[i] == p)
            {
                network *net = p->getnet ();
                _vastworld->destroy_net (net);

                if (_node2id.find (p) != _node2id.end ())
                {
                    //delete _node2id[p];
                    _vastworld->destroy_id (_node2id[p]);
                    _node2id.erase (p);
                    _node2pos.erase (p);
                }
                delete p;
                _peers.erase (_peers.begin () + i);
            
                return;
            }
        }
    }

    // close down an arbitrator
    void 
    vastate_impl::destroy_arbitrator (id_t id)
    {
        // find the particular arbitrator
        for (unsigned int i=0; i<_arbitrators.size (); ++i)
        {
            if (_arbitrators[i]->self->id == id)
            {
                arbitrator * a = _arbitrators[i];

                if (_node2id.find (a) != _node2id.end ())
                {
                    //delete _node2id[a];
                    _vastworld->destroy_id (_node2id[a]);
                    _node2id.erase (a);
                    _node2pos.erase (a);
                }
                delete a;
                _arbitrators.erase (_arbitrators.begin () + i);
            
                return;
            }
        }
    }

    // query for arbitrator promotion / demotion requests
    multimap<int, Node>& 
    vastate_impl::get_requests ()
    {
        return _arb_requests;
    }

    // clear arbitrator promotion / demotion requests
    bool 
    vastate_impl::clean_requests ()
    {
        _arb_requests.clear ();
        return true;
    }


    //
    // private methods
    //

    Position
    vastate_impl::generate_virpos (int dim_x, int dim_y, int num, int total)
    {
        int x, y;

        int d = (int)sqrt ((double)total);
        int block = dim_x / d;
        int start = block/2;

        x = start + (num % d) * block;
        y = start + (num / d) * block;

        //printf ("generating (%d/%d): (%d, %d)\n", num, total, x, y);
        return Position (x, y);
    }
    
} // namespace VAST

