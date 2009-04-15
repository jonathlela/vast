


#include "Manager.h"
#include "MessageQueue.h"

using namespace Vast;

namespace Vast
{   

    char MANAGER_MESSAGE[][20] = 
    {
        "DISCONNECT",
        "UNKNOWN",
        "QUERY",
        "HELLO",
        "EN",
        "MOVE",
        "MOVE_B",
        "NODE",        
    };

    // move the current manager position to a new one
    Position &
    Manager::move (Position &pos)
    {        
        id_t closest;

        // first check whether this is a delta move in comparison to current position,
        // or a JOIN-like move that requires querying for acceptor         
        if (_join_state == ABSENT)
        {            
            closest = _gateway.id;
            // TODO: change the BOGUS gateway address
            //connect (_gateway.addr);
            Addr gateway;
            memset (&gateway, 1, sizeof (gateway));     // bogus IP cannot be all 0 
            gateway.id = _gateway.id;
            connect (gateway);
        }
        else
            closest = _voronoi->closest_to (pos);
            
        if (_join_state == ABSENT) // || (pos.dist (_self.pos) > _self.aoi))
        {
            _join_state = JOINING;
         
            // TODO: change BOGUS self address (BOGUS IP cannot be NULL, or another connect will be invoke)
            _self.pos = pos;
            Addr addr;
            memset (&addr, 1, sizeof(Addr));
            addr.id = _self.id;
            Message msg (QUERY);            
            msg.store ((char *)&_self, sizeof (Node));
            msg.store ((char *)&addr, sizeof (Addr));
            
            sendMessage (closest, msg, true);
        }
        else
        {
            // send move messages to all connected neighbors

            // do necessary adjustments
            //adjustAOI ();
            removeNonOverlapped ();

            // check if my new position overlaps a neighbor, if so then move slightly
            _self.pos   = isOverlapped (pos);
            _self.time  = _net->get_timestamp ();   //_time;
            updateNode (_self);            

#ifdef DEBUG_DETAIL
            printf ("[%lu] sends position (%d, %d) to ", _self.id, (int)_self.pos.x, (int)_self.pos.y);
#endif            
            // notify all connected neighbors, pack my own node info & AOI info              
            id_t target;            
            Message msg (MOVE);
            msg.store ((char *)&_self, sizeof (Node));            

            // go over each neighbor and do a boundary neighbor check
            for (map<id_t, Node>::iterator it = _id2node.begin(); it != _id2node.end(); ++it)
            {
                if (isSelf (target = it->first))
                    continue;

                msg.msgtype = (_voronoi->is_boundary (target, _self.pos, _self.aoi) ? MOVE_B : MOVE);                
                sendMessage (target, msg, false);

#ifdef DEBUG_DETAIL
            printf (" %s (%lu)", MANAGER_MESSAGE[msg.msgtype], target);
#endif            
            }

#ifdef DEBUG_DETAIL
            printf ("\n");
#endif  
        }

        return _self.pos;
    }
        
    // perform initialization tasks for this handler (optional)
    // NOTE that all internal variables (such as handler_no) have been set at this point
    void 
    Manager::initHandler ()
    {
        // build correct self ID
        if (EXTRACT_HANDLER_NO(_self.id) == 0)
            _self.id = COMBINE_ID(_handler_no, _self.id);

        // TODO: no specific numbers
        // set to a initially very small radius
        _self.aoi = _DEFAULT_MANAGER_AOI;
        //_self.addr = getNetworkAddress (_self.id);
        //_self.addr.id = _self.id;
        
        // connect to gateway, and also add gateway as initial node inside the Voronoi
        // we assume:
        //      1. gateway's ID is NET_ID_GATWEAY, handler_no is 2 (second module hooked to MessageQueue after VAST object)
        //      2. gateway's position is at (0,0)
        //      3. gateway's IP is well-known (TODO: how do you learn of the real gateway IP/port?) 
        // TODO: better way (via config) for this?        
        _gateway.id     = COMBINE_ID(2, NET_ID_GATEWAY);
        _gateway.aoi    = _DEFAULT_MANAGER_AOI;
        //_gateway.addr.id = _gateway.id;
                
        if (_self.id == _gateway.id)
        {
            // otherwise we just assume we have joined (as the first gateway node)
            _join_state = JOINED;
            insertNode (_self);
        }
    }

    // returns whether the message was successfully handled
    bool 
    Manager::handleMessage (id_t from, Message &in_msg)
    {
#ifdef DEBUG_DETAIL
        printf ("[%lu] process [%lu] type: (%2d) %-12s size:%3d\n", 
        _self.id, from, in_msg.msgtype, MANAGER_MESSAGE[in_msg.msgtype], in_msg.size);
#endif
        // precaution before processing messages
        if (_join_state == ABSENT)
        {
            // return a query message back to its sender to not interrupt a join
            // TODO: find a better way to do this (should re-send join queries)
            if (in_msg.msgtype == QUERY)
                sendMessage (from, in_msg, true); 

            return false;
        }
        else if (_join_state == JOINING && in_msg.msgtype != NODE)
            return false;

        switch ((Manager_Message)in_msg.msgtype)
        {
        case QUERY:
            //if (in_msg.size == (sizeof (Node)))
            if (in_msg.size == (sizeof (Node) + sizeof (Addr)))
            {                
                // TODO: necessary to do the following?  because nodes will disconnect non-AOI neighbors (as long as gateway is one)
                // to prevent gateway server from over-connection
                if (_self.id == _gateway.id)
                    disconnect (from);

                Node joiner;
                Addr addr;
                in_msg.extract ((char *)&joiner, sizeof (Node));
                in_msg.extract ((char *)&addr, sizeof(Addr));
                
                id_t closest = _voronoi->closest_to (joiner.pos);

                // forward the request if a more approprite node exists
                if (_voronoi->contains (_self.id, joiner.pos) == false &&
                    isSelf (closest) == false &&
                    closest != from) 
                {                  
                    sendMessage (closest, in_msg, true);
                }                    
                else
                {
                    // I am the acceptor, send back initial neighbor list
                    vector<id_t> list;
                 
                    // insert first so we can properly find joiner's EN
                    insertNode (joiner, &addr);

                    // loop through all my known neighbors and determine which ones are relevant to notify
                    for (size_t i=0; i<_neighbors.size (); ++i)
                    {                            
                        if (_neighbors[i]->id != joiner.id && isRelevantNeighbor (_neighbors[i]->id, joiner))                            
                            list.push_back (_neighbors[i]->id);
                    }
                    
                    sendNodes (joiner.id, list, true);
                }
            }
            break;
        
        case HELLO:            
            if (in_msg.size == sizeof (Node))
            {
                Node node;
                in_msg.extract ((char *)&node, sizeof (Node));

                if (isNeighbor (from))
                    updateNode (node);
                else
                {
                    // accept all HELLO request
                    // we assume over-connections can be taken care later by adjust_aoi mechanism
                    insertNode (node);
                }
            }
            break;

        case EN:            
            if ((in_msg.size - sizeof(listsize_t)) % sizeof(id_t) == 0)
            {
                // ignore EN request if not properly connected
                if (isNeighbor (from) == false)
                    break;

                listsize_t n;
                in_msg.extract ((char *)&n, sizeof (listsize_t));
                
                map<id_t, int> *list = new map<id_t, int>;                // list of EN received                
                vector<id_t>    missing;                                  // list of missing EN ids (to be sent)
                vector<id_t>    &en_list = _voronoi->get_en (_self.id);   // list of my own EN
                
                size_t i;
                id_t id;
              
                // create a searchable list of EN
                for (i=0; i<n; ++i)
                {
                    in_msg.extract ((char *)&id, sizeof (id_t));
                    list->insert (map<id_t, int>::value_type (id, NEIGHBOR_OVERLAPPED));  
                }

                // store as initial known list 
                // csc 20080305: unnecessary to clear list, destructor should do it while deleting                
                // TODO: do we need to update the neighbor_states here?
                //       because during neighbor discovery checks, what each node knows would be calculated               
                delete _neighbor_states[from];
                _neighbor_states[from] = list;

                size_t en_size = en_list.size ();
                for (i=0; i<en_size; ++i)
                {
                    id = en_list[i];

                    // send only relevant missing neighbors, defined as
                    //  1) one of my enclosing neighbors but not in the EN list received
                    //  2) one of the sender node's relevant neighbors
                    //  3) not the sender node
                    if (list->find (id) == list->end () && isRelevantNeighbor (id, _id2node[from]) && id != from)
                        missing.push_back (id);
                }

                sendNodes (from, missing);
            }
            
            break;

        case MOVE:
        case MOVE_B:
            if (in_msg.size == sizeof (Node))
            {
                Node node;
                in_msg.extract ((char *)&node, sizeof (Node));
               
                // if the remote node has just been disconnected,
                // then no need to process MOVE message in queue
                if (updateNode (node) == false)
                    break;

                // records nodes requesting neighbor discovery check
                if (in_msg.msgtype == MOVE_B)
                    _req_nodes[from] = 1;
            }
            break;

        case NODE:
            if ((in_msg.size - sizeof (listsize_t)) % sizeof (Node) == 0)
            {
                listsize_t n;
                in_msg.extract ((char *)&n, sizeof (listsize_t));

                Node newnode;       // new node discovered
                
                // TODO: find a cleaner way than to check everytime
                if (_join_state = JOINING)
                {
                    _join_state = JOINED;
                    insertNode (_self);
                }

                // TODO: find a cleaner way to notify network (or.. network can learn this some other way)
                //vector<id_t> known_list;      // list of nodes to notify network which IP addresses does the sender know
                
                // store each node notified, we'll process them later in batch                                       
                for (int i=0; i<n; ++i)
                {   
                    _NODE_Message.total++;
                    in_msg.extract ((char *)&newnode, sizeof (Node));
                    
                    // potential BUG:
                    // NOTE: we're using time to decide if a newer node is better than existing one
                    //       but this requires remote nodes' clocks are well-synchronized
                    
                    // if an existing record already exists, then store only if newer
                    if (_new_neighbors.find (newnode.id) == _new_neighbors.end () ||
                        _new_neighbors[newnode.id].time <= newnode.time)
                    {
                        _NODE_Message.normal++;
                        // store the new notified node with my own time
                        // this is to ensure time-based disposal can work properly
                        //newnode.time = _net->get_timestamp (); 
                        _new_neighbors[newnode.id] = newnode;
                    }
                                        
                    //known_list.push_back (EXTRACT_NODE_ID(newnode.id));                   
                }

                // TODO: doesn't look like a clean way to maintain addresses
                //((MessageQueue *)_msgqueue)->notify_id_mapper (from, known_list);
            }
            break;

        case DISCONNECT:            
            if ((in_msg.size - sizeof(char)) % sizeof (id_t) == 0)
            {
                char n;
                in_msg.extract ((char *)&n, sizeof (char));
                id_t id;
                in_msg.extract ((char *)&id, sizeof (id_t));

                from = EXTRACT_NODE_ID(from);

                // loop through all known neighbors that belong to the lost network address
                // TODO: cleaner way?
                vector<id_t> delete_list;
                size_t i;
                for (i=0; i < _neighbors.size (); i++)
                {                                        
                    if (EXTRACT_NODE_ID(_neighbors[i]->id) == from)
                        delete_list.push_back (_neighbors[i]->id);                        
                }
                for (i=0; i<delete_list.size (); i++)
                    deleteNode (delete_list[i], false);
            }
            break;

        default:
            return false;
        }

        return true;
    }

    // performs some tasks the need to be done after all messages are handled
    // such as neighbor discovery checks
    void 
    Manager::postHandling ()
    {
        contactNewNeighbors ();
        checkNeighborDiscovery ();        
    }

    // notify new neighbors HELLO & EN messages
    void 
    Manager::contactNewNeighbors ()
    {
        //
        // new neighbor notification check
        //        
        vector<id_t> new_list;      // list of new, unknown nodes
        vector<id_t> remove_list;   // list of nodes to remove from the new neighbor list        
        
        Message hello_msg (HELLO);
        hello_msg.store ((char *)&_self, sizeof (Node));
        id_t target;

        // loop through each notified neighbor and see if it's unknown
        for (map<id_t, Node>::iterator it = _new_neighbors.begin (); it != _new_neighbors.end (); ++it)
        {            
            Node &new_node = it->second;
            target = new_node.id;
                        
            // check if it's already known
            // TODO: update_node should have time-stamp check
            if (isNeighbor (target))
            {                                  
                updateNode (new_node);

                // decrease counter, assuming notification means relevance
                if (_count_drop[target] > 0) 
                    _count_drop[target]--;          

                // remove this known neighbor from list
                remove_list.push_back (target);
            }            
            // if the notified node is already outdated, also ignore it
            // NOTE: seems like enabling the below would create very bad consistency
            /*
            else if ((_net->get_timestamp () - new_node.time) > MAX_DROP_COUNT)
            {
                remove_list.push_back (target);
            }   
            */
            else
            {                                
                _voronoi->insert (target, new_node.pos);            
                new_list.push_back (target);
            }
        }

        size_t i;
        
        // check through each newly inserted Voronoi for relevance                      
        for (i=0; i<new_list.size (); ++i)
        {
            target = new_list[i];

            // NOTE: it's important here that when inserting new node we should provide network address
            if (isRelevantNeighbor (target, _self, AOI_DETECTION_BUFFER) &&
                //insertNode (_new_neighbors[target], &_new_neighbors[target].addr) == true)
                insertNode (_new_neighbors[target]) == true)
            {                    
                // send HELLO & EN messages to newly discovered nodes                
                sendMessage (target, hello_msg, true, true);                                        
                sendIDs (target, EN, _voronoi->get_en (target));

                remove_list.push_back (target);
            }
            else
                // clear up the temporarily inserted test node from Voronoi (if not becoming neighbor)
                _voronoi->remove (target);
        }

        // remove notified nodes that are already known or newly inserted
        for (i=0; i<remove_list.size (); ++i)
            _new_neighbors.erase (remove_list[i]);
        
        // NOTE: erase new neighbors seems to bring better consistency (different from previous version of VAST)
        _new_neighbors.clear ();
    }


    // neighbor discovery check
    void
    Manager::checkNeighborDiscovery ()
    {

        vector<id_t> notify_list;       // list of neighbors to notify
        id_t from_id;

        //
        // perform neighbor discovery only for those that have requested 
        // (i.e., those that have sent a MOVE_B message)
        //

#ifdef CHECK_REQNODE_ONLY               
        for (map<id_t, int>::iterator it = _req_nodes.begin ();it != _req_nodes.end (); ++it)
        {
            from_id = it->first; 

            // a node requesting for check might be disconnected by now, as requests are processed in batch
            if (isNeighbor (from_id) == false)
                continue;
#else
        // we check for all known neighbors except myself
        int num = _neighbors.size ();
        int idx;
        for (idx=0; idx<num; ++idx)
        {
            from_id = _neighbors[idx]->id;

            if (isSelf (from_id))
                continue;
#endif

            // TODO: clear or new would be faster?
            notify_list.clear ();            
            map<id_t, int> *known_list = new map<id_t, int>;
            map<id_t, int>::iterator it;
            
            id_t id;
            int state, known_state;
              
#ifdef CHECK_EN_ONLY            
            // check for only enclosing neighbor
            vector<id_t> &en_list = _voronoi->get_en (_self.id);
            int n = en_list.size ();            
#else            
            // loop through every known neighbor except myself
            int n = _neighbors.size ();
#endif
            for (int i=0; i<n; ++i)
            {        

#ifdef CHECK_EN_ONLY
                id = en_list[i];
#else
                id = _neighbors[i]->id;
#endif                
                if (isSelf (id) || id == from_id)
                    continue;
                
                // do a simple test to see if this enclosing neighbor is
                // on the right side of me if I face the moving node directly
                // only notify the moving node for these 'right-hand-side' neighbors                
                
                //if (right_of (id, from_id) == false)
                //    continue;                

                state = 0;
                known_state = 0;
                if (isAOINeighbor (id, _id2node[from_id], AOI_DETECTION_BUFFER))                    
                    state = state | NEIGHBOR_OVERLAPPED;
                if (_voronoi->is_enclosing (id, from_id))
                    state = state | NEIGHBOR_ENCLOSED;

                // notify case1: new overlap by moving node's AOI
                // notify case2: new EN for the moving node                
                if (state != 0)
                {                                                           
                    if ((it = _neighbor_states[from_id]->find (id)) != _neighbor_states[from_id]->end ())
                        known_state = it->second;
                    
                    // note: what we want to achieve is:
                    //       1. notify just once when new EN is found
                    //       2. notify for new overlap, even if the node is known to be an EN

                    // the following is checking if the neighbor
                    //   not known to overlap previously but either 
                    //    1) currently overlapped, or... 2) previously wasn't enclosing, but now is
                    if ((known_state & NEIGHBOR_OVERLAPPED) == 0 && 
                        ((state & NEIGHBOR_OVERLAPPED) > 0 || ((known_state & NEIGHBOR_ENCLOSED) == 0 && (state & NEIGHBOR_ENCLOSED) > 0)))                        
                        notify_list.push_back (id);

                    // store the state of this particular neighbor
                    known_list->insert (map<id_t, int>::value_type (id, state));
                }
            }
                        
            delete _neighbor_states[from_id];
            _neighbor_states[from_id] = known_list;

            // notify moving nodes of new neighbors
            if (_req_nodes.find (from_id) != _req_nodes.end ())
                sendNodes (from_id, notify_list);  
        }

        _req_nodes.clear ();
    }

    // check for disconnection from neighbors no longer in view
    int  
    Manager::removeNonOverlapped ()
    {                  
        vector<id_t> delete_list;

        id_t id;
        // go over each neighbor and do an overlap check
        for (map<id_t, Node>::iterator it = _id2node.begin(); it != _id2node.end(); ++it)
        {
            id = it->first;
            
            // check if a neighbor is relevant (within AOI or enclosing)
            if (isSelf (id) || isRelevantNeighbor (id, _self))
            {
                _count_drop[id] = 0;
                continue;
            }

            // record a list of non-overlapped neighbors 
            if (++_count_drop[id] > MAX_DROP_COUNT)
            {
                // if remote node also no longer covers me, then remove it
                if (isAOINeighbor (_self.id, _id2node[id], AOI_DETECTION_BUFFER))
                {
                    _count_drop[id] = 0;
                    continue;
                }
                           
                // TODO: will it cause problem for running the list of neighbors? 
                delete_list.push_back (id);                
            }
        }
        
        size_t n_deleted = delete_list.size ();
        for (size_t i=0; i<n_deleted; i++)
            deleteNode (delete_list[i]);
       
        return n_deleted;
    }


    bool
    Manager::insertNode (Node &node, Addr *addr)
    {
        
        // check for redundency
        if (isNeighbor (node.id))     
            return false;
            
        // avoid self-connection, but alway connect
        // NOTE: connection may already exist with the remote node, but insertion will continue (instead of abort)
        if (isSelf (node.id) == false)
        {    
            // setup an empty address for connection
            if (addr == NULL)
            {
                addr = &_tempAddr;
                memset (addr, 0, sizeof(Addr));
                addr->id = node.id;
            }

            // if connection fails then abort insertion
            if (connect (*addr) == (-1))         
                return false;
        }

#ifdef DEBUG_DETAIL
        printf ("[%lu] inserts [%lu]\n", _self.id, node.id);
#endif
        _voronoi->insert (node.id, node.pos);        
        _id2node[node.id] = node;
        _neighbors.push_back (&_id2node[node.id]);
        _neighbor_states[node.id] = new map<id_t, int>;        
        _count_drop[node.id] = 0;

        return true;
    }

    bool 
    Manager::deleteNode (id_t id, bool cut_connection)
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
        if (cut_connection == true)
            disconnect (id);

        _voronoi->remove (id);
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
        _count_drop.erase (id);
        
        return true;
    }

    bool 
    Manager::updateNode (Node &node)
    {        
        if (isNeighbor (node.id) == false)
            return false;
        
        // TODO: only update the node if it's at a later time?
        if (_id2node[node.id].time <= node.time)
        {
#ifdef DEBUG_DETAIL
            printf ("[%lu] updates [%lu] position: (%d, %d)\n", _self.id, node.id, (int)node.pos.x, (int)node.pos.y);
#endif
            _voronoi->update (node.id, node.pos);
            _id2node[node.id].update (node);   
        }
        return true;
    }

    // send node infos (NODE message) to a particular target
    void 
    Manager::sendNodes (id_t target, vector<id_t> &list, bool reliable)
    {
        listsize_t n = (listsize_t)list.size ();
        if (n == 0)
            return;

        Message msg (NODE);
        msg.store ((char *)&n, sizeof (listsize_t));

#ifdef DEBUG_DETAIL
        printf ("[%lu] sends [%lu] NODES: ", _self.id, target);
#endif 
        // TODO: sort the notify list by distance from target's center (better performance?)
        for (int i=0; i<(int)n; ++i)
        {
#ifdef DEBUG_DETAIL
            printf (" (%lu)", list[i]);
#endif
            msg.store ((char *)&_id2node[list[i]], sizeof (Node));
        }
#ifdef DEBUG_DETAIL
            printf ("\n");
#endif

		// check whether to send the NODE via TCP or UDP
        sendMessage (target, msg, reliable, true);
    }

    // send a list of IDs to a particular node
    void 
    Manager::sendIDs (id_t target, msgtype_t msgtype, vector<id_t> &id_list)
    {
        listsize_t n = id_list.size ();
        if (n == 0)
            return;
                
        Message msg (msgtype);
        msg.store ((char *)&n, sizeof (listsize_t));

#ifdef DEBUG_DETAIL
        printf ("[%lu] sends [%lu] IDs: ", _self.id, target);
#endif 
        for (int i=0; i<(int)n; ++i)
        {
#ifdef DEBUG_DETAIL
            printf (" (%lu)", id_list[i]);
#endif
            msg.store ((char *)&id_list[i], sizeof (id_t));
        }
#ifdef DEBUG_DETAIL
            printf ("\n");
#endif
        
        // we should sent even if there's just one EN, as EN message helps to discover missing neighbors  
        sendMessage (target, msg, true, true);
    }

    bool 
    Manager::isNeighbor (id_t id)
    {
        return (_id2node.find (id) != _id2node.end ());
    }

    Position &
    Manager::isOverlapped (Position &pos)
    {
        // check for position overlap with neighbors and make correction
        map<id_t, Node>::iterator it; 
        for (it = _id2node.begin(); it != _id2node.end(); ++it)
        {
            if (isSelf (it->first))
                continue;
            if (it->second.pos == pos)
            {
                // TODO: better movement?
                pos.x++;
                it = _id2node.begin();
            }
        }
        return pos;
    }

} // end namespace Vast
