

#include "VSOPeer.h"

using namespace Vast;

namespace Vast
{   

    // default constructor
    // NOTE that the VONPeer we create, the strict AOI flag is set to 'false',
    //      so that if the VSOPeer's AOI overlaps slightly with a neighbor's region, 
    //      the neighbor will be considered as AOI neighbor
    VSOPeer::VSOPeer (id_t id, VONNetwork *net, VSOPolicy *policy, length_t aoi_buffer)
        :VONPeer (id, net, aoi_buffer, true), 
         _policy (policy),          
         _overload_count (0)
    {
        _newpos = _self;

        //notifyCandidacy ();

        _next_periodic = 0;
        _overload_timeout = _underload_timeout = 0;

        _vso_state = JOINING;
    }

    // default destructor
    VSOPeer::~VSOPeer ()
    {
    }

    // perform joining the overlay
    void 
    VSOPeer::join (Area &aoi, Node *origin, bool is_static)
    {
        // NOTE: it's important to set newpos correctly so that self-adjust of center can be correct
        _newpos.aoi = aoi;
        VONPeer::join (aoi, origin);
        _is_static = is_static;

        // record the origin (so later VSO_INSERT request can be attached with the origin info)
        _origin = *origin;
    }

    bool 
    VSOPeer::handleMessage (Message &in_msg)    
    {
        // if join is not even initiated, do not process any message 
        //if (_state != JOINED)
        //    return false;

        switch ((VSO_Message)in_msg.msgtype)
        {

        /*
        // registration of a potential node with the gateway
        case VSO_CANDIDATE:
            if (isGateway (_self.id))
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
        */
                         
        // Overloaded node's request for inserting a new helper
        case VSO_INSERT:                      
            // only the gateway can process it
            if (_self.id != _policy->getGatewayID ())
            {
                printf ("[%llu] VSOPeer::handleMessage () VSO_INSERT received by non-gateway\r\n", _self.id);
            }
            else
            {
                float level;        // level of work load
                Position join_pos;  // joining position of the new node
                Node origin;        // the origin node of the requesting overlay

                in_msg.extract (level);
                in_msg.extract (join_pos);
                in_msg.extract (origin);

                // TODO: ignore redundent requests at same position
                
                // promote one of the spare potential nodes
                Addr new_node;

                // TODO: findCandidate () would always be successful (if no candidate found, then create gateway node)
                // return from loop either request successfully served (promotion sent) 
                // or no candidates can be found
                while (_policy->findCandidate (new_node, level))
                {
                    // fill in the stressed node's contact info & join location
                    Node requester;
                    
                    requester.id = in_msg.from;
                    requester.time = _net->getTimestamp ();
                    requester.aoi.center = join_pos;

                    Message msg (VSO_PROMOTE);
                    msg.priority = 1;
                    msg.store (join_pos);
                    msg.store (origin);

                    // send promotion message
                    _net->notifyAddressMapping (new_node.host_id, new_node);
                    msg.addTarget (new_node.host_id);

                    // record the promoted position if sent success
                    if (_net->sendVONMessage (msg) > 0)   
                    {
                        _promote_requests[new_node.host_id] = requester;
                        break;
                    }
                }
            }            
            break;
        
        // a candidate node gets promoted as an actual node
        case VSO_PROMOTE:        
            if (_state == ABSENT)
            {
                // "origin" always needs to be sent so that we know the proper initial contact of the overlay
                Position join_pos;
                Node origin;
                in_msg.extract (join_pos);
                in_msg.extract (origin);
               
                Area aoi (join_pos, 5);

                // join at the specified position, but only if I'm available
                join (aoi, &origin);

                printf ("VSO_PROMOTE: promoted to join at (%f, %f) for origin [%llu]\n", join_pos.x, join_pos.y, origin.id);
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
                //temp_pos += (((neighbor_pos - temp_pos) * VSO_MOVEMENT_FRACTION) * level);
                temp_pos += ((neighbor_pos - temp_pos) * VSO_MOVEMENT_FRACTION);

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

                // TODO: record the joining time for logging total nodes in system?
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
                _policy->copyObject (in_msg.from, list, false);
            }
            break;

        case VSO_DISCONNECT:
            {
                // claim ownership of objects managed by departing peer
                // by setting the reclaim counter to 0 for all objects managed by departing matcher                  
                map<id_t, VSOSharedObject>::iterator it = _objects.begin ();
                        
                for (; it != _objects.end (); it++)
                {                    
                    // record all objects the departing matcher may be managing
                    if (it->second.closest == in_msg.from)
                        _reclaim_timeout[it->first] = 0;
                }

                // then notify VON component to remove the departing peer
                in_msg.msgtype = VON_DISCONNECT;
                VONPeer::handleMessage (in_msg);
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
        timestamp_t now = _net->getTimestamp ();

        // first adjust the current load level record
        if (level == 0)
        {
            // normal loading
            _overload_timeout = 0;
            _underload_timeout = 0;
            return;
        }
        else if (level < 0)
        {
            // underload      
            if (_underload_timeout == 0)
                _underload_timeout = now + (VSO_TIMEOUT_OVERLOAD_REQUEST * _net->getTimestampPerSecond ());
            _overload_timeout = 0;
        }
        else
        {   
            // overload
            _underload_timeout = 0;
            if (_overload_timeout == 0)
                _overload_timeout = now + (VSO_TIMEOUT_OVERLOAD_REQUEST * _net->getTimestampPerSecond ());
        }

        // if overload continues, we try to do something
        if (_overload_timeout != 0 && (now >= _overload_timeout))
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
                    msg.store (level);
                    msg.store (pos);
                    msg.store (_origin);
                    msg.addTarget (_policy->getGatewayID ());
                    _net->sendVONMessage (msg);    
                }

                _overload_count = 0;
            }

            _overload_timeout = 0;
        }
        // underload event
        else if (_underload_timeout != 0 && (now >= _underload_timeout))
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
            else if (isGateway (_self.id) == false)
            {                
                // depart as node if loading is below threshold                
                leave ();

                //notifyCandidacy ();
                _overload_count = 0;
            }
                    
            _underload_timeout = 0;
        }
        // normal loading, reset # of OVERLOAD_M requests
        else if (_overload_timeout == 0 && _underload_timeout == 0)
            _overload_count = 0;        
    }

    // change the center position in response to overload signals
    void 
    VSOPeer::movePeerPosition ()
    {
        // if nothing has changed
        if (_newpos.aoi.center == _self.aoi.center)
            return;

        // check if AOI should be enlarged / reduced
        adjustPeerRadius ();

        // performe actual movement
        this->move (_newpos.aoi);

        // update self info
        // NOTE: currently only position will change, not AOI
        _self.aoi.center = _newpos.aoi.center;

        // update other info (such as AOI) for the new position
        _newpos = _self;
    }

    // enlarge or reduce the AOI radius to cover all objects's interest scope
    void 
    VSOPeer::adjustPeerRadius ()
    {
        // go through all owned objects and find the radius that
        // covers them all
        map<id_t, VSOSharedObject>::iterator it = _objects.begin ();
        double dist = 0;
        double longest = 0;

        Position &center = _newpos.aoi.center;

        for (; it != _objects.end (); it++)
        {
            VSOSharedObject &so = it->second;

            // for either own or in-tranit objects
            if (so.is_owner || so.in_transit != 0)
            {
                // if the object's AOI is circular, we take distance + AOI radius
                if (so.aoi.height == 0)
                    dist = center.distance (so.aoi.center) + so.aoi.radius;
                // otherwise for rectangular AOI, we use the longest dimension (width or height)
                else
                    dist = center.distance (so.aoi.center) + (so.aoi.radius > so.aoi.height ? so.aoi.radius : so.aoi.height);
            
                if (dist > longest)
                    longest = dist;           
            }
        }

        // reflect the new AOI, with some buffer
        _newpos.aoi.radius = (length_t)(longest + VSO_PEER_AOI_BUFFER);

#ifdef DEBUG_DETAIL
        printf ("[%llu] VSOPeer::adjustPeerRadius () to (%.2f, %.2f) r: %u\n", _self.id, _newpos.aoi.center.x, _newpos.aoi.center.y, (unsigned)_newpos.aoi.radius);
#endif
    }

    // check if neighbors need to be notified of object updates
    // returns the # of updates sent
    // TODO: should we check whether the object states have changed?
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
        
                // if the AOI of this object overlaps into other VSOpeers
                // then send updates to these neighbors (that is, notify the neighbor VSOpeer of its subscription interest
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
            _policy->copyObject (itu->first, *itu->second, true);                                
            delete itu->second;
        }

        return num_updates;
    }

    // remove obsolete objects (those unowned objects no longer being updated)
    // and send keepalive for owned objects
    void 
    VSOPeer::refreshObjects ()
    {
        timestamp_t now = _net->getTimestamp ();
        timestamp_t remove_timeout = (timestamp_t)(VSO_TIMEOUT_AUTO_REMOVE * _net->getTimestampPerSecond ());
        //timestamp_t alive_timeout  = (timestamp_t)(VSO_TIMEOUT_AUTO_REMOVE / 3 * _net->getTimestampPerSecond ());

        vector<id_t> remove_list;        

        map<id_t, VSOSharedObject>::iterator it = _objects.begin ();
        for (; it != _objects.end (); it++)
        {
            VSOSharedObject &so = it->second;

            // if object is not owned by me and is not updated for a while, remove it
            if (so.is_owner == false)
            {
                if (now - so.last_update >= remove_timeout)
                    remove_list.push_back (it->first);
            }
            /*
            else
            {
                // if the object has not been updated in a while, auto-update it
                // this will ensure other neighbors to keep object replicas
                if (now - so.last_update >= alive_timeout)
                    updateSharedObject (it->first, so.aoi);
            }
            */
        }

        if (remove_list.size () > 0)
        {
#ifdef DEBUG_DETAIL
            printf ("VSOPeer::refreshObjects () removing objects: \n");
#endif

            // remove each obsolete object
            for (size_t i=0; i < remove_list.size (); i++)
            {
#ifdef DEBUG_DETAIL
                printf ("[%llu]\n", remove_list[i]);
#endif

                // deleteSharedObject should be called automatically
                _policy->removeObject (remove_list[i]);
            }
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
        map<id_t, vector<VSOOwnerTransfer> *> transfer_list;
        
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
                    // set the timeout time for ownership transfer
                    if (_transfer_timeout.find (obj_id) == _transfer_timeout.end ())
                        _transfer_timeout[obj_id] = now + (timestamp_t)(VSO_TIMEOUT_TRANSFER * _net->getTimestampPerSecond ());

                    // if timeout exceeds, then begin transfer to nearest node
                    else if (now >= _transfer_timeout[obj_id])
                    {
                        id_t closest = _Voronoi->closest_to (so.aoi.center);
                        
                        if (closest != _self.id)
                        {            
                            // record the transfer
                            VSOOwnerTransfer transfer (obj_id, closest, _self.id);

                            if (transfer_list.find (closest) == transfer_list.end ())
                                transfer_list[closest] = new vector<VSOOwnerTransfer>;

                            transfer_list[closest]->push_back (transfer);
                                                          
                            // we release ownership for now (but will still be able to process events via in-transit records)
                            // this is important as we can only transfer ownership to one neighbor at a time
    
                            so.is_owner = false;
                            so.in_transit = now + (timestamp_t)(VSO_TIMEOUT_TRANSFER * _net->getTimestampPerSecond () * 3); 

                            _transfer_timeout.erase (obj_id);
                        }                        
                    }
                }

                // if object is no longer in my region, remove reclaim counter
                else if (_reclaim_timeout.find (obj_id) != _reclaim_timeout.end ())
                    _reclaim_timeout.erase (obj_id);
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
                    if (_reclaim_timeout.find (obj_id) == _reclaim_timeout.end ())
                        _reclaim_timeout[obj_id] = now + (timestamp_t)(VSO_TIMEOUT_TRANSFER * _net->getTimestampPerSecond () * 5);
                
                    // we claim ownership if countdown is reached
                    if (now >= _reclaim_timeout[obj_id])
                    {
                        claimOwnership (obj_id, so);
                    }
                }

                // reset transfer timeout, if an object has moved out but moved back again
                else if (_transfer_timeout.find (obj_id) != _transfer_timeout.end ())
                {
                    _transfer_timeout.erase (obj_id);
                }
            }     

            // reclaim in-transit objects taking too long to complete
            if (so.in_transit != 0 && (now >= so.in_transit))
            {
                claimOwnership (obj_id, so);
            }
        }

        // TODO: need to make sure the above countdowns do not stay alive
        //       and errously affect future countdown detection

        // perform the actual transfer from list
        Message msg (VSO_TRANSFER);
        
        map<id_t, vector<VSOOwnerTransfer> *>::iterator it = transfer_list.begin ();

        int num_transfer = 0;
        for (; it != transfer_list.end (); it++)
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
            //       some tests show that copy object in full before ownership transfer helps to improve overall consistency at
            //       slight additional bandwidth
            _policy->copyObject (it->first, obj_list, false);
            _policy->ownershipTransferred (it->first, obj_list);

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
        if (_reclaim_timeout.find (transfer.obj_id) != _reclaim_timeout.end ())
            _reclaim_timeout.erase (transfer.obj_id);        
    }

    // make an object my own
    void 
    VSOPeer::claimOwnership (id_t obj_id, VSOSharedObject &so)
    {
        printf ("[%llu] VSOPeer::claimOwnership () for object [%llu]\n", _self.id, obj_id);

        so.is_owner = true;
        so.in_transit = 0;
        so.last_update = _net->getTimestamp ();

        _reclaim_timeout.erase (obj_id);

        // notify current node about it, so other actions can be taken (e.g., tell a client)
        _policy->objectClaimed (obj_id);
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

    /*
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

        //string str;
        //new_node.addr.toString (str);

        //printf ("\n[%llu] promoting [%llu] %s as new node\n\n", _self.id, new_node.id, str.c_str ());
        printf ("\n[%llu] promoting [%llu] as new node\n\n", _self.id, new_node.id);

        return true;
    }
    */

    /*
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
    */

    /*
    // check if a particular point is within our region
    bool 
    VSOPeer::inRegion (Position &pos)
    {
        return true;
    }
    */

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

            // notify the host for some joining processing, also notify the origin node's ID
            _policy->peerJoined (_origin.id);

            // notify gateway that I've joined (can remove me from request list)
            Message msg (VSO_JOINED);
            msg.priority = 1;
            msg.addTarget (_policy->getGatewayID ());
            _net->sendVONMessage (msg);
        }

        timestamp_t now = _net->getTimestamp ();

        // perform per-second tasks        
        if (now >= _next_periodic)
        {
            // reset timer
            _next_periodic = now + _net->getTimestampPerSecond ();

            // move towards the center of load (subscriptions)
            if (_is_static == false)
            {
                Position center;
                if (getLoadCenter (center))
                    _newpos.aoi.center += ((center - _newpos.aoi.center) * VSO_MOVEMENT_FRACTION);
            }

            // remove obsolete objects (those unowned objects no longer being updated)
            // and send keep alive for owned objects
            refreshObjects ();
        }

        // move the VSOPeer's position 
        movePeerPosition ();

        // check if we need to transfer ownership to neighboring region periodically
        checkOwnershipTransfer ();

        // check if we need to send object updates to neighboring regions
        // NOTE: must run *after* ownership transfer check, so only owned objects are checked
        checkUpdateToNeighbors ();
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
    VSOPeer::updateSharedObject (id_t obj_id, Area &aoi, bool *is_owner)
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

        // update ownership, if available
        if (is_owner != NULL)
            so.is_owner = *is_owner;

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

    // get the center of all current objects I maintain
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

    // check if I'm a joining peer (in progress)
    bool 
    VSOPeer::isJoining ()
    {
        return (_state == JOINING);
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
                _Voronoi->overlaps (id, aoi.center, aoi.radius + VSO_AOI_BUFFER_OVERLAP, true))
                list.push_back (id);          
        }
        
        return (list.size () > 0);
    }

} // end namespace Vast
