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

#include "shared.h"
#include "vworld.h"
#include "vastate_impl.h"
#include "arbitrator_impl.h"
#include "peer_impl.h"
#include "storage_cs.h"

using namespace VASTATE;

namespace VAST
{
    static void object_info (VValue& m)
    {
        cout << "Object information ---" << endl;
        cout << m.encodeToStr () << endl;
        cout << "----------------------" << endl;
    }

    vastate_impl::vastate_impl (vastverse *vastworld, Addr &gatewayIP, const VASTATE::system_parameter_t & sp)
        : vastate (sp), _vastworld(vastworld), _gateway(gatewayIP)
    {
        

        // test data
        cout << "Hello, World!" << endl;
        VEObject *mp = new VEObject (1, Coord3D (318.52, 224.3, 900.0));
        VEObject &mpr = *mp;
        // assignment
        cout << "mpr.add (0, 20)" << endl;
        mpr.add_attribute (0, VSimpleValue_string ("IlMgcer"));
        mpr.add_attribute (1, VSimpleValue_int (100));
        mpr.add_attribute (2, VSimpleValue_int (220));
        mpr.add_attribute (3, VSimpleValue_double (2.5));
        mpr.add_attribute (10, VContainer ());
        mpr.add_attribute (15, VSimpleValue_int (50));
        cout << endl;

        // sword attributes
        cout << "New object: sword" << endl;
        VObject sword ((VASTATE::id_t) 2);
        sword.add_attribute (0, VSimpleValue_string("¶Ã¤C¤KÁV¼C"));
        sword.add_attribute (5, VSimpleValue_int(10));
        sword.add_attribute (6, VSimpleValue_double(2.2));
        sword.add_attribute (7, VSimpleValue_int(100));

        if (mpr.get_attribute (10).get_type () != VValue::T_CONTAINER)
            cout << "error containor type" << endl;

        // find a place to put in
        VContainer & res = * (VContainer *) (&mpr.get_attribute (10));
        short_index_t free_index = 0;
        for (free_index = 0; free_index < ((short_index_t) 0 - (short_index_t) 1); free_index ++)
            if (res.get_attribute (free_index).get_type () == VValue::T_ERROR)
                break;

        res.add_attribute (free_index, sword);

        // demage
        cout << "mpr[5] = 77" << endl;
        int hp_diff = -5;
        //int hp = mpr.get_attribute (1);
        int hp;
        mpr.get_attribute (1).get ((int) hp);
        hp += hp_diff;
        mpr.get_attribute (1).set ((int) hp);
        cout << endl;

    //    if (mp[5] >= 10)
    //        mp[2] = "HP Good!";

        //MBaseObject *pack = new MBaseObject ();
        //pack->add (1, "Red pack");
        //pack->add (2, 10);
        //mpr[24] = *pack;

        object_info (mpr);

        // encoding 2
        cout << "encoding =================" << endl;
        RawData r = VValueFactory::encodeToRaw (mp);

        cout << "data encoded =============" << endl;
        for (RawData::iterator it = r.begin (); it != r.end (); it ++)
            cout << (int) *it << " ";
        cout << " (size: " << r.size () << ") " << endl;

        cout << "decodeing ================" << endl;
        VValue * new_object = VValueFactory::decodeFromRaw (r);

        cout << "object decoded ===========" << endl;
        if (new_object != NULL)
            object_info (*new_object);
        else
            cout << "(empty object pointer)" << endl;

        cout << "encoded delta ============" << endl;
        mp->clear_edit ();
        mp->get_attribute (1).set (100);
        mp->get_attribute (10).get_attribute (0).get_attribute (6).set (4.8);
        RawData r2 = VValueFactory::encodeToRaw (mp, true);

        cout << "data encoded =============" << endl;
        for (RawData::iterator it = r2.begin (); it != r2.end (); it ++)
            cout << (int) *it << " ";
        cout << " (size: " << r2.size () << ") " << endl;

        cout << "decodeing ================" << endl;
        VValue * new_object2 = VValueFactory::decodeFromRaw (r2, mp);

        cout << "object decoded ===========" << endl;
        if (new_object2 != NULL)
            object_info (*new_object2);
        else
            cout << "(empty object pointer)" << endl;

        cout << "==========================" << endl;

        cout << "delete mp" << endl;
        delete mp;
        cout << "delete new_object" << endl;
        delete new_object;
        //cout << "delete new_object2" << endl;
        //delete new_object2;

        cout << endl;
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

            // note that join will be called only once every MAX_JOIN_COUNTDOWN timesteps
            if (p->is_joined () == false && (id = can_join (p)) != NET_ID_UNASSIGNED)
                // TODO: we assume no authentication data for now
                p->join (id, _node2pos[p].pos, _node2pos[p].aoi, 0, 0);
        }
        

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


    // create an initial server
    bool 
    vastate_impl::create_server (vector<arbitrator_logic *> &alogics, 
                                 vector<storage_logic *>    &slogics, 
                                 int dim_x, int dim_y, int n_vpeers)
    {
        if (alogics.size () != n_vpeers || slogics.size () != n_vpeers)
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
            n.pos   = generate_virpos (dim_x, dim_y, i, n_vpeers);            
            create_arbitrator (NET_ID_GATEWAY, alogics[i], slogics[i], n, (i == 0 ? true : false));
        }

        return true;
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
        return p;
    }

    arbitrator *
    vastate_impl::create_arbitrator (id_t parent, arbitrator_logic *alogic, storage_logic *slogic, Node &arb_info, bool is_gateway)
    {            
        vast *vnode     = _vastworld->create_node (_gateway.publicIP.port, 0);
        storage *s      = new storage_cs (slogic);
        arbitrator *a   = new arbitrator_impl (parent, alogic, vnode, s, is_gateway, _gateway, &sysparm);

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
                    delete _node2id[p];
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
                    delete _node2id[a];
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
