

#include "Relay.h"
#include "MessageQueue.h"

namespace Vast
{   
    char VAST_MESSAGE[][20] = 
    {
        "QUERY",
        "QUERY_REPLY",
        "JOIN",
        "JOIN_REPLY",
        "LEAVE",
        "RELAY",
        "SUBSCRIBE",
        "SUBSCRIBE_REPLY",
        "PUBLISH",        
        "MOVE", 
        "MOVE_F",
        "NEIGHBOR",
        "MESSAGE"
    };

    Relay::Relay (id_t host_id, int peer_limit, int relay_limit)
    {
        _self.id        = host_id;        
        _state          = ABSENT;   
        _relay_id       = NET_ID_UNASSIGNED;                  
        _lastmsg        = NULL;
        _peerlimit      = (size_t)peer_limit;    // if 0 means unlimited
        _relaylimit     = (size_t)relay_limit;   // if 0 means unlimited
        _query_timeout  = 0;
        _join_timeout   = TIMEOUT_JOIN;

        if (peer_limit < 0)
        {
            _peerlimit = 0;
            printf ("Relay::Relay () error, peer_limit < 0, set unlimited\n");
        }
        if (relay_limit < 0)
        {
            _relaylimit = 0;
            printf ("Relay::Relay () error, relay_limit < 0, set unlimited\n");
        }
    }

    Relay::~Relay ()
    {
        // release memeory
        if (_lastmsg != NULL)
            delete _lastmsg;

        size_t i;
        for (i=0; i < _msglist.size (); i++)
            delete _msglist[i];

        for (multimap<double, Node *>::iterator it = _relays.begin (); it != _relays.end (); it++)        
            delete it->second;

        for (map<id_t,VONPeer *>::iterator it = _peers.begin (); it != _peers.end (); it++)
            delete it->second;

        for (i=0; i < _neighbors.size (); i++)
            delete _neighbors[i];
    }

    // join the overlay (specifying a physical coordinate)
    bool
    Relay::join (Position &pos, bool as_relay)
    {        
        if (_state == JOINED)
            return true;

        // if my IP is not public, then not allowed to join as relay (by design)
        if (as_relay == true && _net->isPublic () == false)
        {
            printf ("Relay::join () this host does not have public IP, so it's not allowed to join as relay\n");
            as_relay = false;
        }

        if (_state == ABSENT)
        {            
            _self.aoi.center = pos;
            _self.addr       = getAddress (_self.id);
                   
            // if I'm the gateway, consider already joined
            if (_self.id == NET_ID_GATEWAY)
            {
                _state      = JOINED;
                _relay_id   = _self.id;

                return true;
            }

            // if I'm joining as relay, then my relay_id is myself
            if (as_relay)
                _relay_id = _self.id;

            _state = JOINING;
        }
        
        if (_state == JOINING)
        {            
            if (_query_timeout == 0)
            {
                // create a query node to find the initial relay with the current node's ID & position
                // BUG: still works in real network?
                Area a; 
                Node relay_node (NET_ID_GATEWAY, 0, a, getAddress (NET_ID_GATEWAY));
            
                // send query to the gateway to find the physically nearest node to join
                Message msg (QUERY);
                msg.priority = 1;
                msg.store (_self);
                msg.store (relay_node);
                msg.addTarget (NET_ID_GATEWAY);
                sendMessage (msg);

#ifdef DEBUG_DETAIL
                printf ("[%d] Relay::join () sending join QUERY request to gateway\n", _self.id);
#endif

                // set a timeout of re-joining 
                _query_timeout = TIMEOUT_QUERY;
            }
            else
                _query_timeout--;
        }
        
        return false;
    }

    // quit the overlay
    void        
    Relay::leave ()
    {
        if (_state != JOINED)
            return;

        // send a LEAVE message to my relay
        Message msg (LEAVE);
        msg.priority = 1;
        msg.addTarget (_relay_id);
        sendMessage (msg);
         
        _state = ABSENT;
    }
    
	// specify a subscription area for point or area publications 
    // returns a unique subscription number that represents subscribed area 
    id_t
    Relay::subscribe (Area &area, layer_t layer)
    {
        if (_state != JOINED)
            return 0;

        //id_t sub_no = _self.id;
        id_t sub_no = getUniqueID (ID_GROUP_VON_VAST);

        // record my subscription, not yet successfully subscribed        
        Subscription &sub = _sub_list[sub_no];
        
        sub.sub_id = sub_no;
        sub.aoi    = area;
        sub.layer  = layer;
        sub.active = false;

        // send out subscription request
        Message msg (SUBSCRIBE);
        msg.priority = 1;
        msg.store (sub_no);
        msg.store (area);
        msg.store (layer);
        msg.addTarget (_relay_id);
                
        sendMessage (msg); 

        // notify network so incoming messages can be processed by this host
        notifyMapping (sub_no, &_net->getHostAddress ());

        return sub_no;
    }

    // send a message to all subscribers within a publication area
    // TODO: a more efficient way to pack publication message?

    bool
    Relay::publish (Area &area, layer_t layer, Message &message)
    {
        if (_state != JOINED)
            return false;

        // make a copy 
        // (important as we cannot manipulate external message directly, 
        //  may cause access violation for memory allocated in a different library heap)
        Message msg (message);

        // modify the msgtype to indicate this is an app-specific message
        msg.msgtype = (msg.msgtype << VAST_MSGTYPE_RESERVED) | PUBLISH;
        
        msg.store (_net->getTimestamp ());      // to calculate PUBLISH latency
        msg.store (area);
        msg.store (layer);
        
        // send to relay for default processing       
        msg.addTarget (_relay_id);
        sendMessage (msg);

        return true;
    }

/*
    bool
    Relay::publish (Area &area, layer_t layer, Message &msg)
    {
        if (_state != JOINED)
            return false;

        // modify the msgtype to indicate this is an app-specific message
        msg.msgtype = (msg.msgtype << VAST_MSGTYPE_RESERVED) | MESSAGE;

        // check through known neighbors to find potential subscribers
        // TODO: consider forward the publication to appropriate relays to handle area publication        
        msg.targets.clear ();

        layer_t target_layer;

        for (unsigned int i=0; i < _neighbors.size (); i++)
        {
            target_layer = (layer_t)_neighbors[i]->addr.publicIP.pad;

            // if the neighbor is 
            //      1) at the same layer as publisher
            //      2) interested in this publication
            // then add as target
            if (layer == target_layer && _neighbors[i]->aoi.overlaps (area.center))
                msg.addTarget (_neighbors[i]->id);
        }
        
        sendMessage (msg);

        return true;
    }
*/
    
    // move a subscription area to a new position
    // returns actual AOI in case the position is already taken
    Area *  
    Relay::move (id_t subNo, Area &aoi, bool update_only)
    {
        if (_sub_list.find (subNo) == _sub_list.end ())
            return NULL; 
        
        Area &prev_aoi = _sub_list[subNo].aoi;

        if (_state == JOINED)
        {    
            // update center position first
            // NOTE that the AOI info in _sub_list is updated as well
            prev_aoi.center = aoi.center;

            Message msg (MOVE);
            msg.priority = 3;
            msg.store (_net->getTimestamp ());  // to calculate MOVE latency
            msg.store (subNo);

            // if only position's updated
            if (prev_aoi == aoi)
                msg.store (aoi.center);

            // send an entire new subscription update
            else
            {
                msg.msgtype = MOVE_F;
                msg.store (aoi);                
                prev_aoi = aoi;
            }

            msg.addTarget (_relay_id);

            if (update_only == false)
            {
                // MOVE can be delivered unreliably
                //msg.reliable = false;
                sendMessage (msg);
            }
        }
        
        // TODO: possibily perform collison detection for positions
        return &prev_aoi;
    }

    // send a custom message to a particular node
    bool        
    Relay::send (Message &message)
    {
        if (_state != JOINED)
            return false;

        Message msg (message);
        msg.store (_net->getTimestamp ());  // store sendtime for latency calculation

        // modify the msgtype to indicate this is an app-specific message
        msg.msgtype = (msg.msgtype << VAST_MSGTYPE_RESERVED) | MESSAGE;

        return ((sendMessage (msg) > 0) ? true : false);
    }

    // obtain a list of subscribers within an area
    vector<Node *>& 
    Relay::list (Area *area)
    {
        return _neighbors;        
    }

    // get a message from the network queue
    // true: while there are still messages to be received
    Message *
    Relay::receive ()
    {
        // clear last message allocation first
        if (_lastmsg != NULL)
        {
            delete _lastmsg;
            _lastmsg = NULL;
        }
        
        // extract oldest message if available
        if (_msglist.size () != 0)
        {        
            _lastmsg = _msglist[0];
            _msglist.erase (_msglist.begin ());
        }

        return _lastmsg;
    }

    // get current statistics about this node (a NULL-terminated string)
    char *
    Relay::getStat (bool clear)
    {
        return NULL;
    }

    // get the current node's information
    Node * 
    Relay::getSelf ()
    {        
        return &_self;
    }

    // obtain a list of peers hosted on this relay
    map<id_t, VONPeer *>& 
    Relay::getPeers ()
    {
        return _peers;
    }

    bool 
    Relay::isJoined ()
    {         
        // re-send join request if we're joining
        if (_state == JOINING)
            join (_self.aoi.center, isRelay ());

        return (_state == JOINED);
    }

    // whether the current node is listening for publications
    bool 
    Relay::isSubscribing (id_t sub_no)
    {
        if (_sub_list.find (sub_no) == _sub_list.end ())
            return false;

        return _sub_list[sub_no].active;
    }

    // if myself is a relay
    bool 
    Relay::isRelay ()
    {
        return (_relay_id == _self.id);
    }

    // whether I have public IP
    bool 
    Relay::hasPublicIP ()
    {
        return _net->isPublic ();
    }

    // get a particular peer's info
    Node *
    Relay::getPeer (id_t peer_id)
    {
        if (_peers.find (peer_id) != _peers.end ())
            return _peers[peer_id]->getSelf ();
        else
            return NULL;
    }

    // get the neighbors for a particular peer
    // returns NULL if the peer does not exist
    vector<Node *> *
    Relay::getPeerNeighbors (id_t peer_id)
    {
        if (_peers.find (peer_id) != _peers.end ())
            return &_peers[peer_id]->getNeighbors ();
        else
            return NULL;
    }

    // obtain access to Voronoi class (usually for drawing purpose)
    // returns NULL if the peer does not exist
    Voronoi *
    Relay::getVoronoi (id_t peer_id)
    {
        if (_peers.find (peer_id) != _peers.end ())
            return _peers[peer_id]->getVoronoi ();
        else
            return NULL;
    }


    // get message latencies 
    // msgtype == 0 indicates clear up currently collected stat
    StatType * 
    Relay::getMessageLatency (msgtype_t msgtype)
    {
        if (msgtype == 0)
        {
            _latency.clear ();
            return NULL;
        }

        // the message type was not found
        if (_latency.find (msgtype) == _latency.end ())
            return NULL;

        // calculate average
        _latency[msgtype].calculateAverage ();

        return &_latency[msgtype];
    }

    // get # of peers hosted at this relay, returns NULL for no record (non-relay)
    StatType *
    Relay::getPeerStat ()
    {
        if (_relay_id != _self.id)
            return NULL;

        _peerstat.calculateAverage ();
        return &_peerstat;
    }

    //
    // VONNetwork
    //

    // send messages to some target nodes
    // returns number of bytes sent
    size_t 
    Relay::sendVONMessage (Message &msg, bool is_reliable, vector<id_t> *failed_targets)
    {
        // TODO: make return value consistent
        msg.reliable = is_reliable;
        return (size_t)sendMessage (msg, failed_targets);
    }

    // obtain next message in queue
    // return pointer to Message, or NULL for no more message
    Message* 
    Relay::receiveVONMessage (timestamp_t &senttime)
    {
        return NULL;
    }

    // notify the network layer of nodeID -> Address mapping
    
    bool 
    Relay::notifyAddressMapping (id_t node_id, Addr &addr)
    {
        return notifyMapping (node_id, &addr);
    }
    

    // get the IP address of current host machine
    Addr &
    Relay::getHostAddress ()
    {
        return _net->getHostAddress ();
    }

    // get current physical timestamp
    timestamp_t 
    Relay::getTimestamp ()
    {
        return _net->getTimestamp ();
    }


    //
    //  private methods 
    //

    // perform initialization tasks for this handler (optional)
    // NOTE that all internal variables (such as handler_no) have been set at this point
    void 
    Relay::initHandler ()
    {
        _self.addr = _net->getHostAddress ();
    }

    // handler for various incoming messages
    // returns whether the message was successfully handled
    bool 
    Relay::handleMessage (Message &in_msg)
    {
        if (_state == ABSENT)
            return false;

        // store the app-specific message type if exists
        msgtype_t app_msgtype = APP_MSGTYPE(in_msg.msgtype);
        in_msg.msgtype = VAST_MSGTYPE(in_msg.msgtype);

#ifdef DEBUG_DETAIL
        if (in_msg.msgtype < VON_MAX_MSG)
            ; //printf ("[%d] Relay::handleMessage from: %d msgtype: %d, to be handled by VONPeer, size: %d\n", _self.id, in_msg.from, in_msg.msgtype, in_msg.size);            
        else
            printf ("[%d] Relay::handleMessage from: %d msgtype: %d appmsg: %d (%s) size: %d\n", _self.id, in_msg.from, in_msg.msgtype, app_msgtype, VAST_MESSAGE[in_msg.msgtype-VON_MAX_MSG], in_msg.size);
#endif

        switch (in_msg.msgtype)
        {
        // query for the cloest relay
        case QUERY:
            {
                Node joiner, relay;
                in_msg.extract (joiner);
                in_msg.extract (relay);

                bool success = false;
                while (!success)
                {
                    // find the closet relay
                    Node *closest = closestRelay (joiner.aoi.center);
                
                    // respond the query if I'm closest 
                    if (closest->id == _self.id)
                    {
                        Message msg (QUERY_REPLY);
                        msg.priority = 1;
                        msg.store (joiner);
                        msg.store (*closest);
                        msg.addTarget (relay.id);
                                        
                        notifyMapping (relay.id, &relay.addr);
                        sendMessage (msg);
                        success = true;
                    }
                    // otherwise greedy-forward the message to the next closest, valid relay
                    else
                    {
                        in_msg.targets.clear ();
                        in_msg.addTarget (closest->id);
                        success = (sendMessage (in_msg) > 0);

                        // remove the invalid relay, this occurs if the neighbor relay has failed
                        if (!success)
                            removeRelay (closest->id);
                    }                                        
                }

            }
            break;

        // response to QUERY messages
        case QUERY_REPLY:
            {                
                Node joiner;
                Node relay;

                in_msg.extract (joiner);
                in_msg.extract (relay);

                // initiate connection to relay if I'm the original requester, 
                if (joiner.id == _self.id)
                {
                    // store my initial known relay
                    addRelay (relay);                   

                    // if the closest relay is myself, then I'm joining as a relay
                    bool as_relay = (isRelay ());

                    if (as_relay == false)
                    {
                        // store my actual relay ID
                        _relay_id = relay.id;

                        // notify network of default host to route messages
                        // TODO: not a good idea to touch such lower-level stuff here, better way?
                        ((MessageQueue *)_msgqueue)->registerHostID (0, _relay_id);
                    }

                    joinRelay (as_relay, &relay);
                }
                // otherwise, forward it to the joiner
                else
                {
                    in_msg.targets.clear ();
                    in_msg.addTarget (joiner.id);
                    notifyMapping (joiner.id, &joiner.addr);
                    sendMessage (in_msg);
                }
            }
            break;

        // handles a JOIN request of a new node to the overlay, 
        // responds with a list of known relay neighbors
        case JOIN:
            {
                Node joiner;
                bool as_relay;                
                in_msg.extract (joiner);
                in_msg.extract ((char *)&as_relay, sizeof (bool));
                
                // check if I can take in this new node, default is NO 
                // (because joiner is itself a relay, or no more peer space available)
                bool join_reply = false;

                if (as_relay == false && (_peerlimit == 0 || _accepted.size () < (size_t)_peerlimit))
                {
                    join_reply = true;
                    
                    if (_accepted.find (joiner.id) == _accepted.end ())
                        _accepted[joiner.id] = joiner;
                }                                

                // compose return message with a list of known relays
                Message msg (JOIN_REPLY);
                msg.priority = 1;
                msg.store ((char *)&join_reply, sizeof (bool));
                    
                size_t listsize = _relays.size ();
                msg.store ((char *)&listsize, sizeof (size_t));

                if (listsize > 0)
                    for (multimap<double, Node *>::iterator it = _relays.begin (); it != _relays.end (); it++)
                        msg.store (*it->second);
                
                msg.addTarget (in_msg.from);
                sendMessage (msg);
            }
            break;

        // response from previous JOIN request
        case JOIN_REPLY:
            {
                bool as_relay = (isRelay ());

                bool    join_success;
                size_t  num_relays;

                in_msg.extract ((char *)&join_success, sizeof (bool));
                in_msg.extract ((char *)&num_relays, sizeof (size_t));

                // extract list of known relays
                Node relay;
                for (size_t i=0; i < num_relays; i++)
                {
                    in_msg.extract (relay);                    
                    addRelay (relay);
                }

                // if join fail for a client, then find another relay to join                
                if (!as_relay && join_success == false)
                {
                    if (joinRelay (as_relay) == false)
                        break;
                }

                // otherwise, we've successfully joined the relay mesh (as either a Client, or a Relay)
                else
                {
                    _state = JOINED;

                    // if I'm a relay, let each newly learned relay also know
                    if (as_relay)
                    {
                        Message msg (RELAY);
                        msg.priority = 1;
                        msg.store (_self);
                        
                        multimap<double, Node *>::iterator it = _relays.begin ();
                        
                        // notify up to _relaylimit
                        for (size_t i=0; it != _relays.end () && (_relaylimit == 0 || i < _relaylimit); it++)
                        {
                            msg.targets.clear ();
                            msg.addTarget (it->second->id);
                            notifyMapping (it->second->id, &it->second->addr);
                            sendMessage (msg);
                        }
                    }
                    // re-subscribe current subscriptions
                    else if (_sub_list.size () > 0)
                    {
                        // send out subscription request
                        Message msg (SUBSCRIBE);
                        msg.priority = 1;

                        map<id_t, Subscription>::iterator it = _sub_list.begin ();
                        for (; it != _sub_list.end (); it++)
                        {
                            Subscription &sub = it->second;

                            msg.clear (SUBSCRIBE);
                            msg.store (sub.sub_id);
                            msg.store (sub.aoi);
                            msg.store (sub.layer);
                            msg.addTarget (_relay_id);
                                    
                            sendMessage (msg);
                        }
                    }
                }
            }
            break;
        
        // handshake to allow an existing relay know of a new relay
        case RELAY:
            {
                Node new_relay;
                in_msg.extract (new_relay);
                
                addRelay (new_relay);
            }
            break;

        // subscription request for an area
        case SUBSCRIBE:
            {
                id_t sub_no;
                Area area;
                layer_t layer;

                in_msg.extract (sub_no);
                in_msg.extract (area);
                in_msg.extract ((char *)&layer, sizeof (layer_t));

                // only accept either a self request
                // or a previously admitted JOIN request
                if (in_msg.from == _self.id || 
                    _accepted.find (in_msg.from) != _accepted.end ())                
                    addPeer (in_msg.from, sub_no, area, layer);
                
            }
            break;

        // response from Relay regarding whether a subscription is successful
        case SUBSCRIBE_REPLY:
            {
                id_t sub_no;
                in_msg.extract (sub_no);

                if (_sub_list.find (sub_no) == _sub_list.end ())
                {
                    printf ("[%ld] Relay::handleMessage SUBSCRIBE_REPLY received for unknown subscription (%ld)\n", _self.id, sub_no);
                }
                else
                    _sub_list[sub_no].active = true;
            }
            break;

        // move an existing subscription area to a new place
        case MOVE:
        case MOVE_F:
            {
                id_t     sub_no;        // subscription no (i.e., handler_id for the Peer)
                Area     new_aoi;
                
                timestamp_t send_time;  // time of sending initial MOVE

                in_msg.extract (send_time);
                in_msg.extract (sub_no);

                // call up the respective Peer to handle the MOVE request
                if (_peers.find (sub_no) != _peers.end ())
                {                    
                    // only position is updated
                    if (in_msg.msgtype == MOVE)
                    {
                        new_aoi = _peers[sub_no]->getSelf ()->aoi;
                        in_msg.extract (new_aoi.center);
                    }
                    // whole AOI is updated
                    else
                        in_msg.extract (new_aoi);
                    
                    _peers[sub_no]->move (new_aoi, send_time);
                }
            }
            break;
            
        // notification from Relay about currently known AOI neighbors
        case NEIGHBOR:
            {
                // loop through each response 
                listsize_t size;
                in_msg.extract (size);
                vector<id_t> remove_list;           // list of nodes to be deleted

                id_t                    neighbor_id;
                NeighborUpdateStatus    status;
                Node                    node;
                Area                    aoi;
                timestamp_t             time;

                for (int i=0; i < (int)size; i++)
                {
                    in_msg.extract (neighbor_id);
                    in_msg.extract ((char *)&status, sizeof (NeighborUpdateStatus));

                    switch (status)
                    {
                    case INSERTED:
                        {
                            in_msg.extract (node);

                            // check for redundency first
                            vector<Node *>::iterator it = _neighbors.begin ();
                            for (; it != _neighbors.end (); it++)
                            {
                                // redundency found, simply update
                                if ((*it)->id == node.id)
                                {                               
                                    (*it)->update (node);                                    
                                    break;
                                }
                            }
                            
                            // if indeed it's a new neighbor
                            if (it == _neighbors.end ())
                                _neighbors.push_back (new Node (node));
                            
                            recordLatency (MOVE, node.time);
                        }
                        break;

                    case DELETED:
                        remove_list.push_back (neighbor_id);
                        break;

                    case UPDATED:
                        {                            
                            in_msg.extract (aoi);
                            in_msg.extract (time);

                            recordLatency (MOVE, time);

                            vector<Node *>::iterator it = _neighbors.begin ();
                            for (; it != _neighbors.end (); it++)
                            {
                                if ((*it)->id == neighbor_id)
                                {                               
                                    //printf ("[%d] Client updates neighbor [%d]'s position (%d, %d) to (%d, %d)\n", _self.id, neighbor_id, (int)(*it)->aoi.center.x, (int)(*it)->aoi.center.y, (int)aoi.center.x, (int)aoi.center.y);
                                    (*it)->aoi = aoi;
                                    (*it)->time = time;
                                    
                                    break;
                                }
                            }
                        }
                        break;
                    }
                    
                }

                // remove neighbors
                for (size_t i=0; i < remove_list.size (); i++)
                {
                    vector<Node *>::iterator it = _neighbors.begin ();
                    for (; it != _neighbors.end (); it++)
                    {
                        if ((*it)->id == remove_list[i])
                        {
                            delete (*it);
                            _neighbors.erase (it);
                            break;
                        }
                    }
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

                // check through known neighbors to find potential subscribers
                // TODO: consider forward the publication to appropriate relays to handle area publication        
                in_msg.targets.clear ();

                // the layer of the receiver
                layer_t target_layer;
        
                // find the peer that can act as initial point for this publication 
                // TODO: find a more efficient / better way
                map<id_t,VONPeer *>::iterator it = _peers.begin ();                
                for (; it != _peers.end (); it++)
                {
                    // if the peer's Voronoi overlaps with the publication area
                    //if (it->second->getVoronoi ()->contains (it->first, area.center))
                    //    break;

                    // find which neighbors are affected by the publication 
                    // (that is, their AOI covers the point publication)
                    vector<Node *> &neighbors = it->second->getNeighbors ();
                
                    for (unsigned int i=0; i < neighbors.size (); i++)
                    {
                        target_layer = (layer_t)neighbors[i]->addr.publicIP.pad;
                
                        // if the neighbor is 
                        //      1) at the same layer as publisher
                        //      2) interested in this publication
                        // then add as target
                        if (layer == target_layer && neighbors[i]->aoi.overlaps (area.center))
                            in_msg.addTarget (neighbors[i]->id);
                    }
                }                

                if (in_msg.targets.size () > 0)
                {
                    vector<id_t> failed_targets;

                    // some targets are invalid
                    if (sendMessage (in_msg, &failed_targets) < (int)in_msg.targets.size ())
                    {
                        printf ("Relay::handleMessage PUBLISH warning: not all publish targets are valid\n");

                        // TODO: consider combine this with DISCONNECT? as they perform very
                        //       similar tasks
                        Message msg (DISCONNECT);
                        msg.priority = 0;
                       
                        // remove failed targets
                        for (size_t i=0; i < failed_targets.size (); i++)
                        {
                            id_t failed_peer = failed_targets[i];

                            if (_peers.find (failed_peer) != _peers.end ())
                            {
                                removePeer (failed_peer);
                                continue;
                            }

                            msg.from = failed_peer;
                            
                            // send DISCONNECT to all VONpeers
                            for (map<id_t, VONPeer *>::iterator it = _peers.begin (); it != _peers.end (); it++)
                                it->second->handleMessage (msg);
                        }
                    }
                }
#ifdef DEBUG_DETAIL
                else
                    printf ("Relay::handleMessage PUBLISH, no peer covers publication at (%d, %d)\n", (int)area.center.x, (int)area.center.y);
#endif
                   
            }
            break;
        
        // receiving a published or directly-targeted message
        case MESSAGE:
            {
                vector<id_t> remote_hosts;

                // check if this is a local message or remote message
                for (unsigned int i=0; i < in_msg.targets.size (); i++)
                {
                    id_t target = in_msg.targets[i];
                    if (_peer2host.find (target) != _peer2host.end () &&
                        _peer2host[target] != _self.id)
                        remote_hosts.push_back (_peer2host[target]);                        
                }

                bool to_local = in_msg.targets.size () > remote_hosts.size ();

                // check sending to remote hosts
                // NOTE must send to remote first before local, as sendtime will be extracted by local host
                //      and the message structure will get changed
                if (remote_hosts.size () > 0)
                {
                    in_msg.msgtype = (app_msgtype << VAST_MSGTYPE_RESERVED) | MESSAGE;
                    in_msg.targets = remote_hosts;
                    sendMessage (in_msg);
                }

                // check if the message is targeted locally
                if (to_local)
                {
                    // restore app-specific message type
                    in_msg.msgtype = app_msgtype;
                    
                    timestamp_t sendtime;
                    in_msg.extract (sendtime, true);
                    recordLatency (PUBLISH, sendtime);

                    in_msg.reset ();
                    storeMessage (in_msg);
                }
            }
            break;

        // process universal VASTnet message, msgtype = 0
        case DISCONNECT:
            {                
                // a list of peers that's associated with the disconnected host
                vector<id_t> peerlist;

                // build a reverse map (see if any of my peer belong to this host)
                for (map<id_t, id_t>::iterator it = _peer2host.begin (); it != _peer2host.end (); it++)
                {
                    if (it->second == in_msg.from)
                        peerlist.push_back (it->first);
                }
                                
                // if the message refers directly to a VONPeer
                if (EXTRACT_LOCAL_ID (in_msg.from) > 0)
                    peerlist.push_back (in_msg.from);                
                else 
                {
                    // remove the disconnecting relay from relay list
                    removeRelay (in_msg.from);

                    // re-connect to closest relay if disconnected from current relay
                    if (in_msg.from == _relay_id && in_msg.from != _self.id) 
                    {
                        // clear up neighbors due to current subscriptions 
                        // as we'll need to re-subscribe with the new relay
                        // TODO: more efficient method?
                        for (unsigned int i=0; i < _neighbors.size (); i++)
                            delete _neighbors[i];
                        _neighbors.clear ();

                        joinRelay (false);
                    }
                }

                // the disconnect node is a client or peer itself
                if (peerlist.size () > 0)
                {
                    // deliver DISCONNECT message to the VONPeer (who might be affected)
                    in_msg.msgtype = VON_DISCONNECT;

                    for (unsigned int i=0; i < peerlist.size (); i++)
                    {
                        in_msg.from = peerlist[i];

                        // send the disconnect message to each existing VONPeer
                        for (map<id_t, VONPeer *>::iterator it = _peers.begin (); it != _peers.end (); it++)
                            it->second->handleMessage (in_msg);

                        // remove the Peer
                        removePeer (peerlist[i]);
                    }
                }
            }
            break;

        // assume that this is a message for VONPeer
        default:            
            {
                // ignore messages not directly towards VONPeers
                if (EXTRACT_LOCAL_ID (in_msg.from) == 0)
                    break;

                // check through the targets and deliver to each VONPeer affected            
                for (size_t i=0; i != in_msg.targets.size (); i++)
                {
                    id_t target = in_msg.targets[i];
                    if (_peers.find (target) == _peers.end ())
                    {
#if _DEBUG
                        printf ("Relay::handleMessage () cannot find the message's target peer [%d]\n", target);
#endif
                        continue;
                    }
                    _peers[target]->handleMessage (in_msg);

                    // it's very important to reset the current counter, so that extraction may still work
                    in_msg.reset ();
                }
            }
            break;
        }
        
        return true;
    }

    // perform routine tasks after all messages have been handled 
    //  (i.e., check for reply from requests sent)
    void 
    Relay::postHandling ()
    {
        // TODO: adjust peerlimit based on resource (bandwidth) availability

        // tick each VONPeer
        for (map<id_t,VONPeer *>::iterator it = _peers.begin (); it != _peers.end (); it++)
            it->second->tick ();

        // remove non-essential relays
        cleanupRelays ();

        checkRelayRejoin ();

        // check whether Peers hosted on this relay have joined a VON successfully
        updatePeerStates ();
     
        //
        // send neighbor updates for Clients
        //
        
        // check over updates on each Peer's neighbors and notify the 
        // respective Clients of the changes (insert/delete/update)        
        Message msg (NEIGHBOR);
        msg.priority = 1;

        map<id_t, VONPeer *>::iterator it = _peers.begin ();
        for (; it != _peers.end (); it++)
        {
            id_t peer_id  = it->first;
            VONPeer *peer = it->second;
                            
            map<id_t, NeighborUpdateStatus>& update_status = peer->getUpdateStatus ();
            listsize_t listsize = (listsize_t)update_status.size ();
            if (listsize == 0)
                continue;

            msg.clear (NEIGHBOR); 
            msg.store (listsize);

            // loop through each neighbor for this peer and record its status change
            map<id_t, NeighborUpdateStatus>::iterator itr = update_status.begin ();
            for (; itr != update_status.end (); itr++)
            {               
                // store each updated neighbor info into the notification messages
                id_t neighbor_id = itr->first;
                NeighborUpdateStatus status = itr->second;
                Node *node = NULL;
                
                // change neighbor info to DELETED (something's wrong here)
                if (status == INSERTED || status == UPDATED)
                    if ((node = peer->getNeighbor (neighbor_id)) == NULL)
                        status = DELETED;

                msg.store (neighbor_id);
                msg.store ((char *)&status, sizeof (NeighborUpdateStatus));

                // store actual state update
                switch (status)
                {
                case INSERTED:
                    msg.store (*node);
                    break;
                case DELETED:
                    break;
                case UPDATED:
                    //msg.store (*node);
                    msg.store (node->aoi);
                    msg.store (node->time);
                    break;
                }
            }

            id_t target = _peer2host[peer_id];
            msg.addTarget (target);
            sendMessage (msg);

            // at the end we clear the status record for next time-step
            update_status.clear ();

        } // end for (_peers)
    }

    // find the relay closest to a position, which can be myself
    // returns the relay's Node info
    Node * 
    Relay::closestRelay (Position &pos)
    {
        // set self as default
        Node *closest   = &_self;

        double min_dist = _self.aoi.center.distance (pos);

        // find closest among known neighbors
        multimap<double, Node *>::iterator it = _relays.begin ();
        for (; it != _relays.end (); it++)
        {
            // NOTE: it's important if distance is equal or very close, then 
            //       there's a second way to determine ordering (i.e., by ID)
            //       otherwise the query may be thrown in circles

            double dist = it->second->aoi.center.distance (pos);
            if (dist < min_dist || 
                ((dist - min_dist < EQUAL_DISTANCE) && (it->second->id < closest->id)))
            {
                closest     = it->second;
                min_dist    = dist;
            }            
        }

        // reply the closest
        return closest;
    }

    // join the relay mesh by connecting to the closest known relay
    bool
    Relay::joinRelay (bool as_relay, Node *relay)
    {
        if (_relays.size () == 0)
        {
            printf ("Relay::joinRelay (): no known relay to contact\n");

            // contact gateway for relay list
            // TODO: combine with same code in join ()

            // create a query node to find the initial relay with the current node's ID & position
            Area a; 
            Node relay_node (NET_ID_GATEWAY, 0, a, getAddress (NET_ID_GATEWAY));
            
            // send query to the gateway to find the physically nearest node to join
            Message msg (QUERY);
            msg.priority = 1;
            msg.store (_self);
            msg.store (relay_node);
            msg.addTarget (NET_ID_GATEWAY);
            sendMessage (msg);

            return false;
        }

        // if no relay is provided, find next available
        if (relay == NULL)
        {
            // first find the current relay
            multimap<double, Node *>::iterator it = _relays.begin ();
            for (; it != _relays.end (); it++)
            {
                if (it->second->id == _relay_id)
                    break;
            }

            // we don't have any relay to contact (something's wrong here)
            if (it == _relays.end () || (++it) == _relays.end ())
            {
                printf ("Relay::joinRelay () - last known relay tried, re-try first\n");
                it = _relays.begin ();
            }

            relay = it->second;

            // send JOIN request to the next available relay
            _relay_id = relay->id;
        }
                   
        // send JOIN request to the closest relay
        Message msg (JOIN);
        msg.priority = 1;
        msg.store (_self);
        msg.store ((char *)&as_relay, sizeof (bool));
        msg.addTarget (relay->id);
        
        if (as_relay == false)
            notifyMapping (_relay_id, &relay->addr);

        sendMessage (msg);
        
        _state = JOINING;
        _join_timeout = TIMEOUT_JOIN;

        // invalidate all current subscriptions (necessary during relay re-join after relay failure)
        map<id_t, Subscription>::iterator it = _sub_list.begin ();
        for (; it != _sub_list.end (); it++)
            it->second.active = false;
            
        return true;
    }
    
    // add a newly learned relay
    void
    Relay::addRelay (Node &relay)
    {
        // debug: limit # of known relays of a client to 1
        //if (isRelay () == false && _relays.size () >= 1)
        //    return;

        double dist = relay.aoi.center.distance (_self.aoi.center);

        // avoid inserting the same relay, but note that we accept two relays have the same distance
        multimap<double, Node *>::iterator it = _relays.begin ();
        for (; it != _relays.end (); it++)
            if (it->second->id == relay.id)
                break;

        if (it == _relays.end ())
            _relays.insert (multimap<double, Node *>::value_type (dist, new Node (relay)));
    }
    
    void
    Relay::removeRelay (id_t id)
    {
        multimap<double, Node *>::iterator it = _relays.begin ();
        for (; it != _relays.end (); it++)
        {
            if (it->second->id == id)
            {
                printf ("[%ld] Relay::removeRelay () removing relay [%ld]..\n", _self.id, id);
                delete it->second;
                _relays.erase (it);
                break;
            }
        }   
    }

    // remove non-useful relays
    void 
    Relay::cleanupRelays ()
    {
        // no limit
        if (_relaylimit == 0)
            return;

        // remove known relays that are too far, but only after we've JOINED
        if (_state == JOINED && _relays.size () > _relaylimit)
        {
            // remove the most distant ones, assuming those further down the map are larger values
            int num_removals = _relays.size () - _relaylimit;

            vector<id_t> remove_list;

            multimap<double, Node *>::iterator it = _relays.end ();
            it--;

            int i;
            for (i=0; i < num_removals; it--, i++)
                remove_list.push_back (it->second->id);

            for (i=0; i < num_removals; i++)      
                removeRelay (remove_list[i]);
        }
    }


    // check if relay re-joining is successful, or attempt to join the next available 
    void 
    Relay::checkRelayRejoin ()
    {
        // we're still waiting for a join-reply
        if (_state == JOINING && _relay_id != _self.id)
        {
            if (_join_timeout == 0)
            {
                // remove the timeout'd relay                
                removeRelay (_relay_id);

                joinRelay (false);                                                
            }
            else
                _join_timeout--;
        }       
    }

    // create a new Peer instance at this Relay
    bool 
    Relay::addPeer (id_t fromhost, id_t sub_no, Area &area, layer_t layer)
    {
        // create a Peer using VON procedure, then join a VON
        // TODO: should probably check if the requesting Client has the quota to create a new Peer
        //       or at least the requesting Client has properly joined this Relay
        if (_peers.find (sub_no) != _peers.end ())
            return false;

        VONPeer *peer = new VONPeer (sub_no, this);
               
        // store layer info 
        // TODO: right now this is a hacked solution, just use whatever available space
        peer->getSelf ()->addr.publicIP.pad = (unsigned short)layer;

        // we assume that a VAST node at the gateway has access to the gateway VON peer 
        // so that a joining VON node can reach a gateway VON peer (via the gateway VAST node)
        Node VON_gateway (getUniqueID (ID_GROUP_VON_VAST, true), 0, area, getAddress (NET_ID_GATEWAY));
           
        peer->join (area, &VON_gateway);
        _peers[sub_no] = peer;
        _peer_state[sub_no] = JOINING;
        _peer2host[sub_no] = fromhost;

        return true;
    }

    bool 
    Relay::removePeer (id_t sub_no)
    {
        if (_peers.find (sub_no) == _peers.end ())
            return false;

        // opposite to JOIN, notify my neighbor of my departure
        _peers[sub_no]->leave ();

        delete _peers[sub_no];
        _peers.erase (sub_no);
        _peer_state.erase (sub_no);
        _peer2host.erase (sub_no);
        
        return true;
    }

    void
    Relay::updatePeerStates ()
    {                
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

        // record simple stats on peers
        if (_peers.size () > _peerstat.maximum)
            _peerstat.maximum = _peers.size ();

        if (_peers.size () > 0)
        {
            _peerstat.total += _peers.size ();
            _peerstat.num_records++;
        }
    }

    // store a message to the local queue to be retrieved by receive ()
    void 
    Relay::storeMessage (Message &msg)
    {
        _msglist.push_back (new Message (msg));
    }

    // make one latency record
    void 
    Relay::recordLatency (msgtype_t msgtype, timestamp_t sendtime)
    {
        StatType &stat = _latency[msgtype];

        timestamp_t duration = _net->getTimestamp () - sendtime;
        
        // do not record zero latency
        if (duration == 0)
            return;

        // record min / max / average
        if (duration < stat.minimum)
            stat.minimum = duration;

        if (duration > stat.maximum)
            stat.maximum = duration;

        stat.total += duration;
        stat.num_records++;
    }
                                    
} // end namespace Vast
