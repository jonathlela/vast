
/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007 Shun-Yun Hu (syhu@yahoo.com)
 *               2008 Shao-Jhen Chang (cscxcs at gmail.com)
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

#include "arbitrator_impl.h"
#include "vastutil.h"

/*
    Note:

    - Discovery and Update for arbitrators and aggregators

        For both arbitrators and aggregators, they will send node 
        information when be discovered. But explicit updating message 
        for aggregators is done in adjust_aggnodes_position (), 
        arbitrators is updated throw overlay node information. 
*/

// Constant definations
///////////////////////////////////////
// AOI accepting range is defined as ratio of standard AOI
#define AGGREGATOR_AOI_MAX (3.0)
#define AGGREGATOR_AOI_MIN (0.6)
// AOI difference per adjustment
#define AGGREGATOR_AOI_ADJ_UNIT (5)     // radius unit
// least interval between two adjustments 
// (note: due to other latency may be added by user of vastate, here only apply no limitation on adjustments)
#define AGGREGATOR_AOI_ADJ_CD   (1)     // steps
// how long if one aggregator in underloading will to leave
#define AGGREGATOR_LEAVE_TIME   (20)    // steps
// time period between two full update to my neighbors about myself
#define ARBITRATOR_FULLUPDATE_PERIOD (20) // steps

// default forwarding hops for a event 
// (i.e. after forward DEFAULT_EVENT_TTL times, the event should be dropped)
#define DEFAULT_EVENT_TTL (3)

// distance between two position look as equal point
#define EQUAL_DISTANCE (0.000001)

namespace VAST
{
    extern char VASTATE_MESSAGE[][20];

    // for debug purpose
    errout arbitrator_impl::_eo;
    char arbitrator_impl::_str [VASTATE_BUFSIZ];

    // initialize an arbitrator
    arbitrator_impl::arbitrator_impl (id_t my_parent, 
                                      arbitrator_logic *logic, 
                                      vast *vastnode, storage *s, 
                                      bool is_gateway, Addr &gateway, 
                                      vastverse *vastworld, system_parameter_t * sp, 
                                      bool is_aggregator)
        : arbitrator (my_parent, sp), _joined (false), _vastworld (vastworld)
        ,  _logic(logic), _vnode (vastnode), _storage (s)
        , _is_gateway (is_gateway), _gateway (gateway), _time_diff (0)
        //, _parent_obj (NULL)
        , _is_leaving (false), _is_suspending (false), _arbitrators_lastupdate (1)
        , _arbitrator_vor (NULL)
        , _obj_id_count (1)
        , _overload_time (0), _overload_count (0), _underload_time (0), _underload_count (0)
        , _is_demoted (false)
    {
        _logic->register_interface (this);
        //_logic->register_storage (s);

        // create a voronoi for checking ownership for arbitrators
        _arbitrator_vor = _vastworld->create_voronoi ();
    }
    
    arbitrator_impl::~arbitrator_impl ()
    {                  
        if (_storage != NULL)
            delete _storage;

        if (_vnode != NULL)
            _vastworld->destroy_node (_vnode);

        if (_arbitrator_vor != NULL)
            _vastworld->destroy_voronoi (_arbitrator_vor);

            ///delete _vnode;
    }

    //
    // arbitrator interface
    //
    
    bool 
    arbitrator_impl::join (id_t id, Position &pos, const Addr & entrynode)
    {
        // use VON to join 
        // TODO: AOI is forced to be very small initially as it should be dynamically adjustable
        //      (need to be fixed? or is actually okay?)
        _joined = true;
        _is_suspending = _is_leaving = false;

        Addr entrynode_n = entrynode;

        if (entrynode.id == NET_ID_UNASSIGNED)
            _vnode->join (id, 10, pos, _gateway);
        else
            _vnode->join (id, 10, pos, entrynode_n);

        self = _vnode->getself ();
        _arbitrators[self->id] = *self;
        _arbitrator_vor->insert (self->id, self->pos);

        // make sure storage class is initialized to generate unique query_id        
        _storage->init_id (self->id);

        return true;
    }

    // call the arbitrator/aggregator to leave the system
    bool 
    arbitrator_impl::leave (bool sleeping_only)
    {
        if (sleeping_only)
            _is_suspending = true;
        else
            _is_leaving = true;

        // remove myself from ownership map
        _arbitrators.erase (self->id);
        _arbitrator_vor->remove (self->id);

        // packing DELETE message
        msgtype_t msgt = ARBITRATOR;
        int size = sizeof (Node);;

        Node n (*self);
        n.aoi = -1;
        memcpy (_buf, &n, sizeof (Node));

        sprintf (_buf, "[%lu] Arbitrator tries to sleep. \n", self->id);
        _eo.output (_buf);

        // send DELETE node to all my neighboring nodes
        for (map<id_t, Node>::iterator arbi = _arbitrators.begin (); arbi != _arbitrators.end (); arbi ++)
            if (arbi->first != self->id)
                _net->sendmsg (arbi->first, msgt, _buf, size);

        // TODO: arbitrator: tell peer to leave?

        return false;
    }
        
    // process messages (send new object states to neighbors)
    int 
    arbitrator_impl::process_msg ()
    {
        // create a copy of current state
        if (self != NULL)
            _newpos = *self;

        // advance VON's logical time & also handle network messages
        _vnode->tick ();

        // advance logical time (TODO: here, or at the end?)
        // csc: clock advance move to net_emubridge
        //_time++;
        
        // to prevent sleeping node be called up
        // Potential BUG: should do like this way?
        if (!_joined && _vnode->is_joined ())
        {
            _vnode->leave ();
            // _net->stop ();
        }

        // if we have not joined the VON network, then do nothing
        if (!_joined || _vnode->is_joined () == false)
            return 0;

#ifndef RUNNING_MODE_CLIENT_SERVER
        // if gateway, need not to process any state management things
        if (_is_gateway)
            return 0;
#endif

        // locating parent object for futuring usage
        //locate_parent ();

        // update the list of arbitrators
        update_arbitrators ();

        // update ownership mapping
        refresh_voronoi ();

        // remove any invalid avatar objects 
        // (NOTE: non-AOI objects are removed via notification)
        validate_objects ();

        // process events in sequence
        process_event ();

        // perform some arbitrator logic (not provoked by events)
        _logic->tick ();

        // revise list of interested nodes of each owned object and send them updates
        update_interests ();

        // send all changed states to interested nodes (partial ones, full update is done in update_interests)
        send_updates ();

        // TODO: send regular full update for consistency fixing
        //send_regular_update ();

        // check to see if objects have migrated (TODO: should do it earlier?)
        check_owner_transfer ();

        // checking leaving
        if (_is_suspending)
        {
            if ((_obj_owned.size () == 0) && (_obj_in_transit.size () == 0) &&
                (_peers.size () == 0))
            {
                if (_obj_owned.size () > 0)
                {
                    sprintf (_str, "[%lu] tries to leave with owning objects (count: %d)\n", self->id, _obj_owned.size ());
                    _eo.output (_str);
                }

                sprintf (_str, "[%lu] Arbitrator slept. \n", self->id);
                _eo.output (_str);

                _joined = false;
                _is_suspending = false;
                _vnode->leave ();
                // TODO: more clear way to stop all connections?
                _net->stop ();
                _net->start ();

                _arbitrators.clear ();
                _known_neighbors.clear ();
                _arbitrator_vor->clear ();
                _nodes_knowledge.clear ();

                // Potential BUG: cleaning object model (should do this?)
                for (map<id_t, object *>::iterator obj_i = _obj_store.begin (); obj_i != _obj_store.end (); obj_i ++)
                {
                    object * obj = obj_i->second;

                    _logic->obj_deleted (obj);
                    delete obj;
                }

                //_parent_obj = NULL;
                _obj_store.clear ();
                _obj_owned.clear ();
                _obj_in_transit.clear ();
                _obj_update_time.clear ();

                return 0;
            }
        }
        /*
        else if (_is_leaving)
        {
            if ((_obj_owned.size () == 0) && (_obj_in_transit.size () == 0) &&
                (_peers.size () == 0) &&
                (_managing_nodes.size () == 0 && _managing_nodes_intrans.size () == 0)
                )
            {
                _joined = false;
                _vnode->leave ();

                // demote myself to leave
                _is_demoted = true;

                sprintf (_buf, "[%lu] Aggregator demoted. \n", self->id);
                _eo.output (_buf);

                return 0;
            }
            else
            {
                sprintf (_buf, "[%lu] Aggregator can't leaving due to owned objects %d (%d), peers %d, m_nodes %d (%d) \n", 
                    self->id, _obj_owned.size (), _obj_in_transit.size (), _peers.size (), _managing_nodes.size (), _managing_nodes_intrans.size ());
                _eo.output (_buf);
            }
        }
        */

        /* // position are adjusted in change_pos ()
        else
        {
            adjust_position ();
        }
        */

        // make adjustments to arbitrator AOI
        adjust_aoi ();

        // update the overlay
        _vnode->setAOI (_newpos.aoi);
        _vnode->setpos (_newpos.pos);

        // check for aggregation
        //check_aggregation ();

        // delete exipred arbitrator inserting record
        list<Node>::iterator it;
        do 
        {
            // find expired record
            for (it = _promoted_positions.begin (); it != _promoted_positions.end (); it ++)
                if (get_timestamp () - it->time >= COUNTDOWN_PROMOTE)
                    break;

            // if found, erase it
            if (it != _promoted_positions.end ())
            {
                _promoted_positions.erase (it);
                continue;
            }
            // else leave the loop
            else
                break;

        } while (1);

        // send out pending reliable messages
        _vnode->getnet ()->flush ();

        return 0;
    }


    // obtain any request to demote from arbitrator
    bool 
    arbitrator_impl::is_demoted (Node &info)
    {
        if (_is_demoted == false)
            return false;
        else
        {
            info = *_vnode->getself ();
            _is_demoted = false;
            return true;
        }
    }

    // this method is called by processmsg () in 'vnode'
    bool 
    arbitrator_impl::handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size)
    {
#ifdef DEBUG_DETAIL
        if (msgtype >= 100 && msgtype < VASTATE_MESSAGE_END)
        {
            sprintf (_str, "[%lu] arb handlemsg from [%lu]: (%d)%s \n", self->id, from_id, msgtype, VASTATE_MESSAGE[msgtype-100]);
            _eo.output (_str);
        }
#endif

        if ((_joined == false) &&
            (msgtype != JOIN) && (msgtype != MIGRATE))
            return false;

        switch (msgtype)
        {
        case JOIN:
            // check if message size is correct
            if ((signed) (size - sizeof (Msg_SNODE) - 1) == msg[sizeof (Msg_SNODE)])
            {
                // separate Msg_SNODE and app-specific message
                Msg_SNODE n (msg);
                size_t app_size = msg[sizeof (Msg_SNODE)];

                if (_is_gateway)
                {
                    // to prevent gateway server from over-connection
                    _net->disconnect (from_id);
                    
                    // record the peer's info for later use 
                    // (after arbitrator_logic has authenticated the peer)
                    _join_peers[from_id] = n;
                    
                    // call arbitrator_logic to handle the app-specific aspects of a join 
                    _logic->join_requested (from_id, msg + sizeof (Msg_SNODE)+1, app_size);
                }
                // if it's from a regular arbitrator, simply check for acceptance or forward
                else
                {
                    check_acceptance (from_id, n, msg, size);
                }
            }
            break;
            
        // peer enters a new region
        case ENTER:
            if (validate_size (size, sizeof (Node), sizeof (Msg_OBJ_UPDATEINFO)))
            {
                // record the peer in peer_list
                Node peer;
                char *p = msg;
                memcpy (&peer, p, sizeof (Node));
                _peers[peer.id] = peer;
                p += sizeof (Node);
                
                // loop through all object versions received, if it's up-to-date then
                // simply add the peer to the 'interested node' list 
                // we'll let 'update_interests' to take care of sending the 
                // new peer updated versions of objects 

                map<obj_id_t, version_pair_t> & pk = _nodes_knowledge[peer.id];
                pk.clear ();
                
                //Msg_OBJ_UPDATEINFO info;
                int n = p[0];
                Msg_OBJ_UPDATEINFO *uinfo = (Msg_OBJ_UPDATEINFO *) & (p[1]);

                for (int i=0; i<n; i++)
                {
                    //memcpy (&info, p, sizeof (Msg_OBJ_UPDATEINFO));
                    
                    // if this object is known (if it's not yet known -- meaning that 
                    // the arbitrator's AOI may need enlargement -- we simply ignore)

                    // if I don't know the object, skip it
                    if (_obj_store.find (uinfo[i].obj_id) == _obj_store.end ())
                        continue;

                    // if the peer's object is already up-to-date, simply add it to interested list
                    pk[uinfo[i].obj_id] = version_pair_t (uinfo[i].pos_version, uinfo[i].version);
				}

                // send a list of enclosing arbitrators to the peer
                /*
                char buffer[VASTATE_BUFSIZ];
                int len = encode_arbitrators (buffer);
                _net->sendmsg (peer.id, ARBITRATOR, buffer, len);
                */

                // updates for peers will sent when update_interests
                //send_objects (peer.id, false);
            }
            break;

        // Notification/request of object creation, destruction, update by other arbitrators
        // TODO/BUG: If request update come before OBJECT, position update will NOT occur.
        case OBJECT:
        case TRANSFER_OBJECT:
            if (size == sizeof (Msg_OBJECT))
            {                                         
                Msg_OBJECT info (msg);
                object *obj = (_obj_store.find (info.obj_id) != _obj_store.end () ? 
                               _obj_store[info.obj_id] : NULL);
                // object *obj = _obj_store[info.obj_id];
                bool newobj = false;

                
                if (info.pos_version == 0)
                {
                    // if I haven't the object, and it's a deletion, just ignore it
                    if (obj == NULL)
                       break;

                    // if someone try to delete my-owned object, ignore it
                    else if (is_owner (info.obj_id))
                        break;
                }

                // if the object is new, add to object store
                if (obj == NULL)
                {
                    newobj = true;
                    obj = new object(info.obj_id);
                }

                // update if a newer version update
                if (newobj || info.pos_version > obj->pos_version)
                {
                    // do actual update
                    obj->decode_pos (msg);

                    // also update the version
                    obj->pos_version = info.pos_version;
                }

                // update the update time of object (newobj's timestamp will assigned in store_obj)
                if (!newobj)
                    _obj_update_time [obj->get_id ()] = get_timestamp ();

                // position version 0 indicates deletion of an object
                if (info.pos_version == 0)
                    obj->mark_deleted ();

                // notify for the discovery of a new object
                else if (newobj)
                {
                    store_obj (obj, false);

#ifdef DEBUG_DETAIL
                    sprintf (_str, "[%lu] arb learns of new object [%s] from %lu\n", self->id, obj->tostring(), from_id);
                    _eo.output (_str);
#endif
                }
                
                // pass the update to interested peers
                // csc NOTE: notify when send_updates ()
                //notify_peers (_interested[info.obj_id], OBJECT, msg, size);

                // csc: move this to change_pos due to sequence change
                // update knowledge of peer positions (TODO: aoi update?)
                /*
                if (_peers.find (obj->peer) != _peers.end ())
                {
                    _peers[obj->peer].pos = obj->get_pos ();
                }
                */
            }
            break;

        // Notification/request of attribute creation and update by other arbitrators
        // NOTE: there's no size-check on the message as the length is variable
        case STATE:
        case TRANSFER_STATE:
            {
                Msg_STATE info (msg);
                //object *obj = _obj_store[info.obj_id];

                object *obj = (_obj_store.find (info.obj_id) != _obj_store.end () ? 
                               _obj_store[info.obj_id] : NULL);

                //char *p = msg + sizeof (Msg_STATE);

                // if STATE msg proceeds OBJECT, then abort
                if (obj == NULL)    
                    break;

#ifdef DEBUG_DETAIL
                sprintf (_str, "[%lu] STATE obj_id: %lu version: %d request: %s\n", self->id, info.obj_id, info.version, (info.is_request ? "true" : "false") );
                _eo.output (_str);
#endif

                // check if a newer version
                if (info.version > obj->version)
                {
                    // during unpacking, fields will be automatically added or updated
                    if (obj->decode_states (msg) == false)
                        break;

                    // also update the version
                    obj->version = info.version;                                                                                       

                    // update the update time of object
                    _obj_update_time [obj->get_id ()] = get_timestamp ();
                }

                _obj_update_time [obj->get_id ()] = get_timestamp ();
                // pass the update to interested peers
                // csc NOTE: notify when send_updates ()
                //notify_peers (_interested[info.obj_id], STATE, msg, size);
            }
            break;

        // Tick to others for arbitrators has no event sent this step
        /*
        case TICK_EVENT:
            if (size == sizeof (timestamp_t))
            {
                // check if message from my enclosing neighbor
                if (_arbitrators.find (from_id) != _arbitrators.end ())
                {
                    timestamp_t ts;
                    memcpy (&ts, msg, sizeof (timestamp_t));

                    // tick event just insert a empty vector into map for timestamp advance in process_event ()
                    if (_event_queue[from_id].find (ts) == _event_queue[from_id].end ())
                        _event_queue[from_id][ts] = vector<event *> ();
                }

#ifdef DEBUG_DETAIL
                // else output a error message
                else
                {
                    sprintf (_str, "[%d] receives a TICK_EVENT from not enclosing neighbor [%d]\n", self->id, from_id);
                    _eo.output (_str);
                }
#endif
            }
            break;
            */

        // Notification of a peer-generated event
        /*  Format:
                | (short) n | (short) TTL | (obj_id_t) Aff IDs | (event) e |
                n: number of affected objects
                TTL: Tive to Live for the event (to provent infinite forwarding)
                Aff IDs: IDs of objects affected by the event
                e: the event self
         */
        case EVENT:            
            if (size > (int) (sizeof(Msg_EVENT) + sizeof (Msg_EVENT_HEADER)))
            {
                ExtendedEvent ee;

                Msg_EVENT_HEADER * eh = (Msg_EVENT_HEADER *) msg;

                // double check if legal message size
                if (size < (int) (sizeof (Msg_EVENT) + sizeof (Msg_EVENT_HEADER) + eh->objid_count * sizeof (obj_id_t)))
                    break;

                ee.from_id = from_id;
                ee.TTL     = eh->TTL;
                ee.t_list  = new vector<obj_id_t> ();

                char * msg_p = msg + sizeof (Msg_EVENT_HEADER);

                // decode object id(s)
                for (short sc = 0; sc < eh->objid_count; ++ sc, msg_p += sizeof (obj_id_t))
                    ee.t_list->push_back (* (obj_id_t *) msg_p);

                // unpack data
                ee.e       = new event ();
                if (ee.e->decode (msg_p) == true)
                    _unforward_event.push_back (ee);
                else
                {
                    delete ee.e;
                    delete ee.t_list;
                }
            }
            break;

        // Overloaded arbitrator's request for inserting new arbitrator
        case OVERLOAD_I:
            if (_is_gateway)
            {
                int level;
                memcpy (&level, msg, sizeof (int));
                Position i_pos;
                memcpy (&i_pos, msg + sizeof (int), sizeof (Position));

                // check if a existing promotion position
                list<Node>::iterator it = _promoted_positions.begin ();
                for (; it != _promoted_positions.end (); it ++)
                {
                    if (it->pos.dist (i_pos) < 4 * sysparm->aoi)
                        break;
                }

                // if promoted at the same position before, just ignore this request
                if (it != _promoted_positions.end ())
                    break;

                while (true)
                {
                    // promote one of the spare potential arbitrators
                    Msg_SNODE new_arb;
                    if (find_arbitrator (level, new_arb) == false)
                    {
                        // TODO: promote virtual arbitrator
                        break;
                    }

                    // fill in the stressed arbitrators' info
                    Node n;
                    //n = _arbitrators[from_id];
                    n.id = from_id;
                    n.pos = i_pos;

                    // store it in inserting record
                    Node n_in = n;
                    n_in.time = get_timestamp ();
                    _promoted_positions.push_back (n_in);

                    Msg_SNODE requester;
                    requester.set (n, _net->getaddr (from_id), 0);
                    
                    // copy message to send buffer        
                    char buffer[VASTATE_BUFSIZ];
                    memcpy (buffer, &requester, sizeof (Msg_SNODE));

                    // send a one-time only promotion message
                    bool o_conn_state = _net->is_connected (new_arb.node.id);
                    if (!o_conn_state)
                        //_net->connect (new_arb.node.id, new_arb.addr);
                        _net->connect (new_arb.addr);
                    int send_size = _net->sendmsg (new_arb.node.id, PROMOTE, buffer, sizeof (Msg_SNODE));
                    if (!o_conn_state)
                        _net->disconnect (new_arb.node.id);

                    // quit if send successful
                    if (send_size > 0)
                        break;
                }
            }
            else
            {
                sprintf (_str, "[%lu] receives OVERLOAD_I but not gateway\n", self->id);
                _eo.output (_str);
            }
            break;

        // Overloaded arbitrator's request for moving closer
        case OVERLOAD_M:
            // Sever promotes a peer among a list of candidates by sending a join location and a contact.
            // unused
            /*
            {
                // received not my enclosing neighbor's help signal
                if (_arbitrators.find (from_id) == _arbitrators.end ())
                {
                    sprintf (_str, "[%d] receives OVERLOAD from non-enclosing node [%d]\n", self->id, from_id);
                    _eo.output (_str);
                    break;
                }

                // calculate new position after moving closer to the stressed arbitrator
                Position & arb = _arbitrators[from_id].pos;
                Position temp_new_pos = _newpos.pos;

                // move in 1/10 of the distance between myself and the stressed arbitrator
                temp_new_pos.x = _newpos.pos.x + (arb.x - _newpos.pos.x) * 0.10;
                temp_new_pos.y = _newpos.pos.y + (arb.y - _newpos.pos.y) * 0.10;

                if (is_legal_position (temp_new_pos, false))
                    _newpos.pos = temp_new_pos;
            }
            */
            break;

        // Underloaded arbitrator's request for region adjustment
        case UNDERLOAD:
            // unused
            /*
            {
                // received not my enclosing neighbor's help signal
                if (_arbitrators.find (from_id) == _arbitrators.end ())
                {
#ifdef DEBUG_DETAIL
                    sprintf (_str, "[%d] receives UNDERLOAD from non-enclosing node [%d]\n", self->id, from_id);
                    _eo.output (_str);
#endif
                    break;
                }

                // calculate new position after moving closer to the stressed arbitrator
                Position & arb = _arbitrators[from_id].pos;
                Position temp_new_pos = _newpos.pos;
              
                // move in 1/10 of the distance between myself and the stressed arbitrator
                temp_new_pos.x = temp_new_pos.x - (arb.x - temp_new_pos.x) * 0.10;
                temp_new_pos.y = temp_new_pos.y - (arb.y - temp_new_pos.y) * 0.10;

                if (is_legal_position (temp_new_pos, false))
                    _newpos.pos = temp_new_pos;
            }
            */
            break;

        // Notification of an arbitrator goes to off-line
        case ARBITRATOR_LEAVE:
            if (size == sizeof (id_t))
            {
                // get parent id
                id_t his_parent;
                memcpy ((void *) &his_parent, msg, sizeof (id_t));

                // check if need to decrease promotion count
                if (_is_gateway && his_parent != NET_ID_GATEWAY)
                {
                    // find position of the record
                    for (int index = 0; index < (int) _potential_arbitrators.size (); index ++)
                    {
                        if (_potential_arbitrators[index].node.id == his_parent)
                        {
                            _promotion_count[index] --;
                            break;
                        }
                    }
                }

                // check if any object I should be owner after the arbitrator leaves
                /*
                if (_arbitrators.find (from_id) != _arbitrators.end ())
                {
                    for (map<obj_id_t, object *>::iterator it = _obj_store.begin (); it != _obj_store.end (); it ++)
                    {
                        object * obj = it->second;
                        if (_vnode->getvoronoi ()->contains (from_id, obj->get_pos ()))
                        {
                            Msg_TRANSFER mtr;
                            mtr.obj_id = obj->get_id ();
                            mtr.new_owner = self->id;
                            mtr.orig_owner = NET_ID_UNASSIGNED;

                            _net->sendmsg (self->id, TRANSFER, (char *) & mtr, sizeof (Msg_TRANSFER));
                        }
                    }
                }
                */
            }
            break;
            
        // Ownership transfer from old to new owner
        case TRANSFER:
            //if (validate_size (size, sizeof (obj_id_t) + sizeof (id_t), 0))
            if (size == sizeof (Msg_TRANSFER))
            {
                unpack_transfer (msg);
            }

            break;

        case TRANSFER_ACK:
            if (size == sizeof (obj_id_t))
            {
                obj_id_t obj_id;
                memcpy (&obj_id, msg, sizeof (obj_id_t));
                _obj_in_transit.erase (obj_id);
            }
            break;

        // Auto ownership assumption if arbitrators fail.
        case NEWOWNER:
            break;

        // informing about new arbitrator
        case ARBITRATOR:
            if (size == sizeof (Node))
            {
                Node * new_arbnode = (Node *) msg;

                // if a delete request
                if (new_arbnode->aoi == -1)
                {
                    if (_arbitrators.find (new_arbnode->id) != _arbitrators.end ())
                    {
                        _arbitrators.erase (new_arbnode->id);
                        _nodes_knowledge.erase (new_arbnode->id);
                    }

                    break;
                }

                // insert and update arbitrator information
                if (_arbitrators.find (new_arbnode->id) == _arbitrators.end ())
                    _arbitrator_vor->insert (new_arbnode->id, new_arbnode->pos);
                else
                    _arbitrator_vor->update (new_arbnode->id, new_arbnode->pos);
                _arbitrators[new_arbnode->id] = *new_arbnode;
            }
            break;

        case DISCONNECT:
            {
                // remove any disconnected peers (opposte of ENTER)
                if (_peers.find (from_id) != _peers.end ())
                {
                    _peers.erase (from_id);
                    _nodes_knowledge.erase (from_id);
                }
            }
            // always return false for DISCONNECT
            return false;

        default:                
            return false;            
        }
        return true;
    }


    // create or delete a new object (can only delete if I'm the owner)
    object *
    arbitrator_impl::create_obj (Position &pos, id_t peer_id, void *p, size_t size)
    {        
        object *obj = new object ((self->id << 16) | _obj_id_count++);
        obj->set_pos (pos);
        obj->peer         = peer_id;
        obj->version      = 1;
        obj->pos_version  = 1;
        obj->pos_dirty    = true;
        obj->dirty        = true;
        
#ifdef DEBUG_DETAIL
        sprintf (_str, "[%lu] creates obj [%s]\n", self->id, obj->tostring());
        _eo.output (_str);
#endif

        // require to initialize
        _logic->obj_created (obj, p, size);

        store_obj (obj, true);
        
        return obj;
    }

    
    bool
    arbitrator_impl::delete_obj (object *obj)
    {
        // fail if the object doesn't exist or we don't have ownership
        if (_obj_store.find (obj->get_id ()) == _obj_store.end () ||
            is_owner (obj->get_id ()) == false)
            return false;

        // mark as 'to be deleted' to be processed later during send_updates ()
        obj->pos_dirty = true;
        obj->mark_deleted ();

        return true;
    }


    // create an update message, then send to the respective owner (could be myself)
    bool 
    arbitrator_impl::update_obj (object *obj, int index, int type, void *value)
    {
        // forbbiden to modify un-owned objects (should do this?)
        if (!is_owner (obj->get_id ()))
            return false;

        // apply update into object
        switch (type)
        {
        case VASTATE_ATT_TYPE_BOOL:
            obj->set (index, *((bool *)value));
            break;
        case VASTATE_ATT_TYPE_INT:
            obj->set (index, *((int *)value));
            break;

        case VASTATE_ATT_TYPE_FLOAT:
            obj->set (index, *((float *)value));
            break;

        case VASTATE_ATT_TYPE_STRING:
            {
                string str = (char *)value;
                obj->set (index, str);
            }
            break;

        case VASTATE_ATT_TYPE_VEC3:
            obj->set (index, *((vec3_t *)value));
            break;
        }
        
        // increase version number
        obj->version++;
        return true;
    }


    // create an update message, then send to the respective owner (could be myself)
    bool  
    arbitrator_impl::change_pos (object *obj, Position &newpos)
    {
        // forbbiden to modify un-owned objects (should do this?)
        if (!is_owner (obj->get_id ()))
            return false;

        // if it's mark_deleted, cant do more update
        if (!obj->is_alive ())
            return false;

        // update object by new position
        obj->set_pos (newpos);

        // update peer's position information
        if (obj->peer != 0 && _peers.find (obj->peer) != _peers.end ())
        {
            _peers[obj->peer].pos = obj->get_pos ();
        }

        // adjust arbitrator's position to follow up parent object
        if (parent != NET_ID_UNASSIGNED && parent == obj->peer)
            _newpos.pos = obj->get_pos ();

        // increase version number
        obj->pos_version ++;
        return true;
    }
    
    
    // arbitrator overloaded, call for help
    // note that this will be called continously until the situation improves
    bool
    arbitrator_impl::overload (int level)
    {
        /*
        if (is_aggregator ())
        {
            // adjust aoi to balancing the load
            if (_net->get_curr_timestamp () - _aggnode.aoi_b_lastmodify >= AGGREGATOR_AOI_ADJ_CD)
            {
                aoi_t old_aoi_b = _aggnode.aoi_b;
                _aggnode.aoi_b -= AGGREGATOR_AOI_ADJ_UNIT;
                if (_aggnode.aoi_b <= (aoi_t) ((double) sysparm->aoi * AGGREGATOR_AOI_MIN))
                    _aggnode.aoi_b = (aoi_t) ((double) sysparm->aoi * AGGREGATOR_AOI_MIN);

                // check if modified
                if (old_aoi_b != _aggnode.aoi_b)
                {
                    _aggnode.aoi_b_lastmodify = _net->get_curr_timestamp ();
                    _aggnode_dirty = true;
                }
            }
        }
        else
        */
        {
            //char buffer[128];
            vector<Position> insert_pos;

            if (get_timestamp () - _overload_time >= THRESHOLD_LOAD_COUNTING)
                _overload_count = 0;

            //_overload_count ++;
            _overload_count += level;
            _overload_time = get_timestamp ();

            if (_overload_count % 5 == 0)
            {
                memcpy (_buf, &level, sizeof (int));
                memcpy (_buf + sizeof (int), &self->pos, sizeof (Position));

                bool o_conn_state = _net->is_connected (NET_ID_GATEWAY);
                if (!o_conn_state)
                    _net->connect (_gateway);
                _net->sendmsg (NET_ID_GATEWAY, OVERLOAD_I, (char *)&_buf, sizeof (int) + sizeof (Position));
                if (!o_conn_state)
                    _net->disconnect (NET_ID_GATEWAY);

                return true;
            }
        }

        return false;

            //if (_overload_count % 5 == 0)
            //{
                // try to do a inserting new arbitrator first
                // find a proper insertion position for new arbitrator
                //vector<line2d> & lines = _vnode->getvoronoi ()->getedges ();
                /*
                set<int> & edges = _vnode->getvoronoi ()->get_site_edges (self->id);
                for (set<int>::iterator it = edges.begin (); it != edges.end (); it ++)
                {
                    int i = *it;

                    // if a legal position, insert it
                    //   note: may have replicate
                    Position pos (lines[i].seg.p1.x, lines[i].seg.p1.y);
                    if (is_legal_position (pos))
                    {
                        // suppose
                        if (pos == Position (0,0))
                            continue;

                        insert_pos.push_back (pos);
                    }
        

                    Position pos1 (lines[i].seg.p2.x, lines[i].seg.p2.y);
                    if (is_legal_position (pos1))
                    {
                        if (pos1 == Position (0,0))
                            continue;

                        insert_pos.push_back (pos1);
                    }

                        
                }

                // find inserting position from edge positions
                for (int i = 0; i < 4; i ++)
                {
                    Position pos (self->pos);
                    double values[] = {0, 0, sysparm->width - 1, sysparm->height - 1};
                    if (i % 2 == 0)
                        pos.x = values[i];
                    else
                        pos.y = values[i];

                    if (_vnode->getvoronoi ()->contains (self->id, pos)
                        && is_legal_position (pos))
                        insert_pos.push_back (pos);
                }

                // if at least one proper position
                if (insert_pos.size () > 0)
                {
                    // get a random proper position
                    //int r = rand () % insert_pos.size ();
                    int r = self->id % insert_pos.size ();

                    memcpy (buffer, &level, sizeof (int));
                    memcpy (buffer + sizeof (int), &insert_pos[r], sizeof (Position));

                    // need?
                    bool o_conn_state = _net->is_connected (NET_ID_GATEWAY);
                    if (!o_conn_state)
                        //_net->connect (NET_ID_GATEWAY, _gateway);
                        _net->connect (_gateway);
                    _net->sendmsg (NET_ID_GATEWAY, OVERLOAD_I, (char *)&buffer, sizeof (int) + sizeof (Position));
                    if (!o_conn_state)
                        _net->disconnect (NET_ID_GATEWAY);

                    return true;
                }
                */
            //}

            // else call enclosing neighbor more closer

            /*
            // get a list of my enclosing arbitrators
            vector<id_t> &list = _vnode->getvoronoi()->get_en (self->id);

            // send to each enclosing arbitrator a stress signal except myself (does get_en return myself ?)
            for (int i=0; i < (int)list.size (); i++)
            {
                if (list[i] == self->id)
                    continue;

                _net->sendmsg (list[i], OVERLOAD_M, (char *)&level, sizeof(int));
            }
            */
    }


    // arbitrator underloaded, will possibly depart as arbitrator
    bool 
    arbitrator_impl::underload (int level)
    {
        /*
        if (is_aggregator ())
        {
            if (_net->get_curr_timestamp () - _aggnode.aoi_b_lastmodify >= AGGREGATOR_AOI_ADJ_CD)
            {
                aoi_t old_aoi_b = _aggnode.aoi_b;
                _aggnode.aoi_b += AGGREGATOR_AOI_ADJ_UNIT;
                if (_aggnode.aoi_b >= (aoi_t) ((double) sysparm->aoi * AGGREGATOR_AOI_MAX))
                {
                    _aggnode.aoi_b = (aoi_t) ((double) sysparm->aoi * AGGREGATOR_AOI_MAX);
                    if (_net->get_curr_timestamp () - _aggnode.aoi_b_lastmodify >= AGGREGATOR_LEAVE_TIME)
                        leave ();
                }

                // check if modified
                if (old_aoi_b != _aggnode.aoi_b)
                {
                    _aggnode.aoi_b_lastmodify = _net->get_curr_timestamp ();
                    _aggnode_dirty = true;
                }
            }
        }
        */
        // Nothing to do if underloaded for an arbitrator (actually, there's pretty good for system)
        /*
        else
        {
            if (get_timestamp () - _underload_time >= THRESHOLD_LOAD_COUNTING)
                _underload_count = 0;

            _underload_count ++;
            _underload_time = get_timestamp ();

            if (_underload_count <= 2)
            {
                // get a list of my enclosing arbitrators
                vector<id_t> &list = _vnode->getvoronoi()->get_en (self->id);

                // send to each enclosing arbitrator a stress signal except myself
                for (int i=1; i<(int)list.size (); i++)
                    _net->sendmsg (list[i], UNDERLOAD, (char *)&level, sizeof(int));
            }
            else
            {
                if (!_is_gateway)
                {
                    // mark im demoted
                    _is_demoted = true;

                    // send a message to server to decrease promote count
                    bool o_conn_state = _net->is_connected (NET_ID_GATEWAY);
                    if (!o_conn_state)
                        //_net->connect (NET_ID_GATEWAY, _gateway);
                        _net->connect (_gateway);
                    _net->sendmsg (NET_ID_GATEWAY, ARBITRATOR_LEAVE, (char *) &parent, sizeof (id_t));
                    if (!o_conn_state)
                        _net->disconnect (NET_ID_GATEWAY);
                }

                // reset counter (we assume this will solve our problem)
                _underload_count = 0;
            }
        }
        */
        return true;
    }


    // called by the gateway arbitrator to continue the process of admitting a peer  
    // after the joining peer has been authenticated
    bool 
    arbitrator_impl::insert_peer (id_t peer_id, void *initmsg, size_t size)
    {
        Msg_SNODE n = _join_peers[peer_id];        

        //printf ("[%d] node [%d] join location: (%d, %d) capacity: %d\n", self->id, joinerjor.pos.x, joiner.pos.y, n.capacity);
        
        // Server records the joining peer if it qualifies as a potential arbitrat
        if (_is_gateway && n.capacity > THRESHOLD_ARBITRATOR)
        {
#ifdef DEBUG_DETAIL
            sprintf (_str, "node [%lu] selected as potential arbitrator (capacity: %d)\n", peer_id, n.capacity);
            _eo.output (_str);
#endif
                
            _potential_arbitrators.push_back (n);
            _promotion_count.push_back (0);
        }

        _join_peers.erase (peer_id);

        // prepare a JOIN message to check for acceptance (whether the peer is within my region)
        memcpy (_buf, &n, sizeof (Msg_SNODE));
        _buf[sizeof (Msg_SNODE)] = size;
        if (size > 0)
            memcpy (_buf + sizeof(Msg_SNODE) + 1, initmsg, size);

        return check_acceptance (peer_id, n, _buf, sizeof(Msg_SNODE) + 1 + size);
    }

    bool 
    arbitrator_impl::send_peermsg (vector<id_t> &peers, char *msg, size_t size)
    {
        return true;
    }

    //
    // private helper methods
    //

    void
    arbitrator_impl::store_obj (object *obj, bool is_owner)
    {        
        // store to repository
        _obj_store[obj->get_id ()]  = obj;
        _obj_update_time [obj->get_id ()] = get_timestamp ();

        if (obj->peer != 0)
            _peer2obj [obj->peer] = obj;
        //_interested[obj->get_id ()] = new map<id_t, version_t>;
        if (is_owner)
            _obj_owned[obj->get_id ()]  = true;
        
        // TODO/BUG: object may has not any attribute(s) at this this time
        _logic->obj_discovered (obj);
    }

    void 
    arbitrator_impl::unstore_obj (obj_id_t obj_id)
    {
        if (_obj_store.find (obj_id) == _obj_store.end ())
            return;

        object *obj  = _obj_store[obj_id];
        id_t peer_id = _obj_store[obj_id]->peer;

        // check if is my parent object (should not happened)
        //if (obj == _parent_obj)
        //    _parent_obj = NULL;

        // notify object deletion
        _logic->obj_deleted (obj);

        // check if the object my parent (possible?)
        //if (obj == _parent_obj)
        //    _parent_obj = NULL;

        // remove from repository
        delete obj; 
        _obj_store.erase (obj_id);
        _obj_update_time.erase (obj_id);

        if (_obj_owned.find (obj_id) != _obj_owned.end ())
            _obj_owned.erase (obj_id);

        if (peer_id != 0)
            _peer2obj.erase (peer_id);

        /*
        if (_interested.find (obj_id) != _interested.end ())
        {
            delete _interested[obj_id];
            _interested.erase (obj_id);
        }
        */
    }

    bool 
    arbitrator_impl::check_acceptance (id_t from_id, Msg_SNODE &n, char *msg, size_t size)
    {
        Node &joiner = n.node;

        // finds the managing arbitrator of the peer via greedy forward
        id_t closest_id = (id_t)_vnode->getvoronoi()->closest_to (joiner.pos);

        // forward the request if a more appropriate node exists
        if (_vnode->getvoronoi ()->contains (self->id, joiner.pos) == false &&
            closest_id != self->id &&
            closest_id != from_id) 
        {            
            _net->sendmsg (closest_id, JOIN, msg, size);
        }
        else
        {
#ifdef DEBUG_DETAIL
            sprintf (_str, "[%lu] arbitrator (%f, %f) has taken [%lu] (%f, %f)\n", self->id, self->pos.x, self->pos.y, joiner.id, joiner.pos.x, joiner.pos.y);
            _eo.output (_str);
#endif

            // create the avatar object
            size_t initmsg_size = msg[sizeof (Msg_SNODE)];
            //object *obj = create_obj (joiner.pos, joiner.id, (initmsg_size > 0 ? (msg + sizeof (Msg_SNODE) + 1) : NULL), initmsg_size);
            create_obj (joiner.pos, joiner.id, (initmsg_size > 0 ? (msg + sizeof (Msg_SNODE) + 1) : NULL), initmsg_size);
                                
            // notify the joining node of my presence
            //_net->connect (joiner.id, n.addr);
            _net->connect (n.addr);

            encode_arbitrators (_buf, true);
            _net->sendmsg (joiner.id, ARBITRATOR, _buf, 1 + sizeof (Msg_SNODE));
        }

        return true;
    }

    // see if any of my objects should transfer ownership
    // or if i should claim ownership to any new objects (if neighboring arbitrators fail)
    int 
    arbitrator_impl::check_owner_transfer ()
    {        
        vector <obj_id_t> remove_list;      // objects I own to be transfer to others

        // loop through all ownership in transit objects
        // potential BUG: disconnected but not receiving the ack of ownership transfer
        //                then I re-take ownership to make one single object has two different owner (may happen?)
        for (map<obj_id_t, timestamp_t>::iterator it = _obj_in_transit.begin(); it != _obj_in_transit.end (); it ++)
        {
            if (get_timestamp () - it->second >= COUNTDOWN_TAKEOVER)
            {
                Msg_TRANSFER mtra;
                mtra.obj_id = it->first;
                mtra.new_owner = self->id;
                mtra.orig_owner = NET_ID_UNASSIGNED;
                
                _net->sendmsg (self->id, TRANSFER, (char *) &mtra, sizeof (Msg_TRANSFER));

                remove_list.push_back (it->first);
            }
        }

        // remove objects after takeover
        for (vector<obj_id_t>::iterator it = remove_list.begin (); it != remove_list.end (); it ++)
            _obj_in_transit.erase (*it);

        // clean remost_list for later use
        remove_list.clear ();

        // for short
        //voronoi *v = _arbitrator_vor;
        voronoi* &v = _arbitrator_vor;

        // loop through all objects I know
        //for (map<obj_id_t, object *>::iterator it = _obj_store.begin (); it != _obj_store.end (); ++it)
        for (map<obj_id_t, bool>::iterator it = _obj_owned.begin (); it != _obj_owned.end (); it ++)
        {
            //object *obj = it->second;
            object *obj;
//#ifdef DEBUG_DETAIL
            if (_obj_store.find (it->first) == _obj_store.end ())
            {
                sprintf (_buf, "[%lu] CRITICAL: has ownership of unknown object [%lX].\n", self->id, it->first);
                _eo.output (_buf);
                continue;
            }
            else
//#endif
                obj = _obj_store[it->first];

            id_t id     = obj->get_id ();

            // if this object is no longer within my region, then send message to new owner
            if (is_owner (id) == false)
                //|| v->contains (self->id, obj->get_pos ()) == true)
                continue;

            id_t closest = v->closest_to (obj->get_pos ());

            // continue if need not to transfer ownership
            if (closest == self->id)
                continue;

            /*
            // prevent my parent object to be transferred (should do this?)
            if (parent != NET_ID_UNASSIGNED && obj->peer == parent)
                continue;

            if (parent != NET_ID_UNASSIGNED && obj->peer == parent)
            {
                sprintf (_str, "[%lu] try to transfer parent object [%lX] ownership to [%lu].\n", self->id, it->first, closest);
                _eo.output (_str);
            }
            */

            // transfer ownership (force send an copy of object to ensure consistency)
            if (send_ownership (obj, closest, true))
            {
                remove_list.push_back (id);
                _obj_in_transit [obj->get_id ()] = get_timestamp ();
            }

            // there's an object within my region but I'm not owner, 
            // should claim it after a while
            /*
            else if (is_owner (id) == false)
            {
                // initiate a countdown, and if the countdown is reached, 
                // automatically claim ownership (we assume neighboring arbitrators will
                // have consensus over ownership eventually, as topology info becomes consistent)
                if (_owner_countdown.find (id) == _owner_countdown.end ())
                    _owner_countdown[id] = 1 + rand () % COUNTDOWN_TAKEOVER;

                else if (_owner_countdown[id] > 0)
                    _owner_countdown[id]--;

                else
                {
                    // we claim ownership
                    //_obj_owned[id] = true;
                    // send a transfer message to myself to invoke broadcasting
                    Msg_TRANSFER mtra;
                    mtra.obj_id = id;
                    mtra.new_owner = self->id;
                    mtra.orig_owner = NET_ID_UNASSIGNED;
                    
                    _net->sendmsg (self->id, TRANSFER, (char *) &mtra, sizeof (Msg_TRANSFER));
                    _owner_countdown.erase (id);

                    // BUG/TODO: a little buggy here, we do not remove unfinished countdown
                }
            }
            */
        }

        // remove the transferred objects from the list of owned objects
        for (int i=0; i<(int)remove_list.size (); i++)
        {
            // NOTE: the object should remain in the interest list, as some nodes might 
            // still be interested in the object, so simply remove ownership
            _obj_owned.erase (remove_list[i]);
        }

        // check for all objects in nodes_knowledge is I owned
        vector<pair<id_t, obj_id_t> > remove_obj_list;
        for (map<id_t, map<obj_id_t, version_pair_t> >::iterator nit = _nodes_knowledge.begin (); nit != _nodes_knowledge.end (); nit ++)
            for (map<obj_id_t, version_pair_t>::iterator oit = nit->second.begin (); oit != nit->second.end (); oit ++)
                //if (! is_owner (oit->first))
                if (_obj_store.find (oit->first) == _obj_store.end ())
                    remove_obj_list.push_back (pair<id_t, obj_id_t> (nit->first, oit->first));

        for (vector<pair<id_t, obj_id_t> >::iterator rit = remove_obj_list.begin (); rit != remove_obj_list.end (); rit ++)
            _nodes_knowledge[rit->first].erase (rit->second);

        return remove_list.size ();
    }


    // transfer ownership to specified node
    bool 
    arbitrator_impl::send_ownership (object *obj, id_t target, bool send_object)
    {
        int size, obj_size, states_size;

        if ((obj_size = obj->encode_pos (_buf, false)) > 0)
            _net->sendmsg (target, TRANSFER_OBJECT, _buf, obj_size);

        if ((states_size = obj->encode_states (_buf, false)) > 0)
            _net->sendmsg (target, TRANSFER_STATE, _buf, states_size);

        size = pack_transfer (obj, target, _buf);
        if (_net->sendmsg (target, TRANSFER, _buf, size) != size)
        {
            sprintf (_str, "[%lu] Failed to transfer ownership of object [%lx] to [%lu]\n", self->id, obj->get_id (), target);
            _eo.output (_str);
            return false;
        }

#ifdef DEBUG_DETAIL
            sprintf (_str, "[%lu] Transfer ownership of object [%lx] to [%lu]\n", self->id, obj->get_id (), target);
            _eo.output (_str);
#endif

        return true;
    }


//#define MIN_ARB_MOVE_DIST (sysparm->aoi)
//#define MIN_ARB_MOVE_DIST (1)
    // make adjustments of arbitrator's position
    /*
    void 
    arbitrator_impl::adjust_position ()
    {
        if ((_parent_obj == NULL && parent != NET_ID_UNASSIGNED)
            || (_parent_obj != NULL && parent != _parent_obj->peer))
        {
            // find my parent object (peer_id == parent) in my object_store
            _parent_obj = NULL;
            object *obj = NULL;
            for_each_object (it)
            {
                obj = it->second;
                if (obj->peer == parent)
                {
                    _parent_obj = obj;
                    break;
                }
            }
        }

        // if my parent object exists
        if (_parent_obj != NULL)
        {
            // adjust arbitrator's position to parent object's position
            // on distance far than MIN_ARB_MOVE_DIST
            //if (_newpos.pos.dist (_parent_obj->get_pos ()) >= MIN_ARB_MOVE_DIST)
                _newpos.pos = _parent_obj->get_pos ();
        }
    }
    */

    // make adjustments to arbitrator AOI
    void 
    arbitrator_impl::adjust_aoi ()
    {
        // calculate a new AOI according to the merged AOI of all currently connected peers
        // basically, find the lower-left and upper right corner of the radius of all peers
                
        // start with a very small AOI then expands it accordingly to peers' actual AOI
        // Protential BUG: too small AOI when managing no peers
        _newpos.aoi = 5;

        for (map<id_t, Node>::iterator it = _peers.begin (); it != _peers.end (); it++)
        {
            Node &n = it->second;
            
            // check if the current AOI covers this peer, expand it if not
            double dist = n.pos.dist (_newpos.pos) + n.aoi;
            if (dist >= _newpos.aoi)
                _newpos.aoi = (aoi_t)(dist * (1.05));
        }
    }

    // check with VON to refresh current connected arbitrators
    void 
    arbitrator_impl::update_arbitrators ()
    {
        vector<Node *> &nodes = _vnode->getnodes ();
        //vector<id_t> &ens = _vnode->getvoronoi ()->get_en (self->id);
        map<id_t, Node *> node_map;
        map<id_t, bool>   new_node;

        // loop through new list of arbitrators and update my current list
        int i;
        bool has_changed = false;
        bool packed = false;
        //int packedsize;

        for (i=0; i < (int) nodes.size (); ++i)
        {
            id_t& id = nodes[i]->id;

            // don't send my position to myself
            if (!_is_leaving && 
                !_is_suspending && 
                id != self->id)
            {
                // Here sends arb/agg list only for new discoveryed nodes, list updating will be 
                if (!exists_in_map (_known_neighbors, id) || 
                    _net->get_curr_timestamp () - _arbitrators_lastupdate >= ARBITRATOR_FULLUPDATE_PERIOD)
                {
                    _net->sendmsg (id, ARBITRATOR, (char *) self, sizeof(Node));
                    _known_neighbors[id] = *(nodes[i]);
                    new_node[id] = true;
                }
            }

            // if an known arbitrator, update information
            if (_arbitrators.find (id) != _arbitrators.end ())
            {
                // update the arbitrator's info from overlay
                _arbitrators[id].aoi = nodes[i]->aoi;

                if (_arbitrators[id].pos != nodes[i]->pos)
                {
                    _arbitrators[id].pos = nodes[i]->pos;
                    _arbitrator_vor->update (id, nodes[i]->pos);
                    //has_changed = true;
                }
            }

            // create map for later removal
            node_map[id] = nodes[i];
        }

        if (_net->get_curr_timestamp () - _arbitrators_lastupdate >= ARBITRATOR_FULLUPDATE_PERIOD)
            _arbitrators_lastupdate = _net->get_curr_timestamp ();
        
        // loop through current list of arbitrators to remove those no longer connected
        vector<id_t> remove_list;
        for (map<id_t, Node>::iterator it = _arbitrators.begin (); it != _arbitrators.end (); ++it)
        {
            if (node_map.find (it->first) == node_map.end ())
                remove_list.push_back (it->first);
        }

        for (map<id_t, Node>::iterator it = _known_neighbors.begin (); it != _known_neighbors.end (); it ++)
        {
            if (node_map.find (it->first) == node_map.end ())
                remove_list.push_back (it->first);
        }

        for (i=0; i<(int)remove_list.size (); ++i)
        {
            _known_neighbors.erase (remove_list[i]);
            _nodes_knowledge.erase (remove_list[i]);

            if (_arbitrators.find (remove_list[i]) != _arbitrators.end ())
            {
                // do it before _arbitrators list updates
                send_objects (remove_list[i], true, true);

                _arbitrators.erase (remove_list[i]);
                _arbitrator_vor->remove (remove_list[i]);

#ifdef DEBUG_DETAIL
                sprintf (_str, "[%lu] removes old enclosing arbitrator [%lu]\n", self->id, remove_list[i]);
                _eo.output (_str);
#endif
            }
#ifdef DEBUG_DETAIL
            else
            {
                sprintf (_str, "[%lu] update_arbitrator (): bad remove list id [%lu]\n", self->id, remove_list[i]);
                _eo.output (_str);
            }
#endif
        }

        // notify peers of arbitrator change
        // TODO: customize the notification for each peer, right now we simply re-send
        /*
        if ((has_changed || i > 0) && _peers.size () > 0)
        {
            char buffer[VASTATE_BUFSIZ];
            int len = encode_arbitrators (buffer);
            
            for (map<id_t, Node>::iterator it = _peers.begin (); it != _peers.end (); it++)
                _net->sendmsg (it->first, ARBITRATOR, buffer, len);
        }
        */
    }

/*
    void 
    arbitrator_impl::locate_parent ()
    {
        if (is_aggregator ())
        {
            for (map<id_t, ArbRecord>::iterator it = _managing_nodes.begin (); it != _managing_nodes.end (); it ++)
            {
                //const id_t & node_id = it->first;
                ArbRecord & are = it->second;

                // find parect object if it's null or not matching to parent_id
                if ((are.parent_obj == NULL && are.parent_id != NET_ID_UNASSIGNED) ||
                    (are.parent_obj != NULL && are.parent_id != are.parent_obj->get_id ()))
                {
                    if (_obj_store.find (are.parent_id) != _obj_store.end ())
                        are.parent_obj = _obj_store[are.parent_id];
                    else
                        are.parent_obj = NULL;
                }

                // if known parent_obj, update the position record
                if (are.parent_obj != NULL && are.node.pos != are.parent_obj->get_pos ())
                {
                    are.node.pos = are.parent_obj->get_pos ();
                    _arbitrator_vor->update (are.node.id, are.node.pos);
                    _aggnode_dirty = true;
                }
            }
        }
        else
        {
            if ((_parent_obj == NULL && parent != NET_ID_UNASSIGNED)
                || (_parent_obj != NULL && parent != _parent_obj->peer))
            {
                // find my parent object (peer_id == parent) in my object_store
                _parent_obj = NULL;
                object *obj = NULL;
                for_each_object (it)
                {
                    obj = it->second;
                    if (obj->peer == parent)
                    {
                        _parent_obj = obj;
                        break;
                    }
                }
            }
        }
    }
    */

    // send aggregator and managing node list to a specified node
    /*
    int 
    arbitrator_impl::pack_aggregator (char *buf, bool pack_delete)
    {
        char * buf_p = buf;

        memcpy (buf_p, & _aggnode, sizeof (AggNode));
        buf_p += sizeof (AggNode);
        if (pack_delete)
            // use aoi = -1 to indicates delete the node
            ((AggNode *) buf)->aoi = -1;

        unsigned short count;
        if (pack_delete)
            count = 0;
        else
            count = (unsigned short) _managing_nodes.size ();
        memcpy (buf_p, &count, sizeof (unsigned short));
        buf_p += sizeof (unsigned short);

        int packed_size = sizeof (AggNode) + sizeof (unsigned short) + sizeof(Node) * count;

        if (!pack_delete)
        {
            for (map<id_t, ArbRecord>::iterator it = _managing_nodes.begin (); it != _managing_nodes.end (); it ++)
            {
                memcpy (buf_p, &(it->second.node), sizeof (Node));
                buf_p += sizeof (Node);
            }
        }

        return packed_size;
    }

    // unpack and store information about the aggregator
    int
    arbitrator_impl::unpack_aggregator (char *buf, int size)
    {
        char * buf_p = buf;

        // get AggNode
        AggNode *a = (AggNode *) buf_p;
        buf_p += sizeof (AggNode);

        // if a delete request
        if (a->aoi == -1)
        {
            if (_aggregators.find (a->id) != _aggregators.end ())
            {
                for (map<id_t, Node>::iterator ni = _aggregators_n[a->id].begin (); ni != _aggregators_n[a->id].end (); ni ++)
                {
                    if (_med2ag.find (ni->first) != _med2ag.end () &&
                        _med2ag[ni->first] == a->id)
                    {
                        if (_arbitrators.find (ni->first) == _arbitrators.end ())
                            _arbitrator_vor->remove (ni->first);

                        _med2ag.erase (ni->first);
                    }
                }

                _aggregators.erase (a->id);
                _aggregators_n.erase (a->id);
                _nodes_knowledge.erase (a->id);
            }

            return 0;
        }

        // get count
        unsigned short *count = (unsigned short *) buf_p;
        buf_p += sizeof (unsigned short);

        // check if size is legel
        if (size != (int) (sizeof (AggNode) + sizeof (unsigned short) + (*count) * sizeof (Node)))
            return 0;

        _aggregators[a->id] = *a;

        map<id_t, bool> ori_nodes;
        // record original aggregator managing nodes
        if (_aggregators_n.find (a->id) != _aggregators_n.end ())
            for (map<id_t, Node>::iterator it = _aggregators_n[a->id].begin (); it != _aggregators_n[a->id].end (); it ++)
                ori_nodes[it->first] = true;

        // get managing nodes
        Node *na = (Node *) buf_p;
        for (unsigned short s = 0; s < *count; s ++)
        {
            if (_arbitrators.find (na[s].id) != _arbitrators.end ())
            {
                _arbitrators.erase (na[s].id);
                //_arbitrator_vor->remove (na[s].id);
            }

            // insert the node
            if (_aggregators_n[a->id].find (na[s].id) == _aggregators_n[a->id].end ())
                _arbitrator_vor->insert (na[s].id, na[s].pos);
            _arbitrator_vor->update (na[s].id, na[s].pos);
            _aggregators_n[a->id][na[s].id] = na[s];
            _med2ag[na[s].id] = a->id;
            ori_nodes.erase (na[s].id);
        }

        // remove old node records
        for (map<id_t, bool>::iterator it = ori_nodes.begin (); it != ori_nodes.end (); it ++)
        {
            _aggregators_n[a->id].erase (it->first);
            if (_med2ag.find (it->first) != _med2ag.end () &&
                _med2ag[it->first] == a->id)
            {
                _arbitrator_vor->remove (it->first);
                _med2ag.erase (it->first);
            }
        }

        return 0;
    }
    */

    // remove any invalid avatar objects
    void 
    arbitrator_impl::validate_objects ()
    {        
        // loop through list of owned avatar objects and see if any has disconnected
        for (map<obj_id_t, object *>::iterator oi = _obj_store.begin (); oi != _obj_store.end (); ++oi)
        {
            object * obj = oi->second;

            // if it's an avatar object I own            
            if (is_owner (obj->get_id ()) == true && obj->peer != 0)
            {
                // if it's no longer connected then remove it
                if (_peers.find (obj->peer) == _peers.end ())
                {
                    if (_peers_countdown.find (obj->peer) == _peers_countdown.end ())
                        _peers_countdown [oi->second->peer] = COUNTDOWN_REMOVE_AVATAR;
                    else if (_peers_countdown [obj->peer] == 0)
                    {
//#ifdef DEBUG_DETAIL
                        sprintf (_buf, "[%lu] delete unconnected avatar object [%lX] for peer [%lu].\n", 
                            self->id, obj->get_id (), obj->peer);
//#endif 
                        delete_obj (oi->second);
                        _peers_countdown.erase (oi->second->peer);
                    }
                    else
                        _peers_countdown [oi->second->peer] --;
                }
                else
                    _peers_countdown.erase (oi->second->peer);
            }
        }
    }

    // process event in event queue in sequence
    int arbitrator_impl::process_event ()
    {
        // for later dispatching events
        static char msg_buffer[VASTATE_BUFSIZ];
        static int  msg_buffer_size;

        // events waiting for processing
        vector<ExtendedEvent> event_queue;

        // for all unforwarded events
        for (vector<ExtendedEvent>::iterator it = _unforward_event.begin (); it != _unforward_event.end (); it ++)
        {
            // if delete the event
            bool preserve = false;

            id_t & from_id           = it->from_id;
            event * e                = it->e;
            vector<obj_id_t> *t_list = it->t_list;
            short & ttl              = it->TTL;

            // from a peer, check for affecting objects for the event, and dispatch
            if (_peers.find (from_id) != _peers.end ()
                // Protential BUG: need this?
                && t_list->size () == 0) 
            {
                // determining affecting objects
                const std::vector<VAST::obj_id_t> & aff_oids = _logic->event_affected (from_id, *e);
                t_list->assign (aff_oids.begin (), aff_oids.end ());
                ttl = DEFAULT_EVENT_TTL;
            }

            map<id_t, vector<obj_id_t> > affected_owners;
            for (vector<obj_id_t>::const_iterator oid = t_list->begin (); oid != t_list->end (); oid ++)
            {
                const obj_id_t & af_oid = *oid;
                if (_obj_store.find (af_oid) != _obj_store.end ())
                {
                    object * obj = _obj_store[af_oid];
                    id_t obj_owner = _arbitrator_vor->closest_to (obj->get_pos ());
                    affected_owners [obj_owner].push_back (af_oid);
                }
                else
                {
                    sprintf (_str, "[%lu] ERROR: logic returning unknown object id [%lX]. \n", self->id, 
                        *oid);
                    _eo.output (_str);
                }
            }

            // decrease ttl
            ttl --;

            // init msgbuffer
            msg_buffer_size = 0;
            t_list->clear ();

            // dispatch the event
            for (map<id_t, vector<obj_id_t> >::iterator aos = affected_owners.begin (); aos != affected_owners.end (); aos ++)
            {
                // fetch arbitrator id
                const id_t & aid = aos->first;

                // if for myself, did as received from an external arbitrator
                if (aid == self->id)
                {
                    t_list->assign (aos->second.begin (), aos->second.end ());
                    event_queue.push_back (*it);
                    preserve = true;
                }
                // send it!
                else
                {
                    if (ttl < 0)
                    {
                        sprintf (_str, "[%lu] ERROR: drop expired event (from_id %lu, sender %lu, target %lu, type %d)\n", self->id, 
                            from_id, e->get_sender(), aid, e->type);
                        _eo.output (_str);
                        continue;
                    }

                    Msg_EVENT_HEADER * eh = (Msg_EVENT_HEADER *) msg_buffer;
                    eh->objid_count = aos->second.size ();
                    eh->TTL         = ttl;

                    obj_id_t *pmsg = (obj_id_t *) (msg_buffer + sizeof (Msg_EVENT_HEADER));
                    for (vector<obj_id_t>::iterator obj_i = aos->second.begin (); obj_i != aos->second.end (); obj_i ++)
                        *pmsg++ = *obj_i;

                    msg_buffer_size = sizeof (Msg_EVENT_HEADER) + sizeof (obj_id_t) * aos->second.size ();
                    msg_buffer_size += e->encode (msg_buffer + msg_buffer_size);

                    _net->sendmsg (aid, EVENT, msg_buffer, msg_buffer_size);
                }
            }

            if (!preserve)
            {
                delete it->e;
                delete it->t_list;
            }
        } // for all unforwarded events

        // finish procssing, then clear unforwarded queue
        _unforward_event.clear ();

        // sort events (by bubble sort..)
        if (event_queue.size () > 1)
        {
            size_t e_size = event_queue.size ();
            for (size_t i = 0; i < e_size - 1; ++ i)
            {
                bool swaped = false;
                for (size_t j = e_size - 2; j > i; -- j)
                {
                    ExtendedEvent e1 = event_queue[j-1], e2 = event_queue[j];
                    //event *e1 = event_queue[j-1], *e2 = event_queue[j];
                    if ((e1.e->get_timestamp () > e2.e->get_timestamp ()) ||
                        ((e1.e->get_timestamp () == e2.e->get_timestamp ()) && e1.e->get_sender () > e2.e->get_sender ()))
                    {
                        event_queue[j-1] = e2;
                        event_queue[j] = e1;
                        swaped = true;
                    }
                }
                if (!swaped)
                    break;
            }
        }

        // process events
        for (vector<ExtendedEvent>::iterator it = event_queue.begin (); it != event_queue.end (); it ++)
        {
            ExtendedEvent ee = *it;
            _logic->event_received (ee.e->get_sender (), *(ee.e));

            delete ee.e;
            delete ee.t_list;
        }

        return 0;
    }

    // encode a list of enclosing arbitrators    
    int 
    arbitrator_impl::encode_arbitrators (char *buf, bool only_myself)
    {                
        vector<id_t> &list = _vnode->getvoronoi()->get_en (self->id);
        //int n = list.size ();

        buf[0] = 0;
        char *p = buf+1;
        
        int count=0;

        if (only_myself)
        {
            Msg_SNODE myself (*self, _net->getaddr (self->id), 0);
            memcpy (p, &myself, sizeof (Msg_SNODE));
            count = 1;
        }
        else
        {
            Msg_SNODE nodeinfo;
            // TODO: send only the enclosing arbitrator of a peer?
            // copy each arbitrator into the buffer
            for_each_enclosing_arbitrator (it)
            {
                if (it->first != self->id)
                {
                    vector<id_t>::iterator it2 = list.begin ();
                    for (; it2 != list.end (); it2 ++)
                        if (*it2 == it->first)
                            break;
                    
                    if (it2 == list.end ())
                        continue;
                }

                nodeinfo.node = it->second;
                nodeinfo.addr = _net->getaddr (it->first);
                memcpy (p, &nodeinfo, sizeof (Msg_SNODE));
                p += sizeof (Msg_SNODE);
                count++;
            }
        }

        buf[0] = count;
     
        return 1 + count * sizeof (Msg_SNODE);
    }


    // send all objects (full version) in range
    bool
    arbitrator_impl::send_objects (id_t target, bool owned, bool send_delete)
    {
        char obj_buf[VASTATE_BUFSIZ];
        char states_buf[VASTATE_BUFSIZ];
        
        int  obj_size    = 0;
        int  states_size = 0;

        // check weather target is peer or arbitrator
        bool is_peer = (_peers.find (target) != _peers.end());
        bool is_arbitrator = (_arbitrators.find (target) != _arbitrators.end ());

        // fetch Node from _peers or _arbitrators
        Node * node = NULL;
        if (is_peer)
            node = & _peers[target];
        if (is_arbitrator)
            node = & _arbitrators [target];

        // if found no Node sturcture, terminate send process
        if (node == NULL)
            return false;

        for (map<obj_id_t, object *>::iterator it = _obj_store.begin (); it != _obj_store.end (); ++it)
        {
            object *obj = it->second;

            // if owned only, but not owned, skip it
            if (owned && !is_owner (obj->get_id ()))
                continue;

            /*
            object obj2 = *obj;
            obj = & obj2;

            if (send_delete)
                obj->pos_version = 0;
            */

            // encode all informations about the object
            obj_size    = obj->encode_pos (obj_buf, false, false, send_delete);
            states_size = obj->encode_states (states_buf, false);

            // target is a peer, check AOI extra
            if (is_arbitrator || 
                (is_peer && obj->is_AOI_object (*node, true)))
            {
                if (_net->sendmsg (target, OBJECT, obj_buf, obj_size) != obj_size)
                {
#ifdef DEBUG_DETAIL
                    sprintf (_str, "[%lu] send OBJECT to [%lu] failed.\n", self->id, target);
                    _eo.output (_str);
#endif
                }

                // if send deletion, need not to send states
                if (!send_delete)
                {
                    _net->sendmsg (target, STATE, states_buf, states_size);
                    _nodes_knowledge[target][obj->get_id ()] = version_pair_t (obj->pos_version, obj->version);
                }
                else
                    _nodes_knowledge[target].erase (obj->get_id ());
            }
        }

        return true;
    }

    // do peer's object discovery
    void arbitrator_impl::update_interests ()
    {
        CodingBuffer cb;

        // loop throught all objects I know
        for (map<obj_id_t, object *>::iterator it = _obj_store.begin (); it != _obj_store.end (); ++it)
        {
            object * obj = it->second;

            // reset obj_buf and states_buf
            cb.obj_size = cb.obj_dsize = cb.states_size = 0;

            // check obj is discovered correctly for all connected peers
            for (map<id_t,Node>::iterator itn = _peers.begin (); itn != _peers.end (); itn ++)
                check_knowledge (obj, itn->first, itn->second, &cb);

            // sends only updates of objects I owned to arbitrators
            if (!is_owner (obj->get_id ()))
                continue;

            // check obj is discovered correctly for all arbitrators
            for (map<id_t,Node>::iterator itn = _arbitrators.begin (); itn != _arbitrators.end (); itn ++)
            {
                // skip myself
                if (itn->first == self->id)
                    continue;

                check_knowledge (obj, itn->first, itn->second, &cb);
            }
        }
    }

    // checking the nodes knowing about the object
    void 
    arbitrator_impl::check_knowledge (object *obj, const id_t & id, const Node & node, CodingBuffer *buff)
    {
        bool aoi_obj = obj->is_AOI_object (node, true);
        bool knew = (_nodes_knowledge[id].find (obj->get_id ()) != _nodes_knowledge[id].end ());
        bool knew_oldpos = (knew &&
                        _nodes_knowledge[id][obj->get_id ()].first < obj->pos_version);
        bool knew_old = (knew &&
                        _nodes_knowledge[id][obj->get_id ()].first < obj->version);

        // check if peer should know the object, and if he knews
        if (aoi_obj && (!knew || knew_oldpos || knew_old))
        {
            // encode the object if not encoded
            if (buff->obj_size == 0)
            {
                buff->obj_size    = obj->encode_pos (buff->obj_buf, false);
                buff->states_size = obj->encode_states (buff->states_buf, false);
            }

            // send object
            if (!knew || knew_oldpos)
                _net->sendmsg (id, OBJECT, buff->obj_buf, buff->obj_size);

            if ((buff->states_size > 0) && (!knew || knew_old))
                _net->sendmsg (id, STATE, buff->states_buf, buff->states_size);

            // insert knowledge record
            _nodes_knowledge[id][obj->get_id ()] = version_pair_t (obj->pos_version, obj->version);
        }
        else if (! aoi_obj && knew)
        {
            // encode the object if not encoded
            if (buff->obj_dsize == 0)
                buff->obj_dsize = obj->encode_pos (buff->obj_dbuf, false, false, true);

            if (obj->peer != 0 && id == obj->peer)
            {
                sprintf (_str, "[%lu] arb try send DELETE for peer's avatar object [%lX] peer [%lu]\n", self->id, obj->get_id (), id);
                _eo.output (_str);
            }

            // send delete object
            _net->sendmsg (id, OBJECT, buff->obj_dbuf, buff->obj_dsize);

            // remove list item
            _nodes_knowledge [id].erase (obj->get_id ());

        }
    }
/*
    // check for aggregation
    void 
    arbitrator_impl::check_aggregation ()
    {
        // for aggregator, checks for if any node should leave aggregation (or leave for another aggregator)
        if (is_aggregator ())
        {
            vector<id_t> waked_nodes;

            // for all nodes I'm managing
            for (map<id_t, ArbRecord>::iterator it = _managing_nodes.begin (); it != _managing_nodes.end (); it ++)
            {
                ArbRecord &r = it->second;

                // make sure peer is connected, then doing migration
                if (r.parent_obj == NULL ||
                    _peers.find (r.parent_obj->peer) == _peers.end ())
                    continue;

                AggNode * n = NULL;

                // check for staying 
                if ((! _is_leaving) && (self->pos.dist (r.node.pos) <= (double) _aggnode.aoi_b))
                    n = & _aggnode;

                // select an aggregator who is nearest to me and covered my pos
                for (map<id_t, AggNode>::iterator itg = _aggregators.begin (); itg != _aggregators.end (); itg ++)
                {
                    AggNode & an = itg->second;
                    if (r.node.pos.dist (an.pos) <= (double) (an.aoi_b))
                        if (n == NULL ||
                            (an.pos.distsqr (r.node.pos) < n->pos.distsqr (r.node.pos)))
                            n = &an;
                }

                // do nothing, if I'm the best choise
                if (n == & _aggnode)
                    continue;

                // no aggregator(s) are suitted, wake the original arbitrator up 
                else if (n == NULL)
                {
                    wakeup_node (it->first);
                    waked_nodes.push_back (it->first);
                }

                // or migrate the node to proper aggregator
                else
                {
                    migrate_to_aggregator (n->id, it->first);
                    waked_nodes.push_back (it->first);
                }
            }

            for (vector<id_t>::iterator wn = waked_nodes.begin (); wn != waked_nodes.end (); wn ++)
            {
                id_t & node_id = *wn;
                _managing_nodes_intrans[node_id] = _managing_nodes[node_id];
                _managing_nodes.erase (node_id);
            }
        }
        else
        {
            if (_is_suspending)
                return;

            if (_migration_cd > 0)
                _migration_cd --;

            // make sure I got my parent_obj then transfer it 
            if (_migration_cd <= 0 && _parent_obj != NULL &&
                _obj_owned.find (_parent_obj->get_id ()) != _obj_owned.end ())
            {
                AggNode * n = NULL;
                // select an aggregator who is nearest to me and covered my pos
                for (map<id_t, AggNode>::iterator it = _aggregators.begin (); it != _aggregators.end (); it ++)
                {
                    AggNode & an = it->second;
                    if (self->pos.dist (an.pos) <= an.aoi_b)
                        if (n == NULL ||
                            (self->pos.dist (an.pos) < self->pos.dist (n->pos)))
                            n = &an;
                }

                if (n != NULL)
                {
                    migrate_to_aggregator (n->id);
                    _migration_cd = 10;
                }
            }
        }
    }
*/
    // refreshing arbitrator voronoi map
    void 
    arbitrator_impl::refresh_voronoi ()
    {
        _arbitrator_vor->clear ();

        // insert arbitrators
        for (map<id_t, Node>::iterator it = _arbitrators.begin (); it != _arbitrators.end (); it ++)
            _arbitrator_vor->insert (it->second.id, it->second.pos);
    }

    /*
    // migrating this node to target aggregator
    bool
    arbitrator_impl::migrate_to_aggregator (id_t target_aggr_id, id_t src_node)
    {
#ifdef DEBUG_DETAIL
        sprintf (_str, "[%lu] migrate_to_aggregator (%lu, %lu)\n", self->id, target_aggr_id, src_node);
        _eo.output (_str);
#endif
        //char buff[VASTATE_BUFSIZ];
        //int buff_size;
        vector<obj_id_t> remove_list;

        // double check if target aggregator exists
        if (_aggregators.find (target_aggr_id) == _aggregators.end ())
            return false;

        AggNode & an = _aggregators[target_aggr_id];

        // looping through all my owned objects
        for (map<obj_id_t, bool>::iterator it = _obj_owned.begin (); it != _obj_owned.end (); it ++)
        {
            object * obj = _obj_store[it->first];

            if (src_node == NET_ID_UNASSIGNED ||
                _arbitrator_vor->closest_to (obj->get_pos ()) == src_node)
            {
                if (send_ownership (obj, target_aggr_id, true))
                {
                    remove_list.push_back (obj->get_id ());
                    _obj_in_transit [obj->get_id ()] = get_timestamp ();
                }
            }
        }

        for (vector<obj_id_t>::iterator ri = remove_list.begin (); ri != remove_list.end (); ri ++)
            _obj_owned.erase (*ri);

        remove_list.clear ();

        // finally, remove all owned, and waiting for migration 
        // clearning will be done when received migrate_ack
        //if (src_node == NET_ID_UNASSIGNED)
        //    _obj_owned.clear ();

        // tell peer to connect to new aggregator
        Msg_SNODE aggnode (Node (an.id, an.aoi, an.pos, an.time), _net->getaddr (an.id), 0);
        _buf[0] = 1;
        memcpy (_buf+1, &aggnode, sizeof (Msg_SNODE));
        if (src_node == NET_ID_UNASSIGNED)
        {
            // for normal arbitrator, tell all peers
            for (map<id_t, Node>::iterator itp = _peers.begin (); itp != _peers.end (); itp ++)
                _net->sendmsg (itp->first, ARBITRATOR, _buf, sizeof (Msg_SNODE) +1);
        }
        else
        {
            // for aggregator, only the leaving peer
            id_t src_peer = _managing_nodes[src_node].parent_obj->peer;
            _net->sendmsg (src_peer, ARBITRATOR, _buf, sizeof (Msg_SNODE) +1);
        }

        // send start to migrate
        Node * n_tosend;
        id_t parent_id;
        Addr addr;
        if (src_node == NET_ID_UNASSIGNED)
        {
            n_tosend = self;
            parent_id = _parent_obj->get_id ();
            addr = _net->getaddr (self->id);
        }
        else
        {
            n_tosend = & (_managing_nodes[src_node].node);
            parent_id = _managing_nodes[src_node].parent_obj->get_id ();
            addr = _managing_nodes[src_node].addr;
        }
        Msg_MIGRATE msg_h (*n_tosend, addr, parent_id);
        _net->sendmsg (target_aggr_id, MIGRATE, (char *) & msg_h, sizeof (Msg_MIGRATE));

        // leave the overlay to put self to suspend
        // TODO: when should I leave?
        //_vnode->leave ();
        return true;
    }
    */

    /*
    // wake up arbitrator handled by peers
    bool
    arbitrator_impl::wakeup_node (id_t node_id)
    {
#ifdef DEBUG_DETAIL
        sprintf (_str, "[%lu] wakeup_node (%lu) \n", self->id, node_id);
        _eo.output (_str);
#endif
        if (!is_aggregator () ||
            _managing_nodes.find (node_id) == _managing_nodes.end ())
            return false;

        ArbRecord & ar = _managing_nodes[node_id];

        memcpy (_buf, (char *) & (ar.node.pos), sizeof (Position));
        memcpy (_buf + sizeof (Position), (char *) & (_net->getaddr (self->id)), sizeof (Addr));

        // Send MIGRATE message to wake the node up
        _net->connect (ar.addr);
        _net->sendmsg (ar.node.id, MIGRATE, _buf, sizeof (Position) + sizeof (Addr));

        // sends also ownership of the object
        if (ar.parent_obj != NULL && send_ownership (ar.parent_obj, ar.node.id, true))
        {
            _obj_owned.erase (ar.parent_obj->get_id ());
            _obj_in_transit [ar.parent_obj->get_id ()] = get_timestamp ();
        }
        else 
        {
            // TODO:what should I do ?
            sprintf (_str, "[%lu] fail to transfer object [%lX] to waked up node [%lu]\n", self->id, 
                (ar.parent_obj != NULL) ? ar.parent_obj->get_id () : 0, 
                ar.node.id);
            _eo.output (_str);
        }

        // Let peer to connect to arbitrator
        Msg_SNODE arbnode (ar.node, ar.addr, 0);
        _buf[0] = 1;
        memcpy (_buf+1, &arbnode, sizeof (Msg_SNODE));
        _net->sendmsg (ar.parent_obj->peer, ARBITRATOR, _buf, sizeof (Msg_SNODE) + 1);

        // remove record
        // Note: the record will be moved in check_aggregation
        //_managing_nodes_intrans[node_id] = _managing_nodes[node_id];
        //_managing_nodes.erase (node_id);

        return true;
    }
    */

    // notify local node of updates and send updated object states I own to affected nodes
    bool
    arbitrator_impl::send_updates ()
    {
        char obj_buf[VASTATE_BUFSIZ];
        char states_buf[VASTATE_BUFSIZ];
        
        int  obj_size    = 0;
        int  states_size = 0;

        vector<obj_id_t> delete_list;

        // find all dirty objects and send them to the respective interested nodes
        for (map<obj_id_t, object *>::iterator it = _obj_store.begin (); it != _obj_store.end (); ++it)
        {
            object *obj = it->second;
            obj_size = states_size = 0;

            if (obj->pos_version == 0)
                printf ("[%lu] ERROR: arb: send_updates: found pos_version = 0 object [%lu]\n", self->id, obj->get_id ());

            bool i_am_owner = is_owner (obj->get_id ());
            timestamp_t obj_constant_time = get_timestamp () - _obj_update_time[obj->get_id ()];

            // check if this object need to refresh for following object expiring rules
            if (i_am_owner && obj_constant_time >= THRESHOLD_EXPIRING_OBJECT / 2)
                obj->pos_dirty = true;

            // or if the object may has no owner, delete it
            else if (!i_am_owner && obj_constant_time >= THRESHOLD_EXPIRING_OBJECT)
            {
                //sprintf (_str, "[%lu] mark object [%lX] as deleted by EXPIRING rule\n", self->id, obj->get_id ());
                //_eo.output (_str);
                obj->mark_deleted ();
            }

            // check if position has changed
            if (obj->pos_dirty == true || obj->is_alive () == false)
            {
                // csc: version # increased when chnage_pos/update_obj
                // this is the only place where version # will increase
                //if (obj->is_alive () == true)
                //    obj->pos_version++;
                //else
                if (obj->is_alive () == false)
                {
                    // we use pos_version = 0 to indicate object deletion
                    obj->pos_version = 0;
                    obj->pos_dirty = true;
                    delete_list.push_back (obj->get_id ());
                }
                
                // encode object position (dirty only)
                obj_size = obj->encode_pos (obj_buf, true);
            }
            
            // check if object states have changed
            if (obj->dirty == true)
            {
                // csc: version # increased when chnage_pos/update_obj
                // we advance the object state version # only once here
                // NOTE: it's important both local and foreign nodes see the same 
                //       version number for a given updated value
                //obj->version++;    
                
                // encode only the dirty attributes into this update
                states_size = obj->encode_states (states_buf, true);
            }

            // check changes, if no, skip this object
            if (obj_size == 0 && states_size == 0)
                continue;

            // set update time
            _obj_update_time [obj->get_id ()] = get_timestamp ();

            // really send updates
            // loop through all peers
            for (map<id_t, Node>::iterator it2 = _peers.begin (); it2 != _peers.end (); it2 ++)
            {
                const id_t & node_id = it2->first;

                if (obj->is_AOI_object (it2->second, true) &&
                    (_nodes_knowledge[node_id].find (obj->get_id()) != _nodes_knowledge[node_id].end ()))
                {
                    // skip if arbitrator try to tell peer to delete its avatar object (should this happen?)
                    if (node_id == obj->peer && obj->pos_version == 0)
                    {
                        sprintf (_str, "[%lu] CRITICAL: arb send DELETE for peer's avatar object [%lX] peer [%lu] %s\n", self->id, obj->get_id (), obj->peer, 
                            _obj_owned.find (obj->get_id ()) == _obj_owned.end () ? "not owned" : "owned" );
                        _eo.output (_str);
                        continue;
                    }

                    if (obj_size > 0)
                        _net->sendmsg (node_id, OBJECT, obj_buf, obj_size);

                    if (states_size > 0)
                        _net->sendmsg (node_id, STATE, states_buf, states_size);

                    // set nodes's knowledge as new version
                    _nodes_knowledge[node_id][obj->get_id ()] = version_pair_t (obj->pos_version, obj->version);
                }
            }

            // loop through all arbitratos
            if (i_am_owner)
            {
                for (map<id_t, Node>::iterator it2 = _arbitrators.begin (); it2 != _arbitrators.end (); it2 ++)
                {
                    const id_t & node_id = it2->first;

                    // send update only on the arbitrator already knew the object
                    if (node_id != self->id && 
                        _nodes_knowledge[node_id].find (obj->get_id ()) != _nodes_knowledge[node_id].end ())
                    {
                        if (obj_size > 0)
                            _net->sendmsg (node_id, OBJECT, obj_buf, obj_size);

                        if (states_size > 0)
                            _net->sendmsg (node_id, STATE, states_buf, states_size);

                        // set nodes's knowledge as new version
                        _nodes_knowledge[node_id][obj->get_id ()] = version_pair_t (obj->pos_version, obj->version);
                    }
                }
            }

            // remove a non-owned object that's being deleted
            /*
            if (obj->is_alive () == false)
            {
                // check if I am owner (this should not happen !)
                if (is_owner (obj->get_id ()))
                {
                    sprintf (_str, "[%d] try to delete owned object [%d_%d]\n", self->id, obj->get_id () >> 16, obj->get_id () & 0xFFFF);
                    _eo.output (_str);
                }

                delete_list.push_back (obj->get_id ());
            }

            else 
            */
            if (obj->is_alive () == true)
            {
                // notify the local node of the updates (both position & states)
                if (obj->pos_dirty == true)
                    _logic->pos_changed (obj->get_id (), obj->get_pos(), obj->pos_version);
            
                // loop through each attribute and notify the arbitrator logic
                //notify_updates (obj);                                  
                for (int j=0; j<obj->size (); ++j)
                {
                    int length;
                    void *p;
                    if (obj->get (j, &p, length) == false || obj->is_dirty (j) == false)
                        continue;
                    
                    // if dirty then notify the arbitrator logic layer
                    _logic->state_updated (obj->get_id (), j, p, length, obj->version);
                }
            }

            // reset dirty flag
            obj->reset_dirty ();
            obj->pos_dirty = false;
        }

        // remove all deleted object
        for (int i=0; i<(int)delete_list.size (); i++)
            unstore_obj (delete_list[i]);

        // send tick to arbitrators has no event sends to
        /*
        timestamp_t curr_time = get_timestamp ();
        for (map<id_t, Node>::iterator it = _arbitrators.begin (); it != _arbitrators.end (); it ++)
        {
            if (_arbitrator_lasttick[it->first] + THRESHOLD_EVENTTICK <= curr_time)
            {
                // TODO/BUG: what timestamp should I tick for ?
                _arbitrator_lasttick[it->first] = curr_time - THRESHOLD_EVENTTICK;
                _net->sendmsg (it->first, TICK_EVENT, reinterpret_cast<const char *> (&curr_time), sizeof (timestamp_t));
            }
        }
        */

        return true;
    }

    /*
    // tell arbitrator logic of all updated states during this time-step
    void 
    arbitrator_impl::notify_updates (object *obj)
    {
        // loop through each attribute and notify the arbitrator logic
        for (int j=0; j<obj->size (); ++j)
        {
            int length;
            void *p;
            if (obj->get (j, &p, length) == false || obj->is_dirty (j) == false)
                continue;
            
            // if dirty then notify the arbitrator logic layer
            _logic->state_updated (obj->get_id (), j, p, length, obj->version);
        }            
    }
    */

    // send an object update request to the approprite node (checks for target ownership)
    bool 
    arbitrator_impl::forward_request (obj_id_t obj_id, msgtype_t msgtype, char *buf, int size)
    {
        object *obj = (_obj_store.find (obj_id) != _obj_store.end () ? 
                       _obj_store[obj_id] : NULL);        
            
        // target object does not exist in store
        if (obj == NULL)
            return false;

        // loop through all connected arbitrators to find the nearest
        double min_dist = self->aoi;
        id_t   target = self->id;
        
        for (map<id_t, Node>::iterator it = _arbitrators.begin (); it != _arbitrators.end (); ++it)
        {
            double dist = it->second.pos.dist (obj->get_pos());
            if (dist <= min_dist)
            {
                min_dist = dist;
                target   = it->first;
            }
        }
        
#ifdef DEBUG_DETAIL
        sprintf (_str, "[%lu] forwards %s request to [%lu]\n", self->id, VASTATE_MESSAGE[(int)(msgtype-100)], target);
        _eo.output (_str);
#endif

        _net->sendmsg (target, msgtype, buf, size);

        return true;
    }

    void 
    arbitrator_impl::notify_peers (map<id_t, version_t> *list, msgtype_t msgtype, char *msg, int len)
    {
        if (list == NULL)
            return;

        // iterate through all interested peers
        for (map<id_t, version_t>::iterator it = list->begin (); it != list->end (); it++)
            _net->sendmsg (it->first, msgtype, msg, len);
    }

    // pack object ID and new owner ID buffer
    // returns size of buffer used
    int 
    arbitrator_impl::pack_transfer (object *obj, id_t closest, char *buf)
    {
        Msg_TRANSFER trmsg;
        trmsg.obj_id = obj->get_id ();
        trmsg.new_owner = closest;
        trmsg.orig_owner = self->id;
        memcpy (buf, &trmsg, sizeof(Msg_TRANSFER));

        return (sizeof (Msg_TRANSFER));
    }

    // reverse of pack_transfer, used by the receipant of a TRANSFER message
    // returns the number of items unpacked
    int 
    arbitrator_impl::unpack_transfer (char *buf)
    {
        Msg_TRANSFER trmsg (buf);

        // if I am new owner
        if (self->id == trmsg.new_owner)
        {
            // Known BUG: if I had owner but not object, ignore the TRANSFER (this may introduce object re-own or miss object)
            // check if I am owner but has not the object
            if (_obj_store.find (trmsg.obj_id) == _obj_store.end ())
            {
#ifdef DEBUG_DETAIL
                sprintf (_str, "[%lu] CRITICAL: get OWNER of unknown obj [%lX_%lX]\n", self->id, trmsg.obj_id >> 16, trmsg.obj_id & 0xFFFF);
                _eo.output (_str);
#endif
                return 0;
            }

            // send acknowledgement of TRANSFER
            if (trmsg.orig_owner != NET_ID_UNASSIGNED)
            {
                if (_net->sendmsg (trmsg.orig_owner, TRANSFER_ACK, (char *) &trmsg.obj_id, sizeof (obj_id_t) == sizeof(obj_id_t)))
                {
                    // set I'm owner
                    _obj_owned [trmsg.obj_id] = true;
                    _obj_update_time [trmsg.obj_id] = get_timestamp ();
                }
            }

            /*
            char obj_buf[VASTATE_BUFSIZ];
            char states_buf[VASTATE_BUFSIZ];
            
            int  obj_size    = 0;
            int  states_size = 0;

            object * obj = _obj_store [trmsg.obj_id];
            obj_size    = obj->encode_pos (obj_buf, false);
            states_size = obj->encode_states (states_buf, false);

            // send object states to all my enclosing neighbors
            for (map<id_t, Node>::iterator it = _arbitrators.begin (); it != _arbitrators.end (); ++it)
            {
                // skip myself
                if (it->first == self->id)
                    continue;

                _net->sendmsg (it->first, OBJECT, obj_buf, obj_size);
                _net->sendmsg (it->first, STATE, states_buf, states_size);
            }
            */
        }
#ifdef DEBUG_DETAIL
        // I shouldn't receive the ownership transfer
        else
        {
            sprintf (_str, "[%lu] arbitrator_impl: unpack_transfer (): received illegel ownership transfer of object %lX newowner %lu. \n", 
                self->id, trmsg.obj_id, trmsg.new_owner);
            _eo.output (_str);
        }
#endif

        /*
        // if I am not new owner's enclosing neighbor
        else if (_arbitrators.find (trmsg.new_owner) == _arbitrators.end ())
        {
            // delete the object
            if (_obj_store.find (trmsg.obj_id) != _obj_store.end ())
                 _obj_store[trmsg.obj_id]->mark_deleted ();
        }
        */

        // I am still owner's enclosing neighbor
        //   nothing to do

        return 0;
    }

    // find a suitable new arbitrator given a certain need/stress level
    bool 
    arbitrator_impl::find_arbitrator (int level, Msg_SNODE &new_arb)
    {
        if (_potential_arbitrators.size () == 0)
            return false;

        int index;
        int minimal = 0;

        // find a portential arbitrator has minimal promotion count
        for (index = 0; index < (int) _potential_arbitrators.size (); index ++)
        {
            if (_promotion_count [index] < _promotion_count [minimal])
                minimal = index;
        }

        // return the minimal promoted node
        new_arb = _potential_arbitrators[minimal];
        _promotion_count [minimal] ++;

        return true;
    }

    // get misc node info
    bool 
    arbitrator_impl::get_info (int info_type, char* buffer, size_t & buffer_size)
    {
        /*
        switch (info_type)
        {
         case 1:
            size_t old_bufsize = buffer_size;

            // calculate space needed
            buffer_size = (1+sizeof(Node)*_managing_nodes.size ());

            // false if not sufficient size
            if (!is_aggregator () || old_bufsize < buffer_size)
                return false;

            *buffer = (char) _managing_nodes.size ();

            char * p_buf = buffer + 1;
            for (map<id_t, ArbRecord>::iterator it = _managing_nodes.begin (); it != _managing_nodes.end (); it ++)
            {
                memcpy (p_buf, (char *) & (it->second.node), sizeof (Node));
                p_buf += sizeof (Node);
            }

            return true;
            
        }
        */

        return false;
    }

    // return if I'm a object's owner (for statistics purpose)
    bool 
    arbitrator_impl::is_obj_owner (obj_id_t object_id)
    {
        return (_obj_owned.find (object_id) != _obj_owned.end ());
    }

    const char * 
    arbitrator_impl::to_string ()
    {

        static std::string string_out;
        char buf[80];

        string_out.clear ();

        /*
        sprintf (buf, "AOI2: %d\n", _aggnode.aoi_b);
        string_out.append (buf);
        
        if (_parent_obj != NULL)
            sprintf (buf, "Parent: peer %d %X (obj_id %X pos (%d,%d))\n", parent, _parent_obj, _parent_obj->get_id (), 
                                                                 (int) _parent_obj->get_pos ().x, (int) _parent_obj->get_pos ().y);
        else
            sprintf (buf, "Parent: peer %d %X\n", parent, _parent_obj);
        string_out.append (buf);
        */

        sprintf (buf, "Arbitrators: \n");
        string_out.append (buf);

        for_each_enclosing_arbitrator(it)
        {
            sprintf (buf, "  [%lu] (%d,%d)\n", it->first, (int) it->second.pos.x, (int) it->second.pos.y);
            string_out.append (buf);
        }

        string_out.append ("Getnodes ==\n");

        vector<Node *> &nodes = _vnode->getnodes ();
        for (vector<Node *>::iterator it = nodes.begin () ;it != nodes.end (); it ++)
        {
            Node * n = *it;
            sprintf (buf, "  [%lu] (%d,%d)\n", n->id, (int) n->pos.x, (int) n->pos.y);
            string_out.append (buf);
        }

        if (_peers.size () > 0)
        {
            string_out.append ("Peers ==\n");
            for (map<id_t, Node>::iterator ip = _peers.begin (); ip != _peers.end (); ip ++)
            {
                Node & n = ip->second;
                sprintf (buf, "  [%lu] (%d,%d) aoi %d \n", n.id, (int) n.pos.x, (int) n.pos.y, n.aoi);
                string_out.append (buf);
            }
        }

        return string_out.c_str ();
    }
    
} // namespace VAST

