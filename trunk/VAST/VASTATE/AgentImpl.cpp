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

#include "AgentImpl.h"


namespace Vast
{
    extern char VASTATE_MESSAGE[][20];

    //
    // Agent interface
    //

    AgentImpl::AgentImpl (AgentLogic *logic, VAST *vastnode)
        :Agent (), _logic (logic), _vastnode (vastnode), _state (ABSENT), _admitted (false), _join_attempts (0)
    {
        _self = *vastnode->getSelf ();
        _vastnode->addHandler (this);
        _logic->registerInterface (this);
        _self_objid = NULL_OBJECT_ID;

        _sub_no = 0;
       
    }
    
    AgentImpl::~AgentImpl ()
    {           
        if (isJoined ())
            leave ();

        // remove known objects
        map<obj_id_t, Object *>::iterator it = _obj_store.begin ();
        for (; it != _obj_store.end (); it++)
            delete it->second;
        _obj_store.clear ();
        
        _logic->unregisterInterface ();
        _vastnode->removeHandler (this);

        // NOTE: logic is created outside, so cannot delete here    
    }

    bool    
    AgentImpl::login (char *URL, const char *auth, size_t auth_size)
    {        
       
        id_t target = NET_ID_GATEWAY;
                
        // if login is specified, update gateway info, otherwise use default
        // TODO / BUG: the URL given here may conflict with what's defined as gateway
        //             at the VAST level
        //             should have only one place to assign gateway IP
        if (URL != NULL)
        {
            // extract gateway's IP
            Addr gateway;

            gateway.host_id  = NET_ID_GATEWAY;
            gateway.publicIP = IPaddr (URL, GATEWAY_DEFAULT_PORT);
                               
            notifyMapping (target, &gateway);
        }

        // send login request with authentication token to gateway
        Message msg (LOGIN);
        msg.priority = 1;
        msg.store (auth, auth_size, true);
        msg.addTarget (target);

        // assign the arbitrator's message group, this will allow this message be received by the gateway's arbitrator node
        msg.msggroup = MSG_GROUP_VASTATE_ARBITRATOR;
        sendMessage (msg);

        return true;
    }

    bool 
    AgentImpl::logout ()
    {
        Message msg (LOGOUT);      
        msg.priority = 1;
        msg.addTarget (NET_ID_GATEWAY);

        // assign the arbitrator's message group, this will allow this message be received by the gateway's arbitrator node
        msg.msggroup = MSG_GROUP_VASTATE_ARBITRATOR;
        sendMessage (msg);

        return true;
    }

    // send a message to gateway arbitrator (i.e., server)
    bool        
    AgentImpl::send (Message &out_msg)
    {
        Message msg (out_msg);
        msg.priority = 2;

        // modify msgtype
        msg.msgtype = (msg.msgtype << VAST_MSGTYPE_RESERVED) | AGENT_MSG;

        msg.addTarget (NET_ID_GATEWAY);

        // assign the arbitrator's message group, this will allow this message be received by the gateway's arbitrator node
        msg.msggroup = MSG_GROUP_VASTATE_ARBITRATOR;
        sendMessage (msg);

        return true;
    }

    // join the state management layer
    bool
    AgentImpl::join (Position &pos)
    {
        if (_vastnode->isJoined () == false || _state != ABSENT)
            return false;
       
        // after the VASTnode has successfully joined the overlay, 
        // get assigned unique ID & initiate subscription                

        printf ("[%ld] AgentImpl::join () prepare to subscribe my AOI\n", _self.id);

        // store joining position        
        _self.aoi.center.x = pos.x;
        _self.aoi.center.y = pos.y;

        Area aoi = _self.aoi;
        aoi.radius = (length_t)(aoi.radius * (1.0 + VASTATE_BUFFER_RATIO*3));
              
        // TODO: if it's re-joining, use the same subscription?
        // subscribe my AOI from VAST
        if (_sub_no == 0 || _vastnode->isSubscribing (_sub_no) == false)
            _sub_no = _vastnode->subscribe (aoi, VAST_LAYER_UPDATE);
        else
            printf ("valid sub_no already exists: %ld\n", _sub_no);
               
        // reset self object ID, as we use this to check if join is successful
        _self_objid = NULL_OBJECT_ID;
        
        _join_timeout = TIMEOUT_JOINING;        
        _state = JOINING;   

        return true;
    }
    
    // quit VASTATE
    void
    AgentImpl::leave ()
    {
        if (isJoined () == false)
            return;

        // send a LEAVE message to arbitrator
        Message msg (LEAVE);
        msg.priority = 1;
        sendEvent (msg);

        // NOTE: cannot leave vastnode now because we need to get 
        //       LEAVE message back

        _self_objid = NULL_OBJECT_ID;
        _state = ABSENT;
    }

    // moves to a new position
    void    
    AgentImpl::move (Position &pos)
    {
        if (isJoined () == false)
            return;

        // send out MOVE event with new position
        // TODO: whether this MOVE event will be granted by the arbitrator is unknown
        _self.aoi.center = pos;

        Message msg (MOVEMENT);
        msg.priority = 2;

        msg.store (pos);
        msg.store (_self_objid);

        // move event can be sent unreliably
        //msg.reliable = false;        
        sendEvent (msg);
     
        // change my subscription area
        // TODO: redundent info with MOVEMENT event
        Area aoi = _self.aoi;
        aoi.radius = (length_t)(aoi.radius * (1.0 + VASTATE_BUFFER_RATIO*3));

        _vastnode->move (_sub_no, aoi);
    }
    
    // AOI related functions
    void
    AgentImpl::setAOI (length_t radius)
    {
        _self.aoi.radius = radius;
    }
    
    length_t
    AgentImpl::getAOI ()
    {
        return _self.aoi.radius;
    }
    
    Event *
    AgentImpl::createEvent (msgtype_t event_type)
    {
        // create unique event-id
        return new Event (_self.id, event_type, _net->getTimestamp ());
    }
    
    // send an Event to the current managing arbitrator
    bool
    AgentImpl::act (Event *e)
    {
        if (isJoined () == false)
            return false;

#ifdef DEBUG_DETAIL
        sprintf (_str, "[%d] publishes Event", _self.id);
        _eo.output (_str);
#endif
        /*
        // debug only (test event-based movement)
        if (e->type == 37)
        {
            Position pos;
            e->get (0, pos);

            _self->aoi.center.x = pos.x;
            _self->aoi.center.y = pos.y;

            _vastnode->move (_sub_no, _self->aoi);
        }
        */

        Message msg (EVENT);
        msg.priority = 2;
        msg.store (*e);
       
        bool result = sendEvent (msg);

        // TODO: perform error handling

        delete e;     
       
        return result;
    }
    
    // process messages sent by vastnode
    bool 
    AgentImpl::handleMessage (Message &in_msg)
    {     
        /*
        // if not yet joined, only allow LOGIC, OBJECT_C, POSITION to pass
        if (isJoined () == false && !(
            in_msg.msgtype == LOGIN || 
            in_msg.msgtype == OBJECT_C || 
            in_msg.msgtype == POSITION))
            return false;
        */
     
        // extract the app-specific message type if exists
        msgtype_t app_msgtype = APP_MSGTYPE(in_msg.msgtype);
        in_msg.msgtype = VAST_MSGTYPE(in_msg.msgtype);

#ifdef DEBUG_DETAIL
        sprintf (_str, "[%d] AgentImpl::handleMessage from [%d]: %s\r\n", 
                       _self.id, in_msg.from, VASTATE_MESSAGE[in_msg.msgtype-10]);
        _eo.output (_str);
#endif

        switch (in_msg.msgtype)
        {
        // results of login has returned from gateway
        case LOGIN:
            {
                size_t size = in_msg.extract (_str, 0);

                if (size > VASTATE_BUFSIZ)
                {
                    printf ("AgentImpl::handleMessage LOGIN. Authenication result exceeds VASTATE_BUFSIZ\n");
                }
                else
                {
                    // call arbitrator logic to handle authentication
                    _str[size] = 0;
                    _admitted = true;
                    _logic->onAdmit (_str, size);
                }
            }
            break;

        // object creation notification
        case OBJECT_C:
            {
                obj_id_t obj_id;
                Position pos;
                version_t pos_version;
                id_t     agent_id;
                byte_t   type;                
                byte_t   attr_size;
                
                in_msg.extract (obj_id);
                in_msg.extract (type);
                in_msg.extract (attr_size);
                in_msg.extract (pos); 
                in_msg.extract (pos_version);
                in_msg.extract (agent_id);

                // clear the request record
                if (_obj_requested.find (obj_id) != _obj_requested.end ())
                    _obj_requested.erase (obj_id);

                // get pointer to object in question
                Object *obj = (_obj_store.find (obj_id) != _obj_store.end () ? 
                               _obj_store[obj_id] : NULL);

                // if the Object is new, add to Object store
                if (obj == NULL)
                {
                    obj = new Object (obj_id);
                    obj->setPosition (pos);
                    obj->pos_version = pos_version;
                    obj->type = type;
                    obj->agent = agent_id;

                    // record # of expected attributes for this object
                    _attr_sizes[obj_id] = attr_size;

                    //notify for the discovery of a new object
                    _obj_store[obj_id] = obj;
#ifdef DEBUG_DETAIL
                    sprintf (_str, "[%ld] Agent learns of new Object [%s] from %ld\r\n", _self.id, obj->toString(), in_msg.from);
                    _eo.output (_str);
#endif

                }
                else
                {
                    sprintf (_str, "[%ld] AgentImpl: redundent object creation notice for object [%s] from %ld\r\n", _self.id, obj->toString(), in_msg.from);
                    _eo.output (_str);
                }

                // NOTE: we check it here as we could receive OBJECT_C again 
                //       after arbitrator failure and agent publishes JOIN event again
                if (isJoined () == false && obj->agent == _self.id)
                    _self_objid = obj_id;
            }
            break;

        // object deletion notification
        case OBJECT_D:
            {
                obj_id_t obj_id;
                in_msg.extract (obj_id);

                // get pointer to object in question
                Object *obj = (_obj_store.find (obj_id) != _obj_store.end () ? 
                               _obj_store[obj_id] : NULL);

                if (obj != NULL)
                {
                    // if my avatar object is being deleted, ignore it
                    // TODO: should not normally happen, bug? (as this should only happen when logging out)
                    if (obj->agent == getSelf ()->id)
                    {
                        if (in_msg.from == _arbitrators[0].id)
                        {
                            sprintf (_str, "[%ld] receives DELETE for myself [%ld] from current arbitrator [%ld]\n", getSelf ()->id, obj->getID (), in_msg.from);
                            _eo.output (_str);
                        }
                    }

                    // otherwise prepare for deletion
                    else                            
                        obj->markDeleted ();
                }
                else
                {
                    sprintf (_str, "[%ld] AgentImpl: invalid deletion for object [%ld] from %ld\r\n", _self.id, obj_id, in_msg.from);
                    _eo.output (_str);
                }
            }
            break;

        // Notification of Object creation, destruction, update
        case POSITION:
            if (in_msg.size == sizeof (Msg_OBJECT))
            {                          
                Msg_OBJECT info;
                in_msg.extract ((char *)&info, sizeof (Msg_OBJECT));

                // get pointer to object in question
                Object *obj = (_obj_store.find (info.obj_id) != _obj_store.end () ? 
                               _obj_store[info.obj_id] : NULL);

                if (obj == NULL)
                {
                    double dist = _self.aoi.center.distance (info.pos);

                    // request object if it's coming *close* to my AOI
                    // NOTE this is a different response from how STATE handles it
                    if (dist <= (_self.aoi.radius * (1.0 + VASTATE_BUFFER_RATIO)))
                        requestObject (in_msg.from, info.obj_id);

                    break;
                }

                // restore position info for live objects
                if (obj->isAlive ())
                {
                    in_msg.reset ();
                    obj->decodePosition (in_msg);               
                }
            }
            break;

        // Notification of attribute creation and update
        case STATE:
            {                           
                Msg_STATE info;
                in_msg.extract ((char *)&info, sizeof (Msg_STATE));
                
                Object *obj = (_obj_store.find (info.obj_id) != _obj_store.end () ? 
                               _obj_store[info.obj_id] : NULL);

                // if STATE msg proceeds OBJECT_C, then we request
                if (obj == NULL)
                {
                    requestObject (in_msg.from, info.obj_id);
                    break;
                }

                // update object state for live objects
                if (obj->isAlive ())
                {
                    in_msg.reset ();

                    // during unpacking, fields will be automatically added or updated
                    obj->decodeStates (in_msg); 
                }                                                 
            }
            
            break;

        case GATEWAY_MSG:
            {
                // restore app-specific message type
                in_msg.msgtype = app_msgtype;

                // let the Gateway arbitrator logic to process it
                _logic->onMessage (in_msg);
            }
            break;

        // receive notifiation of my current arbitrator
        case ARBITRATOR:
            {
                _arbitrators.clear ();
                listsize_t n;

                in_msg.extract (n);
                Node arbitrator;

                // extract arbitrator list (current arbitrator is the first)
                for (size_t i=0; i < n; i++)
                {
                    in_msg.extract (arbitrator);
                    _arbitrators.push_back (arbitrator);
                    notifyMapping (arbitrator.id, &arbitrator.addr);
                }
            }
            break;

        /*
        // request from a new arbitrator to re-join the new arbitrator's authority
        case REJOIN:
            {
                Message msg (JOIN);
                msg.store (_self);
                msg.store (_self_objid);

                msg.msggroup = MSG_GROUP_VASTATE_ARBITRATOR;
                msg.addTarget (in_msg.from);
                sendMessage (msg);
            }
            break;
        */

        case DISCONNECT:
            {
                // check if my current arbitrator has disonnected me
                if (_arbitrators.size () > 0)
                {
                    if (in_msg.from == _arbitrators[0].id)
                    {
                        printf ("[%ld] AgentImpl: disconnected by my current arbitrators\n", _self.id);
                        
                        // re-initiate the JOIN procedure
                        _state = ABSENT;
                        join (_self.aoi.center);
                    }
                    else
                    {
                        // if the disconnecting node is any of the arbitrators, then remove it
                        for (size_t i=0; i < _arbitrators.size (); i++)
                        {
                            if (in_msg.from == _arbitrators[i].id)
                            {
                                _arbitrators.erase (_arbitrators.begin () + i);
                                break;
                            }
                        }
                    }
                }                
            }
            break;

        default:
            {
                printf ("[%ld] AgentImpl:handleMessage () unhandled message, msgtype: %d\n", _self.id, in_msg.msgtype);
            }
            return false;
        }

        return true;
    }


    // perform some tasks after all messages have been handled (default does nothing)        
    void 
    AgentImpl::postHandling ()
    {        
        if (_state == ABSENT)
            return;

        // 1st stage of join, checking if we've successfully subscribed the AOI
        if (_state == JOINING)
        {
            if (_vastnode->isSubscribing (_sub_no))
            {
                printf ("[%ld] AgentImpl::postHandling () AOI area (%ld) subscribed, prepare to send JOIN request\n", _self.id, _sub_no);
                _state = JOINING_2;    
                _join_timeout = 0;
                _join_attempts = 0;
            }
            // attempt to re-join if timeout is reached
            else if (--_join_timeout <= 0)
            {
                join (_self.aoi.center);
            }
        }
        // second stage of join, sending JOIN event to arbitrator
        else if (_state == JOINING_2)
        {            
            // if we've obtained our avatar object from the arbitrator & current arbitrator is known,
            // then it's considered joined
            if (_self_objid != NULL_OBJECT_ID && _arbitrators.size () > 0)
            {
                _state = JOINED;
                _join_attempts = 0;
            }

            else if (--_join_timeout <= 0)
            {
                printf ("[%ld] AgentImpl::postHandling () send JOIN request to my arbitrator\n", _self.id);

                _join_attempts++;

                // publish JOIN event with joining position
                Message msg (JOIN);
                msg.priority = 1;
                msg.store (_self);
                msg.store (NULL_OBJECT_ID);
             
                Area area (_self.aoi.center, 0);
                _vastnode->publish (area, VAST_LAYER_EVENT, msg);

                _join_timeout = TIMEOUT_JOINING;
            }
            
            // we've tried to join three times and fail, restart whole process
            else if (_join_attempts > 3)
            {
                printf ("send JOIN request to arbitrator > 3 times, leave then re-join\n");
                leave ();
                join (_self.aoi.center);
                _join_attempts = 0;
            }

        }

        // successfully joined, perform normal operations
        else
        {
            // check for incoming VAST messages
            Message *msg;
            
            // NOTE: that we will use the same message handling mechanism as other messages
            while ((msg = _vastnode->receive ()) != NULL)
            {
                if (this->handleMessage (*msg) == false)
                    printf ("[%ld] AgentImpl::postHandling () unhandled / unknown msgtype %d from VAST\n", _self.id, msg->msgtype);
            }

            // remove any non-AOI objects
            updateAOI ();

            // check if my avatar object still exists
            checkConsistency ();
        }
    }

    // 
    // private methods
    //

    // deliver an event to current arbitrator (might also include enclosing, depend on method)
    inline bool 
    AgentImpl::sendEvent (Message &msg)
    {
        bool result;
       
#ifdef PUBLISH_EVENT
        // perform a point publication (AOI radius is 0)
        Area area (_self.aoi.center, 0);
        result = _vastnode->publish (area, VAST_LAYER_EVENT, msg);
#else
        // send to current arbitrator
        msg.addTarget (_arbitrators[0].id);

#ifdef EVENT_TO_ENCLOSING_ARBITRATORS
        
        // my distance to current arbitrator
        double min_dist = _arbitrators[0].aoi.center.distance (_self.aoi.center) + 5.0;
        int min_pos = 0;

        for (size_t i=1; i < _arbitrators.size (); i++)
        {
            // only send to the closest enclosing arbitrator
            double dist = _arbitrators[i].aoi.center.distance (_self.aoi.center);
            if (dist < min_dist)
            {
                min_dist = dist;
                min_pos = i;
            }                
        }

        msg.addTarget (_arbitrators[min_pos].id);
#endif
        result = _vastnode->send (msg);
#endif
    
        return result;
    }

    // notify AgentLogic of any updated states and
    // check to see if the Agent needs to remove any non-AOI objects
    void 
    AgentImpl::updateAOI ()
    {
        // do an interest check
        vector<obj_id_t> remove_list;
        for (map<obj_id_t, Object *>::iterator it = _obj_store.begin (); it != _obj_store.end (); ++it)
        {
            Object *obj = it->second;
            obj_id_t obj_id = obj->getID ();
                                  
            // an AOI-object
            // NOTE: should not judge AOI again as we should follow the arbitrator's decision
            // TODO: unless we want to perform local object removal
            if (obj->isAlive ()) 
            {
                // first detmine how far the object is from me
                double dist = _self.aoi.center.distance (obj->getPosition ());
                double limit = (_self.aoi.radius * (1.0 + VASTATE_BUFFER_RATIO));
                //double limit = (_self.aoi.radius * (1.0));

                // if this object hasn't been seen, but is within view, and 
                // number of expected attributes have all arrived, then we notify
                if (obj->visible == false)
                {                    
                    if (dist <= limit && 
                        obj->size () == _attr_sizes[obj_id])
                    {
                        _logic->onCreate (*obj, obj->agent == _self.id);
                        obj->visible = true;
                    }
                } 
                // if the object is outside view, turn it off
                else if (dist > limit)
                {
                    _logic->onDestroy (obj->getID ());
                    obj->visible = false;
                }
                // for currently visible object (that is, already discovered)
                else  
                {
                    // notify for position update of an existing object
                    if (obj->pos_dirty)
                    {
                        _logic->onMove (obj_id, obj->getPosition(), obj->pos_version);
                    }
                
                    // loop through each attribute and notify the Agent logic
                    for (int j=0; j < obj->size (); ++j)
                    {
                        word_t length;
                        void *p;
                        if (obj->get (j, &p, length) == false || obj->isDirty (j) == false)
                            continue;
                        
                        // if dirty then notify the Agent logic layer
                        _logic->onUpdate (obj_id, j, p, length, obj->version);
                    }
                }

#ifdef DISCOVERY_BY_REQUEST
                                
                // if this active object has moved out of my view
                // NOTE that we're a bit more tolerant for deletion 
                // TODO: should also add an inactivity timeout for object removal
                //if (dist > (_self.aoi.radius * (1.0 + VASTATE_BUFFER_RATIO)))  
                if (dist > limit) 
                {                    
                    if (_remove_countdown.find (obj_id) == _remove_countdown.end ())
                        _remove_countdown[obj_id] = 0;

                    _remove_countdown[obj_id]++;

                    // remove the object if it's out of AOI for some time
                    if (_remove_countdown[obj_id] > COUNTDOWN_REMOVE_OBJ)
                    {
                        obj->markDeleted ();
                        _remove_countdown.erase (obj_id);
                    }
                }
                // remove the countdown to remove object if it's AOI object again
                else if (_remove_countdown.find (obj_id) != _remove_countdown.end ())
                    _remove_countdown.erase (obj_id);                
#endif
            }
            // notify AgentLogic of Object deletion
            else 
            {
                // make sure any visible objects are notified deletion first
                if (obj->visible == true)
                {
                    _logic->onDestroy (obj->getID ());
                    obj->visible = false;
                }
                
                remove_list.push_back (it->first);
            }

            // reset dirty flag
            obj->pos_dirty = false;
            obj->resetDirty ();
        }

        // remove non-AOI objects
        for (unsigned int i=0; i < remove_list.size (); i++)
        {
            delete _obj_store[remove_list[i]];
            _obj_store.erase (remove_list[i]);
            _attr_sizes.erase (remove_list[i]);
        }
    }

    // send request to arbitrator for full object states (when POSITION or STATE is received for unknown objects)
    void 
    AgentImpl::requestObject (id_t arbitrator, obj_id_t &obj_id)
    {
        // avoid request redundency
        if (_obj_requested.find (obj_id) != _obj_requested.end ())
            return;

        // print error message
        sprintf (_str, "[%ld] AgentImpl::requestObject () object [%ld] not found for update from [%ld]\n", getSelf ()->id, obj_id, arbitrator);
        _eo.output (_str);

#ifdef DISCOVERY_BY_REQUEST

        // send request to the arbitrator that sends me the update
        Message msg (OBJECT_R);
        msg.priority = 1;
        
        // store the msggroup for the intended resposne response as responding to agent request
        listsize_t n    = MSG_GROUP_VASTATE_AGENT;       
        msg.store (n);

        // only one object to request
        n = 1;
        msg.store (n);

        // store obj_id, 
        // NOTE a copy has to be made as store () takes a Serializable object
        // but obj_id is already a reference, so passing it in would create complier-specific problems
        // that would crash during run-time
        obj_id_t   oid = obj_id;
        msg.store (oid);
        msg.addTarget (arbitrator);
        msg.msggroup = MSG_GROUP_VASTATE_ARBITRATOR;
        sendMessage (msg);

        _obj_requested[oid] = arbitrator;
#endif

    }

    // check if my own avatar object still exists
    void 
    AgentImpl::checkConsistency ()
    {
        if (_obj_store.find (_self_objid) != _obj_store.end ())
        {
            // normal, perceived self is within half AOI of actual self
            if (_obj_store[_self_objid]->getPosition ().distance (_self.aoi.center) < (_self.aoi.radius / 2))
                return;
        }

        printf ("[%ld] AgentImpl::checkConsistency () my avatarObject is missing or too far\n", _self.id);

        // re-join, NOTE that the below will only run once
        _state = ABSENT;
        join (_self.aoi.center);

        /*
        // distance between my actual self & perceived self
        double dist = 0;

        map<obj_id_t, Object *>::iterator it = _obj_store.begin ();
        for (; it != _obj_store.end (); ++it)
        {
            Object *obj = it->second;
            obj_id_t obj_id = obj->getID ();

            if (obj->agent == _self.id)
            {
                dist = obj->getPosition ().distance (_self.aoi.center);
                break;
            }
        }        

        if (it == _obj_store.end () || dist > (_self.aoi.radius / 2))
        {
            printf ("[%ld] AgentImpl::checkConsistency () my avatarObject is missing or too far\n", _self.id);
            leave ();
            join (_self.aoi.center);
        }
        */
    }

} // namespace Vast
