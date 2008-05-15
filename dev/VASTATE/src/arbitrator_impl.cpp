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

// TODO: when does the arbitrator adjust its AOI?

#include "arbitrator_impl.h"
#include "vastutil.h"

using namespace VASTATE;
namespace VAST
{
    extern char VASTATE_MESSAGE[][20];

    // initialize an arbitrator
    arbitrator_impl::arbitrator_impl (id_t my_parent, 
                                      arbitrator_logic *logic, 
                                      vast *vastnode, storage *s, 
                                      bool is_gateway, Addr &gateway, 
                                      system_parameter_t * sp)
        : arbitrator (my_parent, sp), 
          _logic(logic), _vnode (vastnode), _storage (s), 
          _is_gateway (is_gateway), _gateway (gateway), _time_diff (0), _arbitrator_vor (NULL), 
          _obj_id_count (1), _is_demoted (false), 
          _overload_time (0), _overload_count (0), _underload_time (0), _underload_count (0)
    {
        //_logic->register_interface (this);
        //_logic->register_storage (s);

    }
    
    arbitrator_impl::~arbitrator_impl ()
    {                  
        // cannot delete ?
        // can't delete out-DLL 's memory space
        /*
        if (_logic != NULL)
            delete _logic;
        */

        if (_storage != NULL)
            delete _storage;

        if (_vnode != NULL)
            delete _vnode;
    }

    //
    // arbitrator interface
    //
    
    bool 
    arbitrator_impl::join (id_t id, Position &pos)
    {
        // use VON to join 
        // TODO: AOI is forced to be very small initially as it should be dynamically adjustable
        //      (need to be fixed? or is actually okay?)
        _vnode->join (id, 10, pos, _gateway);
        self = _vnode->getself ();

        // make sure storage class is initialized to generate unique query_id        
        _storage->init_id (self->id);

        return true;
    }
        
    // process messages (send new object states to neighbors)
    int 
    arbitrator_impl::process_msg ()
    {        
        // create a copy of current state
        if (self != NULL)
            _newpos = *self;

        // advance VON's logical time & also handle network messages
        //   -> handlemsg () may be called many times
        _vnode->tick ();

        // if we have not joined the VON network, then do nothing
        if (_vnode->is_joined () == false)
            return 0;

        // update the list of arbitrators
        update_arbitrators ();

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

        // make adjustments to arbitrator AOI
        //adjust_aoi ();

        // check and adjust if I violate arbitrator distance rule (dist between two arbitrators must >= 2 * aoi)
        //adjust_position ();

        // update the overlay
        _vnode->setAOI (_newpos.aoi);
        _vnode->setpos (_newpos.pos);

        // delete exipred arbitrator inserting record
        list<Node>::iterator it;
        while (true) 
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
        }

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
        sprintf (_str, "[%d] arb handlemsg from [%d]: (%d)%s\r\n", self->id, from_id, msgtype, (msgtype >= 100)?VASTATE_MESSAGE[(int)(msgtype-100)]:"UNKNOWN");
        _eo.output (_str);
#endif

        switch (msgtype)
        {
        case JOIN:
            // check if message size is correct
            if (size-sizeof (Msg_NODE)-1 == msg[sizeof (Msg_NODE)])
            {
                // separate Msg_NODE and app-specific message
                Msg_NODE n (msg);
                size_t app_size = msg[sizeof (Msg_NODE)];

                if (_is_gateway)
                {
                    // to prevent gateway server from over-connection
                    _net->disconnect (from_id);
                    
                    // record the peer's info for later use 
                    // (after arbitrator_logic has authenticated the peer)
                    _join_peers[from_id] = n;
                    
                    // call arbitrator_logic to handle the app-specific aspects of a join 
                    _logic->join_requested (from_id, msg + sizeof (Msg_NODE)+1, app_size);
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

                /*
                map<obj_id_t, version_t> & pk = _peers_knowledge[peer.id];
                pk.clear ();
                
                Msg_OBJ_UPDATEINFO info;
                int n = p[0];
                p += 1;

                for (int i=0; i<n; i++)
                {
                    memcpy (&info, p, sizeof (Msg_OBJ_UPDATEINFO));                    
                    
                    // if this object is known (if it's not yet known -- meaning that 
                    // the arbitrator's AOI may need enlargement -- we simply ignore)

                    // if I don't know the object, skip it
                    if (_obj_store.find (info.obj_id) == _obj_store.end ())
                        continue;

                    // if the peer's object is already up-to-date, simply add it to interested list
                    pk[info.obj_id] = info.version;

                    p += sizeof (Msg_OBJ_UPDATEINFO);
				}
                */

                // send a list of enclosing arbitrators to the peer
                char buffer[VASTATE_BUFSIZ];
                int len = encode_arbitrators (buffer);
                _net->sendmsg (peer.id, ARBITRATOR, buffer, len);

                send_objects (peer.id, false);
            }
            break;

        // Notification/request of object creation, destruction, update by other arbitrators
        // TODO/BUG: If request update come before OBJECT, position update will NOT occur.
        case OBJECT:
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
                    sprintf (_str, "[%d] arb learns of new object [%s] from %d\r\n", self->id, obj->tostring(), from_id);
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
            {
                Msg_STATE info (msg);
                //object *obj = _obj_store[info.obj_id];

                object *obj = (_obj_store.find (info.obj_id) != _obj_store.end () ? 
                               _obj_store[info.obj_id] : NULL);

                char *p = msg + sizeof (Msg_STATE);

                // if STATE msg proceeds OBJECT, then abort
                if (obj == NULL)    
                    break;

#ifdef DEBUG_DETAIL
                sprintf (_str, "[%d] STATE obj_id: %d version: %d request: %s\r\n", self->id, info.obj_id, info.version, (info.is_request ? "true" : "false") );
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
                    sprintf (_str, "[%d] receives a TICK_EVENT from not enclosing neighbor [%d]\r\n", self->id, from_id);
                    _eo.output (_str);
                }
#endif
            }
            break;

        // Notification of a peer-generated event
        case EVENT:            
            if (size > sizeof(Msg_EVENT))
            {
                // unpack data
                event *e = new event ();
                
                if (e->decode (msg) == true)
                    _unforward_event.push_back (pair<id_t, event*> (from_id, e));
                else
                    delete e;
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
                    Msg_NODE new_arb;
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

                    Msg_NODE requester;
                    requester.set (n, _net->getaddr (from_id), 0);
                    
                    // copy message to send buffer        
                    char buffer[VASTATE_BUFSIZ];
                    memcpy (buffer, &requester, sizeof (Msg_NODE));

                    // send a one-time only promotion message
                    bool o_conn_state = _net->is_connected (new_arb.node.id);
                    if (!o_conn_state)
                        //_net->connect (new_arb.node.id, new_arb.addr);
                        _net->connect (new_arb.addr);
                    int send_size = _net->sendmsg (new_arb.node.id, PROMOTE, buffer, sizeof (Msg_NODE));
                    if (!o_conn_state)
                        _net->disconnect (new_arb.node.id);

                    // quit if send successful
                    if (send_size > 0)
                        break;
                }
            }
            else
            {
                sprintf (_str, "[%d] receives OVERLOAD_I but not gateway\r\n", self->id);
                _eo.output (_str);
            }
            break;

        // Overloaded arbitrator's request for moving closer
        case OVERLOAD_M:
            // Sever promotes a peer among a list of candidates by sending a join location and a contact.
            {
                // received not my enclosing neighbor's help signal
                if (_arbitrators.find (from_id) == _arbitrators.end ())
                {
                    sprintf (_str, "[%d] receives OVERLOAD from non-enclosing node [%d]\r\n", self->id, from_id);
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
            break;

        // Underloaded arbitrator's request for region adjustment
        case UNDERLOAD:
            {
                // received not my enclosing neighbor's help signal
                if (_arbitrators.find (from_id) == _arbitrators.end ())
                {
#ifdef DEBUG_DETAIL
                    sprintf (_str, "[%d] receives UNDERLOAD from non-enclosing node [%d]\r\n", self->id, from_id);
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
                // csc NOTE:
                //      _obj_owned[] will be change in unpack_transfer ()
                // unpack transfer content and do proper setups
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

        case DISCONNECT:
            {
                // remove any disconnected peers (opposte of ENTER)
                if (_peers.find (from_id) != _peers.end ())
                    _peers.erase (from_id);
            }
            break;

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
        sprintf (_str, "[%d] creates obj [%s]\r\n", self->id, obj->tostring());
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
    void
    arbitrator_impl::update_obj (object *obj, int index, int type, void *value)
    {           
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
    }


    // create an update message, then send to the respective owner (could be myself)
    void 
    arbitrator_impl::change_pos (object *obj, Position &newpos)
    {
        // if it's mark_deleted, cant do more update
        if (!obj->is_alive ())
            return ;

        // update object by new position
        obj->set_pos (newpos);

        // update peer's position information
        if (obj->peer != 0 && _peers.find (obj->peer) != _peers.end ())
        {
            _peers[obj->peer].pos = obj->get_pos ();
        }

        // increase version number
        obj->pos_version ++;
    }
    
    
    // arbitrator overloaded, call for help
    // note that this will be called continously until the situation improves
    bool
    arbitrator_impl::overload (int level)
    {
        /*
        char buffer[128];
        vector<Position> insert_pos;

        if (get_timestamp () - _overload_time >= THRESHOLD_LOAD_COUNTING)
            _overload_count = 0;

        _overload_count ++;
        _overload_time = get_timestamp ();

        if (_overload_count % 5 == 0)
        {
            // try to do a inserting new arbitrator first
            // find a proper insertion position for new arbitrator
            vector<line2d> & lines = _vnode->getvoronoi ()->getedges ();
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
        }

        // else call enclosing neighbor more closer

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

        return true;
    }


    // arbitrator underloaded, will possibly depart as arbitrator
    bool 
    arbitrator_impl::underload (int level)
    {
        /*
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
        */

        return true;
    }


    // called by the gateway arbitrator to continue the process of admitting a peer  
    // after the joining peer has been authenticated
    bool 
    arbitrator_impl::insert_peer (id_t peer_id, void *initmsg, size_t size)
    {
        Msg_NODE n = _join_peers[peer_id];        

        //printf ("[%d] node [%d] join location: (%d, %d) capacity: %d\n", self->id, joinerjor.pos.x, joiner.pos.y, n.capacity);
        
        // Server records the joining peer if it qualifies as a potential arbitrat
        if (_is_gateway && n.capacity > THRESHOLD_ARBITRATOR)
        {
#ifdef DEBUG_DETAIL
            sprintf (_str, "node [%d] selected as potential arbitrator (capacity: %d)\r\n", peer_id, n.capacity);
            _eo.output (_str);
#endif
                
            _potential_arbitrators.push_back (n);
            _promotion_count.push_back (0);
        }

        _join_peers.erase (peer_id);

        // prepare a JOIN message to check for acceptance (whether the peer is within my region)
        memcpy (_buf, &n, sizeof (Msg_NODE));
        _buf[sizeof (Msg_NODE)] = size;
        if (size > 0)
            memcpy (_buf + sizeof(Msg_NODE) + 1, initmsg, size);

        return check_acceptance (peer_id, n, _buf, sizeof(Msg_NODE) + 1 + size);
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

        // notify object deletion
        _logic->obj_deleted (obj);

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
    arbitrator_impl::check_acceptance (id_t from_id, Msg_NODE &n, char *msg, size_t size)
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
            sprintf (_str, "[%d] arbitrator (%f, %f) has taken [%d] (%f, %f)\r\n", self->id, self->pos.y, joiner.id, joiner.pos.x, joiner.pos.y);
            _eo.output (_str);
#endif

            // create the avatar object
            size_t initmsg_size = msg[sizeof (Msg_NODE)];
            object *obj = create_obj (joiner.pos, joiner.id, (initmsg_size > 0 ? (msg + sizeof (Msg_NODE) + 1) : NULL), initmsg_size);
                                
            // notify the joining node of my presence
            //_net->connect (joiner.id, n.addr);
            _net->connect (n.addr);

            Msg_NODE myself (*self, _net->getaddr (self->id), 0);
            _buf[0] = 1;
            memcpy (_buf+1, &myself, sizeof (Msg_NODE));
            _net->sendmsg (joiner.id, ARBITRATOR, _buf, 1 + sizeof (Msg_NODE));
        }

        return true;
    }

    // see if any of my objects should transfer ownership
    // or if i should claim ownership to any new objects (if neighboring arbitrators fail)
    int 
    arbitrator_impl::check_owner_transfer ()
    {        
        vector <obj_id_t> remove_list;      // objects I own to be transfer to others

        voronoi *v = _vnode->getvoronoi();
        char buffer[VASTATE_BUFSIZ];

        // loop through all ownership in transit objects
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

        // loop through all objects I know
        for (map<obj_id_t, object *>::iterator it = _obj_store.begin (); it != _obj_store.end (); ++it)
        {
            object *obj = it->second;
            id_t id     = obj->get_id ();

            // if this object is no longer within my region, then send message to new owner
            if (v->contains (self->id, obj->get_pos ()) == false)
            {
                if (is_owner (id) == true)
                {
                    id_t closest = v->closest_to (obj->get_pos ());
                    int size = pack_transfer (obj, closest, buffer);
                    for_each_enclosing_arbitrator (it2)
                    {
                        if (it2->first != self->id)
                            _net->sendmsg (it2->first, TRANSFER, buffer, size);
                    }

                    remove_list.push_back (id);
                    _obj_in_transit [obj->get_id ()] = get_timestamp ();
                }
            }
            // there's an object within my region but I'm not owner, 
            // should claim it after a while
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
        }

        // remove the transferred objects from the list of owned objects
        for (int i=0; i<(int)remove_list.size (); i++)
        {
            // NOTE: the object should remain in the interest list, as some nodes might 
            // still be interested in the object, so simply remove ownership
            _obj_owned.erase (remove_list[i]);
        }

        return remove_list.size ();
    }

    // make adjustments to arbitrator AOI
    void 
    arbitrator_impl::adjust_aoi ()
    {
        // calculate a new AOI according to the merged AOI of all currently connected peers
        // basically, find the lower-left and upper right corner of the radius of all peers
                
        // start with a very small AOI then expands it accordingly to peers' actual AOI
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
        vector<id_t> &ens = _vnode->getvoronoi ()->get_en (self->id);
        map<id_t, Node *> node_map;

        // loop through new list of arbitrators and update my current list
        int i;
        bool has_changed = false;
        
        for (i=0; i < (int) nodes.size (); ++i)
        {
            id_t id = nodes[i]->id;

            if (id != self->id)
            {
                // find if a enclosing neighbor
                vector<id_t>::iterator itn;
                for (itn = ens.begin (); itn != ens.end (); itn ++)
                {
                    if (*itn == id)
                        break;
                }

                // not enclosing neighbor, just skip it
                if (itn == ens.end ())
                    continue;
            }

            // check if a new arbitrator found
            if (_arbitrators.find (id) == _arbitrators.end ())
            {
                // add a new arbitrator
                _arbitrators[id] = *(nodes[i]);
                _arbitrator_lasttick[id] = 0;
                if (_event_queue.find (id) == _event_queue.end ())
                    _event_queue[id] = map<timestamp_t, vector<event *> > ();

                // set arbitrators has changes, need to send ARBITRATOR to peers
                has_changed = true;
                
                // not to send objects/events to myself
                if (id != self->id)
                {
                    // send all owned objects first
                    send_objects (id, true);

                    // send all events in event queue (in event_queue[my_id])
                    // loop through all time
                    for (map<timestamp_t, vector<event *> >::iterator it = _event_queue[self->id].begin (); it != _event_queue[self->id].end (); it ++)
                    {
                        // loop through all event at the same time
                        vector<event *> & ev = it->second;
                        for (unsigned int ei = 0; ei < ev.size (); ei ++)
                        {
                            // send the event out
                            event *e = ev[ei];
                            int size = e->encode (_buf);
                            if (size > 0)
                                _net->sendmsg (id, EVENT, _buf, size);
                        }
                    }
                }

#ifdef DEBUG_DETAIL
                sprintf (_str, "[%d] learns about new enclosing arbitrator [%d]\r\n", self->id, id);
                _eo.output (_str);
#endif
            }
            else 
            {
                // update the arbitrator's info
                _arbitrators[id].aoi = nodes[i]->aoi;

                if (_arbitrators[id].pos != nodes[i]->pos)
                {
                    _arbitrators[id].pos = nodes[i]->pos;
                    has_changed = true;
                }
            }

            // create map for later removal
            node_map[id] = nodes[i];
        }
        
        // loop through current list of arbitrators to remove those no longer connected
        vector<id_t> remove_list;
        for (map<id_t, Node>::iterator it = _arbitrators.begin (); it != _arbitrators.end (); ++it)
        {
            if (node_map.find (it->second.id) == node_map.end ())
                remove_list.push_back (it->second.id);
        }

        for (i=0; i<(int)remove_list.size (); ++i)
        {
            // do it before _arbitrators list updates
            send_objects (remove_list[i], true, true);

            _arbitrators.erase (remove_list[i]);
            _arbitrator_lasttick.erase (remove_list[i]);
            _event_queue.erase (remove_list[i]);

#ifdef DEBUG_DETAIL
            sprintf (_str, "[%d] removes old enclosing arbitrator [%d]\r\n", self->id, remove_list[i]);
            _eo.output (_str);
#endif
        }


        // notify peers of arbitrator change
        // TODO: customize the notification for each peer, right now we simply re-send
        if ((has_changed || i > 0) && _peers.size () > 0)
        {
            char buffer[VASTATE_BUFSIZ];
            int len = encode_arbitrators (buffer);
            
            for (map<id_t, Node>::iterator it = _peers.begin (); it != _peers.end (); it++)
                _net->sendmsg (it->first, ARBITRATOR, buffer, len);
        }
    }    

    // remove any invalid avatar objects
    void 
    arbitrator_impl::validate_objects ()
    {        
        // loop through list of owned avatar objects and see if any has disconnected
        for (map<obj_id_t, object *>::iterator oi = _obj_store.begin (); oi != _obj_store.end (); ++oi)
        {
            // if it's an avatar object I own            
            if (is_owner (oi->first) == true && oi->second->peer != 0)
            {
                // if it's no longer connected then remove it
                if (_peers.find (oi->second->peer) == _peers.end ())
                {
                    if (_peers_countdown.find (oi->second->peer) == _peers_countdown.end ())
                        _peers_countdown [oi->second->peer] = COUNTDOWN_REMOVE_AVATAR;
                    else if (_peers_countdown [oi->second->peer] == 0)
                    {
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
        static char msg_buffer[VASTATE_BUFSIZ];
        static int  msg_buffer_size;

        bool not_saved;

        // dispatch event first
        for (vector<pair<id_t, event *> >::iterator it = _unforward_event.begin (); it != _unforward_event.end (); it ++)
        {
            id_t & from_id = it->first;
            event * e = it->second;
            // if the event comes from my enclosing arbitrator
            if (_arbitrators.find (from_id) != _arbitrators.end ())
            {
                // push it in event queue
                _event_queue[from_id][e->get_timestamp ()].push_back (e);
                //_arbitrator_lasttick[from_id] = get_timestamp ();

#ifdef DEBUG_DETAIL
                sprintf (_str, "[%d] arb handlemsg from [%d] EVENT push in queue time [%d]\r\n", self->id, from_id, e->get_timestamp ());
                _eo.output (_str);
#endif
            }

            // if the event comes from peers I am managing
            else if (_peers.find (from_id) != _peers.end ())
            {
                // find no mapping from peer to obj (this should not happen!)
                if (_peer2obj.find (e->get_sender ()) == _peer2obj.end ())
                {
                    // can't process, just delete the event
                    delete e;

                    sprintf (_str, "[%d] founds no mapping from peer [%d] to obj.\r\n", self->id, e->get_sender ());
                    _eo.output (_str);
                    continue;
                }

                // send event to related arbitrator (should include myself, and received again, processed by previous "if"
                not_saved = true;
                msg_buffer_size = 0;
                Position object_pos = _peer2obj[e->get_sender ()]->get_pos ();
                for (map<id_t, Node>::iterator it2 = _arbitrators.begin (); it2 != _arbitrators.end (); it2 ++)
                {
                    id_t the_arb = it2->first;
                    if (_vnode->getvoronoi ()->overlaps (it2->second.id, object_pos, _peers[e->get_sender ()].aoi, true))
                    {
                        // if that is me
                        if (it2->first == self->id)
                        {
                            // push it in event queue
                            _event_queue[self->id][e->get_timestamp ()].push_back (e);
                            _arbitrator_lasttick[self->id] = get_timestamp ();

                            // not to delete event
                            not_saved = false;
                        }
                        else
                        {
                            if (msg_buffer_size = 0)
                                msg_buffer_size = e->encode (msg_buffer);
                                
                            _net->sendmsg (it2->first, EVENT, msg_buffer, msg_buffer_size);
                            _arbitrator_lasttick[it2->first] = get_timestamp ();
                        }
#ifdef DEBUG_DETAIL
                        sprintf (_str, "[%d] arb handlemsg from [%d] EVENT forward to [%d]\r\n", self->id, from_id, it2->first);
                        _eo.output (_str);
#endif
                    }
                }

                if (not_saved)
                    delete e;
            }

            // receive event from unknown node (should this happens?)
            else
            {
#ifdef DEBUG_DETAIL
                sprintf (_str, "[%d] receive event from unknown peer [%d]\r\n", self->id, e->get_sender ());
                _eo.output (_str);
#endif

                delete e;
            }
        }

        // clear un-forwarded event queue
        _unforward_event.clear ();

        // maximum runnable
        timestamp_t m_timestamp = get_timestamp () + 1;

        for (map<id_t, Node>::iterator it = _arbitrators.begin (); it != _arbitrators.end (); it ++)
        {
            // if there's no event in queue, I can't process any event
            if (_event_queue.find (it->first) == _event_queue.end ())
                return 0;

            // find minimal between end of each arbitrators event queue
            if (_event_queue[it->first].end ()->first < m_timestamp)
                m_timestamp = _event_queue[it->first].end ()->first;
        }

#ifdef DEBUG_DETAIL
        // debuging messages
        sprintf (_str, "[%d] process event before time [%d]\r\n", self->id, m_timestamp);
        _eo.output (_str);
#endif

        // queue for save waiting for running event(s) (runnable event(s))
        vector<event *> ready_queue;
        map<id_t, vector<timestamp_t> > delete_list;

        // fetch all events in event_queue that are runnable
        for (map<id_t, Node>::iterator it = _arbitrators.begin (); it != _arbitrators.end (); it ++)
        {
            for (map<timestamp_t, vector<event *> >::iterator it2 = _event_queue[it->first].begin (); it2 != _event_queue[it->first].end (); it2 ++)
            {
                if (it2->first > m_timestamp)
                    break;

                if (!it2->second.empty ())
                    ready_queue.insert (ready_queue.end (), it2->second.begin (), it2->second.end ());
                delete_list[it->first].push_back (it2->first);
            }
        }

        // sort all "ready to process" events (by bubble sort..)
        int ready_queue_size = (int) ready_queue.size ();
        for (int i = 0; i < ready_queue_size - 1; i ++)
        {
            for (int j = ready_queue_size - 1; j > i; j--)
            {
                if ((ready_queue[j-1]->get_timestamp () > ready_queue[j]->get_timestamp ())
                    || ((ready_queue[j-1]->get_timestamp () == ready_queue[j]->get_timestamp ()) && (ready_queue[j-1]->get_sender () > ready_queue[j]->get_sender ())))
                {
                    event * e = ready_queue [j];
                    ready_queue[j] = ready_queue[j-1];
                    ready_queue[j-1] = e;
                }
            }
        }

        // process events
        for (vector<event *>::iterator it = ready_queue.begin (); it != ready_queue.end (); it ++)
        {
            event *e = *it;
            _logic->event_received (e->get_sender (), (*e));
        }

        // delete processed event record
        for (map<id_t, vector<timestamp_t> >::iterator it = delete_list.begin (); it != delete_list.end (); it ++)
        {
            for (vector<timestamp_t>::iterator it2 = it->second.begin (); it2 != it->second.end (); it2 ++)
                _event_queue[it->first].erase (*it2);
        }

        // delete processed event
        for (vector<event *>::iterator it = ready_queue.begin (); it != ready_queue.end (); it ++)
            delete (event *) (*it);

        return 0;
    }

    // encode a list of enclosing arbitrators    
    int 
    arbitrator_impl::encode_arbitrators (char *buf)
    {                
        vector<id_t> &list = _vnode->getvoronoi()->get_en (self->id);
        int n = list.size ();

        buf[0] = 0;
        char *p = buf+1;

        Msg_NODE nodeinfo;
        int count=0;

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
            memcpy (p, &nodeinfo, sizeof (Msg_NODE));
            p += sizeof (Msg_NODE);
            count++;
        }

        buf[0] = count;
     
        return 1 + count * sizeof (Msg_NODE);
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

            object obj2 = *obj;
            obj = & obj2;

            if (send_delete)
                obj->pos_version = 0;

            // encode all informations about the object
            obj_size    = obj->encode_pos (obj_buf, false);
            states_size = obj->encode_states (states_buf, false);

            // target is a peer, check AOI extra
            if (is_arbitrator || 
                (is_peer && obj->is_AOI_object (*node, true)))
            {
                if (_net->sendmsg (target, OBJECT, obj_buf, obj_size) != obj_size)
                {
#ifdef DEBUG_DETAIL
                    sprintf (_str, "[%d] send OBJECT to [%d] failed.\r\n", self->id, target);
                    _eo.output (_str);
#endif
                }

                // if send deletion, need not to send states
                if (!send_delete)
                    _net->sendmsg (target, STATE, states_buf, states_size);

                // if is peer, insert the record of "peer knows the object"
                if (is_peer)
                    _peers_knowledge[target][obj->get_id ()] = obj->version;
            }
        }

        return true;
    }

    // do peer's object discovery
    void arbitrator_impl::update_interests ()
    {
        char obj_buf[VASTATE_BUFSIZ];
        char obj_dbuf[VASTATE_BUFSIZ];
        char states_buf[VASTATE_BUFSIZ];
        
        int obj_size    = 0;
        int obj_dsize   = 0;
        int states_size = 0;

        // loop throught all objects I know
        for (map<obj_id_t, object *>::iterator it = _obj_store.begin (); it != _obj_store.end (); ++it)
        {
            object * obj = it->second;

            // check obj is discovered correctly for all connected peers
            obj_size = obj_dsize = 0;
            for (map<id_t,Node>::iterator it = _peers.begin (); it != _peers.end (); it ++)
            {
                bool aoi_obj = obj->is_AOI_object (it->second, true);
                bool peer_knew = (_peers_knowledge[it->first].find (obj->get_id ()) != _peers_knowledge[it->first].end ());
                // check if peer should know the object, and if he knews
                if (aoi_obj && !peer_knew)
                {
                    // check if already encoded
                    if (obj_size == 0)
                    {
                        obj_size = obj->encode_pos (obj_buf, false);
                        states_size = obj->encode_states (states_buf, false);
                    }

                    // insert knowledge record
                    _peers_knowledge[it->first][obj->get_id ()] = obj->version;

                    // send object
                    _net->sendmsg (it->first, OBJECT, obj_buf, obj_size);
                    if (states_size > 0)
                        _net->sendmsg (it->first, STATE, states_buf, states_size);
                }
                else if (!aoi_obj && peer_knew)
                {
                    if (obj_dsize == 0)
                    {
                        object obj2 = *obj;
                        obj2.pos_version = 0;
                        obj_dsize = obj2.encode_pos (obj_dbuf, false);
                    }

                    // remove list item
                    _peers_knowledge [it->first].erase (obj->get_id ());

                    // send delete object
                    _net->sendmsg (it->first, OBJECT, obj_dbuf, obj_dsize);
                }
            }
        }
    }

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

            // debug
            if (obj->pos_version == 0)
                printf ("[%d] arb: send_updates: found pos_version = 0 object [%d]\n", self->id, obj->get_id ());
            ////////

            bool i_am_owner = is_owner (obj->get_id ());
            timestamp_t obj_constant_time = get_timestamp () - _obj_update_time[obj->get_id ()];

            // check if this object need to refresh for following object expiring rules
            if (i_am_owner && obj_constant_time >= THRESHOLD_EXPIRING_OBJECT / 2)
                obj->pos_dirty = true;

            // or if the object may has no owner, delete it
            else if (!i_am_owner && obj_constant_time >= THRESHOLD_EXPIRING_OBJECT)
                obj->mark_deleted ();

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
                if (obj->is_AOI_object (it2->second, true))
                {
                    // debug
                    if (it2->first == obj->peer &&
                        obj->pos_version == 0)
                        printf ("[%d] arb try to send DELETE update to peer's avatar object [%d] peer [%d]\n", self->id, obj->get_id (), obj->peer);
                    ////////

                    if (obj_size > 0)
                        _net->sendmsg (it2->first, OBJECT, obj_buf, obj_size);

                    if (states_size > 0)
                        _net->sendmsg (it2->first, STATE, states_buf, states_size);
                }
            }

            // loop through all arbitratos
            if (is_owner (obj->get_id()))
            {
                for (map<id_t, Node>::iterator it2 = _arbitrators.begin (); it2 != _arbitrators.end (); it2 ++)
                {
                    if (it2->first == self->id)
                        continue;

                    if (obj_size > 0)
                        _net->sendmsg (it2->first, OBJECT, obj_buf, obj_size);

                    if (states_size > 0)
                        _net->sendmsg (it2->first, STATE, states_buf, states_size);
                }
            }

            // remove a non-owned object that's being deleted
            /*
            if (obj->is_alive () == false)
            {
                // check if I am owner (this should not happen !)
                if (is_owner (obj->get_id ()))
                {
                    sprintf (_str, "[%d] try to delete owned object [%d_%d]\r\n", self->id, obj->get_id () >> 16, obj->get_id () & 0xFFFF);
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
        sprintf (_str, "[%d] forwards %s request to [%d]\r\n", self->id, VASTATE_MESSAGE[(int)(msgtype-100)], target);
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
                sprintf (_str, "[%d] get OWNER of unknown obj [%d]\r\n", self->id, trmsg.obj_id >> 16, trmsg.obj_id & 0xFFFF);
                _eo.output (_str);
#endif
                return 0;
            }

            // set I'm owner
            _obj_owned [trmsg.obj_id] = true;
            _obj_update_time [trmsg.obj_id] = get_timestamp ();

            // send acknowledgement of TRANSFER
            if (trmsg.orig_owner != NET_ID_UNASSIGNED)
                _net->sendmsg (trmsg.orig_owner, TRANSFER_ACK, (char *) &trmsg.obj_id, sizeof (obj_id_t));

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
        }

        // if I am not new owner's enclosing neighbor
        else if (_arbitrators.find (trmsg.new_owner) == _arbitrators.end ())
        {
            // delete the object
            if (_obj_store.find (trmsg.obj_id) != _obj_store.end ())
                 _obj_store[trmsg.obj_id]->mark_deleted ();
        }

        // I am still owner's enclosing neighbor
        //   nothing to do

        return 0;
    }

    // find a suitable new arbitrator given a certain need/stress level
    bool 
    arbitrator_impl::find_arbitrator (int level, Msg_NODE &new_arb)
    {
        if (_potential_arbitrators.size () == 0)
            return false;

        /*
        // for now, simply return the first available
        new_arb = _potential_arbitrators[0];
        _potential_arbitrators.erase (_potential_arbitrators.begin ());
        */

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
    
    const char * arbitrator_impl::to_string ()
    {

        static std::string string_out;
        char buf[80];

        string_out.clear ();

        for_each_enclosing_arbitrator(it)
        {
            sprintf (buf, "e_arb [%d] (%d,%d)\r\n", it->first, (int) it->second.pos.x, (int) it->second.pos.y);
            string_out.append (buf);
        }

        string_out.append ("getnodes ==\r\n");

        vector<Node *> &nodes = _vnode->getnodes ();
        for (vector<Node *>::iterator it = nodes.begin () ;it != nodes.end (); it ++)
        {
            Node * n = *it;
            sprintf (buf, "e_arb [%d] (%d,%d)\r\n", n->id, (int) n->pos.x, (int) n->pos.y);
            string_out.append (buf);
        }

        return string_out.c_str ();
    }
    
} // namespace VAST