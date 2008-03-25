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

#include "peer_impl.h"


namespace VAST
{
    extern char VASTATE_MESSAGE[][20];

    //
    // peer interface
    //

    peer_impl::peer_impl (peer_logic *logic, network *net, int capacity, Addr &gateway)
        :_logic(logic), /*_net(net),*/ _gateway(gateway), _capacity(capacity), _time_diff (0),
         _joined (false), _arbitrator_error (0), _joinmsg (NULL)
    {
        _logic->register_interface (this);
        _self.id = NET_ID_UNASSIGNED;
        _arbitrator_request.id = NET_ID_UNASSIGNED;
        _curr_arbitrator.node.id = NET_ID_UNASSIGNED;

        this->setnet (net);
        net->start ();
    }

    // process messages (send new object states to neighbors)
    // returns the # of arbitrator promotions
    int 
    peer_impl::process_msg ()
    {   
        // logic need not to advance time anymore
        //_time++;

        // allow msghandler to process the incoming messages
        this->processmsg ();

        //printf ("peer [%d] increment time %d\n", _self.id, _time);        

        // remove any non-AOI objects
        update_interests ();

        // check if need to resync clock
        update_time ();

        // return the # of arbitrator requested for creation
        return 0;
    }
    

    // join VSP
    bool
    peer_impl::join (id_t id, Position &pt, aoi_t radius, char *auth, size_t size)
    {
        // 1.1 Joining peer contacts the server for a unique ID.
        _self.id  = id;
        _self.pos = pt;
        _self.aoi = radius;

        // copy down app-specific join/authentication message
        _joinmsg_size = size;
        if (_joinmsg != NULL)
            delete _joinmsg;
        _joinmsg = new char[_joinmsg_size];
        memcpy (_joinmsg, auth, size);

        // establish connection to server
        //_net->start ();                     // move it to vastate::create_peer for the purpose start net for vastid::get_id to send ID to server
        //_net->register_id (_self.id);       // already did in msghandler when receive ID from server
        
        // TODO/BUG: note this caps valid node-id to 65535
        _event_id_count = ((event_id_t)_self.id << 16)+1;                    
                
        // send out query for managing arbitrator (JOIN command with capacity)
        Msg_NODE node (_self, _net->getaddr (_self.id), _capacity);
        memcpy (_buf, &node, sizeof (Msg_NODE));

        // append app-specific authentication message
        char *p = _buf + sizeof (Msg_NODE);
        p[0] = _joinmsg_size;
        memcpy (p+1, _joinmsg, _joinmsg_size);

        _net->connect (NET_ID_GATEWAY, _gateway);
        _net->sendmsg (NET_ID_GATEWAY, JOIN, _buf, sizeof (Msg_NODE) + 1 + _joinmsg_size);

        return true;
    }
    
    // quit VSP
    void
    peer_impl::leave (bool notify)
    {
        // TODO: send leave message to arbitrator?
        _joined = false;
        _net->stop ();
    }
    
    // AOI related functions
    void
    peer_impl::set_aoi (aoi_t radius)
    {
        _self.aoi = radius;
    }
    
    aoi_t
    peer_impl::get_aoi ()
    {
        return _self.aoi;
    }
    
    /*
    // get self object (necessary?)        
    object *
    peer_impl::get_self ()
    {
        return NULL;
    }
    */

    event *
    peer_impl::create_event ()
    {
        if (_joined == false)
            return NULL;
        
        // create unique event-id
        event *e = new event (_self.id, _event_id_count++, _net->get_curr_timestamp ());
        return e;
    }
    
    // send an event to the current managing arbitrator
    bool
    peer_impl::send_event (event *e)
    {
        // no event allowed before I'm connected
        if (_curr_arbitrator.node.id == NET_ID_UNASSIGNED)
            return false;
#ifdef DEBUG_DETAIL
        sprintf (_str, "[%d] sends event [%d_%d] to [%d]\r\n", _self.id, e->get_id () >> 16, e->get_id () & 0xFFFF, _curr_arbitrator.node.id);
        _eo.output (_str);
#endif

        int size = e->encode (_buf);
        
        // send to current arbitrator
        bool result = (size > 0 && _net->sendmsg (_curr_arbitrator.node.id, EVENT, _buf, size) == size);
        delete e;     // TODO: BUG: memory leak, but if use delete will cause program to crash

        // if transmittion failure, count _arbitrator_error
        if (size > 0 && result == false)
            _arbitrator_error ++;
        else if (result == true)
            _arbitrator_error = 0;

        // check if there are too many errors
        if (_arbitrator_error >= 3)
        {
            _arbitrator_error = 0;
            _arbitrators.erase (_curr_arbitrator.node.id);
            _curr_arbitrator.node.id = NET_ID_UNASSIGNED;

            check_handover ();
        }
        
        return result;
    }
    
    // obtain any request to promote as arbitrator
    bool
    peer_impl::is_promoted (Node &info)
    {
        if (_arbitrator_request.id == NET_ID_UNASSIGNED)
            return false;
        else
        {
            info = _arbitrator_request;
            _arbitrator_request.id = NET_ID_UNASSIGNED;
            return true;
        }
    }

    // process messages sent by vastnode
    bool 
    peer_impl::handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size)
    {

#ifdef DEBUG_DETAIL
        sprintf (_str, "[%d] peer processmsg from [%d]: %s\r\n", 
                 _self.id, from_id, VASTATE_MESSAGE[(int)(msgtype-100)]);
        _eo.output (_str);
#endif
        
        switch (msgtype)
        {

        // Notification of object creation, destruction, update
        case OBJECT:
            if (size == sizeof (Msg_OBJECT))
            {                          
                Msg_OBJECT info (msg);
                object *obj;

                // if the object is new, add to object store
                if (_obj_store.find (info.obj_id) == _obj_store.end ())
                {
                    // if it's a deletion of unknown object, ignore it
                    if (info.pos_version == 0)
                        break;

                    obj = new object(info.obj_id);
                    _obj_store[info.obj_id] = obj;
                    
#ifdef DEBUG_DETAIL
                    sprintf (_str, "[%d] peer learns of new object [%s] from %d\r\n", _self.id, obj->tostring(), from_id);
                    _eo.output (_str);
#endif
                }
                else
                    obj = _obj_store[info.obj_id];
                
                // if try to delete my avatar object, ignore it
                if (obj->peer == get_self ().id
                    && info.pos_version == 0)
                {
                    printf ("[%d] receives DELETE self avatar object [%d] from [%d]\n", get_self ().id, obj->get_id (), from_id);
                    break;
                }

                // do actual update                    
                obj->decode_pos (msg);

                // also update the version
                obj->pos_version = info.pos_version;

                if (obj->pos_version == 0)
                    obj->mark_deleted ();

                // if it's my position update
                if (obj->peer == _self.id)
                {
                    _self.pos = obj->get_pos();
                    check_handover ();
                }
            }
            break;

        // Notification of attribute creation and update
        case STATE:
            {                   
                Msg_STATE info (msg);
                object *obj;
                
                // if STATE msg proceeds OBJECT, then we abort
                if (_obj_store.find (info.obj_id) == _obj_store.end ())
                    break;
                else
                    obj = _obj_store[info.obj_id];

                char *p = msg + sizeof (Msg_STATE);
                
                // during unpacking, fields will be automatically added or updated
                obj->reset_dirty ();
                if (obj->decode_states (msg) == false)
                    break;
                
                // also update the version
                obj->version = info.version;                                        
            }
            
            break;

        // Notification of arbitrator movement
        case ARBITRATOR:
            if ((size-1) % sizeof (Msg_NODE) == 0)
            {

                /*
                // TODO: adjust time with the server version
                timestamp_t server_time;
                memcpy (&server_time, msg + sizeof(id_t), sizeof(timestamp_t));
                
                printf ("%4d [%3d] got my id: %d sync time with server: %d to %d\n", _time, (int)_self.id, (int)_self.id, _time, server_time);
                _time = server_time;                                           
                */

                // we're considered joined only after having contacted valid arbitrators
                _joined = true;

                // clean up arbitrators
                _arbitrators.clear ();

                // store to arbitrator list
                int n = msg[0];
                char *p = msg+1;
                Msg_NODE node;
                for (int i=0; i<n; i++)
                {
                    memcpy (&node, p, sizeof (Msg_NODE));
                    _arbitrators[node.node.id] = node;
                    p += sizeof (Msg_NODE);
                }
                check_handover ();
            }
            break;

        // Info for the overloaded arbitrator
        case PROMOTE:
            if (size == sizeof (Msg_NODE))
            {
                Msg_NODE requester (msg);
                
                // we'll join a little bit off the requesting arbitrator's position
                //requester.node.pos.x += 10;
                //requester.node.pos.y += 10;

                // record this promotion request
                _arbitrator_request = requester.node;
            }
            break;

        default:
            return false;
        }

        return true;
    }


    // 
    // private methods
    //

    void
    peer_impl::check_handover ()
    {
#ifdef DEBUG_DETAIL
        sprintf (_str, "peer [%d] check_handover () called\r\n", _self.id);
        _eo.output (_str);
#endif

        double shortest = _arbitrators.begin ()->second.node.pos.dist (_self.pos);
        Msg_NODE *ptr = &(_arbitrators.begin ()->second);

        // find out the closest arbitrator
        for (map<id_t, Msg_NODE>::iterator it = _arbitrators.begin (); 
             it != _arbitrators.end (); it++)
        {
            double dist = it->second.node.pos.dist (_self.pos);

            //sprintf (str, "peer [%d] shortest: [%d] %f dist: [%d] %f\r\n", _self.id, ptr->node.id, shortest, it->second.node.id, dist);
            //_eo.output (str);
            
            if (dist < shortest)
            {
                shortest = dist;
                ptr = &it->second;
            }
        }               

        /*
        if (_curr_arbitrator != NULL)
        {
            sprintf (str, "[%d] curr_arbitrator [%d] selected [%d]\r\n", _self.id, _curr_arbitrator->node.id, ptr->node.id);
            _eo.output (str);
        }
        */

        // check if it's the current one, if not then switch by sending a ENTER
        if (ptr->node.id != _curr_arbitrator.node.id)
        {
#ifdef DEBUG_DETAIL
            sprintf (_str, "peer [%d] switches arbitrator from [%d] to [%d]\r\n", _self.id, _curr_arbitrator.node.id, ptr->node.id);
            _eo.output (_str);      
#endif
            char *p = _buf;
            memcpy (p, &_self, sizeof (Node));
            p += sizeof (Node);

            // store list size
            p[0] = _obj_store.size ();
            p += 1;
            
            // attach a list of objects I know and their version numbers
            for (map<obj_id_t, object *>::iterator it = _obj_store.begin (); 
                 it != _obj_store.end (); ++it)
            {
                object *obj = it->second;
                
                Msg_OBJ_UPDATEINFO info;
                info.obj_id  = obj->get_id();
                info.version = obj->version;
                
                memcpy (p, &info, sizeof (Msg_OBJ_UPDATEINFO));
                p += sizeof (Msg_OBJ_UPDATEINFO);
            }
                
            _net->connect (ptr->node.id, ptr->addr);
            _net->sendmsg (ptr->node.id, ENTER, _buf, 
                           sizeof (Node) + 1 + sizeof (Msg_OBJ_UPDATEINFO) * _obj_store.size (), true);
            if (_curr_arbitrator.node.id != NET_ID_UNASSIGNED)
                _net->disconnect (_curr_arbitrator.node.id);
            _curr_arbitrator = *ptr;
        }
    }

    // notify peer_logic of any updated states and
    // check to see if the peer needs to remove any non-AOI objects
    void 
    peer_impl::update_interests ()
    {
        // do an interest check
        vector<obj_id_t> remove_list;
        for (map<obj_id_t, object *>::iterator it = _obj_store.begin (); 
             it != _obj_store.end (); ++it)
        {
            object *obj = it->second;
            double dist = _self.pos.dist (obj->get_pos());
            
            // an AOI-object
            if (obj->is_alive () && dist <= _self.aoi)
            {
                if (obj->visible == false)
                {
                    _logic->obj_discovered (obj, obj->peer == _self.id);
                    obj->visible = true;
                }
                // notify for position update of an existing object
                else if (obj->pos_dirty)
                {
                    _logic->pos_changed (obj->get_id (), obj->get_pos(), obj->pos_version);
                    obj->pos_dirty = false;
                }

                // loop through each attribute and notify the peer logic
                for (int j=0; j<obj->size (); ++j)
                {
                    int length;
                    void *p;
                    if (obj->get (j, &p, length) == false || obj->is_dirty (j) == false)
                        continue;
                    
                    // if dirty then notify the peer logic layer
                    _logic->state_updated (obj->get_id (), j, p, length, obj->version);
                }
            }
            // notify peer_logic of object deletion
            else 
            {
                if (obj->visible == true)
                {
                    _logic->obj_deleted (obj);
                    obj->visible = false;
                }

                // if object is already out of the buffered AOI area, remove it
                if (dist > (_self.aoi * VASTATE_BUFFER_MULTIPLIER) || 
                    obj->is_alive () == false)
                    remove_list.push_back (it->first);
            }

            // reset dirty flag
            obj->reset_dirty ();
        }

        // remove non-AOI objects
        for (int i=0; i<(int)remove_list.size (); i++)
        {
            delete _obj_store[remove_list[i]];
            _obj_store.erase (remove_list[i]);
        }
    }

    void
    peer_impl::update_time ()
    {
        // TODO: implement a simple NTP protocol to sync time with gateway (or a time sync source)
        _time_diff = 0;
    }

} // namespace VAST




