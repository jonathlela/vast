


#include "VASTMatcher.h"
#include "MessageQueue.h"


using namespace Vast;

namespace Vast
{   

    VASTMatcher::VASTMatcher (int overload_limit)
            :MessageHandler (MSG_GROUP_VAST_MATCHER), 
             _state (ABSENT),
             _VONpeer (NULL),
             _overload_limit (overload_limit),
             _load_counter (0),
             _tick (0)
    {
    }

    VASTMatcher::~VASTMatcher ()
    {
    }

    // join the matcher mesh network with a given location
    bool 
    VASTMatcher::join (const Position &pos)
    {
        // avoid redundent join for a given arbitrator
        if (_VONpeer != NULL)
            return false;

        // we use the hostID of the matcher as the ID in the matcher VON
        _VONpeer = new VONPeer (_self.id, this);
        
        // NOTE: use a small default AOI length as we only need to know enclosing matchers
        _self.aoi.center = pos;
        _self.aoi.radius = 5;
        _newpos = _self;

        // we assume that a VAST node at the gateway can access the gateway VON node, 
        // so a joining VON node can reach a gateway VON node (via the gateway VAST node) 
        Node VON_gateway (_gateway.host_id, 0, _self.aoi, _gateway);
        _VONpeer->join (_self.aoi, &VON_gateway);

        _state = JOINING;

        return true;
    }

    // leave the matcher overlay
    bool 
    VASTMatcher::leave ()
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

    // get the current node's information
    Node * 
    VASTMatcher::getSelf ()
    {
        return &_self;
    }

    bool 
    VASTMatcher::isJoined()
    {
        return (_state == JOINED);
    }

    // set the gateway node for this world
    bool
    VASTMatcher::setGateway (const IPaddr &gatewayIP)
    {
        // convert possible "127.0.0.1" to actual IP address
        IPaddr gateway = gatewayIP;
        if (_net->validateIPAddress (gateway) == false)
            return false;
      
        // record gateway first
        _gateway = Addr (VASTnet::resolveHostID (&gateway), &gateway);

        return true;
    }

    // whether I'm a gateway node
    bool 
    VASTMatcher::isGateway ()
    {
        return (_gateway.host_id == _self.id);
    }

    /*   
    // get a particular peer's info
    Node *
    VASTMatcher::getPeer (id_t peer_id)
    {
        if (_peers.find (peer_id) != _peers.end ())
            return _peers[peer_id]->getSelf ();
        else
            return NULL;
    }

    // obtain a list of peers hosted on this matcher
    map<id_t, VONPeer *>& 
    VASTMatcher::getPeers ()
    {
        return _peers;
    }

    // get # of peers hosted at this relay, returns NULL for no record (non-relay)
    StatType *
    VASTMatcher::getPeerStat ()
    {
        if (_relay_id != _self.id)
            return NULL;

        _peerstat.calculateAverage ();
        return &_peerstat;
    }

    // get the neighbors for a particular peer
    // returns NULL if the peer does not exist
    vector<Node *> *
    VASTMatcher::getPeerNeighbors (id_t peer_id)
    {
        if (_peers.find (peer_id) != _peers.end ())
            return &_peers[peer_id]->getNeighbors ();
        else
            return NULL;
    }
    */

    // obtain access to Voronoi class (usually for drawing purpose)
    // returns NULL if the peer does not exist
    Voronoi *
    VASTMatcher::getVoronoi ()
    {
        if (_VONpeer != NULL)
            return _VONpeer->getVoronoi ();
        else
            return NULL;
    }



    //
    // VONNetwork
    //

    // send messages to some target nodes
    // returns number of bytes sent
    size_t 
    VASTMatcher::sendVONMessage (Message &msg, bool is_reliable, vector<id_t> *failed_targets)
    {
        // TODO: make return value consistent
        msg.reliable = is_reliable;
        return (size_t)sendMessage (msg, failed_targets);
    }

    // obtain next message in queue
    // return pointer to Message, or NULL for no more message
    Message* 
    VASTMatcher::receiveVONMessage (timestamp_t &senttime)
    {
        return NULL;
    }

    // notify the network layer of nodeID -> Address mapping
    bool 
    VASTMatcher::notifyAddressMapping (id_t node_id, Addr &addr)
    {
        return notifyMapping (node_id, &addr);
    }
    
    // get the IP address of current host machine
    Addr &
    VASTMatcher::getHostAddress ()
    {
        return _net->getHostAddress ();
    }

    // get current physical timestamp
    timestamp_t 
    VASTMatcher::getTimestamp ()
    {
        return _net->getTimestamp ();
    }

    // perform initialization tasks for this handler (optional)
    void 
    VASTMatcher::initHandler ()
    {
        _self.id        = _net->getHostID ();
        _self.addr      = _net->getHostAddress ();
    }

    // handler for various incoming messages
    // returns whether the message was successfully handled
    bool 
    VASTMatcher::handleMessage (Message &in_msg)
    {
        if (_state == ABSENT)
            return false;

        // store the app-specific message type if exists
        msgtype_t app_msgtype = APP_MSGTYPE(in_msg.msgtype);
        in_msg.msgtype = VAST_MSGTYPE(in_msg.msgtype);

#ifdef DEBUG_DETAIL
        if (in_msg.msgtype < VON_MAX_MSG)
            ; //printf ("[%d] VASTMatcher::handleMessage from: %d msgtype: %d, to be handled by VONPeer, size: %d\n", _self.id, in_msg.from, in_msg.msgtype, in_msg.size);            
        else
            printf ("[%d] VASTMatcher::handleMessage from: %d msgtype: %d appmsg: %d (%s) size: %d\n", _self.id, in_msg.from, in_msg.msgtype, app_msgtype, VAST_MESSAGE[in_msg.msgtype-VON_MAX_MSG], in_msg.size);
#endif

        switch (in_msg.msgtype)
        {

        // subscription request for an area
        case SUBSCRIBE:
            {               
                // check if the requester has already made a request to subscribe
                if (_subscriptions.find (in_msg.from) != _subscriptions.end ())
                {
                    printf ("VASTMatcher::handleMessage () SUBSCRIBE subscription already made by host: [%lld]\n", in_msg.from);
                    break;
                }

                Subscription sub;
                in_msg.extract (sub);

                Voronoi *voronoi = _VONpeer->getVoronoi ();

                // find the closest neighbor and forward
                id_t closest = voronoi->closest_to (sub.aoi.center);

                // check if we should accept this subscription or forward
                if (voronoi->contains (_self.id, sub.aoi.center) || 
                    closest == _self.id)
                {

                    // store which host requests for the subscription
                    sub.host_id = in_msg.from;

                    // assign a unique subscription number                    
                    sub.id = _net->getUniqueID (ID_GROUP_VON_VAST);

                    addSubscription (sub);

                    // send back acknowledgement of subscription to client
                    Message msg (SUBSCRIBE_R);
                    msg.msggroup = MSG_GROUP_VAST_CLIENT;
                    msg.priority = 1;

                    // store both the assigned subscription ID, and also this matcher's address
                    // (so the client may switch the current matcher)
                    msg.store (sub.id);
                    msg.store (_self.addr);
                    msg.addTarget (in_msg.from);
                    sendMessage (msg);
                }
                else
                {
                    // forward the message to neighbor closest to the subscribed point
                    in_msg.reset ();
                    in_msg.targets.clear ();
                    in_msg.addTarget (closest);                         

                    sendMessage (in_msg);
                }

                /*
                // only accept either a self request
                // or a previously admitted JOIN request
                if (in_msg.from == _self.id) //|| _accepted.find (in_msg.from) != _accepted.end ())                
                {
                    if (addSubscription (in_msg.from, sub_no, area, layer) == true)
                        _host2peer[in_msg.from] = sub_no;
                }
                */
            }
            break;

        // move an existing subscription area to a new place
        case MOVE:
        case MOVE_F:
            {                
                // extract subscripton id first
                id_t sub_id;
                in_msg.extract (sub_id);

                map<id_t, Subscription>::iterator it = _subscriptions.find (sub_id);

                // modify the position of currently subscribed peer accordingly
                // TODO: check last update time?
                if (it != _subscriptions.end ())
                {                                  
                    timestamp_t send_time = 0;  // time of sending initial MOVE

#ifdef VAST_RECORD_LATENCY
                    in_msg.extract (send_time);
#endif

                    // extract position first
                    Area &new_aoi = it->second.aoi;

                    VONPosition pos;
                    in_msg.extract ((char *)&pos, sizeof (VONPosition));
                    new_aoi.center.x = pos.x;
                    new_aoi.center.y = pos.y;

                    // if radius is also updated
                    if (in_msg.msgtype == MOVE_F)
                    {
                        in_msg.extract ((char *)&new_aoi.radius, sizeof (length_t));
                        in_msg.extract ((char *)&new_aoi.height, sizeof (length_t));
                    }
                    
                    // record time of movement of the subscribers
                    it->second.time = send_time;
                    
                    // perform movement?
                    //_peers[sub_no]->move (new_aoi, send_time);
                }
            }
            break;

        case PUBLISH:
            {
                in_msg.msgtype = (app_msgtype << VAST_MSGTYPE_RESERVED) | MESSAGE;

                // extract publication layer (from end of message)
                layer_t layer;
                Area    area;
                
                in_msg.extract (layer, true);
                in_msg.extract (area, true);

                in_msg.reset ();

                // check through known subscribers
                // TODO: forward publication to neighbors for area publication, or 
                //       for subscribers hosted by neighboring matchers
                in_msg.targets.clear ();

                // the layer of the receiver
                //layer_t target_layer;
        
                // find the peer that can act as initial point for this publication 
                // TODO: find a more efficient / better way

                map<id_t, Subscription>::iterator it = _subscriptions.begin ();               
                for (; it != _subscriptions.end (); it++)
                {                
                    Subscription &sub = it->second;

                    // if the neighbor is 
                    //      1) at the same layer as publisher
                    //      2) interested in this publication
                    // then add as target
                    if (layer == sub.layer && sub.aoi.overlaps (area.center))
                        in_msg.addTarget (sub.id);
                }                

                if (in_msg.targets.size () > 0)
                {
                    vector<id_t> failed_targets;

                    in_msg.msggroup = MSG_GROUP_VAST_CLIENT;
                    sendMessage (in_msg, &failed_targets);

                    // some targets are invalid, remove the subscribers
                    // TODO: perhaps should check / wait? 
                    if (failed_targets.size () > 0)
                    {
                        printf ("VASTMatcher::handleMessage PUBLISH warning: not all publish targets are valid\n");
                       
                        // remove failed targets
                        for (size_t i=0; i < failed_targets.size (); i++)
                            removeSubscription (failed_targets[i]);
                    }
                }
#ifdef DEBUG_DETAIL
                else
                    printf ("VASTMatcher::handleMessage PUBLISH, no peer covers publication at (%d, %d)\n", (int)area.center.x, (int)area.center.y);
#endif
                   
            }
            break;
        
        // receiving a published message
        case MESSAGE:
            {

                // check sending to remote hosts
                // NOTE must send to remote first before local, as sendtime will be extracted by local host
                //      and the message structure will get changed
                in_msg.msgtype = (app_msgtype << VAST_MSGTYPE_RESERVED) | MESSAGE;
                in_msg.msggroup = MSG_GROUP_VAST_CLIENT;
                sendMessage (in_msg);

                // we should not have any local targets, as matchers receiving this message
                // can only be relays 

                /*
                // check if the message is targeted locally
                if (to_local)
                {
                    // restore app-specific message type
                    in_msg.msgtype = app_msgtype;
                    
#ifdef VAST_RECORD_LATENCY
                    timestamp_t sendtime;
                    in_msg.extract (sendtime, true);
                    recordLatency (PUBLISH, sendtime);
#endif

                    in_msg.reset ();
                    storeMessage (in_msg);
                }
                */
            }
            break;

        // process universal VASTnet message, msgtype = 0
        case DISCONNECT:
            {   
                // NOTE: the disconnecting host may be either a regular client
                //       or a matcher host. In the latter case, we should also
                //       notify the VONpeer component of its departure

                // removing a simple client (subscriber)
                // NOTE that this will work because MessageQueue will put all associated sub_id for the 
                //      disconnecting host as 'in_msg.from' when notifying the DISCONNECT event
                removeSubscription (in_msg.from);

                // if the host is a matcher, notify the VONPeer component
                in_msg.msgtype = VON_DISCONNECT;
                _VONpeer->handleMessage (in_msg);

            }
            break;

        // assume that this is a message for VONPeer
        default:            
            {
                // check if the message is directed towards the VONpeer
                if (in_msg.msgtype < VON_MAX_MSG)
                    _VONpeer->handleMessage (in_msg);

                /*
                // ignore messages not directly towards VONPeers
                if (VASTnet::extractIDGroup (in_msg.from) != ID_GROUP_VON_VAST)
                    break;

                // check through the targets and deliver to each VONPeer affected            
                for (size_t i=0; i != in_msg.targets.size (); i++)
                {
                    id_t target = in_msg.targets[i];
                    if (_peers.find (target) == _peers.end ())
                    {
#if _DEBUG
                        printf ("VASTMatcher::handleMessage () cannot find the message's target peer [%d]\n", target);
#endif
                        continue;
                    }
                    _peers[target]->handleMessage (in_msg);

                    // it's very important to reset the current counter, so that extraction may still work
                    in_msg.reset ();
                }
                */
            }
            break;
        }
        
        return true;
    }

    // perform routine tasks after all messages have been handled 
    //  (i.e., check for reply from requests sent)
    void 
    VASTMatcher::postHandling ()
    {
        // perform regular tick and check if our VONpeer has joined
        if (_VONpeer == NULL)
            return;

        _VONpeer->tick ();

        if (_VONpeer->isJoined () == false)
            return;

        // make sure this matcher has properly notify gateway of its join
        checkMatcherJoin ();

        // perform per-second tasks
        if (++_tick == _net->getTickPerSecond ())
        {
            _tick = 0;

            // report loading to neighbors
            //reportLoading ();

            // move matcher to new position
            moveMatcher ();

        }

        // update the list of neighboring matchers
        updateMatchers ();

        // check to call additional matchers for load sharing
        //checkOverload ();

        // check to see if subscriptions have migrated 
        transferOwnership ();

        // collect stat periodically
        // TODO: better way than to pass _para?
        //if (_tick % (_net->getTickPerSecond () * STAT_REPORT_INTERVAL_IN_SEC) == 0)
        //    reportStat ();

        // TODO: adjust client_limit based on resource (bandwidth) availability

        // discover for each subscriber other neighbors
        updateSubscriptionNeighbors ();
                   
        // send neighbor updates to VASTClients        
        notifyClients ();
    }

    // create a new Peer instance at this VASTMatcher
    bool 
    //VASTMatcher::addSubscription (id_t fromhost, id_t sub_no, Area &area, layer_t layer)
    VASTMatcher::addSubscription (Subscription &sub)
    {      
        // perhaps we should simply replace it?
        // avoid redundency
        if (_subscriptions.find (sub.id) != _subscriptions.end ())
            return false;
                
        // store layer info 
        // TODO: right now this is a hacked solution, just use whatever available space
        sub.relay.publicIP.pad = (unsigned short)sub.layer;

        // record the subscription
        _subscriptions[sub.id] = sub;

        // notify network layer of the subscriberID -> relay hostID mapping
        notifyMapping (sub.id, &sub.relay);

        /*
        // we assume that a VAST node at the gateway has access to the gateway VON peer 
        // so that a joining VON node can reach a gateway VON peer (via the gateway VAST node)
        Node VON_gateway (((MessageQueue *)_msgqueue)->getGatewayID (ID_GROUP_VON_VAST), 0, area, getGateway ());
           
        peer->join (area, &VON_gateway);
        _peers[sub_no] = peer;
        _peer_state[sub_no] = JOINING;
        _peer2host[sub_no] = fromhost;
        */

        return true;
    }

    bool 
    VASTMatcher::removeSubscription (id_t sub_no)
    {
        if (_subscriptions.find (sub_no) == _subscriptions.end ())
            return false;

        _subscriptions.erase (sub_no);

        /*
        _peer_state.erase (sub_no);
        _peer2host.erase (sub_no);
        */
        
        return true;
    }

    // update the neighbors for each subscriber
    // 'neighbor' is defined as other clients subscribing at the same layer & within AOI
    void
    VASTMatcher::updateSubscriptionNeighbors ()
    {    
        // nodes within the Voronoi        
        Voronoi *voronoi = _VONpeer->getVoronoi ();

        // clear visible neighbor states for all subscribers
        map<id_t, Subscription>::iterator it = _subscriptions.begin (); 
        for (; it != _subscriptions.end (); it++)
        {
            it->second.clearStates ();

            // build up a map for subscribers within the Voronoi region of this matcher
            if (voronoi->contains (_self.id, it->second.aoi.center))
                it->second.in_region = true;
        }

        // loop through all known neighbors, and check against all other known subscribers
        // for visibility
        
        it = _subscriptions.begin (); 

        for (; it != _subscriptions.end (); it++)
        {
            Subscription &sub1 = it->second;

            // check each other subscriber till the end of list
            map<id_t, Subscription>::iterator it2 = it;
            it2++;                                    

            for (; it2 != _subscriptions.end (); it2++)
            {
                // check for overlap separately for the two subscribers                
                // NOTE: that if layer 0 is subscribed, then all known subscribers within the Voronoi region of this matcher are reported
                Subscription &sub2 = it2->second;

                // if subscriber 1 can see subscriber 2
                if ((sub1.layer == 0 && sub2.in_region == true) || 
                    (sub1.layer == sub2.layer && sub1.aoi.overlaps (sub2.aoi.center)))
                    sub1.addNeighbor (&sub2);

                // if subscriber 2 can see subscriber 1
                if ((sub2.layer == 0 && sub1.in_region == true) || 
                    (sub2.layer == sub1.layer && sub2.aoi.overlaps (sub1.aoi.center)))
                    sub2.addNeighbor (&sub1);
            }

            // add self (NOTE: must add self before clearing neighbors, or self will be cleared)
            sub1.addNeighbor (&sub1);

            // remove neighbors no longer within AOI
            sub1.clearInvisibleNeighbors ();

        }

        /*
        for (map<id_t,VONPeer *>::iterator it = _peers.begin (); it != _peers.end (); it++)
        {
            id_t sub_no = it->first;

            if (it->second->isJoined () && _peer_state[sub_no] == JOINING)
            {
                // this Peer has successfully joined the VON and is now subscribing
                _peer_state[sub_no] = JOINED;
                
                // send a SUBSCRIBE_REPLY back to the corresponding Client
                Message msg (SUBSCRIBE_REPLY);
                msg.priority = 1;
                msg.store (sub_no);
                msg.addTarget (_peer2host[sub_no]);
                sendMessage (msg);
            }        
        }

        // record simple stats on subscribers
        if (_peers.size () > _peerstat.maximum)
            _peerstat.maximum = _peers.size ();

        if (_peers.size () > 0)
        {
            _peerstat.total += _peers.size ();
            _peerstat.num_records++;
        }
        */
        
    }
   
    void
    VASTMatcher::checkMatcherJoin ()
    {
        // check whether the matcher has properly notify gateway of its join
        if (_state == JOINING)
        {
            // if I'm not the gateway)
            if (isGateway () == false)
            {
                // notify gateway that I'll be joining as a matcher
                // (so remove me from waiting list or potential list)
                Message msg (MATCHER_JOINED);
                msg.priority = 1;
                msg.store (_self.id);
                msg.addTarget (_gateway.host_id);
                sendMessage (msg);
            }

            _state = JOINED;
        }
    }

    // change position of this arbitrator in response to overload signals
    void 
    VASTMatcher::moveMatcher ()
    {
       // move myself towards the center of agents
        Position center;
        if (getSubscriptionCenter (center))
            _newpos.aoi.center += ((center - _newpos.aoi.center) * MATCHER_MOVEMENT_FRACTION);

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

    // check with VON to refresh current neighboring matchers
    void 
    VASTMatcher::updateMatchers ()
    {
        /*
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
        */
    }    

    // check to call additional matchers for load balancing
    void 
    VASTMatcher::checkOverload () 
    {
        int n = _subscriptions.size ();

        // collect stats on # of agents 
        if (n > 0)
            _stat_sub.addRecord (n); 

        // check if no limit is imposed
        if (_overload_limit == 0)
            return;

        // if # of subscriptions exceed limit, notify for overload
        else if (n > _overload_limit)
            notifyLoading (n);
        
        // underload
        // TODO: if UNDERLOAD threshold is not 0,
        // then we need proper mechanism to transfer subscription out before matcher departs
        else if (n == 0)
            notifyLoading (-1);
        
        // normal loading
        else
            notifyLoading (0);
    }


    // Arbitrator overloaded, call for help
    // note that this will be called continously until the situation improves
    void
    VASTMatcher::notifyLoading (int status)
    {                               
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

        /*
        // if overload persists, we try to insert new matcher
        if (_load_counter > TIMEOUT_OVERLOAD_REQUEST * _net->getTickPerSecond ())
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
                
                //leave ();

                // notify gateway that I'm available again
                //Message msg (ARBITRATOR_C);
                //msg.priority = 1;
                //msg.store (_self);
                //msg.addTarget (NET_ID_GATEWAY);
                //sendMessage (msg);
                

                _overload_requests = 0;
            }
                    
            _load_counter = 0;
        }
        // normal loading, reset # of OVERLOAD_M requests
        else if (_load_counter == 0)
            _overload_requests = 0;
        */
    }

    // see if any of my objects should transfer ownership
    // or if i should claim ownership to any new objects (if neighboring arbitrators fail)
    int 
    VASTMatcher::transferOwnership ()
    {        
        if (_VONpeer == NULL || _state != JOINED)
            return 0;
        
        
        // get a reference to the Voronoi object and my VON id
        Voronoi *voronoi = _VONpeer->getVoronoi ();
        id_t VON_selfid  = _VONpeer->getSelf ()->id;

        int num_transfer = 0;
        /*
        
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

                            
                            // TODO: notify other arbitrators to remove ghost objects?
                            // remove closest & send to all other neighbors
                            // causes crash? 

                            
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
                        //
                        // notify avatar object's agent's of its new arbitrator
                        if (obj->agent != 0)
                        {                                                        
                            Message msg (REJOIN);
                            msg.priority = 1;
                            msg.addTarget (obj->agent);
                            msg.msggroup = MSG_GROUP_VASTATE_AGENT;                        
                            sendAgent (msg);                        
                        } 
                        
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


        // check & reclaim ownership of in-tranit objects
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


    // tell clients updates of their neighbors (changes in other nodes subscribing at same layer)
    void
    VASTMatcher::notifyClients ()
    {
        // check over updates on each Peer's neighbors and notify the 
        // respective Clients of the changes (insert/delete/update)        
        Message msg (NEIGHBOR);
        msg.priority = 1;
        msg.msggroup = MSG_GROUP_VAST_CLIENT;

        Node node;      // neighbor info

        map<id_t, Subscription>::iterator it = _subscriptions.begin (); 
        for (; it != _subscriptions.end (); it++)
        {
            id_t sub_id  = it->first;
            Subscription &subscriber = it->second;
                            
            map<id_t, NeighborUpdateStatus>& update_status = subscriber.getUpdateStatus ();
            
            if (update_status.size () == 0)
                continue;

            msg.clear (NEIGHBOR); 
            msg.msggroup = MSG_GROUP_VAST_CLIENT;
            //msg.store (listsize);

            listsize_t listsize = 0;
            // loop through each neighbor for this peer and record its status change
            map<id_t, NeighborUpdateStatus>::iterator itr = update_status.begin ();
            for (; itr != update_status.end (); itr++)
            {               
                // store each updated neighbor info into the notification messages
                NeighborUpdateStatus status = itr->second;

                // NOTE: we do not update nodes that have not changed
                // TODO: but send periodic keep-alive?
                if (status == UNCHANGED)
                    continue;
                
                listsize++;
                id_t neighbor_id = itr->first;
                
                // obtain neighbor info if it's not a deleted neighbor
                if (status != DELETED)
                {
                    map<id_t, Subscription>::iterator it_neighbor = _subscriptions.find (neighbor_id);
                                    
                    // change neighbor info to DELETED if not exist (something's wrong here)
                    if (it_neighbor == _subscriptions.end ())
                    {
                        printf ("neighbor [%llu] cannot be found when sending neighbor update\n", neighbor_id); 
                        status = DELETED;
                    }
                    else
                    {
                        Subscription &neighbor = it_neighbor->second;
                
                        // convert subscriber info to node info
                        node.id     = neighbor.id;
                        node.addr   = neighbor.relay;
                        node.aoi    = neighbor.aoi;
                        node.time   = neighbor.time;
                    }
                }

                msg.store (neighbor_id);
                msg.store ((char *)&status, sizeof (NeighborUpdateStatus));

                // store actual state update
                switch (status)
                {
                case INSERTED:
                    msg.store (node);
                    break;
                case DELETED:
                    break;
                case UPDATED:
                    //msg.store (*node);
                    // TODO: store only center of AOI (instead of radius too?)
                    msg.store (node.aoi);
#ifdef VAST_RECORD_LATENCY
                    msg.store (node.time);
#endif
                    break;
                }
            }

            // we send the update only if there are nodes to update
            if (listsize > 0)
            {
                // store number of entries at the end
                msg.store (listsize);
                
                //id_t target = _peer2host[peer_id];
                msg.addTarget (sub_id);            
                sendMessage (msg);
            }

            // at the end we clear the status record for next time-step
            update_status.clear ();

        } // end for each subscriber      

    }


    // get the center of all current agents I maintain
    bool
    VASTMatcher::getSubscriptionCenter (Position &sub_center)
    {
        if (_subscriptions.size () == 0)
            return false;

        sub_center.set (0, 0, 0);

        // NOTE/BUG if all coordinates are large, watch for overflow
        map<id_t, Subscription>::iterator it = _subscriptions.begin ();
                
        for (; it != _subscriptions.end (); it++)
        {
            sub_center.x += it->second.aoi.center.x; 
            sub_center.y += it->second.aoi.center.y;
        }

        sub_center.x /= _subscriptions.size ();
        sub_center.y /= _subscriptions.size ();

        return true;
    }


} // end namespace Vast
