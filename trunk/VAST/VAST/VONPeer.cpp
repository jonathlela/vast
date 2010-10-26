

#include "VONPeer.h"
#include "VoronoiSF.h"

using namespace Vast;

namespace Vast
{   
    char VON_MESSAGE[][20] = 
    {
        "VON_DISCONNECT",
        "VON_QUERY",
        "VON_HELLO",
        "VON_HELLO_R",
        "VON_EN",
        "VON_MOVE",
        "VON_MOVE_F",
        "VON_MOVE_B",
        "VON_MOVE_FB",
        "VON_BYE",
        "VON_NODE"
    };

    VONPeer::VONPeer (id_t id, VONNetwork *net, length_t aoi_buffer, bool strict_aoi)
    {
        _net = net;
        _aoi_buffer = aoi_buffer;
        _strict_aoi = strict_aoi;

        // initialize self info except AOI
        _self.id    = id;            
        
        // NOTE that we do not initialzie time as local time as the time should be
        //      the VON peer's owner (which could be a different host with a different
        //      logical clock), and the owner's time should be respected
        //      it's important as otherwise a more advanced local time will cause
        //      the owner's time never get updated
        //_self.time  = _net->getTimestamp ();
        _self.time  = 0;
        _self.addr  = _net->getHostAddress ();

        _state      = ABSENT;
        _Voronoi    = new Vast::VoronoiSF ();

        // initialize internal counter for ticks elapsed
        //_tick_count = 0;
    }
    
    VONPeer::~VONPeer ()
    {
        // perform leave () but do not send out VON_LEAVE message
        // which might cause connections be created again 
        // (while the receiving thread is already destroyed)
        leave (false);
        delete _Voronoi;
    }

    void
    VONPeer::join (Area &aoi, Node *gateway)
    {
        if (isJoined ())
            return;

        _self.aoi = aoi;
        
        insertNode (_self);

        // if I'm gateway then considered joined
        if (_self.id == gateway->id)
        {
            _state = JOINED;
            return;
        }

        // send out join request
        _net->notifyAddressMapping (gateway->id, gateway->addr);

        Message msg (VON_QUERY, _self.id);
        msg.priority = 0;
        msg.store (_self);
        msg.addTarget (gateway->id);
        _net->sendVONMessage (msg, true);

        _state = JOINING;
    }

    // leave the overlay
    void 
    VONPeer::leave (bool notify_neighbors)
    {
        if (notify_neighbors)
        {
            // send out VON_BYE to all known neighbors or potential neighbors 
            // as potential neighbors may have already added me
            Message msg (VON_BYE, _self.id);
            msg.priority = 0;
             
            for (size_t i = 0; i < _neighbors.size (); i++)
            {
                if (_neighbors[i]->id != _self.id)
                    msg.addTarget (_neighbors[i]->id);
            }
            
            map<id_t, Node>::iterator it = _potential_neighbors.begin ();
            for (; it != _potential_neighbors.end (); it++)
                msg.addTarget (it->second.id);
        
            if (msg.targets.size () > 0)
                _net->sendVONMessage (msg, true);
        }

        // delete allocated memory                
        // NOTE: no need to clear the Node pointers in _neighbors as 
        //       they are simply references to data in _id2node
        for (map<id_t, map<id_t, int> *>::iterator it = _neighbor_states.begin (); it != _neighbor_states.end (); it++)
            delete it->second;
        _neighbor_states.clear ();

        _id2node.clear ();
        _neighbors.clear ();

        // stop all connections                       
        _state = ABSENT;
    }

    // move the current position to a new one by 
    // sending move messages to all connected neighbors
    Area &
    VONPeer::move (Area &aoi, timestamp_t sendtime)
    {                      
        // check if my new position overlaps a neighbor, if so then move slightly
        _self.aoi.center = aoi.center;
        bool aoi_reshaped = (_self.aoi == aoi ? false : true);
        _self.aoi = aoi;

        // avoid to move to the same position as my neighbor
        _self.aoi.center = isOverlapped (_self.aoi.center);
        
        // record send time of the movement
        // this is to help receiver determine which position update should be used
        // also to help calculate latency
        // if external time is supplied (we respect the sender's time), then record it
        if (sendtime != 0)
            _self.time = sendtime;
        else
            _self.time = _net->getTimestamp ();
        
        updateNode (_self);

#ifdef DEBUG_DETAIL
        printf ("[%lu] VONPeer::move to (%d, %d)\n", _self.id, (int)_self.aoi.center.x, (int)_self.aoi.center.y);
#endif            
        // notify all connected neighbors, pack my own node info & AOI info              
        id_t target;            
        Message msg (VON_MOVE, _self.id);
        msg.priority = 1;

        if (aoi_reshaped)
            msg.store (_self);
        else       
        {
            // both position & time are needed (so that only the most recent update is used by neighbors)
            //msg.store (_self.aoi.center);
            VONPosition pos;
            pos.x = _self.aoi.center.x;
            pos.y = _self.aoi.center.y;

            // NOTE: we use an optimized version of position with only x & y
            msg.store ((char *)&pos, sizeof (VONPosition));                        
            msg.store (_self.time);
        }

        // go over each neighbor and do a boundary neighbor check	
        vector<id_t> boundary_list;

        for (map<id_t, Node>::iterator it = _id2node.begin(); it != _id2node.end(); ++it)
        {
            if (isSelf (target = it->first)) // || !isAOINeighbor (_self.id, it->second))
                continue;

            if (_Voronoi->is_boundary (target, _self.aoi.center, _self.aoi.radius))
                boundary_list.push_back (target);                
            else
                msg.addTarget (target);                        
        }

        // send MOVE to regular neighbors        
        msg.msgtype = (msgtype_t)(aoi_reshaped ? VON_MOVE_F : VON_MOVE);
        vector<id_t> failed_targets;

        if (msg.targets.size () > 0)
        {
            //_net->sendVONMessage (msg, false, &failed_targets);       // MOVE is sent unreliably
            _net->sendVONMessage (msg, true, &failed_targets);       // MOVE is sent reliably

#ifdef DEBUG_DETAIL
            printf ("%s ", VON_MESSAGE[msg.msgtype]);
            for (size_t i=0; i < msg.targets.size (); i++)
                printf ("%d ", msg.targets[i]);
            printf ("\n");
#endif  
            // remove neighbors that have failed
            for (size_t i=0; i < failed_targets.size (); i++)
                deleteNode (failed_targets[i]);
        }

        // send MOVE to boundary neighbors
        msg.targets.clear ();
        msg.targets = boundary_list;
        msg.msgtype = (msgtype_t)(aoi_reshaped ? VON_MOVE_FB : VON_MOVE_B);

        failed_targets.clear ();
        if (msg.targets.size () > 0)
        {
            //_net->sendVONMessage (msg, false, &failed_targets);       // MOVE is sent unreliably
            _net->sendVONMessage (msg, true, &failed_targets);       // MOVE is sent unreliably

#ifdef DEBUG_DETAIL
            printf ("%s ", VON_MESSAGE[msg.msgtype]);
            for (size_t i=0; i < msg.targets.size (); i++)
                printf ("%d ", msg.targets[i]);
            printf ("\n");
#endif  
            // remove neighbors that have failed
            for (size_t i=0; i < failed_targets.size (); i++)
                deleteNode (failed_targets[i]);
        }

        return _self.aoi;
    }

    // whether the current node has joined
    bool 
    VONPeer::isJoined ()
    {         
        // this is to avoid checking if we're joined for every NODE message
        if (_state == JOINING)
        {
            if (_neighbors.size () > 1)
                _state = JOINED;
        }

        return (_state == JOINED);
    }

    // get info for a particular neighbor
    // returns NULL if neighbor doesn't exist
    Node *      
    VONPeer::getNeighbor (id_t id)
    {
        size_t i;
        for (i=0; i < _neighbors.size (); i++)
            if (id == _neighbors[i]->id)
                break;

        if (i == _neighbors.size ())
            return NULL;
        else
            return _neighbors[i];
    }

    // obtain a list of subscribers with an area
    vector<Node *>& 
    VONPeer::getNeighbors ()
    {
        return _neighbors;
    }

    // get the current node's information
    Node * 
    VONPeer::getSelf ()
    {
        return &_self;
    }

    // obtain access to Voronoi class (usually for drawing purpose)
    Voronoi *
    VONPeer::getVoronoi ()
    {
        return _Voronoi;
    }

    // obtain states on which neighbors have changed
    map<id_t, NeighborUpdateStatus>& 
    VONPeer::getUpdateStatus ()
    {
        return _updateStatus;
    }

    // tasks performed every cycle
    void 
    VONPeer::tick ()
    {
        // process incoming messages
        // NOTE: the alternative is to call handleMessage directly when there's incoming messages (current approach)
        timestamp_t senttime;
        Message *msg;

        while ((msg = _net->receiveVONMessage (senttime)) != NULL)
            handleMessage (*msg);

        // performs tasks after all messages are handled
        sendKeepAlive ();
        contactNewNeighbors ();
        checkNeighborDiscovery ();        
        removeNonOverlapped ();
    }

    // returns whether the message was successfully handled
    bool 
    VONPeer::handleMessage (Message &in_msg)
    {
#ifdef DEBUG_DETAIL
        printf ("[%lu] VONPeer::handleMessage from [%lu] msgtype: %d (%s) size:%d\n", _self.id, in_msg.from, in_msg.msgtype, VON_MESSAGE[in_msg.msgtype], in_msg.size);
#endif
        // if join is not even initiated, do not process any message 
        if (_state == ABSENT)
            return false;

        // pause any operations if I'm not yet joined (unless it's neighbor discovery purpose)
        //else if (_state == JOINING && (in_msg.msgtype != VON_NODE || in_msg.msgtype != VON_MOVE))
        //    return false;

        switch ((VON_Message)in_msg.msgtype)
        {
        case VON_QUERY:            
            //if (in_msg.size == sizeof (Node))
            {                
                Node joiner;
                in_msg.extract (joiner); 
                
                id_t closest = _Voronoi->closest_to (joiner.aoi.center);

                // forward the request if a more approprite node exists
                if (_Voronoi->contains (_self.id, joiner.aoi.center) == false &&
                    isSelf (closest) == false &&
                    closest != in_msg.from) 
                {                 
                    in_msg.targets.clear ();
                    in_msg.addTarget (closest);
                    _net->sendVONMessage (in_msg);
                }                    
                else
                {
                    // I am the acceptor, send back initial neighbor list
                    vector<id_t> list;
                 
                    // insert first so we can properly find joiner's EN
                    insertNode (joiner);

                    id_t neighbor;

                    // loop through all my known neighbors and determine which ones are relevant to notify
                    for (size_t i=0; i<_neighbors.size (); ++i)
                    {                            
                        neighbor = _neighbors[i]->id;
                        if (neighbor != joiner.id && 
                            isRelevantNeighbor (*_neighbors[i], joiner, _aoi_buffer) &&
                            isTimelyNeighbor (neighbor))
                            list.push_back (neighbor);
                    }
                    
                    sendNodes (joiner.id, list, true);
                }
            }
            break;
        
        case VON_HELLO:            
            //if ((in_msg.size - sizeof (Node) - sizeof (listsize_t)) % sizeof(id_t) == 0)
            {
                Node node;
                in_msg.extract (node);

                // update existing or new neighbor status                                            
                if (isNeighbor (in_msg.from))
                    updateNode (node);
                else                                      
                    insertNode (node);

                // send HELLO_R as response
                Message msg (VON_HELLO_R, _self.id);
                msg.priority = 0;
                msg.store (_self.aoi.center);
                msg.addTarget (in_msg.from);
                _net->sendVONMessage (msg, true);

                // check if enclosing neighbors need any update
                checkConsistency (in_msg.from);
            
            }
        // a list of supposed enclosing neighbors to check if any is missing 
        case VON_EN:
            {            
                //
                // check my enclosing neighbors to notify moving node any missing neighbors
                //

                listsize_t n;
                in_msg.extract (n);
                
                map<id_t, int> *list = new map<id_t, int>;                // list of EN received                
                vector<id_t>    missing;                                  // list of missing EN ids (to be sent)
                vector<id_t>    &en_list = _Voronoi->get_en (_self.id);   // list of my EN
                
                size_t i;
                id_t id;
                
                // create a searchable list of EN
                for (i=0; i<n; ++i)
                {
                    in_msg.extract (id);
                    list->insert (map<id_t, int>::value_type (id, NEIGHBOR_OVERLAPPED));  
                }
                
                // store as initial known list                 
                // TODO: do we need to update the neighbor_states here?
                //       because during neighbor discovery checks, each node's knowledge will be calculated               
                delete _neighbor_states[in_msg.from];
                _neighbor_states[in_msg.from] = list;
                
                size_t en_size = en_list.size ();
                for (i=0; i < en_size; ++i)
                {
                    id = en_list[i];
                
                    // send only relevant missing neighbors, defined as
                    //  1) not the sender node  
                    //  2) one of my enclosing neighbors but not in the EN list received
                    //  3) one of the sender node's relevant neighbors
                    //  
                    if (id != in_msg.from && 
                        list->find (id) == list->end () && 
                        isRelevantNeighbor (_id2node[id], _id2node[in_msg.from], _aoi_buffer))
                        missing.push_back (id);
                }
                
                // notify the node sending me HELLO of neighbors it should know
                if (missing.size () > 0)
                    sendNodes (in_msg.from, missing);               
            }            
            break;

        case VON_HELLO_R:
            {
                map<id_t, Node>::iterator it;

                // check if it's a response from new neighbor
                if ((it = _potential_neighbors.find (in_msg.from)) != _potential_neighbors.end ())
                {                                        
                    // insert the new node as a confirmed neighbor with updated position
                    in_msg.extract (it->second.aoi.center);
                    insertNode (it->second);                
                    _potential_neighbors.erase (it);
                }
            }
            break;

        case VON_MOVE:
        case VON_MOVE_B:
        case VON_MOVE_F:
        case VON_MOVE_FB:
            //if (in_msg.size == sizeof (Node))
            {
                Node node;
                
                // we only take MOVE from known neighbors
                if (isNeighbor (in_msg.from) == false)
                    break;

                // extract full node info or just position update
                if (in_msg.msgtype == VON_MOVE_F || in_msg.msgtype == VON_MOVE_FB)
                    in_msg.extract (node);
                else
                {
                    if (isNeighbor (in_msg.from))
                    {
                        node = _id2node[in_msg.from];

                        // NOTE both position & time are used
                        //in_msg.extract (node.aoi.center);
                        VONPosition pos;
                        in_msg.extract ((char *)&pos, sizeof (VONPosition));
                        node.aoi.center.x = pos.x;
                        node.aoi.center.y = pos.y;

                        in_msg.extract (node.time);
                    }
                    else
                    {
                        printf ("VONPeer::handleMessage () MOVE_x received from unknown neighbor %llu\n", in_msg.from);
                        break;                      
                    }
                }
               
                // if the remote node has just been disconnected,
                // then no need to process MOVE message in queue
                if (updateNode (node) == false)
                    break;

                // records nodes requesting neighbor discovery check
                if (in_msg.msgtype == VON_MOVE_B || in_msg.msgtype == VON_MOVE_FB)
                    _req_nodes[in_msg.from] = true;

                //printf ("[%d] learns of [%d] (%d, %d)\n", _self.id, node.id, (int)node.aoi.center.x, (int)node.aoi.center.y);
            }
            break;

        case VON_NODE:
            //if ((in_msg.size - sizeof (listsize_t)) % sizeof (Node) == 0)
            {
                listsize_t n;
                in_msg.extract (n);

                Node newnode;       // new node discovered                
                // store each node notified, process them later in batch                                       
                for (int i=0; i < (int)n; ++i)
                {   
                    _NEIGHBOR_Message.total++;
                    in_msg.extract (newnode);
                                        
                    // store the new node and process later
                    // if there's existing notification, then replace only if newer     
                    if (_new_neighbors.find (newnode.id) == _new_neighbors.end () ||
                        _new_neighbors[newnode.id].time <= newnode.time)
                    {
                        _NEIGHBOR_Message.normal++;
                        _new_neighbors[newnode.id] = newnode;
                    }                                                           
                }               
            }
            break;

        case VON_BYE:
        case VON_DISCONNECT:
            {               
                size_t i;
                for (i=0; i < _neighbors.size (); i++)
                {                                        
                    if (_neighbors[i]->id == in_msg.from)
                    {
                        checkConsistency (in_msg.from);
                        deleteNode (in_msg.from);

                        break;
                    }                   
                }
            }
            break;

        default:
            return false;
        }

        return true;
    }

    // re-send current position once in a while to keep neighbors interested in me
    void 
    VONPeer::sendKeepAlive ()
    {
        if (isJoined () && isTimelyNeighbor (_self.id, MAX_TIMELY_PERIOD/2) == false)
        {
            printf ("[%llu] sendKeepAlive ()\n", _self.id);
            move (_self.aoi);
        }
    }

    // notify new neighbors with HELLO messages
    void 
    VONPeer::contactNewNeighbors ()
    {
        //
        // new neighbor notification check
        //
        vector<id_t> new_list;      // list of new, unknown nodes
                
        id_t target;

        // loop through each notified neighbor and see if it's unknown
        for (map<id_t, Node>::iterator it = _new_neighbors.begin (); it != _new_neighbors.end (); ++it)
        {            
            Node &new_node = it->second;
            target = new_node.id;
                        
            // update existing info if the node is known, otherwise prepare to add
            if (isNeighbor (target))
                updateNode (new_node);
            
            else
            {                                
                _Voronoi->insert (target, new_node.aoi.center);            
                new_list.push_back (target);
            }
        }

        size_t i;
        
        // check through each newly inserted Voronoi for relevance                      
        for (i=0; i < new_list.size (); ++i)
        {
            target = new_list[i];
            Node &node = _new_neighbors[target];

            // if the neighbor is relevant and we insert it successfully
            // NOTE that we're more tolerant for AOI buffer when accepting notification
            if (isRelevantNeighbor (node, _self, _aoi_buffer))
            {   
                // store new node as a potential neighbor, pending confirmation from the new node 
                // this is to ensure that a newly discovered neighbor is indeed relevant
                _potential_neighbors[target] = node;

                // notify mapping for sending Hello message
                // TODO: a cleaner way (less notifymapping call?)
                _net->notifyAddressMapping (node.id, node.addr);

                // send HELLO message to newly discovered nodes
                // NOTE that we do not perform insert yet (until the remote node has confirmed via MOVE)
                //      this is to avoid outdated neighbor discovery notice from taking effect
                sendHello (target, _Voronoi->get_en (target));
            }           
        }

        // clear up the temporarily inserted test node from Voronoi (if not becoming neighbor)
        for (i=0; i < new_list.size (); ++i)            
            _Voronoi->remove (new_list[i]);
        
        // NOTE: erase new neighbors seems to bring better consistency 
        //       (rather than keep to next round, as previous VAST)
        _new_neighbors.clear ();
    
        // TODO: _potential_neighbors should be cleared once in a while
    }

    // neighbor discovery check
    void
    VONPeer::checkNeighborDiscovery ()
    {

        vector<id_t> notify_list;       // list of neighbors to notify
        id_t from_id;

        //
        // perform neighbor discovery only for those that have requested 
        // (i.e., those that have sent a MOVE_B message)
        //

#ifdef CHECK_REQUESTING_NODES_ONLY
        for (map<id_t, bool>::iterator itr = _req_nodes.begin (); itr != _req_nodes.end (); ++itr)
        {
            from_id = itr->first; 

            // a node requesting for check might be disconnected by now, as requests are processed in batch
            if (isNeighbor (from_id) == false)
                continue;
#else
        // we check for all known neighbors except myself
        size_t num = _neighbors.size ();
        size_t index;
        for (index=0; index < num; ++index)
        {
            from_id = _neighbors[index]->id;

            if (isSelf (from_id))
                continue;
#endif
            // TODO: determine whether clear or new is better / faster (?)
            notify_list.clear ();            
            map<id_t, int> *known_list = new map<id_t, int>;    // current neighbor states
            map<id_t, int>::iterator it;                        // iterator for neighbor states
            
            id_t id;
            int state, known_state;
              
#ifdef CHECK_EN_ONLY            
            // check for only enclosing neighbor
            vector<id_t> &en_list = _Voronoi->get_en (_self.id);
            size_t n = en_list.size ();            
#else            
            // loop through every known neighbor except myself
            size_t n = _neighbors.size ();
#endif
            for (size_t i=0; i < n; ++i)
            {        

#ifdef CHECK_EN_ONLY
                id = en_list[i];
#else
                id = _neighbors[i]->id;
#endif                
                if (isSelf (id) || id == from_id)
                    continue;
                
                // TODO:
                // do a simple test to see if this enclosing neighbor is
                // on the right side of me if I face the moving node directly
                // only notify the moving node for these 'right-hand-side' neighbors                
                
                //if (right_of (id, from_id) == false)
                //    continue;                

                state = 0;
                known_state = 0;
                if (isAOINeighbor (id, _id2node[from_id], _aoi_buffer))
                    state = state | NEIGHBOR_OVERLAPPED;
                if (_Voronoi->is_enclosing (id, from_id))
                    state = state | NEIGHBOR_ENCLOSED;

                // notify case1: new overlap by moving node's AOI
                // notify case2: new EN for the moving node                
                if (state != 0)
                {                                                           
                    if ((it = _neighbor_states[from_id]->find (id)) != _neighbor_states[from_id]->end ())
                        known_state = it->second;
                    
                    // note: what we want to achieve is:
                    //       1. notify just once when new enclosing neighbor (EN) is found
                    //       2. notify about new overlap, even if the node is known to be an EN

                    // check if the neighbors not overlapped previously is
                    //    1) currently overlapped, or... 
                    //    2) previously wasn't enclosing, but now is
                    
                    if ((known_state & NEIGHBOR_OVERLAPPED) == false && 
                        ((state & NEIGHBOR_OVERLAPPED) > 0 || ((known_state & NEIGHBOR_ENCLOSED) == 0 && (state & NEIGHBOR_ENCLOSED) > 0)))                        
                        notify_list.push_back (id);
                    
                    // store the state of this particular neighbor
                    known_list->insert (map<id_t, int>::value_type (id, state));
                }
            }

            // update known states about neighbors
            delete _neighbor_states[from_id];
            _neighbor_states[from_id] = known_list;

            // notify moving node of new neighbors
            if (_req_nodes.find (from_id) != _req_nodes.end ())
                sendNodes (from_id, notify_list);  
        }

        _req_nodes.clear ();
    }

    // check consistency of enclosing neighbors
    void
    VONPeer::checkConsistency (id_t skip_id)
    {
        // NOTE: must make a copy of the enclosing neighbors, as sendEN will also call get_en ()
        //vector<id_t> en_list = _Voronoi->get_en (in_msg.from);
        vector<id_t> en_list = _Voronoi->get_en (_self.id);
                                
        // notify my enclosing neighbors to check for discovery        
        for (size_t i=0; i<en_list.size (); i++)
        {
            id_t target = en_list[i];            
            if (target != skip_id)
                sendEN (target);
        }                      
    }

    // check for disconnection from neighbors no longer in view
    int  
    VONPeer::removeNonOverlapped ()
    {                  
        vector<id_t> delete_list;

        timestamp_t now = _net->getTimestamp ();
        timestamp_t grace_period = now + (MAX_DROP_SECONDS * _net->getTimestampPerSecond ());

        id_t id;
        // go over each neighbor and do an overlap check
        for (map<id_t, Node>::iterator it = _id2node.begin(); it != _id2node.end(); ++it)
        {
            id = it->first;
            
            // check if a neighbor is relevant (within AOI or enclosing)
            // or if the neighbor still covers me
            // NOTE: the AOI_BUFFER here should be slightly larger than that used for 
            //       neighbor discovery, this is so that if a node is recently disconnect
            //       here, other neighbors can re-notify should it comes closer again
            if (isSelf (id) ||
                (isRelevantNeighbor (_self, _id2node[id], (length_t)(_aoi_buffer*NONOVERLAP_MULTIPLIER)) && 
                 isTimelyNeighbor (id)))
            {
                _time_drop[id] = grace_period;
                continue;
            }
   
            // if current time exceeds grace period, then prepare to delete
            if (now >= _time_drop[id]) 
            {
                delete_list.push_back (id);
                _time_drop.erase (id);
            }
        }
        
        size_t n_deleted = delete_list.size ();
        
        // send BYE message to disconnected node
        if (n_deleted > 0)
        {
            Message msg (VON_BYE, _self.id);
            msg.priority = 0;
            msg.targets = delete_list;
            _net->sendVONMessage (msg, true);
        
            // perform node removal
            for (size_t i=0; i < n_deleted; i++)
                deleteNode (delete_list[i]);
        }

        return n_deleted;
    }

    bool
    VONPeer::insertNode (Node &node)
    {        
        // check for redundency
        if (isNeighbor (node.id))     
            return false;
          
        // notify network layer about connection info, no need to actually connect for now
        _net->notifyAddressMapping (node.id, node.addr);
        
        // update last access time
        //node.addr.lastAccessed = _tick_count;
        node.addr.lastAccessed = _net->getTimestamp ();

        _Voronoi->insert (node.id, node.aoi.center);        
        _id2node[node.id] = node;
        _neighbors.push_back (&_id2node[node.id]);
        _neighbor_states[node.id] = new map<id_t, int>;
        _time_drop[node.id] = 0;
        
        _updateStatus[node.id] = INSERTED;

        return true;
    }

    bool 
    VONPeer::deleteNode (id_t id)
    {
        
        // NOTE: it's possible to remove self or EN, use carefully.
        //       we don't check for that to allow force-remove in
        //       the case of disconnection by remote node, or 
        //       when leaving the overlay (removal of self)
        if (isNeighbor (id) == false)            
            return false;        
        
#ifdef DEBUG_DETAIL
        printf ("[%lu] disconnecting [%lu]\n", _self.id, id);
#endif

        _Voronoi->remove (id);
        vector<Node *>::iterator it = _neighbors.begin();
        for (; it != _neighbors.end (); it++)
        {
            if ((*it)->id == id)
            {
                _neighbors.erase (it);
                break;
            }
        }

        _id2node.erase (id);        
        delete _neighbor_states[id];
        _neighbor_states.erase (id);    
        _time_drop.erase (id);
                
        _updateStatus[id] = DELETED;

        return true;
    }

    bool 
    VONPeer::updateNode (Node &node)
    {        
        if (isNeighbor (node.id) == false)
            return false;
        
        // only update the node if it's at a later time
        if (node.time < _id2node[node.id].time)
            return false;

#ifdef DEBUG_DETAIL
            printf ("[%lu] updates [%lu] position: (%d, %d)\n", _self.id, node.id, (int)node.aoi.center.x, (int)node.aoi.center.y);
#endif

        _Voronoi->update (node.id, node.aoi.center);
        _id2node[node.id].update (node);   

        // NOTE: should not reset drop counter here, as it might make irrelevant neighbor 
        //       difficult to get disconnected
        //_time_drop[node.id] = 0;

        // update last access time
        //node.addr.lastAccessed = _tick_count;
        node.addr.lastAccessed = _net->getTimestamp ();

        // prevent only send updates for newly inserted nodes
        if (_updateStatus.find (node.id) == _updateStatus.end () || _updateStatus[node.id] != INSERTED)
            _updateStatus[node.id] = UPDATED;    

        return true;
    }

    // send node infos (NODE message) to a particular target
    void 
    VONPeer::sendNodes (id_t target, vector<id_t> &list, bool reliable)
    {
        listsize_t n = (listsize_t)list.size ();
        if (n == 0)
            return;

        Message msg (VON_NODE, _self.id);
        msg.priority = 0;
        msg.store (n);

#ifdef DEBUG_DETAIL
        printf ("[%lu] sends [%lu] NEIGHBORS: ", _self.id, target);
#endif 
        // TODO: sort the notify list by distance from target's center (better performance?)
        for (int i=0; i<(int)n; ++i)
        {
#ifdef DEBUG_DETAIL
            printf (" (%lu)", list[i]);
#endif
            msg.store (_id2node[list[i]]);
        }
#ifdef DEBUG_DETAIL
            printf ("\n");
#endif

		// check whether to send the NEIGHBOR via TCP or UDP
        msg.addTarget (target);
        _net->sendVONMessage (msg, reliable);
    }

    // send a list of IDs to a particular node
    void 
    VONPeer::sendHello (id_t target, vector<id_t> &id_list)
    {
        listsize_t n = (listsize_t)id_list.size ();
        if (n == 0)
            return;

        // TODO: do not create new HELLO message every time? (local-side optimization)
        Message msg (VON_HELLO, _self.id);
        msg.priority = 0;
        msg.store (_self);            
        msg.store (n);

#ifdef DEBUG_DETAIL
        printf ("[%lu] sends [%lu] IDs: ", _self.id, target);
#endif 
        for (int i=0; i<(int)n; ++i)
        {
#ifdef DEBUG_DETAIL
            printf (" (%lu)", id_list[i]);
#endif
            msg.store (id_list[i]);
        }
#ifdef DEBUG_DETAIL
        printf ("\n");
#endif
                
        msg.addTarget (target);
        _net->sendVONMessage (msg, true);
    }

    // send a particular node its perceived enclosing neighbors
    void 
    VONPeer::sendEN (id_t target)
    {
        vector<id_t> id_list = _Voronoi->get_en (target);

        listsize_t n = (listsize_t)id_list.size ();

        Message msg (VON_EN, _self.id);     
        msg.priority = 0;
        msg.store (n);      

#ifdef DEBUG_DETAIL
        printf ("[%lu] sends [%lu] IDs: ", _self.id, target);
#endif 
        for (size_t i=0; i<n; ++i)
        {
#ifdef DEBUG_DETAIL
            printf (" (%lu)", id_list[i]);
#endif
            msg.store (id_list[i]);
        }
#ifdef DEBUG_DETAIL
        printf ("\n");
#endif
                
        msg.addTarget (target);
        _net->sendVONMessage (msg, true);
    }

    bool 
    VONPeer::isNeighbor (id_t id)
    {
        return (_id2node.find (id) != _id2node.end ());
    }

    Position &
    VONPeer::isOverlapped (Position &pos)
    {
        // check for position overlap with neighbors and make correction
        map<id_t, Node>::iterator it; 
        for (it = _id2node.begin(); it != _id2node.end(); ++it)
        {
            if (isSelf (it->first))
                continue;
            if (it->second.aoi.center == pos)
            {
                // TODO: better movement?
                pos.x++;
                it = _id2node.begin();
            }
        }
        return pos;
    }

    // is a particular neighbor within AOI
    // NOTE: right now we only consider circular AOI, not rectangular AOI.. 
    inline bool 
    VONPeer::isAOINeighbor (id_t id, Node &neighbor, length_t buffer)
    {
        return _Voronoi->overlaps (id, neighbor.aoi.center, neighbor.aoi.radius + buffer, (_strict_aoi == false));
    }

    // whether a neighbor is either 1) AOI neighbor or 2) an enclosing neighbor
    inline bool 
    VONPeer::isRelevantNeighbor (Node &node1, Node &node2, length_t buffer)
    {
        return (_Voronoi->is_enclosing (node1.id, node2.id) || 
                isAOINeighbor (node1.id, node2, buffer) || 
                isAOINeighbor (node2.id, node1, buffer));                    
    }

    // whether a neighbor has stayed alive with regular updates
    // input period is in # of seconds
    inline bool 
    VONPeer::isTimelyNeighbor (id_t id, int period)
    {
        return true;
/*
        if (isNeighbor (id) == false)
            return false;

        timestamp_t timeout = period * _net->getTimestampPerSecond ();   
        //printf ("tick: %u last_acces: %u timeout: %u\n", _tick_count, _id2node[id].addr.lastAccessed, timeout);
        return ((_tick_count - _id2node[id].addr.lastAccessed) < timeout);
*/

        //return true;                
    }

    // check if a node is self
    inline bool 
    VONPeer::isSelf (id_t id)
    {
        return (_self.id == id);
    }

} // end namespace Vast
