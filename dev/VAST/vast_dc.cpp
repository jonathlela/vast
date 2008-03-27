/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005 Shun-Yun Hu (syhu@yahoo.com)
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


#include "vast_dc.h"
#include "vor_SF.h"

namespace VAST
{
#define CHECK_EN_ONLY               // when doing neighbor discovery checks, 
                                    // a boundary neighbor checks only its enclosing neighbors as opposed to all AOI neighbors
#define CHECK_REQNODE_ONLY_         
        
    char VAST_DC_MESSAGE[][20] = 
    {
//        "DISCONNECT",
//        "ID",
        "QUERY",
        "HELLO",
        "EN",
        "MOVE",
        "MOVE_B",
        "NODE",
        "OVERCAP",
        "UNKNOWN"
    };
    
    // constructor 
    vast_dc::vast_dc (network *netlayer, aoi_t detect_buffer, int conn_limit)
        :_detection_buffer (detect_buffer), _connlimit (conn_limit)
    {
        
        this->setnet (netlayer);

        _net->start ();
        
        _count_dAOI = MAX_ADJUST_COUNT;
        
        _NM_total = _NM_known = 0;
        
        _voronoi = new vor_SF ();
    }            

    // destructor cleans up any previous allocation
    vast_dc::
    ~vast_dc ()
    {    
        if (_joined == true)
            this->leave ();

        // delete allocated memory                
        map<id_t, Node>::iterator it = _id2node.begin ();
        for (; it != _id2node.end (); it++)
            delete _neighbor_states[it->first];

        delete _voronoi;
        _net->stop ();
    }

    // join VON to obtain unique id
    // NOTE: join is considered complete ('_joined' is set) only after node id is obtained
    bool
    vast_dc::join (id_t id, aoi_t AOI, Position &pos, Addr &gateway)
    {   
#ifdef DEBUG_DETAIL
        printf ("[%d] attempt to join at (%d, %d)\n", (int)id, (int)pos.x, (int)pos.y);
#endif
        if (_joined == true)
            return false;

        _original_aoi     = AOI;
        _self.id          = id;
        _self.aoi         = AOI;
        _self.pos         = pos;

        //this should be done in vastid::handlemsg  while receiving an ID from gateway
         _net->register_id (id);

        insert_node (_self);//, _net->getaddr (_self.id));

        // the first node is automatically considered joined        
        if (id == NET_ID_GATEWAY)
            _joined = true;
        else
        {
            // send query to find acceptor if I'm a regular peer
            Msg_QUERY info (_self, _net->getaddr (_self.id));

            if (_net->connect (gateway) == (-1))
                return false;

            // NOTE: gateway will disconnect me immediately after receiving QUERY
            _net->sendmsg (NET_ID_GATEWAY, DC_QUERY, (char *)&info, sizeof (Msg_QUERY));
        }

        return true;
    }

    // quit VON
    void    
    vast_dc::leave ()
    {
        // remove & disconnect all connected nodes (include myself)
        vector<id_t> remove_list;
        map<id_t, Node>::iterator it = _id2node.begin ();
        for (; it != _id2node.end (); it++)
            if (it->first != _self.id)
                remove_list.push_back (it->first);

        int size = remove_list.size ();
        for (int i=0; i<size; ++i)
            delete_node (remove_list[i]);
        
        _joined = false;
    }

    // AOI related functions
    void    
    vast_dc::setAOI (aoi_t radius)
    {
        _self.aoi = radius;
    }	

    aoi_t   
    vast_dc::getAOI ()
    {
        return _self.aoi;
    }
    
    // move to a new position
    Position & 
    vast_dc::setpos (Position &pt)
    {                
        // do not move if we havn't joined (no neighbors are known unless I'm gateway)
        if (_joined == true)
        {
            // do necessary adjustments
            adjust_aoi ();
            remove_nonoverlapped ();
            
            // check for position overlap with neighbors and make correction
            map<id_t, Node>::iterator it; 
            for (it = _id2node.begin(); it != _id2node.end(); ++it)
            {
                if (it->first == _self.id)
                    continue;
                if (it->second.pos == pt)
                {
                    pt.x++;
                    it = _id2node.begin();
                }
            }            

            // update location information
            //double dis = _self.pos.dist (pt);
            _self.pos = pt;
            _self.time = _net->get_curr_timestamp ();//_time;
            update_node (_self);
            
#ifdef DEBUG_DETAIL
            printf ("[%3d] setpos (%d, %d)\n", (int)_self.id, (int)_self.pos.x, (int)_self.pos.y);
#endif            
            // notify all connected neighbors
            msgtype_t msgtype;            
            id_t target_id;
            
            // go over each neighbor and do a boundary neighbor check
            int n = 0;
            for (it = _id2node.begin(); it != _id2node.end(); ++it)
            {
                if ((target_id = it->first) == _self.id)
                    continue;
                
                if (_voronoi->is_boundary (target_id, _self.pos, _self.aoi) == true)
                    msgtype = DC_MOVE_B;
                else
                    msgtype = DC_MOVE;
                
                _net->sendmsg (target_id, (msgtype_t)msgtype, (char *)&_self, sizeof (Node), false);
                n++;
            }
        }
        
        return _self.pos;
    }    

    // process a single message in queue
    bool 
    vast_dc::handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size)
    {
#ifdef DEBUG_DETAIL
        printf ("%4d [%3d] processmsg from: %3d type: (%2d)%-12s size:%3d\n", (int)_net->get_curr_timestamp (), (int)_self.id, (int)from_id, 
                msgtype, (msgtype>=10 && msgtype<=DC_UNKNOWN)?VAST_DC_MESSAGE[msgtype-10]:"UNKNOWN", size);
#endif
        switch ((VAST_DC_Message)msgtype)
        {
        case DC_QUERY:
            if (size == sizeof (Msg_QUERY))
            {
                // to prevent gateway server from over-connection
                if (_self.id == NET_ID_GATEWAY)
                    _net->disconnect (from_id);

                Msg_QUERY n (msg);
                Node &joiner = n.node;
                
                id_t closest_id = (id_t)_voronoi->closest_to (joiner.pos);

                // forward the request if a more approprite node exists
                if (_voronoi->contains (_self.id, joiner.pos) == false &&
                    closest_id != _self.id &&
                    closest_id != from_id) 
                {                    
                    _net->sendmsg (closest_id, DC_QUERY, msg, size, true, true);
                }
                    
                else
                {
                    // I am the acceptor, send back initial neighbor list
                    vector<id_t> list;
                 
                    // insert first so we can properly find joiner's EN
                    // TODO: what if joiner exceeds connection limit?
                    //       should remove closest to AOI
                    //
                    insert_node (joiner, &n.addr);
                    int size = _neighbors.size (); 
                    for (int i=0; i<size; ++i)
                    {
                        if (_neighbors[i]->id == joiner.id)
                            continue;
                            
                        // send only relevant neighbors
                        if (_voronoi->overlaps (_neighbors[i]->id, joiner.pos, joiner.aoi) ||
                            _voronoi->is_enclosing (_neighbors[i]->id, joiner.id))
                            list.push_back (_neighbors[i]->id);
                    }
                    
                    send_nodes (joiner.id, list, true);
                }
            }
            break;

        case DC_HELLO:
            if (size == sizeof (Msg_NODE))
            {
                Msg_NODE newnode(msg);

                if (is_neighbor (from_id))
                    update_node (newnode.node);
                else
                {
                    // accept all HELLO request
                    // we assume over-connections can be taken care
                    // later by adjust_aoi mechanism
                    insert_node (newnode.node);//, newnode.addr);                                  
                }
            }
            break;

        case DC_EN:
            if ((size-1) % sizeof(id_t) == 0)
            {
                // ignore EN request if not properly connected
                if (is_neighbor (from_id) == false)
                    break;

                int n = (int)msg[0];
                char *p = msg+1;

                // BUG: potential memory leak for the allocation of map?
                map<id_t, int>  *list = new map<id_t, int>; // list of EN received
                
                vector<id_t>    missing;   // list of missing EN ids                                                
                vector<id_t> &en_list = _voronoi->get_en (_self.id); // list of my own EN
                
                int i;
                // create a searchable list of EN (BUG, mem leak for insert?)                   
                for (i=0; i<n; ++i, p += sizeof (id_t))
                    list->insert (map<id_t, int>::value_type (*(id_t *)p, STATE_OVERLAPPED));  

                // store as initial known list (clear necessary to prevent memory leak?)
                // csc 20080305: unnecessary, destructor should do it while deleting
                //_neighbor_states[from_id]->clear ();
                delete _neighbor_states[from_id];
                _neighbor_states[from_id] = list;

                int en_size = en_list.size ();
                for (i=0; i<en_size; ++i)
                {
                    // send only relevant missing neighbors
                    if (list->find (en_list[i]) == list->end () && 
                        en_list[i] != from_id &&                            
                        _voronoi->overlaps (en_list[i], _id2node[from_id].pos, _id2node[from_id].aoi))
                        missing.push_back (en_list[i]);
                }

                send_nodes (from_id, missing);
            }
            break;

        case DC_MOVE:
        case DC_MOVE_B:
            if (size == sizeof (Node))
            {
                Node *node = (Node *)msg;

                // if the remote node has just been disconnected,
                // then no need to process MOVE message in queue
                if (update_node (*node) == false)
                    break;

                // records nodes requesting neighbor discovery check
                if (msgtype == DC_MOVE_B)
                    _req_nodes[from_id] = 1;
                    //req_nodes.push_back (from_id);

                /*
                // prevent incorrect judgement when later I become a BN
                else
                    _neighbor_states[from_id]->clear ();
                */
            }
            break;

        case DC_NODE:
            if ((size-1) % sizeof (Msg_NODE) == 0)
            {
                int n = (int)msg[0];
                char *p = msg+1;
                Msg_NODE newnode;                    // new node discovered

                // TODO: find a cleaner way than to reset everytime
                _joined = true;
                
                // store each node notified, we'll process them later in batch
                int i;
                bool store;
                map<id_t, Msg_NODE>::iterator it;

                for (i=0; i<n; ++i, p += sizeof (Msg_NODE))
                {   
                    _NM_total++;
                    
                    memcpy (&newnode, p, sizeof (Msg_NODE));

                    store = true;
                    it = _notified_nodes.find (newnode.node.id);
                    if (it != _notified_nodes.end ())
                    {
                        _NM_known++;
                        
                        // potential BUG:
                        // NOTE: we're using time to decide if using a newer one to replace
                        //       an existing notified node, however this might give
                        //       false prefererence if remote nodes' clocks are not
                        //       well-synchronized
                        if (newnode.node.time < (it->second).node.time)
                            store = false;
                    }
                    
                    if (store) 
                    {
                        // store the new notified node with my own time
                        // this is to ensure time-based disposal can work properly
                        newnode.node.time = _net->get_curr_timestamp (); //_time;
                        _notified_nodes[newnode.node.id] = newnode;
                    }

                    /*
                    if (_notified_nodes.find (newnode.node.id) != _notified_nodes.end ())
                    {
                        _NM_known++;
                        if (_notified_nodes[newnode.node.id].node.time < newnode.node.time)
                            _notified_nodes[newnode.node.id] = newnode;
                    }
                    else 
                        _notified_nodes[newnode.node.id] = newnode;                        
                    */

                    // notify network knowledge source of IP address
                    // TODO: queue all nofities in the same step
                    vector<id_t> idlist;
                    idlist.push_back (newnode.node.id);
                    _net->notify_id_mapper (from_id, idlist);
                }
            }
            break;

        case DC_OVERCAP:
            if (size == 0)
            {
                if(is_neighbor (from_id) == false)
                    break;

                // remote node has exceeded its capacity, 
                // we need to shrink our own AOI
                adjust_aoi (&_id2node[from_id].pos);
            }
            break;
        /*
        case DC_PAYLOAD:
            {                    
                // create a buffer
                netmsg *newnode = new netmsg (from_id, msg, size, msgtype, recvtime, NULL, NULL);

                // put the content into a per-neighbor queue
                if (_id2msg.find (from_id) == _id2msg.end ())
                    _id2msg[from_id] = newnode;
                else
                    _id2msg[from_id]->append (newnode, recvtime);
            }
            break;
        */

        case DISCONNECT:
            if ((size-1) % sizeof (id_t) == 0)
            {
                int n = msg[0];
                char *p = msg+1;
                id_t id;

                for (int i=0; i<n; ++i, p+=sizeof (id_t))
                {
                    memcpy (&id, p, sizeof (id_t));
                    
                    // see if it's a remote node disconnecting me
                    if (id == _self.id)
                        delete_node (from_id, false);                        
                    /*
                    // delete this node from list of known neighbors
                    else 
                    {
                      
                        map<id_t, int> *list;
                        map<id_t, int>::iterator it;
                        if (_neighbor_states.find (from_id) != _neighbor_states.end ())
                        {
                            list = _neighbor_states[from_id];
                            if ((it = list->find (id)) != list->end ())
                                list->erase (it);                                
                        }
                      
                    }
                    */
                }
                
            }
            // allow other handlers to handle the DISCONNECT message
            return false;

        default:
#ifdef DEBUG_DETAIL
            // throw to additional message handler, if it exists
            ERROR_MSG_ID (("unknown message"));
#endif
            return false;
        }

        return true;
    }


    // do neighbor discovery check for AOI neighbors
    //void 
    //vast_dc::check_neighbor_discovery ()
    void vast_dc::post_processmsg ()
    {

        //
        // new neighbor notification check
        //        
        vector<id_t> new_list;      // list of new, unknown nodes
        vector<id_t> hello_list;    // list of new nodes to connect
        vector<id_t> remove_list;   // list of nodes to remove from the notified list
        map<id_t, Msg_NODE>::iterator it = _notified_nodes.begin ();
        
        // loop through each notified neighbor and see if it's unknown
        for (; it != _notified_nodes.end (); ++it)
        {
            Msg_NODE &newnode = it->second;
            Node &node = newnode.node;
                        
            // check if it's already known
            // TODO: update_node should have time-stamp check
            if (is_neighbor (node.id) == true)
            {
                _NM_known++;         
                // NOTE: do not update, as an older value might replace what's already
                //       on record, causing larger drift distance
                //update_node (node);
                
                // decrease counter, assuming notification means relevance
                if (_count_drop[node.id] > 0) 
                    _count_drop[node.id]--;          

                // remove this known neighbor from list
                remove_list.push_back (node.id);
            }
            // if the notified node is already outdated, also ignore it
            else if ((/*_time*/ _net->get_curr_timestamp () - newnode.node.time) > MAX_DROP_COUNT)
            {
                remove_list.push_back (node.id);
            }
            else
            {
                _voronoi->insert (node.id, node.pos);
                new_list.push_back (node.id);
            }
        }                               

        // loop through newly inserted nodes and check for relevance
        int n = new_list.size ();
        int i;

        for (i=0; i<n; ++i)
        {
            //
            // we will not connect to a new node if:
            //  1) the node is irrelevant (outside of AOI)
            //  2) connection limit has reached
            //                                                                           
            // NOTE that we've added '1' units to the overlap test to avoid roundoff errors
            
            if (_voronoi->is_enclosing (new_list[i]) == true ||
                _voronoi->overlaps (new_list[i], _self.pos, (aoi_t)(_self.aoi + _detection_buffer + 1)))
            {                
/*
                if (over_connected () == true)
                {
                    // shrink the AOI to exclude the new node
                    adjust_aoi (&node.pos);
                    continue;
                }
*/
                hello_list.push_back (new_list[i]);
            }                                  
        }

        // clear up voronoi first
        for (i=0; i<n; ++i)
            _voronoi->remove (new_list[i]);
                                                   
        // send HELLO & EN messages to newly discovered nodes
        n = hello_list.size ();
        Msg_NODE hello (_self);//, _net->getaddr (_self.id));   // HELLO message to be sent        
        id_t target;
        for (i=0; i<n; ++i)
        {
            target = hello_list[i];
            Msg_NODE &newnode = _notified_nodes[target];
/*
#ifdef DEBUG_DETAIL
            char ip[16];
            newnode.addr.publicIP.get_string (ip);
            printf ("[%d] learn about new node [%d] (%s:%d)\n", (int)_self.id, (int)newnode.node.id, ip, (int)newnode.addr.publicIP.port);
#endif            
*/
            if (insert_node (newnode.node/*, newnode.addr*/) == true)
            {             
                // send HELLO
                _net->sendmsg (target, DC_HELLO, (char *)&hello, sizeof (Msg_NODE), true, true);
                
                // send EN
                vector<id_t> &en_list = _voronoi->get_en (target);
                send_ID (target, DC_EN, en_list);                
            }            

            // remove this node from those being considered
            remove_list.push_back (target);            
        }
        
        // remove notified nodes that are outdated or already known        
        n = remove_list.size ();
        for (i=0; i<n; ++i)
            _notified_nodes.erase (remove_list[i]);
        
        // NOTE: simply clear out notified nodes gives lower Topology Consistency
        //_notified_nodes.clear ();

        //
        // neighbor discovery check
        //        
        vector<id_t> notify_list;
        id_t from_id;

#ifdef CHECK_REQNODE_ONLY         
        map<id_t, int>::iterator it2 = _req_nodes.begin ();
        for (;it2 != _req_nodes.end (); ++it2)
        {
            from_id = it2->first; 
            // a node requesting for new neighbor check 
            // might have been disconnected by now,  
            // as we process the request in batch
            if (is_neighbor (from_id) == false)
                continue;
#else
        // we check for all known neighbors except myself
        int num = _neighbors.size ();
        int idx;
        for (idx=1; idx<num; ++idx)
        {
            from_id = _neighbors[idx]->id;
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
                if (id == _self.id || id == from_id)
                    continue;
                
                // do a simple test to see if this enclosing neighbor is
                // on the right side of me if I face the moving node directly
                // only notify the moving node for these 'right-hand-side' neighbors                
                //if (right_of (id, from_id) == false)
                //    continue;                

                state = 0;
                known_state = 0;
                if (_voronoi->overlaps (id, _id2node[from_id].pos, (aoi_t)(_id2node[from_id].aoi + _detection_buffer + 1)) == true)
                    state = state | STATE_OVERLAPPED;
                if (_voronoi->is_enclosing (id, from_id) == true)
                    state = state | STATE_ENCLOSED;

                // notify case1: new overlap by moving node's AOI
                // notify case2: new EN for the moving node                
                if (state != 0)
                {                                                           
                    if ((it = _neighbor_states[from_id]->find (id)) != _neighbor_states[from_id]->end ())
                        known_state = it->second;
                    
                    // note: what we want to achieve is:
                    //       1. notify just once when new EN is found
                    //       2. notify for new overlap, even if the node is known to be an EN
                    if ((known_state & STATE_OVERLAPPED) == 0 && 
                        ((state & STATE_OVERLAPPED) > 0 || ((known_state & STATE_ENCLOSED) == 0 && (state & STATE_ENCLOSED) > 0)))                        
                        notify_list.push_back (id);
                                                                
                    // store the state of this particular neighbor
                    known_list->insert (map<id_t, int>::value_type (id, state));
                }
            }
            
            //_neighbor_states[from_id]->clear ();
            delete _neighbor_states[from_id];
            _neighbor_states[from_id] = known_list;

            // notify moving nodes of new neighbors
            if (_req_nodes.find (from_id) != _req_nodes.end ())
                send_nodes (from_id, notify_list);                
        }

        _req_nodes.clear ();

		//=================================================================================================================

		// send out all pending reliable messages
        _net->flush ();
    }
        
/*
    int 
    vast_dc::recv (id_t target, char *buffer)
    {
        if (_joined == false)
        {
            printf ("[%d] attempt to receive before joined successfully\n", _self.id);
            return 0;            
        }

        if (_id2msg.find (target) == _id2msg.end ())
            return 0;
        
        netmsg *queue = _id2msg[target];
        netmsg *msg;
        queue = queue->getnext (&msg);

        // update the mapping to queue
        if (queue == NULL)
            _id2msg.erase (target);
        else
            _id2msg[target] = queue;
        
        // copy the msg into buffer
        int size = msg->size;
        memcpy (buffer, msg->msg, size);
        delete msg;

        return size;
    }
*/

    //
    //  private methods
    //

    bool
    vast_dc::insert_node (Node &node, Addr *addr)
    {
        // check for redundency
        if (is_neighbor (node.id))     
            return false;
            
        // avoid self-connection
        // note: connection may already exist with remote node, in which case the
        //       insert_node process will continue (instead of aborting..)
        if (node.id != _self.id && !(_net->is_connected (node.id)))
        {
            int result = (addr != NULL ? _net->connect (*addr) : _net->connect (node.id));
            if (result == (-1))
                return false;
        }

        _voronoi->insert (node.id, node.pos);        
        _id2node[node.id] = node;
        _id2vel[node.id] = 0;
        _neighbors.push_back (&_id2node[node.id]);
        _neighbor_states[node.id] = new map<id_t, int>;        
        _count_drop[node.id] = 0;

        return true;
    }


/*
    // overloaded version for the node's address is prefetched
    bool
    vast_dc::insert_node (Node &node, Addr &addr)
    {
        // check for redundency
        if (is_neighbor (node.id))     
            return false;
            
        if ( node.id != _self.id && !(_net->is_connected (node.id)) )
            if (_net->connect (addr) == (-1))
                return false;

        return _post_insert_node (node);
    }

    bool 
    vast_dc::insert_node (Node &node)
    {
        // check for redundency
        if (is_neighbor (node.id))     
            return false;
            
        // avoid self-connection
        // note: connection may already exist with remote node, in which case the
        //       insert_node process will continue (instead of aborting..)
        if ( node.id != _self.id && !(_net->is_connected (node.id)) )
            if (_net->connect (node.id) == (-1))
                return false;

        return _post_insert_node (node);
    }

    bool
    vast_dc::_post_insert_node (Node &node)
    {
        _voronoi->insert (node.id, node.pos);        
        _id2node[node.id] = node;
        _id2vel[node.id] = 0;
        _neighbors.push_back (&_id2node[node.id]);
        _neighbor_states[node.id] = new map<id_t, int>;        
        _count_drop[node.id] = 0;

        return true;
    }

*/

    bool 
    vast_dc::delete_node (id_t id, bool disconnect)
    {
        // NOTE: it's possible to remove self or EN, use carefully.
        //       we don't check for that to allow force-remove in
        //       the case of disconnection by remote node, or 
        //       when leaving the overlay (removal of self)
        if (is_neighbor (id) == false)            
            return false;        
        
#ifdef DEBUG_DETAIL
        printf ("[%d] disconnecting [%d]\n", (int)_self.id, (int)id);
#endif
        if (disconnect == true)
            _net->disconnect (id);

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
        _id2vel.erase (id);
        delete _neighbor_states[id];
        _neighbor_states.erase (id);        
        _count_drop.erase (id);

        return true;
    }

    bool 
    vast_dc::update_node (Node &node)
    {
        if (is_neighbor (node.id) == false)
            return false;
        
        // only update the node if it's at a later time
        if (_id2node[node.id].time <= node.time)
        {
            // calculate displacement from last position
            double dis = _id2node[node.id].pos.dist (node.pos);
            _voronoi->update (node.id, node.pos);
            _id2node[node.id].update (node);
            _id2vel[node.id] = dis;            
        }

        return true;
    }

    // send node infos (NODE message) to a particular node
    // 'list' is a list of indexes to _neighbors    
    void 
    vast_dc::send_nodes (id_t target, vector<id_t> &list, bool reliable)
    {
        int n = list.size ();
        if (n == 0)
            return;

        // TODO: sort the notify list by distance from target's center
        _buf[0] = (unsigned char)n;
        char *p = _buf+1;

#ifdef DEBUG_DETAIL
        printf ("                                      aoi: %d ", (aoi_t)(_id2node[target].aoi + _detection_buffer));
#endif
        
        Msg_NODE node;
        for (int i=0; i<n; ++i, p += sizeof (Msg_NODE))
        {                  
            node.set (_id2node[list[i]]);//, _net->getaddr (list[i]));
/*
#ifdef DEBUG_DETAIL
            char ip[16];
            node.addr.publicIP.get_string (ip);
            printf ("[%d] (%s:%d) ", (int)list[i], ip, (int)node.addr.publicIP.port);
#endif 
*/
            memcpy (p, (char *)&node, sizeof (Msg_NODE));
        }

#ifdef DEBUG_DETAIL
        printf ("\n");
#endif

		// check whether to send the NODE via TCP or UDP		
		_net->sendmsg (target, DC_NODE, _buf, 1 + n * sizeof (Msg_NODE), reliable, true);

    }

    // send to target node a list of IDs
    void 
    vast_dc::send_ID (id_t target, msgtype_t msgtype, vector<id_t> &id_list)
    {
        int n = id_list.size ();
        char *p = _buf+1;
                
        for (int j=0; j<n; ++j, p += sizeof (id_t))
            memcpy (p, &id_list[j], sizeof (id_t));
        
        // we should sent even if there's just one EN
        // because EN message will help to discover missing neighbors
        _buf[0] = (unsigned char)n;
        _net->sendmsg (target, msgtype, (char *)&_buf, 1 + n * sizeof (id_t), /*_time,*/ true, true);
    }

    // dynamic AOI adjustment to keep transmission bounded
    void 
    vast_dc::adjust_aoi (Position *invoker)
    {
        if (_connlimit == 0)
            return;
        
        // see if adjustment was being invoked
        if (invoker != NULL)
        {            
            // adjust only if delay countdown has reached
            if (--_count_dAOI > 0)
                return;

            // find new AOI-radius that keeps invoker out of AOI
            aoi_t new_aoi = (aoi_t)_self.pos.dist (*invoker);
            if (new_aoi < (_self.aoi + _detection_buffer))
            {
                // do an immediate shrink
                //_self.aoi = (aoi_t)(((double)new_aoi - _detection_buffer) * (1-RATIO_AOI_ADJUST));
                _self.aoi = (aoi_t)(((double)new_aoi - _detection_buffer) - AOI_ADJUST_SIZE);
                if (_self.aoi <= 5)
                    _self.aoi = 5;
            }
        }

        // normal adjustment mechanism
        else
        {
            bool shrink = false;        
            
            // see if we're already at equilibrium
            if (over_connected ())
                shrink = true;
            else if (_self.aoi == _original_aoi)
            {
                // we're already at equilibrium
                _count_dAOI = MAX_ADJUST_COUNT;
                return;            
            }            
            
            // adjust only if delay countdown has reached
            if (--_count_dAOI > 0)
                return;            
            
            // calculate adjustment 
            //aoi_t adjust = (aoi_t)((double)_self.aoi * RATIO_AOI_ADJUST);
            aoi_t adjust = (aoi_t)AOI_ADJUST_SIZE;
            if (adjust == 0)
                adjust = 1;        
            
            // shrink AOI
            if (shrink == true)
            {            
                _self.aoi -= adjust;            
                if (_self.aoi <= 5)
                    _self.aoi = 5;
            }
            // enlarge AOI
            else
            {            
                _self.aoi += adjust;            
                if (_self.aoi > _original_aoi)
                    _self.aoi = _original_aoi;         
            }
        }

        // adjustment done, reset counter
        _count_dAOI = MAX_ADJUST_COUNT;        
    }

    // check for disconnection from neighbors no longer in view
    int  
    vast_dc::remove_nonoverlapped ()
    {        
        vector<id_t> remove_list;   // nodes considered for removing        
        id_t id;

        // go over each neighbor and do an overlap check
        for (map<id_t, Node>::iterator it = _id2node.begin(); it != _id2node.end(); ++it)
        {
            id = it->first;
            
            // check if a neighbor is relevant (within AOI)
            // NOTE: we add 1 to the overlap test to avoid round-off error
            if (id == _self.id || 
                _voronoi->is_enclosing (id) == true ||                
                _voronoi->overlaps (id, _self.pos, (aoi_t)(_self.aoi+_detection_buffer + 1)) == true)
            {
                _count_drop[id] = 0;
                continue;
            }

            // record a list of non-overlapped neighbors 
            if (++_count_drop[id] > MAX_DROP_COUNT)
                remove_list.push_back (id);                        
        }
        
        int n = remove_list.size ();
        int n_deleted = 0;
        for (int i=0; i<n; ++i)
        {
            id = remove_list[i];
                                    
            // notify remote node to shrink AOI if it still covers me
            //if (_voronoi->overlaps (_self.id, _id2node[id].pos, (aoi_t)(_id2node[id].aoi+_id2vel[id]*BUFFER_MULTIPLIER)) == true)
            if (_voronoi->overlaps (_self.id, _id2node[id].pos, (aoi_t)(_id2node[id].aoi+_detection_buffer+1)) == true)
            {
                // NOTE: do not force disconnection nor notify OVERCAP
                //_net->sendmsg (id, OVERCAP, 0, 0, _time);
                continue;
            }
            
            n_deleted++;
            delete_node (id);
        }
        return n_deleted;
    }
    
    char * 
    vast_dc::getstat (bool clear)
    {
        static char str[VAST_BUFSIZ];
        char buf[80];
        str[0] = 0;

        // print out NODE message ratio
        sprintf (buf, " NM ratio: %6d/%6d = %f", _NM_known, _NM_total, (float)_NM_known/(float)_NM_total);
        strcat (str, buf);
        
        return str;
    }

    bool 
    vast_dc::right_of (id_t test_node, id_t moving_node)
    {
        Position &p1 = _id2node[moving_node].pos;
        Position &p2 = _id2node[test_node].pos;
        Position &p3 = _self.pos;
        
        // see if the angle formed between the edge of test_node to moving_node
        // and the edge of myself and moving node is positive or negative
        
        // p3: best reduction with a little sacrifice on TC (~ 0.1% TC)
        //     8% - 10% reduction in average message transmision
        // change sign affects 1 - 2% TC
        //return true;
        //return ((p1.x - p3.x) * (p2.x - p3.x) + (p1.y - p3.y) * (p2.y - p3.y) <= 0 ? true : false);
        
        
        // p1: original (reduce a little, change sign lose TC significantly)
        return ((p3.x - p1.x) * (p2.x - p1.x) + (p3.y - p1.y) * (p2.y - p1.y) >= 0 ? true : false);
        
        // p2: not much difference is use '>' or '>=' for comparisons
        // 2% reduction with 0.01% lower TC, change sign makes 5% lower TC
        //return ((p1.x - p2.x) * (p3.x - p2.x) + (p1.y - p2.y) * (p3.y - p2.y) >= 0 ? true : false);

        // combine (same as just using p3)
        //return ((p1.x - p3.x) * (p2.x - p3.x) + (p1.y - p3.y) * (p2.y - p3.y) <= 0 &&
        //        (p3.x - p1.x) * (p2.x - p1.x) + (p3.y - p1.y) * (p2.y - p1.y) >= 0 ? true : false);
    }

} // end namespace VAST
