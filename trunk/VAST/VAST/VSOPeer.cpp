

#include "VSOPeer.h"

using namespace Vast;

namespace Vast
{   

    // default constructor
    VSOPeer::VSOPeer (id_t id, VONNetwork *net, VSOPolicy *policy, length_t aoi_buffer)
        :VONPeer (id, net, aoi_buffer), 
         _policy (policy), 
         _load_counter (0), 
         _overload_count (0)
    {
        _newpos = _self;

        notifyCandidacy ();

        _vso_state = JOINING;
    }

    // default destructor
    VSOPeer::~VSOPeer ()
    {
    }

    bool 
    VSOPeer::handleMessage (Message &in_msg)    
    {
        // if join is not even initiated, do not process any message 
        //if (_state != JOINED)
        //    return false;

        switch ((VSO_Message)in_msg.msgtype)
        {

        // registration of a potential node with the gateway
        case VSO_CANDIDATE:
            if (_self.id == _policy->getGatewayID ())
            {
                Node candidate;

                in_msg.extract (candidate);

                // store potential nodes to join
                _candidates.push_back (candidate);
            }
            else
            {
                printf ("[%llu] VSOPeer::handleMessage () VSO_CANDIDATE received by non-gateway\r\n", _self.id);
            }
            break;
                         
        // Overloaded node's request for inserting a new helper
        case VSO_INSERT:                      
            // only the gateway can process it
            if (_self.id != _policy->getGatewayID ())
            {
                printf ("[%llu] VSOPeer::handleMessage () VSO_INSERT received by non-gateway\r\n", _self.id);
            }
            else
            {
                float level;
                Position join_pos;

                in_msg.extract ((char *)&level, sizeof (float));
                in_msg.extract (join_pos);

                // TODO: ignore redundent requests at same position
                
                // promote one of the spare potential nodes
                Node new_node;

                // TODO: if findCandidate () fails, insert virtual
                // return from loop either request successfully served (promotion sent) 
                // or no candidates can be found
                while (findCandidate (level, new_node))
                {
                    // fill in the stressed node's contact info & join location
                    Node requester;
                    
                    requester.id = in_msg.from;
                    requester.time = _net->getTimestamp ();
                    requester.aoi.center = join_pos;

                    Message msg (VSO_PROMOTE);
                    msg.priority = 1;
                    msg.store (join_pos);
                    msg.store (_net->getHostAddress ());

                    // send promotion message
                    _net->notifyAddressMapping (new_node.id, new_node.addr);
                    msg.addTarget (new_node.id);

                    // record the promoted position if sent success
                    if (_net->sendVONMessage (msg) > 0)   
                    {
                        _promote_requests[new_node.id] = requester;
                        break;
                    }
                }
            }            
            break;
        
        // a candidate node gets promoted as an actual node
        case VSO_PROMOTE:        
            if (_state == ABSENT)
            {
                // TODO: find a way so gateway address need not be sent?
                Position join_pos;
                Addr addr;
                in_msg.extract (join_pos);
                in_msg.extract (addr);

                Node gateway;
                gateway.id = in_msg.from;
                gateway.addr = addr;

                Area aoi (join_pos, 5);

                // join at the specified position, but only if I'm available
                join (aoi, &gateway);
            }
            break;

        // Overloaded node's request for neighbors to move closer
        case VSO_MOVE:            
            {                             
                // received not my enclosing neighbor's help signal
                if (_Voronoi->is_enclosing (in_msg.from) == false)
                {
                    printf ("[%llu] VSO_MOVE received from non-enclosing neighbor [%llu]\r\n", _self.id, in_msg.from);                    
                    break;
                }

                // extract loading
                float level;
                in_msg.extract ((char *)&level, sizeof (float));

                // calculate new position after moving closer to the stressed node
                Position neighbor_pos = _Voronoi->get (in_msg.from);
                Position temp_pos = _newpos.aoi.center;

                // move at a speed proportional to the severity of the overload
                temp_pos += (((neighbor_pos - temp_pos) * VSO_MOVEMENT_FRACTION) * level);

                if (isLegalPosition (temp_pos, false))
                    _newpos.aoi.center = temp_pos;        
            }                
            break;

        case VSO_JOINED:
            // only the gateway can process it
            if (_self.id != _policy->getGatewayID ())
            {
                printf ("[%llu] VSOPeer::handleMessage () VSO_JOINED received by non-gateway\r\n", _self.id);
            }
            else
            {
                printf ("[%llu] VSOPeer: VSO_JOINED received, new VSO node [%llu] joined\r\n", _self.id, in_msg.from);

                if (_promote_requests.find (in_msg.from) != _promote_requests.end ())
                    _promote_requests.erase (in_msg.from);
            }
            break;
        
        // Ownership transfer from old to new owner
        case VSO_TRANSFER:
            {
                processTransfer (in_msg);
            }
            break;
        
        // acknowledgment of ownership transfer received
        case VSO_TRANSFER_ACK:            
            {
                id_t obj_id;
                in_msg.extract (obj_id);                
                
                // check if object exists
                if (_objects.find (obj_id) == _objects.end ())
                {
                    printf ("[%llu] VSOPeer: TRANSFER_ACK received for unknown object [%llu]\n", _self.id, obj_id);
                    break;
                }

                // update in_transit stat                
                _objects[obj_id].in_transit = 0;
            }
            break;

        // request to send full object info to another node
        case VSO_REQUEST:
            {
                listsize_t n;
                in_msg.extract (n);
                id_t obj_id;
                vector<id_t> list;

                for (listsize_t i=0; i < n; i++)
                { 
                    in_msg.extract (obj_id);
                    list.push_back (obj_id);
                }

                // request object transfer from the policy layer
                _policy->copyObject (in_msg.from, list, false, false);
            }
            break;

        default:
            return VONPeer::handleMessage (in_msg);
        }

        return true;

    } // end handleMessage ()

    // current node overloaded, call for help
    // note that this will be called continously until the situation improves
    void
    VSOPeer::notifyLoading (float level)
    {                               
        // first adjust the current load level record
        if (level == 0)
        {
            // normal loading
            _load_counter = 0;
            return;
        }
        else if (level < 0)
        {
            // underload       
            _load_counter--;
        }
        else
        {   
            // overload
            _load_counter++;
        }

        // if overload continus, we try to do something
        // TODO: 
        if (_load_counter > VSO_TIMEOUT_OVERLOAD_REQUEST * _net->getTickPerSecond ())
        {    
            // reset # of detected overloads
            if (_overload_count < 0)
                _overload_count = 0;

            _overload_count++;
                       
            // if the overload situation just occurs, try to move boundary first
            if (_overload_count < VSO_INSERTION_TRIGGER) 
            {                        
                // send the level of loading to neighbors
                // NOTE: important to store the VONpeer ID as this is how the responding node will recongnize
                Message msg (VSO_MOVE);
                msg.from = _self.id;   
                msg.priority = 1;            
                msg.store ((char *)&level, sizeof (float));

                msg.targets = _Voronoi->get_en (_self.id);  
                
                if (msg.targets.size () > 0)
                    _net->sendVONMessage (msg);
            }

            // if the overload situation persists, request matcher insertion from gateway           
            else
            {           
                // find a position to insert a new node to help 
                // TODO: more exotic position than load center?
                Position pos;
                if (getLoadCenter (pos))
                {
                    Message msg (VSO_INSERT);
                    msg.priority = 1;
                    msg.store ((char *)&level, sizeof (float));
                    msg.store (pos);
                    msg.addTarget (_policy->getGatewayID ());
                    _net->sendVONMessage (msg);    
                }

                _overload_count = 0;
            }

            _load_counter = 0;
        }
        // underload event
        else if (_load_counter < -(VSO_TIMEOUT_OVERLOAD_REQUEST * _net->getTickPerSecond ()))
        {            
            // reset
            if (_overload_count > 0)
                _overload_count = 0;

            _overload_count--;

            if (_overload_count > (-VSO_INSERTION_TRIGGER))
            {
                // TODO: notify neighbors to move further away?
            }

            // if we're not gateway then can depart
            else if (_self.id != _policy->getGatewayID ())
            {                
                // depart as node if loading is below threshold                
                leave ();

                notifyCandidacy ();               
                _overload_count = 0;
            }
                    
            _load_counter = 0;
        }
        // normal loading, reset # of OVERLOAD_M requests
        else if (_load_counter == 0)
            _overload_count = 0;        
    }

    // change the center position in response to overload signals
    void 
    VSOPeer::movePeerPosition ()
    {
        // if nothing has changed
        if (_newpos.aoi.center == _self.aoi.center)
            return;

        // performe actual movement
        this->move (_newpos.aoi);

        // update self info
        // NOTE: currently only position will change, not AOI
        _self.aoi.center = _newpos.aoi.center;

        // update other info (such as AOI) for the new position
        _newpos = _self;
    }

    int
    VSOPeer::checkUpdateToNeighbors ()
    {
        int num_updates = 0;

        // go through each owned object and see if neighbors need updates on them
        map<id_t, VSOSharedObject>::iterator it = _objects.begin ();

        map<id_t, vector<id_t> *> update_list;

        for (; it != _objects.end (); it++)
        {
            VSOSharedObject &so = it->second;
            
            // update which VSOpeer is closest to this object
            so.closest = getClosestEnclosing (so.aoi.center);
     
            // check if owned object's AOI overlaps with regions of other VSOPeers
            // if so, then we should send updates to the VSOPeers affected
            if (so.is_owner == true)
            {
                vector<id_t> neighbors;
        
                // if the AOI of this object overlaps into other nodes, 
                // then send updates to these neighbors
                if (getOverlappedNeighbors (so.aoi, neighbors, so.closest) == true)
                {
                    num_updates++;

                    for (size_t i=0; i < neighbors.size (); i++)
                    {
                        id_t neighbor = neighbors[i];
                        if (update_list.find (neighbor) == update_list.end ())
                            update_list[neighbor] = new vector<id_t>;
                        update_list[neighbor]->push_back (it->first);
                    }
                }
            }
        }       

        // for each neighbor, send the objects it needs to know
        map<id_t, vector<id_t> *>::iterator itu = update_list.begin ();
        for (; itu != update_list.end (); itu++)
        {
            _policy->copyObject (itu->first, *itu->second, false, true);                                
            delete itu->second;
        }

        return num_updates;
    }

    // remove obsolete objects (those unowned objects no longer being updated)
    void 
    VSOPeer::removeObsoleteObjects ()
    {
        timestamp_t now = _net->getTimestamp ();
        timestamp_t timeout = TIMEOUT_EXPIRING_OBJECT * _net->getTickPerSecond ();

        vector<id_t> remove_list;

        map<id_t, VSOSharedObject>::iterator it = _objects.begin ();
        for (; it != _objects.end (); it++)
        {
            VSOSharedObject &so = it->second;

            // if object is not owned by me and has not been updated for a while, 
            // remove it
            if (so.is_owner == false && (now - so.last_update > timeout))
                remove_list.push_back (it->first);
        }

        // remove each obsolete object
        for (size_t i=0; i < remove_list.size (); i++)
        {
            // deleteSharedObject should be called automatically
            //deleteSharedObject (remove_list[i]);
            _policy->removeObject (remove_list[i]);
        }
    }


    // see if any of my objects should transfer ownership
    // or if i should claim ownership to any new objects (if neighboring nodes fail)
    int 
    VSOPeer::checkOwnershipTransfer ()
    {        
        if (isJoined () == false)
            return 0;
                                             
        //
        // transfer current subscriptions to neighbor nodes
        //

        // a list of ownership transfer info, grouped by transfer target (closest node)
        map<id_t, vector<VSOOwnerTransfer> *> list;
        
        for (map<id_t, VSOSharedObject>::iterator it = _objects.begin (); it != _objects.end (); ++it)
        {
            // first obtain the subscriptionID and the subscription position
            id_t obj_id         = it->first;
            VSOSharedObject &so = it->second;
            timestamp_t now     = _net->getTimestamp ();
                        
            if (_Voronoi->contains (_self.id, so.aoi.center) == false)
            {
                if (so.is_owner == true)
                {
                    // update ownership transfer counter
                    if (_transfer_countdown.find (obj_id) == _transfer_countdown.end ())
                        _transfer_countdown[obj_id] = COUNTDOWN_TRANSFER;
                    else
                        _transfer_countdown[obj_id]--;

                    // if counter's up, then begin transfer to nearest node
                    if (_transfer_countdown[obj_id] == 0)
                    {
                        id_t closest = _Voronoi->closest_to (so.aoi.center);
                        
                        if (closest != _self.id)
                        {            
                            // record the transfer
                            VSOOwnerTransfer transfer (obj_id, closest, _self.id);

                            if (list.find (closest) == list.end ())
                                list[closest] = new vector<VSOOwnerTransfer>;

                            list[closest]->push_back (transfer);
                                                          
                            // we release ownership for now (but will still be able to process events via in-transit records)
                            // this is important as we can only transfer ownership to one neighbor at a time
    
                            so.is_owner = false;
                            so.in_transit = now; 

                            _transfer_countdown.erase (obj_id);
                        }                        
                    }
                }

                // if object is no longer in my region, remove reclaim counter
                else if (_reclaim_countdown.find (obj_id) != _reclaim_countdown.end ())
                    _reclaim_countdown.erase (obj_id);
            }
                        
            // the object is in my region but I'm not owner, 
            // should claim it after a while
            else 
            {                
                if (so.is_owner == false)
                {
                    // initiate a countdown, and if the countdown is reached, 
                    // automatically claim ownership (we assume neighboring nodes will
                    // have consensus over ownership eventually, as topology becomes consistent)
                    if (_reclaim_countdown.find (obj_id) == _reclaim_countdown.end ())
                        _reclaim_countdown[obj_id] = COUNTDOWN_TRANSFER * 4;
                    else
                        _reclaim_countdown[obj_id]--;
                
                    // we claim ownership if countdown is reached
                    if (_reclaim_countdown[obj_id] == 0)
                    {                         
                        // reclaim only if we're the closest node
                        if (so.closest == 0)
                        {
                            so.is_owner = true;
                            so.in_transit = 0;
                            so.last_update = now;                                            
                        }
                        // it's not really mine, delete it ?
                        else
                        {
                            //destroyObject (obj_id);
                        }

                        _reclaim_countdown.erase (obj_id);                       
                    }                
                }

                // reset transfer counter, if an object has moved out but moved back again
                else if (_transfer_countdown.find (obj_id) != _transfer_countdown.end ())
                {
                    _transfer_countdown.erase (obj_id);
                }
            }     

            // reclaim in transit objects

            if (so.in_transit != 0 && 
                ((now - so.in_transit) > (COUNTDOWN_TRANSFER * 2)))
            {
                so.in_transit = 0;
                so.is_owner = true;
            }
        }

        // TODO: need to make sure the above countdowns do not stay alive
        //       and errously affect future countdown detection

        // perform the actual transfer from list
        Message msg (VSO_TRANSFER);
        
        map<id_t, vector<VSOOwnerTransfer> *>::iterator it = list.begin ();

        int num_transfer = 0;
        for (; it != list.end (); it++)
        {
            msg.clear (VSO_TRANSFER);
            msg.priority = 1;

            vector<VSOOwnerTransfer> &transfers = *it->second;            
            vector<id_t> obj_list;

            listsize_t n = (listsize_t)transfers.size ();
            msg.store (n);
        
            // store each transfer
            for (listsize_t i=0; i < n; i++)
            {
                msg.store (transfers[i]);
                obj_list.push_back (transfers[i].obj_id);
                num_transfer++;               
            }
        
            // perform actual object transfer first
            // NOTE: important to first send out objects, then transfer ownership
            // TODO: this may be wasteful as the object may already has a copy at the neighbor node
            _policy->copyObject (it->first, obj_list, true, false);

            // TODO: notify other nodes to remove ghost objects?
            // remove closest & send to all other neighbors
            // causes crash? 
            
            // send ownership transfer message to closest & all enclosing neighbors
            msg.targets = _Voronoi->get_en (_self.id);
            msg.addTarget (it->first);                            
            
            _net->sendVONMessage (msg);

            // cleanup
            delete it->second;
        }

        // TODO: also transfer some states?        
        return num_transfer;
    }

    void 
    VSOPeer::processTransfer (Message &in_msg)
    {
        // if we've not yet joined properly, or if this node is already departed
        if (isJoined () == false)
            return;

        listsize_t n;
        in_msg.extract (n);

        // missing objects to request
        vector<id_t> request_list;

        // go through each transfer
        for (listsize_t i=0; i < n; i++)
        {
            VSOOwnerTransfer transfer;
            in_msg.extract (transfer);

            bool to_me = (_self.id == transfer.new_owner);
        
            if (_objects.find (transfer.obj_id) == _objects.end ())
            {
                // if the object is transferred to me but I don't have the object, 
                // queue the message & request the object
                if (to_me)
                {
                    printf ("[%llu] VSOPeer::processTransfer () unknown object [%llu], requesting object first...", _self.id, transfer.obj_id);
                                        
                    request_list.push_back (transfer.obj_id);
                
                    // record the ownership transfer (to be re-processed later when full object is received)
                    _transfer[transfer.obj_id] = transfer;              
                }   
                continue;
            }
                        
            // if I am new owner
            if (to_me)
                acceptTransfer (transfer);

        } // for each transfer

        // send request for missing objects to policy layer
        if (request_list.size () > 0)
            requestObjects (in_msg.from, request_list);
    }

    // accept and ownership transfer
    void 
    VSOPeer::acceptTransfer (VSOOwnerTransfer &transfer)
    {    
        // get reference to object
        VSOSharedObject &so = _objects[transfer.obj_id];

        // set self ownership
        so.is_owner     = true;
        so.last_update  = _net->getTimestamp ();
        
        // send acknowledgement of TRANSFER
        if (transfer.old_owner != 0)
        {
            Message msg (VSO_TRANSFER_ACK);
            msg.priority = 1;
            msg.store (transfer.obj_id);
            msg.addTarget (transfer.old_owner);
            _net->sendVONMessage (msg);
        }
        
        // reset reclaim countdown 
        if (_reclaim_countdown.find (transfer.obj_id) != _reclaim_countdown.end ())
            _reclaim_countdown.erase (transfer.obj_id);        
    }

    // send request to a neighbor for a copy of a full object 
    void 
    VSOPeer::requestObjects (id_t target, vector<id_t> &obj_list)
    {
        vector<id_t> new_request;

        // do not request objects already requested
        // TODO: unless timeout? 
        for (size_t i=0; i < obj_list.size (); i++)
        {
            if (_obj_requested.find (obj_list[i]) == _obj_requested.end ())
                new_request.push_back (obj_list[i]);
        }

        if (new_request.size () == 0)
            return;
           
        // send request to the matcher that sends me the update
        Message msg (VSO_REQUEST);
        msg.priority = 1;
        
        // only one object to request
        listsize_t n = (listsize_t)new_request.size ();
        msg.store (n);

        // store object IDs
        // NOTE a copy has to be made as store () takes a Serializable object
        // but if oid is already a reference, passing it in would create complier-specific problems
        // crashable at run-time
        // TODO: still need it? 
        for (size_t i=0; i < new_request.size (); i++)
        {
            id_t obj_id = new_request[i];
            msg.store (obj_id);

            // make a record that this object has been requested
            _obj_requested[obj_id] = target;
        }

        msg.addTarget (target);       
        _net->sendVONMessage (msg);
    }

    // find a suitable new node to join given a certain need/stress level
    bool 
    VSOPeer::findCandidate (float level, Node &new_node)
    {
        if (_candidates.size () == 0)
            return false;
        
        // simply return the first
        // TODO: better method?
        new_node = _candidates[0];
        _candidates.erase (_candidates.begin ());

        string str;
        new_node.addr.toString (str);

        printf ("\n[%llu] promoting [%llu] %s as new node\n\n", _self.id, new_node.id, str.c_str ());

        return true;
    }

    // notify the gateway that I can be available to join
    bool 
    VSOPeer::notifyCandidacy ()
    {
        // register with gateway as a candidate node, as agreed by policy
        if (isGateway (_self.id) == true || _policy->isCandidate () == false)
            return false;
        
        Message msg (VSO_CANDIDATE);
        msg.priority = 1;
        msg.store (_self);
        msg.addTarget (_policy->getGatewayID ());
        _net->sendVONMessage (msg);

        return true;       
    }

    // check if a particular point is within our region
    bool 
    VSOPeer::inRegion (Position &pos)
    {
        return true;
    }

    // process incoming messages & other regular maintain stuff
    void 
    VSOPeer::tick ()
    {
        // perform parent class' tick first
        VONPeer::tick ();

        // check if the VONpeer has joined successfully
        if (_vso_state == JOINING && this->isJoined ())
        {
            _vso_state = JOINED;

            // notify the host for some join processing
            _policy->peerJoined ();

            // notify gateway that I've joined (can remove me from request list)
            Message msg (VSO_JOINED);
            msg.priority = 1;
            msg.addTarget (_policy->getGatewayID ());
            _net->sendVONMessage (msg);
        }

        // perform per-second tasks
        if (_tick_count % _net->getTickPerSecond () == 0)
        {
           // move myself towards the center of load (subscriptions)
            Position center;
            if (getLoadCenter (center))
                _newpos.aoi.center += ((center - _newpos.aoi.center) * VSO_MOVEMENT_FRACTION);
        }

        // move the VSOPeer's position 
        movePeerPosition ();

        // check if we need to transfer ownership to neighboring region periodically
        checkOwnershipTransfer ();

        // check if we need to send object updates to neighboring regions
        // NOTE: must run *after* ownership transfer check, so only owned objects are checked
        checkUpdateToNeighbors ();

        // remove obsolete objects (those unowned objects no longer being updated)
        removeObsoleteObjects ();
    }

    // add a particular shared object into object pool for ownership management
    bool 
    VSOPeer::insertSharedObject (id_t obj_id, Area &aoi, bool is_owner, void *obj)
    {
        // avoid redundent insert
        if (_objects.find (obj_id) != _objects.end ())
        {
            printf ("VSOPeer::insertSharedObject () object [%llu] already inserted\n", obj_id);
            return false;
        }

        // initialize & store the shared object
        VSOSharedObject so;
        so.aoi      = aoi;
        so.is_owner = is_owner;
        so.obj      = obj;
        so.last_update = _net->getTimestamp ();

        _objects[obj_id] = so;

        // process prior ownership transfer message, if any
        // NOTE that if this object was requested because I receive an ownership transfer
        //      then immediately assume ownership
        if (_transfer.find (obj_id) != _transfer.end ())        
        {                    
            VSOOwnerTransfer &transfer = _transfer[obj_id];
            acceptTransfer (transfer);
            _transfer.erase (obj_id);
        }

        // remove previous object request, if any
        if (_obj_requested.find (obj_id) != _obj_requested.end ())
            _obj_requested.erase (obj_id);

        return true;
    }


    // change position for a particular shared object
    bool 
    VSOPeer::updateSharedObject (id_t obj_id, Area &aoi)
    {
        map<id_t, VSOSharedObject>::iterator it = _objects.find (obj_id);

        if (it == _objects.end ())
        {
            printf ("VSOPeer::updateSharedObject () object [%llu] not found\n", obj_id);
            return false;
        }

        VSOSharedObject &so = it->second;

        // NOTE that it's okay to update an object not owned by me 
        // for example, if object's AOI covers this VSOPeer's region
        so.aoi = aoi;
        so.last_update = _net->getTimestamp ();

        return true;
    }

    // remove a shared object
    bool 
    VSOPeer::deleteSharedObject (id_t obj_id)
    {
        map<id_t, VSOSharedObject>::iterator it = _objects.find (obj_id);

        if (it == _objects.end ())
        {
            printf ("VSOPeer::deleteSharedObject () object [%llu] not found\n", obj_id);
            return false;
        }

        _objects.erase (it);
        // TODO: notify to other peers?

        return true;
    }

    // obtain reference to a shared object, returns NULL for invalid object
    VSOSharedObject *
    VSOPeer::getSharedObject (id_t obj_id)
    {
        map<id_t, VSOSharedObject>::iterator it = _objects.find (obj_id);

        if (it == _objects.end ())
        {
            printf ("VSOPeer::getSharedObject () object [%llu] not found\n", obj_id);
            return NULL;
        }
        else
            return &it->second;
    }

    // get the number of objects I own
    int 
    VSOPeer::getOwnedObjectSize ()
    {
        int size = 0;

        for (map<id_t, VSOSharedObject>::iterator it = _objects.begin (); it != _objects.end (); it++)
            if (it->second.is_owner)
                size++;

        return size;
    }

    // check if I'm the owner of an object
    bool 
    VSOPeer::isOwner (id_t obj_id)
    {
        map<id_t, VSOSharedObject>::iterator it = _objects.find (obj_id);

        if (it == _objects.end ())
            return false;

        return it->second.is_owner;
    }

    // get the center of all current agents I maintain
    bool
    VSOPeer::getLoadCenter (Position &center)
    {
        if (_objects.size () == 0)
            return false;

        center.set (0, 0, 0);

        // NOTE/BUG if all coordinates are large, watch for overflow
        map<id_t, VSOSharedObject>::iterator it = _objects.begin ();
                
        int own_objs = 0;
        for (; it != _objects.end (); it++)
        {
            VSOSharedObject &so = it->second;

            // only owned objects are considered as load (for now)
            if (so.is_owner)
            {
                own_objs++;
                center.x += so.aoi.center.x; 
                center.y += so.aoi.center.y;
            }
        }

        if (own_objs == 0)
            return false;
        
        center.x /= own_objs;
        center.y /= own_objs;
        
        return true;
    }

    bool 
    VSOPeer::isLegalPosition (const Position &pos, bool include_self)
    {        
        /*
        // check for redundency
        for (size_t i=0; i < _legal_pos.size (); i++)
            if (_legal_pos[i] == pos)
                return false;    
        

        // check if position is out of map
        if (pos.x < 0.0 || pos.y < 0.0 || pos.x >= _para.world_width || pos.y >= _para.world_height)
            return false;
        */

        // loop through all enclosing nodes
        // make sure we are somewhat far from it
        
        for (size_t i=0; i < _neighbors.size (); i++)            
        {
            Node *node = _neighbors[i];

            if (!include_self && node->id == _self.id)
                continue;

            // check if the position to check is somewhat disant to all known neighbors
            //if (pos.distance (it->second.aoi.center) < (_para.default_aoi / 2))
            if (pos.distance (node->aoi.center) < 5)
                return false;
        }
        
        return true;
    }

    // find the closest enclosing neighbor to a given position (other than myself)
    // returns 0 for no enclosing neighbors
    id_t 
    VSOPeer::getClosestEnclosing (Position &pos)
    {
        // if we know no neighbors
        if (_neighbors.size () <= 1)
            return 0;
        // if only one other neighbor exist, return it
        else if (_neighbors.size () == 2)
            return _neighbors[1]->id;

        // assume first neighbor as closest
        id_t closest = _neighbors[1]->id;       
        double shortest_dist =  pos.distance (_neighbors[1]->aoi.center);
        double dist;
       
        // TODO: go through only enclosing neighbors? (right now it's all neighbors)
        for (size_t i=2; i < _neighbors.size (); i++)
        {
            id_t id = _neighbors[i]->id;

            if ((dist = pos.distance (_neighbors[i]->aoi.center)) < shortest_dist)
            {
                shortest_dist = dist;
                closest = id;
            }
        }
       
        return closest;
    }

    // get a list of neighbors whose regions are covered by the specified AOI
    // NOTE: we also add the option to include closest neighbor so all objects 
    //       will be backup to at least one neighbor node
    bool 
    VSOPeer::getOverlappedNeighbors (Area &aoi, vector<id_t> &list, id_t closest)
    {
        list.clear ();

        for (size_t i=0; i < _neighbors.size (); i++)
        {
            id_t id = _neighbors[i]->id;

            // skip self
            if (id == _self.id)
                continue;
           
            // NOTE we use the accurate mode of overlap test, so that 
            //      AOI indeed needs to overlap at least partially with the region
            if ((closest != 0 && id == closest) || 
                _Voronoi->overlaps (id, aoi.center, aoi.radius + VSO_AOI_OVERLAP_BUFFER, true))
                list.push_back (id);          
        }
        
        return (list.size () > 0);
    }

} // end namespace Vast
