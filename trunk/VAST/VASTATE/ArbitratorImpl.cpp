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

// TODO: when does the Arbitrator adjust its AOI?

#include "ArbitratorImpl.h"

namespace Vast
{
    extern char VASTATE_MESSAGE[][20];

    // initialize an arbitrator
    ArbitratorImpl::ArbitratorImpl (ArbitratorLogic *logic, VAST *vastnode, VASTATEPara &para)
        : Arbitrator (), _state (ABSENT), _VONpeer (NULL), 
          _vastnode (vastnode), _sub_no (0), _logic (logic),  
          obj_msg (0), pos_msg (0), state_msg (0),
          _obj_id_count (1), _load_counter (0), _overload_requests (0), _tick (0), 
          _last_send (0), _last_recv (0), _statfile (NULL), 
          _para (para)
    {
        _self = *_vastnode->getSelf ();
        _vastnode->addHandler (this);
        _logic->registerInterface (this);

        if (_para.default_aoi == 0 || _para.world_height == 0 || _para.world_width == 0)
        {
            // default world model after Second Life's region
            _para.default_aoi = 65;
            _para.world_height = _para.world_height = 256;
            printf ("ArbitratorImpl warning: VASTATEpara world_height, world_width, or default_aoi not defined, use default values (256, 256) world, AOI: 65\n");
        }

        // register with gateway as a potential arbitrator (only if I've got public IP)
        if (isGateway (_self.id) == false && _vastnode->hasPublicIP () == true)
        {
            Message msg (ARBITRATOR_C);
            msg.priority = 1;
            msg.store (_self);
            msg.addTarget (NET_ID_GATEWAY);
            sendMessage (msg);            
        }

        // start logging statistics if I'm gateway
        if (isGateway (_self.id))
        {
            _statfile = fopen ("GW_stat.txt", "wb");
        }       
    }
    
    ArbitratorImpl::~ArbitratorImpl ()
    {                                  
        // make sure we've left & released first
        this->leave ();

        _logic->unregisterInterface ();
        _vastnode->removeHandler (this);

        // can't delete out-DLL 's memory space

        // remove object store        
        for (map<obj_id_t, StoredObject>::iterator it = _obj_store.begin (); it != _obj_store.end (); it++)
        {
            if (it->second.obj != NULL)
                delete it->second.obj;
        }
        _obj_store.clear ();

        // remove unprocessed events        
        for (multimap<timestamp_t, Event *>::iterator it = _events.begin (); it != _events.end (); it++)
            delete it->second;
        _events.clear ();
        
        for (map<obj_id_t, Message *>::iterator it = _transfer_msg.begin (); it != _transfer_msg.end (); it++)
            delete it->second;
        _transfer_msg.clear ();

        // clear agent's knowledge of AOI objects
        _known_objs.clear ();

        // close up file pointer
        if (_statfile != NULL)
        {
            fclose (_statfile);
            _statfile = NULL;
        }
    }

    //
    // Arbitrator interface
    //

    // join the arbitrator mesh network with a given arbitrator ID 
    bool 
    ArbitratorImpl::join (const Position &pos)
    {
        // avoid redundent join for a given arbitrator
        if (_VONpeer != NULL)
            return false;

        id_t id = getUniqueID (ID_GROUP_VON_VASTATE);

        _VONpeer = new VONPeer (id, this);
        
        // note that we're combining the basic network info from the vastnode with
        // position from the VONpeer (the arbitrator's position as existed in a VON)
        _self.aoi.center = pos;
        _newpos = _self;

        // use a small default AOI length (not important, as we only need to know enclosing arbitrators)
        Area aoi (pos, 5);

        // we assume that a VAST node at the gateway has access to the gateway VON node, 
        // so that a joining VON node can reach a gateway VON node (via the gateway VAST node) 
        // TODO: we have certain assumption about ID generation for a VAST VON network
        //       cleaner way without assumption?
        id_t gateway_id = getUniqueID (ID_GROUP_VON_VASTATE, true); 
        Node VON_gateway (gateway_id, 0, aoi, getAddress (NET_ID_GATEWAY));
        _VONpeer->join (aoi, &VON_gateway);

        // notify gateway that I'll be joining as arbitrator 
        // (so can remove me from waiting list or potential list)
        Message msg (ARBITRATOR_J);
        msg.priority = 1;
        msg.store (_self.id);
        msg.addTarget (NET_ID_GATEWAY);
        sendMessage (msg);

        _state = JOINING;

        return true;
    }

    // leave the arbitrator overlay
    bool 
    ArbitratorImpl::leave ()
    {
        if (isJoined () == false)
            return false;

        // leave the arbitrator overlay
        _VONpeer->leave ();
        _VONpeer->tick ();

        delete _VONpeer;
        _VONpeer = NULL;

        _state = ABSENT;
      
        return true;
    }

    // sends the results of a join request to a joining Agent
    void    
    ArbitratorImpl::admit (id_t agent, char *status, size_t size)
    {
        Message msg (LOGIN);
        msg.priority = 1;
        msg.store (status, size, true);
        msg.addTarget (agent);

        sendAgent (msg);

        // record the admitted status
        // TODO: admitted is not used?
        _admitted[agent] = _self;

        // TODO:: should have timeout or send out a verifiable ticket
        
    }

    // send a message to a given agent
    bool 
    ArbitratorImpl::send (Message &out_msg)
    {
        // no target agent specified
        if (out_msg.targets.size () == 0)
            return false;

        Message msg (out_msg);
        msg.priority = 2;

        // modify msgtype
        msg.msgtype = (msg.msgtype << VAST_MSGTYPE_RESERVED) | GATEWAY_MSG;

        if (sendAgent (msg) > 0)
            return true;
        else
            return false;
    }

    // create or delete a new Object (can only delete if I'm the owner)
    
    Object *
    ArbitratorImpl::createObject (byte_t type, const Position &pos, obj_id_t *obj_id, id_t agent_id)
    {   
        // TODO: right now we limit the number of valid hosts to only 2^16 = 65536,
        //       as only 16 bits can be used for hostID
        
        // create internal ID if not available
        bool internal_id = false;
        
        if (obj_id == NULL)
        {
            obj_id = new obj_id_t ((_vastnode->getSelf ()->id << VASTATE_OBJID_PRIVATE_BITS) | _obj_id_count++);
            internal_id = true;
        }
        // check for existing object
        else if (getObject (*obj_id) != NULL)
            return NULL;
            
        Object *obj = new Object (*obj_id);
        obj->setPosition (pos);
        obj->type         = type;
        obj->agent        = agent_id;
        obj->version      = 1;
        obj->pos_version  = 1;

        // the dirty flags are unset, 
        // so only after attributes or positions are updated will
        // this object be sent to interested agents
        obj->pos_dirty    = false;
        obj->dirty        = false;
        
#ifdef DEBUG_DETAIL
        sprintf (_str, "[%d] creates obj [%s]\r\n", _vastnode->getSelf ()->id, obj->toString());
        _eo.output (_str);
#endif

        // require to initialize
        storeObject (obj, true);

        // store to newly created object list, to notify agents / neighbor arbitrators later in sendUpdates () 
        _new_objs[*obj_id] = true;
        
        // clear memory allocated
        if (internal_id)
            delete obj_id;

        return obj;
    }
  
    bool
    ArbitratorImpl::destroyObject (const obj_id_t &obj_id)
    {
        Object *obj;

        if ((obj = getObject (obj_id)) == NULL)
            return false;

        obj->pos_dirty = true;
        obj->markDeleted ();

        return true;
    }


    // create an update message, then send to the respective owner (could be my_self)
    bool
    ArbitratorImpl::updateObject (const obj_id_t &obj_id, int index, int type, void *value)
    {           
        Object *obj;
        if ((obj = getObject (obj_id)) == NULL)
            return false;
      
        // apply update into object
        switch (type)
        {
        case VASTATE_ATTRIBUTE_TYPE_BOOL:
            obj->set (index, *((bool *)value));
            break;
        case VASTATE_ATTRIBUTE_TYPE_INT:
            obj->set (index, *((int *)value));
            break;

        case VASTATE_ATTRIBUTE_TYPE_FLOAT:
            obj->set (index, *((float *)value));
            break;

        case VASTATE_ATTRIBUTE_TYPE_STRING:
            {
                string str = (char *)value;
                obj->set (index, str);
            }
            break;

        case VASTATE_ATTRIBUTE_TYPE_VEC3:
            obj->set (index, *((Position *)value));
            break;
        }
        
        // increase version number
        obj->version++;

        return true;
    }


    // create an update message, then send to the respective owner (could be my_self)
    // TODO: perform ownership check?
    bool
    ArbitratorImpl::moveObject (const obj_id_t &obj_id, const Position &newpos)
    {
        Object *obj = getObject (obj_id);

        // if object doesn't exist or is markDeleted, cant do more update
        if (obj == NULL || obj->isAlive () == false)
            return false;

        // update Object by new position
        obj->setPosition (newpos);

        // update Agent's position information 
        // so that interest filtering is performed correctly
        if (obj->agent != 0 && _agents.find (obj->agent) != _agents.end ())
        {
            _agents[obj->agent].aoi.center = obj->getPosition ();
        }

        // increase version number
        obj->pos_version++;

        return true;
    }


    // Arbitrator overloaded, call for help
    // note that this will be called continously until the situation improves
    void
    ArbitratorImpl::notifyLoading (int status)
    {        
        if (isJoined () == false)
            return;
                       
        switch (status)
        {
        // normal load
        case 0:           
            _load_counter = 0;
            return;
        // underload
        case -1:
            _load_counter--;
            break;
        default:
            _load_counter++;
        }

        // if overload persists, then we try to insert new arbitrator
        if (_load_counter > TIMEOUT_OVERLOAD_REQUEST)
        {    
            // reset
            if (_overload_requests < 0)
                _overload_requests = 0;

            _overload_requests++;

                        

            // if the overload situation just occurs, try to move boundary first
            if (_overload_requests < 5) 
            {
                // notify neighbors to move closer                            
                id_t self_id = _VONpeer->getSelf ()->id;
                listsize_t load = (listsize_t)status;
                        
                // send the level of loading to neighbors
                // NOTE: important to store the VONpeer ID as this is how the responding arbitrator will recongnize
                Message msg (OVERLOAD_M);        
                msg.from = self_id;         
                msg.priority = 1;
            
                msg.store (load);
                if (getEnclosingArbitrators (msg.targets) == true)
                    sendMessage (msg);
            }

            // if the overload situation persists, 
            // ask gateway to insert arbitrator at given location
            else
            {           
                Position pos = findArbitratorInsertion (_VONpeer->getVoronoi (), _VONpeer->getSelf ()->id);
            
                Message msg (OVERLOAD_I);
                msg.priority = 1;
                msg.store ((char *)&status, sizeof (int));
                msg.store (pos);
                msg.addTarget (NET_ID_GATEWAY);
                sendMessage (msg);    

                _overload_requests = 0;
            }      

            _load_counter = 0;
        }
        // underload event
        else if (_load_counter < (-TIMEOUT_OVERLOAD_REQUEST))
        {            
            // reset
            if (_overload_requests > 0)
                _overload_requests = 0;

            _overload_requests--;

            if (_overload_requests > (-5))
            {
                // TODO: notify neighbors to move further away?
            }

            // check for arbitrator departure
            else if (isGateway (_self.id) == false)
            {                
                // depart as arbitrator if loading is below threshold
                /*
                leave ();

                // notify gateway that I'm available again
                Message msg (ARBITRATOR_C);
                msg.priority = 1;
                msg.store (_self);
                msg.addTarget (NET_ID_GATEWAY);
                sendMessage (msg);
                */

                _overload_requests = 0;
            }
                    
            _load_counter = 0;
        }
        // normal loading, reset # of OVERLOAD_M requests
        else if (_load_counter == 0)
            _overload_requests = 0;
        
    }

    //
    //  MessageHandler methods
    //

    // this method is called by MessageQueue automatically  () in 'vnode'
    bool 
    ArbitratorImpl::handleMessage (Message &in_msg)
    {

        // extract the app-specific message type if exists
        msgtype_t app_msgtype = APP_MSGTYPE(in_msg.msgtype);
        in_msg.msgtype = VAST_MSGTYPE(in_msg.msgtype);

        if (app_msgtype != 0)
            printf ("[%ld] ArbitratorImpl::handleMessage (): appmsgtype specified\n", _self.id);

#ifdef DEBUG_DETAIL
        sprintf (_str, "[%ld] ArbitratorImpl::handleMessage from [%ld]: (%d)%s\r\n", _self.id, in_msg.from, in_msg.msgtype, (in_msg.msgtype > VON_MAX_MSG ? VASTATE_MESSAGE[in_msg.msgtype-VON_MAX_MSG] : "VAST msg"));
        _eo.output (_str);
#endif

        switch (in_msg.msgtype)
        {

        // initial join sent to gateway for user authentication
        case LOGIN:
            {   
                // only the gateway can authenticate
                if (isGateway (_self.id))
                {
                    size_t size = in_msg.extract (_buf, 0);


                    if (size > VASTATE_BUFSIZ)
                    {
                        printf ("ArbitratorImpl::handleMessage LOGIN. Authenication message exceeds VASTATE_BUFSIZ\n");                        
                    }
                    else
                    {
                        // call arbitrator logic to handle authentication
                        _logic->onLogin (in_msg.from, _buf, size);
                    }
                }
            }
            break;

        // leaving the system (should record/store the user stat)
        case LOGOUT:
            {
                // allow the application to handle directly
                _logic->onLogout (in_msg.from);
            }
            break;

        // an agent has shown up at a location in the virtual world
        // NOTE: this is a published message, re-routed from VAST node's receive ()
        case JOIN:

            // check if message size is correct
            //if (in_msg.size == sizeof (Node))
            {
                Node info;
                obj_id_t obj_id;

                in_msg.extract (info);         // extract node info
                in_msg.extract (obj_id);

                printf ("[%ld] ArbitratorImpl::handleMessage () JOIN event received from [%ld]\n", _vastnode->getSelf ()->id,  info.id);

                if (isWithinRegion (info.aoi.center))
                    addAgent (in_msg.from, info, (obj_id == NULL_OBJECT_ID ? NULL : &obj_id));
            }

            break;

        // store events from users into queue to be processed later
        case EVENT:
            {
                bool stored = false;
                Event *e = new Event ();
  
                // unpack data and store to queue to be processed
                if (in_msg.extract (*e) > 0)
                {
                    // find the avatar object for the agent that produces the event
                    map<id_t, Object *>::iterator it = _agent2obj.find (e->getSender ());                
                    Object *obj = (it != _agent2obj.end () ? it->second : NULL);

                    // check if the event belongs to me (if the sender is within my VON area)
                    if (obj != NULL && (isOwner (obj->getID (), true)))
                    {
                        //_unforward_event.push_back (pair<id_t, Event *> (in_msg.from, e));
                        _events.insert (multimap<timestamp_t, Event *>::value_type (e->getTimestamp (), e));
                        stored = true;
                        //_events[e->getTimestamp ()] = e;
                    }
                    else
                        printf ("[%ld] event from [%ld] doesn't belong to me\n", _vastnode->getSelf ()->id, e->getSender ());
                }
                
                if (stored == false)
                    delete e;               
            }
            break;

        case MOVEMENT:
            {
                Position    pos;                
                obj_id_t    obj_id;   
                
                in_msg.extract (pos);
                in_msg.extract (obj_id);                

                // if owner or in-transit object, process directly
                if (isOwner (obj_id, true))
                {
                    _logic->onMoveEvent (in_msg.from, obj_id, pos);
                }
                else
                {
                    printf ("[%ld] move event from [%ld] doesn't belong to me\n", _vastnode->getSelf ()->id, in_msg.from);                    
                }
            }                        
            break;                   
                                     
        // Notification/request of Object creation, destruction, update by other arbitrators
        case OBJECT_C:               
            {                        
                obj_id_t obj_id;
                Position pos;
                version_t pos_version;
                id_t     agent;
                byte_t   type;
                byte_t   attr_size;      // # of attributes

                in_msg.extract (obj_id);
                in_msg.extract (type);
                in_msg.extract (attr_size);
                in_msg.extract (pos);
                in_msg.extract (pos_version);
                in_msg.extract (agent);

                // clear the request record
                if (_obj_requested.find (obj_id) != _obj_requested.end ())
                    _obj_requested.erase (obj_id);

                // if object exists, ignore request 
                if (_obj_store.find (obj_id) != _obj_store.end ())
                {
                    // TODO: try to avoid redundent object creation message
                    //       a known case exists when an arbitrator receives object ownership
                    //       it needs to notify its enclosing arbitrators of the object's creation
                    //       however, some neighbors may already know the object
                    sprintf (_str, "[%ld] ArbitratorImp: Object creation notice from [%ld] denied, id [%ld] already exists\r\n", _vastnode->getSelf ()->id, in_msg.from, obj_id);
                    _eo.output (_str);                   
                }
                // create new object
                else
                {
                    Object *obj = new Object (obj_id);
                    obj->setPosition (pos);
                    obj->pos_version = pos_version;
                    obj->type  = type;
                    obj->agent = agent;
                
                    // record # of expected attributes for this object
                    // TODO: currently this is un-used (for agents this is used to check
                    //       whether all attributes have arrived and thus can be safe to notify onCreate ()) 
                    _attr_sizes[obj_id] = attr_size;
                
                    // notify for the discovery of a new object
                    storeObject (obj, false);
                }
                    
                // TODO: handle ownership transfer at the same time?                
                // NOTE that if this object was requested because I receive an ownership transfer
                //      then immediately assume ownership
                if (_transfer_msg.find (obj_id) != _transfer_msg.end ())
                {                    
                    Message *msg = _transfer_msg[obj_id];
                    processTransfer (*msg);
                    delete msg;
                    _transfer_msg.erase (obj_id);
                }

#ifdef DEBUG_DETAIL
                sprintf (_str, "[%d] arb learns of new Object [%s] from %d\r\n", _vastnode->getSelf ()->id, obj->toString(), in_msg.from);
                _eo.output (_str);
#endif
            }
            break;

        case OBJECT_D:
            {
                obj_id_t obj_id;
                
                in_msg.extract (obj_id);
                              
                Object *obj = (_obj_store.find (obj_id) != _obj_store.end () ?
                               _obj_store[obj_id].obj : NULL);
                   
                // if the object exists and belongs to someone else, mark for deletion
                // TODO: BUG: possible that the arbitrator sending the message
                //            is not the actual owner of the object, in this case
                //            the object would be destroyed incorrectly
                if (obj != NULL && isOwner (obj_id) == false)
                    obj->markDeleted ();
            }
            break;

        // request to an object owner to send back full states of its owned object
        case OBJECT_R:
            {
                // first parameter determines whether it's from an arbitrator or agent
                listsize_t msggroup;
                in_msg.extract (msggroup);

                listsize_t n;
                in_msg.extract (n);

                obj_id_t obj_id;
                map<obj_id_t, StoredObject>::iterator it;
                vector<id_t> targets;

                // send full object states to requester for each object requested
                for (unsigned char i=0; i < n; i++)
                {
                    in_msg.extract (obj_id);
                    if ((it = _obj_store.find (obj_id)) == _obj_store.end ())
                    {    
                        printf ("[%ld] ArbitratorImpl::handleMessage () OBJECT_R  requested object not found from [%ld]\n", _self.id, in_msg.from);
                        continue;
                    }

                    targets.clear ();
                    targets.push_back (in_msg.from);
                    
                    sendFullObject (it->second.obj, targets, msggroup);

                    // also prepare to transfer ownership if the request was from an arbitrator
                    //_transfer_countdown[obj_id] = 1;
                }
            }
            break;

        case POSITION:
            if (in_msg.size == sizeof (Msg_OBJECT))
            {                                         
                Msg_OBJECT info;
                in_msg.extract ((char *)&info, sizeof (Msg_OBJECT));
                
                StoredObject *so = getStoredObject (info.obj_id);
                
                //Object *obj = (_obj_store.find (info.obj_id) != _obj_store.end () ? _obj_store[info.obj_id].obj : NULL);

                // request for unknown objects
                if (so == NULL)    
                {
                    vector<obj_id_t> request_list;
                    request_list.push_back (info.obj_id);
                    
                    requestObject (in_msg.from, request_list);
                    break;
                }
 
                // update position
                // NOTE that onMove is not called
                in_msg.reset ();

                if (so->obj->decodePosition (in_msg))
                {
                    // record the last update time of Object (newobj's timestamp is assigned in storeObject)
                    so->last_update = _tick;

                    // indicate that I'm still being updated of position change
                    so->closest_arb = 0;
                }
            }
            break;

        // Notification/request of attribute creation and update by other arbitrators
        // NOTE: there's no size-check on the message as the length is variable
        case STATE:
            {                
                Msg_STATE info;
                in_msg.extract ((char *)&info, sizeof (Msg_STATE));
                
                //Object *obj = (_obj_store.find (info.obj_id) != _obj_store.end () ? _obj_store[info.obj_id].obj : NULL);
                StoredObject *so = getStoredObject (info.obj_id);

                // request the full object states if it is unknown, or we've missed some updates
                // TODO: right now we just request a re-send, 
                //       too expensive if just some states are missing
                if (so == NULL || so->obj->version < (info.version-1))
                {
                    vector<obj_id_t> request_list;
                    request_list.push_back (info.obj_id);

                    requestObject (in_msg.from, request_list);
                    break;
                }

                in_msg.reset ();
                if (so->obj->decodeStates (in_msg) == true)
                {
                    // record the last update time of Object
                    so->last_update = _tick;

                    // indicate that I'm still being updated of state change
                    so->closest_arb = 0;
                }

#ifdef DEBUG_DETAIL
                sprintf (_str, "[%d] STATE obj_id: %d version: %d \r\n", _vastnode->getSelf ()->id, info.obj_id, info.version);
                _eo.output (_str);
#endif              
            }
            break;

        case AGENT_MSG:
            // only Gateway can process it
            if (isGateway (_self.id))
            {
                // restore app-specific message type
                in_msg.msgtype = app_msgtype;

                // let the Gateway arbitrator logic to process it
                _logic->onMessage (in_msg);
            }
            break;

        // notification of objects owned by a neighbor arbitrator
        case OWNER:
            {
                listsize_t n;
                in_msg.extract (n);
            
                vector<obj_id_t> request_list;
                obj_id_t    obj_id;
                version_t   version;

                for (int i=0; i < (int)n; i++)
                {
                    in_msg.extract (obj_id);
                    in_msg.extract (version);

                    map<obj_id_t, StoredObject>::iterator it = _obj_store.find (obj_id);
                   
                    // if object doesn't exist or is outdated
                    if (it == _obj_store.end () || 
                        it->second.obj->version < version)
                    {
                        request_list.push_back (obj_id);
                    }                       
                }

                if (request_list.size () > 0)
                {
                    // send request for full object updates
                    requestObject (in_msg.from, request_list);
                }
            }
            break;

        // a candidate arbitrator gets promoted as an actual arbitrator
        case PROMOTE:        
            if (_state == ABSENT)
            {
                Node requester;
                in_msg.extract (requester);

                // join at the specified position, but only if I'm available
                join (requester.aoi.center);
            }
            break;    

        // Overloaded arbitrator's request for inserting new arbitrator
        case OVERLOAD_I:
            if (isGateway (_self.id))
            {
                int level;
                Position join_pos;

                in_msg.extract ((char *)&level, sizeof (int));
                in_msg.extract (join_pos);

                // TODO: ignore redundent requests at same position
                
                // promote one of the spare potential arbitrators
                Node new_arb;

                while (findArbitrator (level, new_arb))
                {
                    // fill in the stressed arbitrators contact info & join location
                    id_t arb = in_msg.from;
                    Area a (join_pos, 0); 
                    Node requester (arb, getTimestamp (), a, getAddress (in_msg.from));

                    Message msg (PROMOTE);
                    msg.priority = 1;
                    msg.store (requester);

                    // record the promoted position
                    _promotion_requests[new_arb.id] = requester;
                    //_promoted_nodes.push_back (requester);

                    // send promotion message
                    notifyMapping (new_arb.id, &new_arb.addr);
                    msg.addTarget (new_arb.id);

                    if (sendMessage (msg) > 0)
                        break;
                    // we cannot send promotion message, erase record
                    else
                        _promotion_requests.erase (new_arb.id);
                }

                // TODO: if findArbitrator () fails, insert virtual 
            }
            else
            {
                sprintf (_str, "[%ld] ArbitratorImpl::handleMessage () OVERLOAD_I received by non-gateway\r\n", _vastnode->getSelf ()->id);
                _eo.output (_str);
            }
            break;

        // Overloaded arbitrator's request for moving closer
        case OVERLOAD_M:            
            {
                // received not my enclosing neighbor's help signal
                if (_arbitrators.find (in_msg.from) == _arbitrators.end ())
                {
                    sprintf (_str, "[%ld] receives OVERLOAD_M from non-enclosing arbitrator [%ld]\r\n", _vastnode->getSelf ()->id, in_msg.from);
                    _eo.output (_str);
                    break;
                }

                // extract loading
                listsize_t load;
                in_msg.extract (load);
                float multiplier = (_para.overload_limit > 0 ? ((float)load / _para.overload_limit) : load);

                // calculate new position after moving closer to the stressed arbitrator
                Position &arb = _arbitrators[in_msg.from].aoi.center;
                Position temp_pos = _newpos.aoi.center;

                // move in 1/10 of the distance between my_self and the stressed arbitrator
                //temp_pos += ((arb - temp_pos) * ARBITRATOR_MOVEMENT_FRACTION);

                // move at a speed proportional to the severity of the overload
                temp_pos += (((arb - temp_pos) * ARBITRATOR_MOVEMENT_FRACTION) * multiplier);

                //temp_pos.x = (coord_t)(_newpos.aoi.center.x + (arb.x - _newpos.aoi.center.x) * 0.10);
                //temp_pos.y = (coord_t)(_newpos.aoi.center.y + (arb.y - _newpos.aoi.center.y) * 0.10);

                if (isLegalPosition (temp_pos, false))
                    _newpos.aoi.center = temp_pos;

            }          
            break;

        // record loading from current arbitrators
        case LOADING:
            {
                listsize_t load;
                in_msg.extract (load);

                // update loading info
                _arb_loading[in_msg.from] = load;
            }
            break;
                

        /*

        // Notification of an Arbitrator goes to off-line
        case ARBITRATOR_LEAVE:
            if (size == sizeof (id_t))
            {
                // get parent id
                id_t his_parent;
                memcpy ((void *) &his_parent, msg, sizeof (id_t));

                // check if need to decrease promotion count
                if (isGateway (_self.id) && his_parent != NET_ID_GATEWAY)
                {
                    // find position of the record
                    for (int index = 0; index < (int) _potentials.size (); index ++)
                    {
                        if (_potentials[index].node.id == his_parent)
                        {
                            _promotion_count[index] --;
                            break;
                        }
                    }
                }

                // check if any Object I should be owner after the Arbitrator leaves
                /
                if (_arbitrators.find (from_id) != _arbitrators.end ())
                {
                    for (map<obj_id_t, Object *>::iterator it = _obj_store.begin (); it != _obj_store.end (); it ++)
                    {
                        Object * obj = it->second;
                        if (_vastnode->getVoronoi ()->contains (from_id, obj->getPosition ()))
                        {
                            Msg_TRANSFER mtr;
                            mtr.obj_id = obj->getID ();
                            mtr.new_owner = _vastnode->getSelf ()->id;
                            mtr.orig_owner = NET_ID_UNASSIGNED;

                            _net->sendmsg (_vastnode->getSelf ()->id, TRANSFER, (char *) & mtr, sizeof (Msg_TRANSFER));
                        }
                    }
                }
                
            }
            break;
        */

        // Ownership transfer from old to new owner
        case TRANSFER:
            {
                //if (isJoined ())
                // no need to check join, could save it first, process later
                    processTransfer (in_msg);
            }
            break;
        
        // acknowledgment of ownership transfer received
        case TRANSFER_ACK:            
            {
                obj_id_t obj_id;
                in_msg.extract (obj_id);                
                
                // check if object exists
                if (_obj_store.find (obj_id) == _obj_store.end ())
                {
                    printf ("[%ld] ArbitratorImpl: TRANSFER_ACK received for objects I do not have\n", _self.id);
                    break;
                }

                // update in_transit stat                
                _obj_store[obj_id].in_transit = 0;

                // if it's avatar object transfer, remove agent info
                id_t agent = _obj_store[obj_id].obj->agent;
                if (agent != 0 && _agents.find (agent) != _agents.end ())
                {
                    removeAgent (_agents[agent].addr.host_id, false);
                    
                    // remove agent's knowledge of known objects
                    if (_known_objs.find (agent) != _known_objs.end ())
                        _known_objs.erase (agent);
                }
#ifdef SEND_OBJECT_ON_TRANSFER                
                // remove the transferred object altogether (to avoid notifying agent of ghost objects)
                //unstoreObject (obj_id);
#endif
            }
            break;

        // registration of a potential arbitrator with the gateway
        case ARBITRATOR_C:
            {
                Node arb;

                in_msg.extract (arb);

                // store potential arbitrator info
                _potentials.push_back (arb);
            }
            break;

        case ARBITRATOR_J:
            if (isGateway (_self.id))
            {
                id_t id; 
                in_msg.extract (id);

                // successful promotion, erase record
                if (_promotion_requests.find (id) != _promotion_requests.end ())
                {                    
                    _promotion_requests.erase (id);                    
                }
                
                // we also remove the node from list of potential arbitrator
                for (size_t i=0; i < _potentials.size (); i++)
                {
                    if (_potentials[i].id == id)
                    {
                        // TODO: there may be other critera for arbitrator promotion
                        unsigned long host = _potentials[i].addr.publicIP.host;
                        if (_promoted_hosts.find (host) != _promoted_hosts.end ())
                            _promoted_hosts.erase (host);

                        _potentials.erase (_potentials.begin () + i);
                        break;
                    }
                }

                // TODO: periodic check of unfulfilled requests and find other
                //       candidate to promote
            }
            break;

        // Auto ownership assumption if arbitrators fail.
        case NEWOWNER:
            break;

        case SWITCH:
            {
                obj_id_t obj_id;
                id_t     closest_arb;

                in_msg.extract (obj_id);
                in_msg.extract (closest_arb);

                if (isOwner (obj_id, true) == false)
                {
                    StoredObject *so = getStoredObject (obj_id);
                    if (so != NULL)
                        so->closest_arb = closest_arb;

                    //destroyObject (obj_id);
                }
            }
            break;

        // statistics report from arbitrators
        case STAT:
            if (isGateway (_self.id) && _statfile != NULL)
            {
                size_t send, recv;
                in_msg.extract ((char *)&send, sizeof (size_t));
                in_msg.extract ((char *)&recv, sizeof (size_t));

                StatType stat;
                in_msg.extract ((char *)&stat, sizeof (StatType));
                    
                fprintf (_statfile, "[%ld] send: %10u, recv: %10u, agent min: %2u, max: %2u, avg: %f\n", 
                    in_msg.from, send, recv, stat.minimum, stat.maximum, stat.average);
                fflush (_statfile);
            }
            break;

        case LEAVE:
        case DISCONNECT:
            {
                // if the departing node is an arbitrator, make sure _VONpeer knows it
                if (EXTRACT_LOCAL_ID (in_msg.from) != 0)
                {
                    // NOTE that before arbitrator has joined, DISCONNECT message may be received 
                    // from connecting agents requesting unique host ID
                    // so we need to check the validity of VONpeer
                    if (_VONpeer != NULL)
                    {
                        in_msg.msgtype = VON_DISCONNECT;
                        _VONpeer->handleMessage (in_msg);
                    }

                    // update my own arbitrator list? 
                    // should be done by updateArbitrators (), 
                    // TODO: do it here is more efficient / correct?
                }

                // remove any disconnected Agents (opposite of JOIN)                
                else if (removeAgent (in_msg.from) == true)
                {
                    // allow the application to handle agent departure
                    // NOTE that disconnect may occur before an agent is known, so 
                    //      onLogout is called only for valid, existing agents
                    _logic->onLogout (in_msg.from);
                }             
            }
            break;
         
        default:
            {
                // assume the message is a VON message
                if (EXTRACT_LOCAL_ID (in_msg.from) != 0)
                {
                    if (_VONpeer != NULL)
                        return _VONpeer->handleMessage (in_msg);
                    else
                        return false;
                }
                else
                {
                    sprintf (_str, "[%ld] ArbitratorImpl::handleMessage () unhandled message\r\n", _self.id);
                    _eo.output (_str);
                }
            }
            // otherwise the message is unhandled
            return false;
        }

        return true;
    }

    // process messages (send new Object states to neighbors)
    void
    ArbitratorImpl::postHandling ()
    {        
        // # of times Arbitrator has been ticked
        _tick++;

        // if we have not joined the VON or VAST network, then do nothing
        if (_VONpeer == NULL)
            return;
    
        _VONpeer->tick ();

        if (_VONpeer->isJoined () == false || _vastnode->isJoined () == false)
            return;

#ifdef DEBUG_DETAIL
        printf ("ArbitratorImpl::postHandling (): tick %u\n", _tick);
#endif

        // make sure this arbitrator has properly joined a VON and perform related functions
        checkVONJoin ();

        // perform per-second tasks
        if (_tick % _net->getTickPerSecond () == 0)
        {
#ifdef REPORT_LOADING
            // report loading to neighbors
            reportLoading ();
#endif

            // move myself towards the center of agents
            Position center;
            if (getAgentCenter (center))
                _newpos.aoi.center += ((center - _newpos.aoi.center) * ARBITRATOR_MOVEMENT_FRACTION);
        }

        // move arbitrator to new position
        moveArbitrator ();

        // update the list of arbitrators
        updateArbitrators ();

        // remove any invalid avatar objects 
        // (NOTE: non-AOI objects are removed via notification)
        validateObjects ();

        // process events in sequence
        processEvent ();

        // check to call additional additional arbitrators for load balancing
        checkOverload ();

        // perform some Arbitrator logic (not provoked by events)
        _logic->tick ();

#ifdef DISCOVERY_BY_NOTIFICATION
        // revise list of interested nodes and send them full updates
        // TODO: redundent OBJECT notification could be sent 
        //       first by updateAOI (), then publish by sendUpdates ()
        //       currently filtered by receiver, but better ways?
        updateAOI ();
#endif

        // send all changed states to interested nodes (partial ones, full update is done in updateAOI)
        sendUpdates ();

        // TODO: send regular full update for consistency fixing
        //sendFullUpdates ();

        // check to see if objects have migrated (TODO: should do it earlier?)
        transferOwnership ();

        // collect stat periodically
        // TODO: better way than to pass _para?
        if (_tick % (_net->getTickPerSecond () * STAT_REPORT_INTERVAL_IN_SEC) == 0)
            reportStat ();
    }

    //
    // VONNetwork
    //

    // send messages to some target nodes
    // returns number of bytes sent
    size_t 
    ArbitratorImpl::sendVONMessage (Message &msg, bool is_reliable, vector<id_t> *failed_targets)
    {
        // TODO: make return value consistent
        msg.reliable = is_reliable;
        return (size_t)sendMessage (msg, failed_targets);
    }

    // obtain next message in queue
    // return pointer to Message, or NULL for no more message
    Message* 
    ArbitratorImpl::receiveVONMessage (timestamp_t &senttime)
    {
        return NULL;
    }

    // notify the network layer of nodeID -> Address mapping    
    bool 
    ArbitratorImpl::notifyAddressMapping (id_t node_id, Addr &addr)
    {
        return notifyMapping (node_id, &addr);
    }
    
    // get the IP address of current host machine
    Addr &
    ArbitratorImpl::getHostAddress ()
    {
        return _net->getHostAddress ();
    }

    // get current physical timestamp
    inline timestamp_t 
    ArbitratorImpl::getTimestamp ()
    {
        return (timestamp_t)((int) _net->getTimestamp ());
    }

    bool 
    ArbitratorImpl::isWithinRegion (obj_id_t &obj_id)
    {
        map<obj_id_t, StoredObject>::iterator it = _obj_store.find (obj_id);
        if (it == _obj_store.end ())
            return false;

        return isWithinRegion (it->second.obj);
    }

    // whether a sending agent is in my region
    bool 
    ArbitratorImpl::isWithinRegion (const Object *obj)
    {
        return isWithinRegion (obj->getPosition ());
    }

    bool 
    ArbitratorImpl::isWithinRegion (const Position &pos)
    {
        //return isOwner (obj->getID ());
        
        if (_VONpeer->getVoronoi ()->contains (_VONpeer->getSelf ()->id, pos))
            return true;

        return false;
    }

    // obtain the position to insert a new arbitrator
    Position
    ArbitratorImpl::findArbitratorInsertion (Voronoi *voronoi, id_t self_id)
    {
        _legal_pos.clear ();

        Position agent_center;      // center of agents        
        
        // get center of all known agents
        getAgentCenter (agent_center);
        
        // find center of arbitrators
        Position arb_center;        
        
        // final insert position
        Position insert_pos;        

        if (getArbitratorCenter (arb_center))
        {
            // final center        
            insert_pos.x = (coord_t)((arb_center.x + agent_center.x) / 2.0);
            insert_pos.y = (coord_t)((arb_center.y + agent_center.y) / 2.0);
        }
        else
            insert_pos = agent_center;
      
        if (isLegalPosition (insert_pos))
            _legal_pos.push_back (insert_pos);
        else if (isLegalPosition (arb_center))
            _legal_pos.push_back (arb_center);        
        else if (isLegalPosition (agent_center))
            _legal_pos.push_back (agent_center);
                
        // agent center only
        //_legal_pos.push_back (agent_center);

        /* arbitrator center only
        if (isLegalPosition (arb_center))
            _legal_pos.push_back (arb_center);
        else if (isLegalPosition (agent_center))
            _legal_pos.push_back (agent_center);
        */

        /*
        // try to do a inserting new Arbitrator first
        // find a proper insertion position for new arbitrator
        vector<line2d> &lines = voronoi->getedges ();
        set<int> &edges = voronoi->get_site_edges (self_id);
    
        // try Voronoi intersections    
        for (set<int>::iterator it = edges.begin (); it != edges.end (); it ++)
        {
            int i = *it;

            // find & record all legal positions
            // NOTE may contain duplications
            Position pos ((coord_t)lines[i].seg.p1.x, (coord_t)lines[i].seg.p1.y);
            
            if (pos != Position (0,0) && isLegalPosition (pos))
                _legal_pos.push_back (pos);
    
            pos.x = (coord_t)lines[i].seg.p2.x;
            pos.y = (coord_t)lines[i].seg.p2.y;

            if (pos != Position (0,0) && isLegalPosition (pos))
                _legal_pos.push_back (pos);
        }
        */
        
        /*
        // TODO: find inserting position from the edge of the world
        double values[] = {0, 0, sysparm->width - 1, sysparm->height - 1};
        for (int i = 0; i < 4; i ++)
        {
            Position pos (_self->aoi.center);
            
            if (i % 2 == 0)
                pos.x = values[i];
            else
                pos.y = values[i];

            if (voronoi->contains (_self->id, pos) && 
                isLegalPosition (pos))
                legal_pos.push_back (pos);
        }
        */

        // generate a legal random position if no positions are available
        if (_legal_pos.size () == 0)
        {
            // create a deterministic random seed
            //srand (_VONpeer->getSelf ()->id * _self.id);

            Position pos;
            do
            {
                pos.set ((coord_t)(rand () % _para.world_width), (coord_t)(rand () % _para.world_height), 0);
            } 
            while (!isLegalPosition (pos));

            _legal_pos.push_back (pos);
        }

        int r = _self.id % _legal_pos.size ();
        return _legal_pos[r];

        // otherwise we return a default world center position        
        //return Position ((coord_t)(_para.world_width/2), (coord_t)(_para.world_height/2));
    }

    bool 
    ArbitratorImpl::isLegalPosition (const Position &pos, bool include_self)
    {
        // check for redundency
        for (size_t i=0; i < _legal_pos.size (); i++)
            if (_legal_pos[i] == pos)
                return false;    

        // check if position is out of map
        if (pos.x < 0.0 || pos.y < 0.0 || pos.x >= _para.world_width || pos.y >= _para.world_height)
            return false;

        // loop through all enclosing arbitrators
        for (map<id_t,Node>::iterator it = _arbitrators.begin (); it != _arbitrators.end (); it++)
        {
            if (!include_self && it->first == _VONpeer->getSelf ()->id)
                continue;

            // check if the position to check is somewhat disant to all known arbitrators
            //if (pos.distance (it->second.aoi.center) < (_para.default_aoi / 2))
            if (pos.distance (it->second.aoi.center) < 5)
                return false;
        }
        return true;
    }

    // get a list of my enclosing arbitrators
    bool 
    ArbitratorImpl::getEnclosingArbitrators (vector<id_t> &list)
    {
        vector<Node *> &nodes   = _VONpeer->getNeighbors ();
        Voronoi *voronoi        = _VONpeer->getVoronoi ();

        if (nodes.size () <= 1 || voronoi == NULL)
            return false;

        id_t self_id = _VONpeer->getSelf ()->id;

        list.clear ();

        for (size_t i=0; i < nodes.size (); i++)
        {
            id_t id = nodes[i]->id;

            // skip self or non-enclosing neighbors
            if (id == self_id || voronoi->is_enclosing (id) == false)
                continue;

            list.push_back (id);
        }

        return true;
    }

    // get the center of all current agents I maintain
    bool
    ArbitratorImpl::getAgentCenter (Position &agent_center)
    {
        if (_agents.size () == 0)
            return false;

        agent_center.set (0, 0, 0);

        // NOTE/BUG if all coordinates are large, watch for overflow
        map<id_t, Node>::iterator it = _agents.begin ();
        
        for (; it != _agents.end (); it++)
        {
            agent_center.x += it->second.aoi.center.x;
            agent_center.y += it->second.aoi.center.y;
        }

        agent_center.x /= _agents.size ();
        agent_center.y /= _agents.size ();

        return true;
    }

    // get the center of all neighbor arbitrators
    bool 
    ArbitratorImpl::getArbitratorCenter (Position &arb_center)
    {
        arb_center.set (0, 0, 0);
        Voronoi *voronoi = _VONpeer->getVoronoi ();                
        vector<id_t> en_list = voronoi->get_en (_VONpeer->getSelf ()->id);

        if (en_list.size () > 2)
        {
            for (size_t i=0; i < en_list.size (); i++)
            {
                Position pos = voronoi->get (en_list[i]);
                arb_center.x += pos.x;
                arb_center.y += pos.y;
            }
            arb_center.x /= en_list.size ();
            arb_center.y /= en_list.size ();

            return true;
        }
        // no neighbors exist to calculate center
        else
            return false;        
    }



    // insert a new agent and create its avatar object
    bool 
    ArbitratorImpl::addAgent (id_t from, Node &info, obj_id_t *obj_id)
    {
        // TODO: should check if authentication ticket is correct
        //       otherwise every node can send JOIN message and have avatar objects created
                   
        // create avatar object for the joining node        
        if (createObject (0, info.aoi.center, obj_id, info.id) == NULL)
        {
#ifdef FAULT_TOLERANT_1
            // TODO: perhaps not necessary to backup in lean+request mode? (as 
            //       a neighbor arbitrator that receives updates for unknown object would simply request the object

            // the agent's avatar object already exists (ownership transfer from another arbitrator)
            // send object creation notice to neighboring arbitrators
            // this ensures those neighbors will have a copy
            Object *obj = _obj_store[*obj_id].obj;

            Message obj_msg (OBJECT_C);
            obj_msg.store (*obj_id);
            obj_msg.store (obj->type);          // store object type
            obj_msg.store (obj->size ());       // store # of attributes with the object
            obj_msg.store (info.aoi.center);    // the object's location
            obj_msg.store (obj->pos_version);
            obj_msg.store (info.id);            // agent ID
        
            // also send creation notice to neighboring arbitrators
            if (getEnclosingArbitrators (obj_msg.targets) != false)
                sendMessage (obj_msg);       
#endif
        }

        _agents[info.id] = info;

        // TODO: see if this is redundent (hostID may be the same as agentID)
        _host2agent[from] = info.id;

        // notify arbitrator logic of a new agent entering this region
        _logic->onAgentEntering (info);

        // notify mapping, as the new arbitrator may not know the agent's host
        notifyMapping (from, &info.addr);

        // notify the agent that I'm its arbitrator
        // note that it's possible the agent is failed by now
        if (notifyArbitratorship (from))
        {
            // send back full object states of AOI objects for this node
            updateAOI (info.id);
        }

        return true;
    }

    // remove an existing agent
    bool 
    ArbitratorImpl::removeAgent (id_t from, bool remove_obj)
    {
        if (_host2agent.find (from) == _host2agent.end ())
            return false;

        id_t agent = _host2agent[from];

        if (_agent2obj.find (agent) == _agent2obj.end ())
            return false;

        // notify logic of agent departure from this region
        if (_agents.find (agent) != _agents.end ())
            _logic->onAgentLeaving (_agents[agent]);

        if (remove_obj)
        {
            Object *obj = _agent2obj[agent];
            destroyObject (obj->getID ());
        }

        _agents.erase (agent);
        _host2agent.erase (from);

        // remove agent's known object list, if exists
        if (_known_objs.find (agent) != _known_objs.end ())
            _known_objs.erase (agent);

        return true;
    }

    //
    // private helper methods
    //

    void
    ArbitratorImpl::storeObject (Object *obj, bool isOwner)
    {        
        // store to repository
        StoredObject &so    = _obj_store[obj->getID ()];
        so.obj              = obj;
        so.last_update      = _tick;
        so.is_owner         = isOwner;
        so.in_transit       = 0;

        // store agent ID to avatar object mapping
        if (obj->agent != 0)
            _agent2obj [obj->agent] = obj;
                
        // TODO/BUG: Object may not have any attribute(s) at this time
        // notify arbitratorLogic on object creation
        // NOTE, if this is a locally created object, attributes may be filled 
        //       after calling onCreate (), 
        //       then onCreate should not put attributes inside        
        _logic->onCreate (obj, isOwner);

        // avoid onMove () being called after onCreate ()
        obj->pos_dirty = false;
    }

    void 
    ArbitratorImpl::unstoreObject (obj_id_t obj_id)
    {
        if (_obj_store.find (obj_id) == _obj_store.end ())
            return;

        StoredObject &so = _obj_store[obj_id];
        id_t agent_id    = so.obj->agent;

        // notify Object deletion
        _logic->onDestroy (so.obj->getID ());

        // remove from repository
        delete so.obj;
        _obj_store.erase (obj_id);        

        if (agent_id != 0)
            _agent2obj.erase (agent_id);
    }

    Object *
    ArbitratorImpl::getObject (const obj_id_t &obj_id)
    {
        map<obj_id_t, StoredObject>::iterator it;
        if ((it = _obj_store.find (obj_id)) == _obj_store.end ())
            return NULL;

        return it->second.obj;
    }

    StoredObject *
    ArbitratorImpl::getStoredObject (const obj_id_t &obj_id)
    {
        map<obj_id_t, StoredObject>::iterator it;
        if ((it = _obj_store.find (obj_id)) == _obj_store.end ())
            return NULL;

        return &it->second;
    }

    // see if any of my objects should transfer ownership
    // or if i should claim ownership to any new objects (if neighboring arbitrators fail)
    int 
    ArbitratorImpl::transferOwnership ()
    {        
        if (_VONpeer == NULL || _state == ABSENT)
            return 0;

        // get a reference to the Voronoi object and my VON id
        Voronoi *voronoi = _VONpeer->getVoronoi ();
        id_t VON_selfid  = _VONpeer->getSelf ()->id;

        int num_transfer = 0;

        //
        // transfer ownership of currently owned objects to neighbor arbitrators
        //
        for (map<obj_id_t, StoredObject>::iterator it = _obj_store.begin (); it != _obj_store.end (); ++it)
        {
            Object *obj = it->second.obj;
            obj_id_t obj_id = obj->getID ();

            // if the Object is no longer within my region, then transfer to new owner
            Position pos = obj->getPosition ();
            if (voronoi->contains (VON_selfid, pos) == false)
            {
                if (it->second.is_owner == true)
                {
                    // update ownership transfer counter
                    if (_transfer_countdown.find (obj_id) == _transfer_countdown.end ())
                        _transfer_countdown[obj_id] = COUNTDOWN_TRANSFER;
                    else
                        _transfer_countdown[obj_id]--;

                    // if counter's up, then begin transfer to nearest arbitrator
                    if (_transfer_countdown[obj_id] == 0)
                    {
                        id_t closest = voronoi->closest_to (obj->getPosition ());
                        
                        vector<id_t> en_arb;
                        if (closest != VON_selfid)
                        {
#ifdef SEND_OBJECT_ON_TRANSFER
                            // send full states to the neighbor arbitrator first
                            vector<id_t> targets;
                            targets.push_back (closest);                            
                            sendFullObject (obj, targets, MSG_GROUP_VASTATE_ARBITRATOR);
#endif
                            Message msg (TRANSFER);
                            msg.priority = 1;
                        
                            msg.store (obj_id);
                            msg.store (closest);
                            msg.store (VON_selfid);

                            
                            /* TODO: notify other arbitrators to remove ghost objects?
                            // remove closest & send to all other neighbors
                            // causes crash? */
                            /*
                            if (getEnclosingArbitrators (en_arb))
                            {
                                // remove the closest from list
                                for (size_t i=0; i < en_arb.size (); i++)
                                {
                                    if (en_arb[i] == closest) 
                                    {
                                        en_arb.erase (en_arb.begin () + i);
                                        break;
                                    }
                                }
                                // send to non-closest enclosing arbitrators
                                if (en_arb.size () > 0)
                                {
                                    msg.targets = en_arb;
                                    sendMessage (msg);
                                    msg.targets.clear ();
                                }
                            }
                            */
                            
                            // transfer agent info 
                            // only transfer to closest
                            if (obj->agent != 0 && _agents.find (obj->agent) != _agents.end ())
                            {
                                //if (obj->agent == 20)
                                //    printf ("notice\n");

                                msg.store (_agents[obj->agent]);

#ifdef DISCOVERY_BY_NOTIFICATION
                                // also transfer agent's obj knowledge for avatar object
                                if (_known_objs.find (obj->agent) != _known_objs.end ())
                                {
                                    map<obj_id_t, version_t> &known_obj = _known_objs[obj->agent];
                                    listsize_t n = (listsize_t)known_obj.size ();
                                
                                    msg.store (n);
                                
                                    map<obj_id_t, version_t>::iterator it = known_obj.begin ();
                                    for (; it != known_obj.end (); it++)
                                    {
                                        msg.store (it->first);
                                        msg.store (it->second);
                                    }
                                }
#endif
                            }

                            msg.addTarget (closest);                            
                            sendMessage (msg);
                                                          
                            // we release ownership for now (but still be able to process events via transit records)
                            // this is important as we can only transfer ownership to one neighbor at a time
                            it->second.is_owner = false;
                            //_obj_transit [obj_id] = _tick;
                            it->second.in_transit = _tick;
                            num_transfer++;                                              

                            _transfer_countdown.erase (obj_id);
                        }                        
                    }
                }

                // if object is no longer in my region and I'm still not owner, remove reclaim counter
                else if (_reclaim_countdown.find (obj_id) != _reclaim_countdown.end ())
                    _reclaim_countdown.erase (obj_id);
            }
                        
            // there's an Object in my region but I'm not owner, 
            // should claim it after a while
            else 
            {                
                if (it->second.is_owner == false)
                {
                    // initiate a countdown, and if the countdown is reached, 
                    // automatically claim ownership (we assume neighboring arbitrators will
                    // have consensus over ownership eventually, as topology becomes consistent)
                    if (_reclaim_countdown.find (obj_id) == _reclaim_countdown.end ())
                        _reclaim_countdown[obj_id] = COUNTDOWN_TRANSFER * 4;
                    else
                        _reclaim_countdown[obj_id]--;
                
                    // we claim ownership if countdown is reached
                    if (_reclaim_countdown[obj_id] == 0)
                    {                         
                        // reclaim only if we're the closest arbitrator
                        if (it->second.closest_arb == 0)
                        {
                            it->second.is_owner = true;
                            it->second.in_transit = 0;
                            it->second.last_update = _tick;                                            
                        }
                        // it's not really mine, delete it
                        else
                        {
                            //destroyObject (obj_id);
                        }

                        _reclaim_countdown.erase (obj_id);
                        
                        // NOTE: we do not notify the agent, as the agent
                        //       should detect current arbitrator leave by itself,
                        //       and reinitiate sending the JOIN event to 
                        //       its new current arbitrator
                        /*
                        // notify avatar object's agent's of its new arbitrator
                        if (obj->agent != 0)
                        {                                                        
                            Message msg (REJOIN);
                            msg.priority = 1;
                            msg.addTarget (obj->agent);
                            msg.msggroup = MSG_GROUP_VASTATE_AGENT;                        
                            sendAgent (msg);                        
                        } 
                        */
                    }                
                }

                // reset transfer counter, if an object has moved out but moved back again
                else if (_transfer_countdown.find (obj_id) != _transfer_countdown.end ())
                {
                    _transfer_countdown.erase (obj_id);
                }
            }     

            // reclaim in transit objects

            if (it->second.in_transit != 0 && 
                ((_tick - it->second.in_transit) > (COUNTDOWN_TRANSFER * 2)))
            {
                it->second.in_transit = 0;
                it->second.is_owner = true;
            }
        }

        // TODO: need to make sure the above countdowns do not stay alive
        //       and errously affect future countdown detection

        /*
        // decrement all ownership claiming countdown
        for (map<obj_id_t, int>::iterator it = _transfer_countdown.begin (); it != _transfer_countdown.end (); it++)
        {
            // removing unreached countdown
            if (it->second == 0)
            {
                map<obj_id_t, int>::iterator it2 = it;
                it = it--;
                _transfer_countdown.erase (it2);
            }
            // decrement countdown for others         
            else
                it->second--;
        }
        */

        // check & reclaim ownership of in-tranit objects
        /*
        map<obj_id_t, timestamp_t>::iterator it = _obj_transit.begin ();
        for (; it != _obj_transit.end (); it++)
        {
            // if some threshold has exceeded
            if (_tick - it->second > (COUNTDOWN_TRANSFER * 2))
            {
                // reclaim ownership
                if (_obj_store.find (it->first) != _obj_store.end ())
                {
                    _obj_store[it->first].is_owner

                }
                // erase record
                _obj_transit.erase (it);
                break;
            }
        }
        */

        return num_transfer;
    }

    // change position of this arbitrator in response to overload signals
    void 
    ArbitratorImpl::moveArbitrator ()
    {
        // if nothing has changed
        if (_newpos.aoi.center == _self.aoi.center)
            return;

        // performe actual movement
        _VONpeer->move (_newpos.aoi);

        // update self info
        // NOTE: currently only position will change, not AOI
        _self.aoi.center = _newpos.aoi.center;

        // update other info (such as AOI) for the new position
        _newpos = _self;
    }
    
    // check with VON to refresh current connected arbitrators
    void 
    ArbitratorImpl::updateArbitrators ()
    {
        vector<id_t> en_arb;
        if (getEnclosingArbitrators (en_arb) == false)
            return;

        map<id_t, Node *> node_map;
        vector<id_t> targets;

        // loop through new list of arbitrators and update my current list
        size_t i;
        bool has_changed = false;

        for (i=0; i < en_arb.size (); ++i)
        {
            id_t id = en_arb[i];
            Node *node = _VONpeer->getNeighbor (id);

            // check if a new Arbitrator is found
            if (_arbitrators.find (id) == _arbitrators.end ())
            {
                // add a new arbitrator
                _arbitrators[id] = *node;

                //if (_event_queue.find (id) == _event_queue.end ())
                //    _event_queue[id] = map<timestamp_t, vector<Event *> > ();

                // arbitrator set has changed
                has_changed = true;
                                
                // send owned objects to the new arbitrator
                //sendObjects (id, true);
                
                // notify new arbitrator of objects that I own
                targets.clear ();
                targets.push_back (id);
                notifyOwnership (targets);

                /*
                // send all events in Event queue (in event_queue[my_id])
                // loop through all time
                for (map<timestamp_t, vector<Event *> >::iterator it = _event_queue[_self->id].begin (); it != _event_queue[_self->id].end (); it ++)
                {
                    // loop through all Event at the same time
                    vector<Event *> & ev = it->second;
                    for (unsigned int ei = 0; ei < ev.size (); ei ++)
                    {
                        // send the Event out
                        Event *e = ev[ei];
                        int size = e->encode (_buf);
                        if (size > 0)
                            _net->sendmsg (id, EVENT, _buf, size);
                    }
                }
                */
                
#ifdef DEBUG_DETAIL
                sprintf (_str, "[%d] learns about new enclosing Arbitrator [%d]\r\n", _vastnode->getSelf ()->id, id);
                _eo.output (_str);
#endif
            }
            else 
            {
                // update the arbitrator's info
                _arbitrators[id].aoi.radius = node->aoi.radius;

                if (_arbitrators[id].aoi.center != node->aoi.center)
                {
                    _arbitrators[id].aoi.center = node->aoi.center;
                    has_changed = true;
                }
            }

            // create lookup map to find arbitrators that no longer are enclosing
            node_map[id] = node;
        }
        
        // loop through current list of arbitrators to remove those no longer connected
        vector<id_t> remove_list;
        for (map<id_t, Node>::iterator it = _arbitrators.begin (); it != _arbitrators.end (); ++it)
        {
            if (node_map.find (it->first) == node_map.end ())
            {
                remove_list.push_back (it->first);
                has_changed = true;
            }
        }

        if (remove_list.size () > 0)
        {
            // remove arbitrators that are no longer enclosing
            for (i=0; i < remove_list.size (); ++i)
            {
                // do it before _arbitrators list updates
                //sendObjects (remove_list[i], true, true);
                
                _arbitrators.erase (remove_list[i]);            
                
                //_event_queue.erase (remove_list[i]);
        
#ifdef DEBUG_DETAIL
                sprintf (_str, "[%d] removes old enclosing Arbitrator [%d]\r\n", _vastnode->getSelf ()->id, remove_list[i]);
                _eo.output (_str);
#endif
            }
        }

        // notify connected agents of the change in enclosing arbitrators
        if (has_changed)
        {
            for (map<id_t, Node>::iterator it = _agents.begin (); it != _agents.end (); it++)
            {
                id_t agent = it->second.id;
                notifyArbitratorship (agent);
            }
        }
    }    
   
    void
    ArbitratorImpl::checkVONJoin () 
    {
        // if we have just joined the arbitrator mesh, then perform SPS over managing area
        if (_state == JOINING && _VONpeer->isJoined ())
        {
            // TODO: define the subscription area (i.e., the area of interest, AOI)
            Area aoi; 
            
            // right now just use whole area
            // NOTE / TODO: right now VONpeer's overlap check is only circular,
            //              so right now we're using over-subscription to receive all events properly
            //              very inefficient
            aoi.center.x    = (coord_t)_para.world_width / 2;
            aoi.center.y    = (coord_t)_para.world_height / 2;
            aoi.radius      = (length_t)_para.world_width;
            aoi.height      = (length_t)_para.world_height;
            
            /*
            // the two points defining the bounding box
            // we assume coordinates increase from top to bottom, left to right
            Position topleft, bottomright;  

            topleft.x = bottomright.x = (coord_t)(_para.world_width / 2);
            topleft.y = bottomright.y = (coord_t)(_para.world_height / 2);

            vector<Vast::line2d> &edges = _VONpeer->getVoronoi ()->getedges ();

            // loop through each edge to determine the bounding box
            // ignore half-edges? 
            vector<Vast::line2d>::iterator it = edges.begin ();
            for (; it != edges.end (); it++)
            {
            }
            */

            vector<Node *> &neighbors = _VONpeer->getNeighbors ();
            printf ("Arbitrator [%ld] neighbors: ", _self.id);
            for (unsigned int i=0; i<neighbors.size (); i++)
                printf ("[%ld] (%d, %d) ", neighbors[i]->id, (int)neighbors[i]->aoi.center.x, (int)neighbors[i]->aoi.center.y);
            printf ("\n");
            
            // perform subscription
            _sub_no = _vastnode->subscribe (aoi, VAST_LAYER_EVENT);           
            
            _state = JOINING_2;
        }
        else if (_state == JOINING_2)
        {
            if (_vastnode->isSubscribing (_sub_no))
                _state = JOINED;
        }
        else if (_state == JOINED)
        {
            // check for incoming VAST messages
            Message *msg;
            
            // NOTE: that we will use the same message handling mechanism as other messages
            while ((msg = _vastnode->receive ()) != NULL)
            {
                if (this->handleMessage (*msg) == false)
                    printf ("[%ld] ArbitratorImpl::checkVONJoin() unhandled / unknown msgtype %d from VAST\n", _vastnode->getSelf ()->id, msg->msgtype);
            }
        }
    }

    // remove any invalid avatar objects 
    // (defined as agents no longer in _agents for over COUNTDOWN_REMOVE_AVATAR period        
    void 
    ArbitratorImpl::validateObjects ()
    {        
        // check removal for any disconnected avatar objects
        for (map<obj_id_t, StoredObject>::iterator it = _obj_store.begin (); it != _obj_store.end (); ++it)
        {
            obj_id_t obj_id = it->first;
            Object *obj = it->second.obj;

            timestamp_t curr_time = _tick;
            timestamp_t unchanged_period = curr_time - it->second.last_update;

            // if it's an avatar Object I own            
            if (isOwner (obj_id))
            {
                // it's an avatar object
                if (obj->agent != 0)
                {
                    // if it's no longer connected then remove it
                    if (_agents.find (obj->agent) == _agents.end ())
                    {
                        if (_agents_expire.find (obj->agent) == _agents_expire.end ())
                            _agents_expire[obj->agent] = COUNTDOWN_REMOVE_AVATAR;
                
                        else if (_agents_expire[obj->agent] == 0)
                        {
                            destroyObject (obj->getID ());
                            _agents_expire.erase (obj->agent);
                        }
                
                        else
                            _agents_expire[obj->agent]--;
                    }
                    else
                        _agents_expire.erase (obj->agent);
                }

                // check if this Object needs to refresh according to object expiring rules
                if (unchanged_period >= (TIMEOUT_EXPIRING_OBJECT/2))
                {
#ifdef OBJECT_KEEP_ALIVE
                    obj->pos_dirty = true;
                    it->second.last_update = curr_time;
#endif
                }

            }

            // if the object has no owner, delete it
            else
            {   
                if (unchanged_period >= TIMEOUT_EXPIRING_OBJECT)
                {
#ifdef CLEAN_NONALIVE_OBJECT
                    obj->markDeleted ();
                    it->second.last_update = curr_time;
#endif
                }
            }
        }
    }

    // process Event in Event queue in sequence
    // returns the number of events processed
    int ArbitratorImpl::processEvent ()
    {
        int processed = 0;

        // process events
        multimap<timestamp_t, Event *>::iterator it = _events.begin ();
        for (; it != _events.end (); it++)
        {
            Event *e = it->second;
            if (_logic->onEvent (e->getSender (), (*e)) == true)
                processed++;
            delete e;
        }

        _events.clear ();

        return processed;
    }

    void ArbitratorImpl::processTransfer (Message &in_msg)
    {
        // if we've not yet joined properly, or if this node is already departed
        if (_state == ABSENT)
            return;

        obj_id_t obj_id;
        id_t     new_owner;
        id_t     old_owner;

        in_msg.extract (obj_id);
        in_msg.extract (new_owner);
        in_msg.extract (old_owner);

        bool to_me = (_VONpeer->getSelf ()->id == new_owner);

        if (_obj_store.find (obj_id) == _obj_store.end ())
        {
            // if the object is transferred to me but I don't have the object, 
            // queue the message & request the object
            if (to_me)
            {
                sprintf (_str, "[%ld] ArbitratorImpl::processTransfer () get OWNER of unknown obj [%ld]\r\n", _self.id, obj_id);
                _eo.output (_str);

                vector<obj_id_t> request_list;
                request_list.push_back (obj_id);

                requestObject (in_msg.from, request_list);

                // record the ownership transfer (to be re-processed later when full object is received)
                in_msg.reset ();
                Message *m = new Message (in_msg);
                _transfer_msg[obj_id] = m;                
            }   

            return;
        }
            
        // get reference to object
        StoredObject &so = _obj_store[obj_id];

        // if I am new owner
        if (to_me)
        {                                      
            // set self ownership
            so.is_owner     = true;
            so.last_update  = _tick;
        
            // create agent if it's an avatar object
            if (so.obj->agent != 0)
            {
                Node agent_info;
                
                if (in_msg.extract (agent_info) > 0)
                {
                    // TODO: BUG: we assume agent's 'from' is recorded in address
                    obj_id_t obj_id = so.obj->getID ();
                    addAgent (agent_info.addr.host_id, agent_info, &obj_id);
            
                #ifdef DISCOVERY_BY_NOTIFICATION
                    // extract agent's knowledge on AOI objects
                    listsize_t n;
                    in_msg.extract (n);
            
                    map<obj_id_t, version_t> known_objs;
                    version_t version;
                    for (int i=0; i<(int)n; i++)
                    {
                        in_msg.extract (obj_id);
                        in_msg.extract (version);
                        known_objs[obj_id] = version;
                    }
            
                    if (known_objs.size () > 0)
                        _known_objs[agent_info.id] = known_objs;
                #endif                    
                }
                else
                {
                    sprintf (_str, "[%ld] ArbitratorImpl::processTransfer received agent_info cannot be extracted\r\n", _self.id);
                    _eo.output (_str);
                }
            }

            // send acknowledgement of TRANSFER
            if (old_owner != NET_ID_UNASSIGNED)
            {
                Message msg (TRANSFER_ACK);
                msg.priority = 1;
                msg.store (obj_id);
                msg.addTarget (in_msg.from);
                sendMessage (msg);
            }

            // reset reclaim countdown 
            if (_reclaim_countdown.find (obj_id) != _reclaim_countdown.end ())
                _reclaim_countdown.erase (obj_id);

        #ifdef DECLEAR_OWNERSHIP_ON_TRANSFER
            // notify neighbor arbitrators of ownership of this object
            // so that they may request it if it's not known to them
            vector<id_t> targets;
            if (getEnclosingArbitrators (targets))
                notifyOwnership (targets, &obj_id);
        #endif
        }
        
        /*
        // if I am not the owner, remove object from store
        // but also check if the sender is considered a legal current owner of the object
        else if (isOwner (obj_id) == false)
        {
            id_t closest = _VONpeer->getVoronoi ()->closest_to (so.obj->getPosition ());
                       
            if (old_owner == closest)
                printf ("");
                // right now causes avatar object still be deleted by current arbitrators)
                //destroyObject (obj_id);
                
        }
        */
    }

    // check to call additional arbitrators for load balancing
    void ArbitratorImpl::checkOverload ()
    {
        size_t n = _agents.size ();

        // collect stats on # of agents 
        if (n > 0)
            _stat_agents.addRecord (n);

        // check if no limit is imposed
        if (_para.overload_limit == 0)
            return;

        // if # of agents exceed limit, notify for overload
        else if (n > _para.overload_limit)
            notifyLoading (n);
        
        // underload
        // TODO: if UNDERLOAD threshold is not 0,
        // then we need proper mechanism to transfer objects out before arbitrator departs
        else if (n <= 0)
            notifyLoading (-1);
        
        // normal loading
        else
            notifyLoading (0);
    }

    // perform statistics collection
    void ArbitratorImpl::reportStat ()
    {
        Message msg (STAT);
        msg.priority = 2;

        // store send & recv size    
        size_t size = _net->getSendSize () - _last_send;
        msg.store ((char *)&size, sizeof (size_t));
        _last_send = _net->getSendSize ();

        size = _net->getReceiveSize () - _last_recv;
        msg.store ((char *)&size, sizeof (size_t));
        _last_recv = _net->getReceiveSize ();

        // store agent size stat
        _stat_agents.calculateAverage ();
        msg.store ((char *)&_stat_agents, sizeof (StatType));
        _stat_agents.reset ();
        
        msg.addTarget (NET_ID_GATEWAY);
        sendMessage (msg);
    }

    // send loading to neighbors
    void ArbitratorImpl::reportLoading ()
    {
        Message msg (LOADING);
        msg.priority = 1;

        listsize_t n = (listsize_t)_agents.size ();
        msg.store (n);
        
        if (getEnclosingArbitrators (msg.targets) == true)
            sendMessage (msg);
    }

    // perform object discovery for agents
    void ArbitratorImpl::updateAOI (id_t target_agent)
    {       
        vector<id_t> create_list;
        //vector<id_t> delete_list;

        // loop through all objects I know
        for (map<obj_id_t, StoredObject>::iterator it = _obj_store.begin (); it != _obj_store.end (); ++it)
        {
            Object *obj = it->second.obj;            
            create_list.clear ();
            //delete_list.clear ();

            // NOTE: if object states are fully backup at all enclosing arbitrators,
            // then we can actively notify agents of potential AOI objects
            // otherwise, the states may be partial and inaccurate (as updates are not continous)
            // thus an arbitrator can / should only notify agents of its own objects

            //
            // TODO: cleanup backup objects if they're unused / inaccurate?
            //       do not execute the following will show those backup objects
#ifndef FAULT_TOLERANT_1
            // only notify agents of my owned objects
            if (it->second.is_owner == false)
                continue;
#endif
            // check if obj is discovered correctly for all connected Agents
            for (map<id_t, Node>::iterator it = _agents.begin (); it != _agents.end (); it ++)
            {
                id_t agent_id = it->first;

                // TODO: make this more efficient
                if ((target_agent != 0) && (agent_id != target_agent))
                    continue;

                bool aoi_obj = obj->isAOIObject (it->second, true);
                bool known = (_known_objs[agent_id].find (obj->getID ()) != _known_objs[agent_id].end ());
                
                // check if the agent should know the object
                if (aoi_obj && !known)
                {
                    // insert knowledge record
                    _known_objs[agent_id][obj->getID ()] = obj->version;                    
                    create_list.push_back (agent_id);
                }
                
                // but otherwise note that the agent no longer knows (or needs to know) the object
                else if (!aoi_obj && known)
                {
                    // remove list item
                    _known_objs [agent_id].erase (obj->getID ());
                    //delete_list.push_back (agent_id);
                }                
            }

            // send obj create / delete notification
            if (create_list.size () > 0)
                sendFullObject (obj, create_list, MSG_GROUP_VASTATE_AGENT);

            // no need to send delete
            //if (delete_list.size () > 0)
            //    sendFullObject (obj, delete_list, MSG_GROUP_VASTATE_AGENT, true);
             
            // NOTE: that this agent will still receive incremental updates 
            //       as updates are published without knowing who'll be the receivers)
            // TODO: more efficient way to reduce redundent notifications?
        }
    }

    // publish updated Object states I own to affected nodes
    bool
    ArbitratorImpl::sendUpdates ()
    {
        if (_state == ABSENT)
            return false;

        Message create_msg (OBJECT_C);      // object creation message
        Message pos_msg (POSITION);         // object position update to be sent
        Message state_msg (STATE);          // state upate to be sent
        Message delete_msg (OBJECT_D);      // object deletion message

        vector<obj_id_t> delete_list;       // list of objects marked for deletion

        // all the updates sent now are targeted towards agents
        //pos_msg.msggroup = state_msg.msggroup = delete_msg.msggroup = MSG_GROUP_VASTATE_AGENT;
        
        vector<id_t> neighbor_arbitrators;      // list of enclosing arbitrators
        vector<id_t> relevant_arbitrators;      // list of enclosing arbitrators interested in the object

#ifdef BACKUP_STATES
        getEnclosingArbitrators (neighbor_arbitrators);
        relevant_arbitrators = neighbor_arbitrators;
#endif
        // send all dirty objects to the interested nodes
        for (map<obj_id_t, StoredObject>::iterator it = _obj_store.begin (); it != _obj_store.end (); ++it)
        {
            StoredObject &so = it->second;
            Object *obj      = so.obj;
            obj_id_t obj_id  = obj->getID ();

            // debug
            if (obj->pos_version == 0)
            {
                printf ("[%ld] ArbitratorImpl::sendUpdates () pos_version = 0 for Object [%ld], marked for deletion\n", _vastnode->getSelf ()->id, obj->getID ());
                                
                // also remove the associated object (or agent, if it's an avatar object)
                if (obj->agent != 0)
                    removeAgent (obj->agent, true);
                
                destroyObject (obj->getID ());
                    
                //obj->markDeleted ();
                
                continue;
            }

            // store if object should be deleted
            // NOTE: delete_list must store object ID now or objects managed by
            //       other arbitrators won't get deleted
            if (obj->isAlive () == false)            
                delete_list.push_back (obj_id);            

            // perform update only for owned objects or objects in transit
            if (so.is_owner == false && so.in_transit == 0)
                continue;

            create_msg.clear (OBJECT_C, 0, 1);
            delete_msg.clear (OBJECT_D, 0, 1);
            pos_msg.clear (POSITION, 0, 1);
            state_msg.clear (STATE, 0, 1);

            // send delete messages if objects is to be deleted
            if (obj->isAlive () == false)
            {   
                delete_msg.store (obj_id);                           
            }

            else 
            {
                // if this is a new object created, publish to agents & notify neighbor arbitrators
                if (_new_objs.find (obj_id) != _new_objs.end ())
                {    
                    // send object creation update to nodes who might be interested                    
                    create_msg.store (obj_id);
                    create_msg.store (obj->type);          // object type
                    create_msg.store (obj->size ());       // # of attributes
                    create_msg.store (obj->getPosition ());
                    create_msg.store (obj->pos_version);
                    create_msg.store (obj->agent);          
                }

                // check if position has changed or the object shall be deleted
                if (obj->pos_dirty == true)
                {
                    // encode Object position (dirty only)
                    obj->encodePosition (pos_msg, true);
                }
                
                // encode only the dirty Attributes into this update
                if (obj->dirty == true)                
                    obj->encodeStates (state_msg, true);
            }

            // skip this object if no changes occur
            if (create_msg.size == 0 && delete_msg.size == 0 && pos_msg.size == 0 && state_msg.size == 0)
                continue;


#ifdef BACKUP_STATES_LEAN
            // find which arbitrator actually needs to be notified of this object's update
            // TODO: more efficient method?
            if (neighbor_arbitrators.size () > 0)
            {                            
                Position obj_pos = obj->getPosition ();

                // closest arbitrator
                id_t closest_arb = neighbor_arbitrators[0]; 
                double shortest_dist = obj_pos.distance (_arbitrators[closest_arb].aoi.center);
                double dist;

                for (size_t i=1; i < neighbor_arbitrators.size (); i++)
                {
                    id_t arbitrator_id = neighbor_arbitrators[i];
                    if ((dist = obj_pos.distance (_arbitrators[arbitrator_id].aoi.center)) < shortest_dist)
                    {
                        shortest_dist = dist;
                        closest_arb = arbitrator_id;
                    }
                }

                // check if we should notify previous closest arbitrator of the change
                if (so.closest_arb != closest_arb)
                {                    
                    if (so.closest_arb != 0)
                    {
                        Message msg (SWITCH);
                        msg.priority = 1;
                        msg.store (obj_id);
                        msg.store (closest_arb);
                        msg.addTarget (so.closest_arb);
                        sendMessage (msg);
                    }

                    // record the new closest
                    so.closest_arb = closest_arb;
                }

                relevant_arbitrators.clear ();
                relevant_arbitrators.push_back (closest_arb);
            }            
#endif

            // update time
            so.last_update = _tick;

            // send updates to agents & arbitrators
            // TODO: should check for object ownership before sending updates?
            Area area (obj->getPosition (), 0);

            // IMPORTANT NOTE: the following updates are actually sent to two types of targets
            //                 one is to agents (via publication) and the other is neighboring arbitrators (via send)
            //                 so the same message will need different msggroups be set
            //                 for publication, the msggroup field should be for relay (MSG_GROUP_VAST_RELAY) 
            //                 for sending, msgroup should be for arbitrators (MSG_GROUP_VASTATE_ARBITRATOR)
            //                 so it's best just leave the msggroup field empty and allow vastnode or sendMessage to fill them automatically
            if (create_msg.size > 0)
            {                
                _vastnode->publish (area, VAST_LAYER_UPDATE, create_msg);                          

                if (relevant_arbitrators.size () > 0)
                {
                    create_msg.targets = relevant_arbitrators;
                    sendMessage (create_msg);
                }
            }
            
            if (delete_msg.size > 0)
            {
                _vastnode->publish (area, VAST_LAYER_UPDATE, delete_msg);

                /*
                if (relevant_arbitrators.size () > 0)
                {
                    delete_msg.targets = relevant_arbitrators;
                    sendMessage (delete_msg);
                }
                */

                // NOTE that for delete message, we want to make sure it's sent across
                //      all neighbors so a clean removal is done
                if (neighbor_arbitrators.size () > 0)
                {
                    delete_msg.targets = neighbor_arbitrators;
                    sendMessage (delete_msg);
                }
            }

            if (pos_msg.size > 0)
            {
                _vastnode->publish (area, VAST_LAYER_UPDATE, pos_msg);

                if (relevant_arbitrators.size () > 0)
                {
                    pos_msg.targets = relevant_arbitrators;
                    sendMessage (pos_msg);
                }
            }

            if (state_msg.size > 0)
            {
                _vastnode->publish (area, VAST_LAYER_UPDATE, state_msg);

                if (relevant_arbitrators.size () > 0)
                {
                    state_msg.targets = relevant_arbitrators;
                    sendMessage (state_msg);
                }
            }

            if (obj->isAlive ())
            {
                // notify the local node of the updates (both position & states)
                if (obj->pos_dirty == true)
                    _logic->onMove (obj_id, obj->getPosition(), obj->pos_version);
            
                // loop through each attribute and notify the Arbitrator logic                                
                for (int j=0; j<obj->size (); ++j)
                {
                    word_t size;
                    void *p;
                    if (obj->get (j, &p, size) == false || obj->isDirty (j) == false)
                        continue;
                    
                    // if dirty then notify the Arbitrator logic layer
                    _logic->onUpdate (obj_id, j, p, size, obj->version);
                }
            }

            // reset dirty flag (we've sent the updates now, so this object's clean)
            obj->resetDirty ();
            obj->pos_dirty = false;
        }

        // all new objects should have been notified
        _new_objs.clear ();

        // remove all deleted object
        for (size_t i=0; i < delete_list.size (); i++)
            unstoreObject (delete_list[i]);

        return true;
    }

    // notify another arbitrator of objects that I own, 
    // or notify only a single object (optional)
    bool 
    ArbitratorImpl::notifyOwnership (vector<id_t> &targets, obj_id_t *obj_id)
    {
        // TODO: check if targets are arbitrators

        // TODO: optimize storage (use just one or no list)
        vector<obj_id_t>    obj_list;
        vector<version_t>   ver_list;
 
        if (obj_id != NULL)
        {
            map<obj_id_t, StoredObject>::iterator it = _obj_store.find (*obj_id);
            if (it == _obj_store.end ())
                return false;

            obj_list.push_back (*obj_id);
            ver_list.push_back (it->second.obj->version);
        }
        else
        {
            for (map<obj_id_t, StoredObject>::iterator it = _obj_store.begin (); it != _obj_store.end (); ++it)
            {
                // if send owned objects only, but not owned, skip it
                if (it->second.is_owner)
                {
                    obj_list.push_back (it->second.obj->getID ());
                    ver_list.push_back (it->second.obj->version);
                }
            }
        }

        // prepare notification
        listsize_t n = (listsize_t)obj_list.size ();

        if (n > 0)
        {
            Message msg (OWNER);
            msg.priority = 1;
                      
            msg.store (n);

            for (size_t i = 0; i < obj_list.size (); i++)
            {
                msg.store (obj_list[i]);
                msg.store (ver_list[i]);
            }
        
            msg.targets = targets;
            sendMessage (msg);
        }             
        return true;        
    }

    // notify an agent that I'm the current arbitrator
    bool 
    ArbitratorImpl::notifyArbitratorship (id_t agent)
    {
        Message msg (ARBITRATOR);
        msg.priority = 1;

        listsize_t n = (listsize_t)(1 + _arbitrators.size ());
        msg.store (n);        
        msg.store (_self);

        // store enclosing arbitrators 
        map<id_t, Node>::iterator it = _arbitrators.begin ();
        for (; it != _arbitrators.end (); it++)
            msg.store (it->second);

        msg.addTarget (agent);

        return (sendAgent (msg) == 1);
    }

    // same effect as sendMessage (), but checks and remove failed agents (if any)
    int
    ArbitratorImpl::sendAgent (Message &msg)
    {
        vector<id_t> failed;

        // assign the arbitrator's message group, this will allow this message be received by the gateway's arbitrator node
        msg.msggroup = MSG_GROUP_VASTATE_AGENT;

        int result = sendMessage (msg, &failed);

        if (failed.size () > 0)
        {
            for (size_t i=0; i < failed.size (); i++)
            {
                if (_agents.find (failed[i]) != _agents.end ())
                    removeAgent (failed[i]);
            }           
        }

        // send back whether all sends are successful
        return result;
    }

    // send all objects (full version) in range
    bool
    ArbitratorImpl::sendObjects (id_t target, bool owned, bool send_delete)
    {
        // if target is not an arbitrator
        if (_arbitrators.find (target) == _arbitrators.end ())
            return false;

        vector<id_t> targets;
        targets.push_back (target);

        for (map<obj_id_t, StoredObject>::iterator it = _obj_store.begin (); it != _obj_store.end (); ++it)
        {
            Object *obj = it->second.obj;

            // if send owned objects only, but not owned, skip it
            if (owned && !isOwner (obj->getID ()))
                continue;

            if (send_delete)
                sendFullObject (obj, targets, MSG_GROUP_VASTATE_ARBITRATOR, true);
            else
                sendFullObject (obj, targets, MSG_GROUP_VASTATE_ARBITRATOR, false);
        }

        return true;
    }

    // send full object states to specified target(s)
    bool 
    ArbitratorImpl::sendFullObject (Object *obj, vector<id_t> &targets, byte_t msggroup, bool to_delete)
    {                 
        obj_id_t obj_id = obj->getID ();

        if (to_delete)
        {
            obj_msg.clear (OBJECT_D);
            obj_msg.store (obj_id);
        }
        else
        {
            Position p = obj->getPosition ();
            obj_msg.clear (OBJECT_C);

            obj_msg.store (obj_id);
            obj_msg.store (obj->type);
            obj_msg.store (obj->size ());
            obj_msg.store (p);
            obj_msg.store (obj->pos_version);
            obj_msg.store (obj->agent);
        }
        
        // notfiy object creation / deletion
        obj_msg.msggroup = msggroup;
        obj_msg.targets = targets;
        
        // NOTE: receiver may also be an arbitrator
        // TODO: re-factor the checks?
        if (msggroup == MSG_GROUP_VASTATE_AGENT)
            sendAgent (obj_msg);
        else
            sendMessage (obj_msg);

        // notify position / states
        if (to_delete == false)
        {
            pos_msg.clear (POSITION);
            state_msg.clear (STATE);

            pos_msg.msggroup = state_msg.msggroup = msggroup;

            obj->encodePosition (pos_msg, false);
            obj->encodeStates (state_msg, false);
                
            // send position
            pos_msg.targets = targets;

            if (msggroup == MSG_GROUP_VASTATE_AGENT)
                sendAgent (pos_msg);
            else
                sendMessage (pos_msg);
            
            if (state_msg.size > 0)
            {
                state_msg.targets = targets; 
                if (msggroup == MSG_GROUP_VASTATE_AGENT)
                    sendAgent (state_msg);
                else
                    sendMessage (state_msg);
            }
        }
        return true;
    }

    // send an Object update request to the approprite node (checks for target ownership)
    bool 
    ArbitratorImpl::forwardRequest (obj_id_t obj_id, msgtype_t msgtype, char *buf, int size)
    {
        Object *obj = (_obj_store.find (obj_id) != _obj_store.end () ? 
                       _obj_store[obj_id].obj : NULL);        
            
        // target Object does not exist in store
        if (obj == NULL)
            return false;

        // loop through all connected arbitrators to find the nearest
        double min_dist = _self.aoi.radius;
        id_t   target = _self.id;
        
        for (map<id_t, Node>::iterator it = _arbitrators.begin (); it != _arbitrators.end (); ++it)
        {
            double dist = it->second.aoi.center.distance (obj->getPosition());
            if (dist <= min_dist)
            {
                min_dist = dist;
                target   = it->first;
            }
        }
        
#ifdef DEBUG_DETAIL
        sprintf (_str, "[%d] forwards %s request to [%d]\r\n", _self.id, VASTATE_MESSAGE[(int)(msgtype-10)], target);
        _eo.output (_str);
#endif

        //_net->sendmsg (target, msgtype, buf, size);

        return true;
    }

    // send request to arbitrator for full object states (when POSITION or STATE is received for unknown objects)
    void 
    ArbitratorImpl::requestObject (id_t arbitrator, vector<obj_id_t> &obj_list)
    {
        vector<obj_id_t> new_request;

        // do not request objects already requested
        // TODO: unless timeout? 
        for (size_t i=0; i < obj_list.size (); i++)
        {
            if (_obj_requested.find (obj_list[i]) == _obj_requested.end ())
                new_request.push_back (obj_list[i]);
        }

        if (new_request.size () == 0)
            return;
           
        // send request to the arbitrator that sends me the update
        Message msg (OBJECT_R);
        msg.priority = 1;
        
        // store msggroup for the response as responding to arbitrator request
        listsize_t n    = MSG_GROUP_VASTATE_ARBITRATOR;      
        msg.store (n);

        // only one object to request
        n = (listsize_t)new_request.size ();
        msg.store (n);

        // store obj_id, 
        // NOTE a copy has to be made as store () takes a Serializable object
        // but obj_id is already a reference, so passing it in would create complier-specific problems
        // that would crash during run-time
        // TODO: still need it? 
        for (size_t i=0; i < new_request.size (); i++)
        {
            obj_id_t   oid = new_request[i];
            msg.store (oid);

            // make a record that this object has been requested
            _obj_requested[oid] = arbitrator;
        }

        msg.addTarget (arbitrator);
        msg.msggroup = MSG_GROUP_VASTATE_ARBITRATOR;
        sendMessage (msg);
    }

    // find a suitable new Arbitrator given a certain need/stress level
    bool 
    ArbitratorImpl::findArbitrator (int level, Node &new_arb)
    {
        if (_potentials.size () == 0)
            return false;

        // for now we simply return the first
        /*
        new_arb = _potentials[0];
        _potentials.erase (_potentials.begin ());
        */
        
        size_t i;

        // find the first arbitrator that has a different IP
        for (i = 0; i < _potentials.size (); i++)
        {
            if (_promoted_hosts.find (_potentials[i].addr.publicIP.host) == _promoted_hosts.end ())
            {
                _promoted_hosts[_potentials[i].addr.publicIP.host]++;
                break;
            }
        }

        // if not found, simply return the first
        if (i == _potentials.size ())
            i = 0;

        new_arb = _potentials[i];
        _potentials.erase (_potentials.begin () + i);

        printf ("\n[%ld] promoting [%ld] %s as new arbitrator\n\n", _self.id, new_arb.id, new_arb.addr.toString ().c_str ());
        
        /*
        int index;
        int minimal = 0;

        // find a potential Arbitrator has minimal promotion count
        for (index = 0; index < (int)_potentials.size (); index++)
        {
            if (_promotion_count[index] < _promotion_count[minimal])
                minimal = index;
        }
        */

        // return the minimal promoted node
        //new_arb = _potentials[min];
        
        //_promotion_count[minimal]++;

        return true;
    }
    
    const char * ArbitratorImpl::toString ()
    {

        static std::string string_out;
        char buf[80];

        string_out.clear ();

        for(map<id_t,Node>::iterator it=_arbitrators.begin();
            it != _arbitrators.end(); it++)        
        {
            sprintf (buf, "e_arb [%ld] (%d,%d)\r\n", it->first, (int) it->second.aoi.center.x, (int) it->second.aoi.center.y);
            string_out.append (buf);
        }

        string_out.append ("getnodes ==\r\n");

        vector<Node *> &nodes = _vastnode->list ();
        for (vector<Node *>::iterator it = nodes.begin () ;it != nodes.end (); it ++)
        {
            Node * n = *it;
            sprintf (buf, "e_arb [%ld] (%d,%d)\r\n", n->id, (int) n->aoi.center.x, (int) n->aoi.center.y);
            string_out.append (buf);
        }

        return string_out.c_str ();
    }
    
} // namespace Vast
