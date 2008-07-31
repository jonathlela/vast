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

/*
 *  arbitrator.h -- VASTATE arbitrator class actual implementation
 *
 *  ver 0.1 (2006/07/21)
 *   
 */


#ifndef VASTATE_ARBITRATOR_IMPL_H
#define VASTATE_ARBITRATOR_IMPL_H

#include "arbitrator.h"
#include "storage.h"
#include "vastutil.h"

#include <list>

namespace VAST 
{      
// loop through all neighboring arbitrators
#define for_each_enclosing_arbitrator(it) \
    for(map<id_t,Node>::iterator it=_arbitrators.begin();it != _arbitrators.end();it++)

// loop all objects in obj_store
#define for_each_object(it) \
    for (map<obj_id_t, object *>::iterator (it) = _obj_store.begin (); \
        (it) != _obj_store.end (); (it) ++)

    // Arbitrator Record
    class ArbRecord
    {
    public:
        Node node;
        obj_id_t parent_id;
        object * parent_obj;
        Addr addr;
    };

    // Encoded buffer structure for encoding objects
    class CodingBuffer
    {
    public:
        CodingBuffer ()
            : obj_size (0), obj_dsize (0), states_size (0)
        {}

        ~CodingBuffer ()
        {}

        void clear ()
        {
            obj_size = obj_dsize = states_size = 0;
        }

        char obj_buf[VASTATE_BUFSIZ];
        char obj_dbuf[VASTATE_BUFSIZ];
        char states_buf[VASTATE_BUFSIZ];
        
        int obj_size;
        int obj_dsize;
        int states_size;
    };

    class ExtendedEvent 
    {
    public:
        ExtendedEvent () {}
        ~ExtendedEvent () {}

        id_t               from_id;
        vector<obj_id_t> * t_list;
        event            * e;
        short              TTL;
    };

    class arbitrator_impl : public arbitrator        
    {
    public:
        
        // initialize an arbitrator
        arbitrator_impl (id_t my_parent, arbitrator_logic *logic, vast *vastnode, storage *s, 
                        bool is_gateway, Addr &gateway, vastverse *vastworld, system_parameter_t * sp, 
                        bool is_aggregator = false);
        
        ~arbitrator_impl ();

        //
        // arbitrator interface
        //        
        
        bool join (id_t id, Position &pos, const Addr & entrynode);

        // call the arbitrator/aggregator to leave the system
        bool leave (bool sleeping_only = false);
        
        // process messages (send new object states to neighbors)
        int process_msg ();

        // obtain any request to demote from arbitrator
        bool is_demoted (Node &info);

        //
        //  msghandler methods
        //

        // process messages sent by vastnode
        bool handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size);

        // do things after messages are all handled
        void post_processmsg ()
        {
        }

        // create or delete a new object (can only delete if I'm the owner)
        object *create_obj (Position &pos, id_t peer_id = 0, void *p = NULL, size_t size = 0);
        bool    delete_obj (object *obj);
        
        // updating an existing object
        bool update_obj (object *obj, int index, int type, void *value);
        bool change_pos (object *obj, Position &newpos);
        
        // NOTE: overload & underload should be called continously as long as the 
        //       condition still exist as viewed by the application

        // arbitrator overloaded, call for help
        bool overload (int level);

        // arbitrator underloaded, will possibly depart as arbitrator
        bool underload (int level);

        // called by a managing arbitrator to continue the process of admitting a peer
        bool insert_peer (id_t peer_id, void *ref, size_t size);

        // send to a particular peer an app-specific message        
        bool send_peermsg (vector<id_t> &peers, char *msg, size_t size);

        bool is_joined ()
        {
            return _joined && _vnode->is_joined ();
        }

        // true if is an aggregator
        bool is_aggregator ()
        {
            return _is_aggregator;
        }

        // return aggregator informations
        AggNode *get_aggnode ()
        {
            return &_aggnode;
        }

        // obtain a copy of VAST node
        vast *get_vnode () 
        {
            return _vnode;
        }

        map<obj_id_t, bool> &get_owned_objs ()
        {
            return _obj_owned;
        }

        // get a list of neighboring arbitrators
        map<id_t, Node> &get_arbitrators ()
        {
            return _arbitrators;
        }

        // get misc node info
        bool get_info (int info_type, char* buffer, size_t & buffer_size);

        // return if a object's owner (for statistics purpose)
        bool is_obj_owner (obj_id_t object_id);

        const char * to_string ();
                    
    private:
                
        inline bool validate_size (int size, int variable_size, int item_size)
        {
            return ((size - variable_size - 1) % item_size == 0);
        }

        inline bool is_owner (obj_id_t obj_id)
        {
            return (_obj_owned.find (obj_id) != _obj_owned.end ());
        }

        bool is_legal_position (const Position & pos, bool include_myself = true)
        {
            // check if position is out of map
            if (pos.x < 0 || pos.y < 0 || pos.x >= sysparm->width || pos.y >= sysparm->height)
                return false;

            // loop through all enclosing arbitrators
            for (map<id_t,Node>::iterator it = _arbitrators.begin (); it != _arbitrators.end (); it ++)
            {
                if (!include_myself && it->first == self->id)
                    continue;

                // check distance
                if (pos.dist (it->second.pos) < 4 * sysparm->aoi)
                    return false;
            }
            return true;
        }

        /*
        inline bool is_interested (obj_id_t obj_id)
        {
            return (_interested.find (obj_id) == _interested.end () ? true : false);
        }
        */

        inline timestamp_t get_timestamp ()
        {
            return (timestamp_t)((int) _net->get_curr_timestamp () + _time_diff);
        }
        
        void store_obj (object *obj, bool is_owner);
        void unstore_obj (obj_id_t obj_id);

        // check whether to take in a joining peer as its managing arbitrator
        bool check_acceptance (id_t from_id, Msg_SNODE &peer, char *msg, size_t size);

        // see if any of my objects should transfer ownership
        // or if i should claim ownership to any new objects (if neighboring arbitrators fail)
        // returns number of ownership changes
        int check_owner_transfer ();
        
        // make adjustments of arbitrator's position
        void adjust_position ();

        // for aggregator, make adjustments for all my manaing nodes
        void adjust_aggnodes_position ();

        // make adjustments to arbitrator AOI
        void adjust_aoi ();

        // check with VON to refresh current connected arbitrators
        void update_arbitrators ();

        // remove any non-AOI or invalid avatar objects
        void validate_objects ();

        // do peer's object discovery
        void update_interests ();

        // check for aggregation
        void check_aggregation ();

        // refreshing arbitrator voronoi map
        void refresh_voronoi ();

        // migrating this node to target aggregator
        bool migrate_to_aggregator (id_t target_aggr_id, id_t src_node = NET_ID_UNASSIGNED);

        // wake up arbitrator handled by peers
        bool wakeup_node (id_t node_id);

        // checking the nodes knowing about the object
        void check_knowledge (object *obj, const id_t & id, const Node & node, CodingBuffer *buffers);

        // locating parent object for futuring usage
        void locate_parent ();

        // encode a list of enclosing arbitrators
        int encode_arbitrators (char *buf, bool only_myself = false);
        
        // send updated states of objects I own to affected nodes (called once per timestep)
        bool send_updates ();

        // send all objects I known
        bool send_objects (id_t target, bool owned, bool send_delete = false);

        // transfer ownership to specified node
        bool send_ownership (object *obj, id_t target, bool send_object = true);

        // tell arbitrator logic of all updated states during this time-step
        //inline void notify_updates (object *obj);

        // send an object update request to the approprite node (checks for target ownership)
        bool forward_request (obj_id_t obj_id, msgtype_t msgtype, char *buf, int size);
                                
        // forward messages to interested peers
        void notify_peers (map<id_t, version_t> *list, msgtype_t msgtype, char *msg , int len);

        // prepare a bytestring for ownership transfer (containing list of nodes interested in the object)
        // returns size
        int pack_transfer (object *obj, id_t closest, char *buf);

        // reverse of pack_transfer, used by the receipant of a TRANSFER message
        int unpack_transfer (char *buf);

        // pack aggregator and managing node list
        int pack_aggregator (char *buf, bool pack_delete = false);

        // unpack and store information about the aggregator
        int unpack_aggregator (char *buf, int size);

        // find a suitable new arbitrator given a certain need/stress level
        bool find_arbitrator (int level, Msg_SNODE &new_arb);

        // process event in event queue in sequence
        int process_event ();

        bool              _joined;          // if joined
        vastverse        *_vastworld;       // vastverse for creating Voronoi
        arbitrator_logic *_logic;           // to reference app-specific callbacks
        vast             *_vnode;           // object to join and access a VON
        storage          *_storage;         // to access a shared storage
        bool              _is_gateway;      // flag to indicate if I'm a gateway
        Addr              _gateway;         // gateway IP
        int               _time_diff;       // time difference between clcok of node and time source
        object           *_parent_obj;      // object pointer of my parent (avatar object)

        bool              _is_aggregator;   // if act as aggregator
        bool              _is_leaving;      // if the node gonna to leave (so stop all possible distributing self process)
        bool              _is_suspending;   // if the node gonna to sleep

        map<obj_id_t, object *>          _obj_store;            // repository of all known objects
        map<obj_id_t, bool>              _obj_owned;            // list of objects of which I have ownership
        map<obj_id_t, timestamp_t>       _obj_in_transit;       // countdown of objects are just transit owner to others
        map<obj_id_t, timestamp_t>       _obj_update_time;      // time of objects last update used for object expiring

        map<id_t, Node>                  _known_neighbors;      // neighbors already known from overlay
        timestamp_t                      _arbitrators_lastupdate; // last time full update to my neighbors about myself
        map<id_t, Node>                  _arbitrators;          // list of all connected arbitrators
        voronoi                        * _arbitrator_vor;       // voronoi diagram of current arbitrators
                                                                // note: because voronoi diagram from _vnode->get_voronoi () and _arbitrators will not consistent
                                                                //       so maintain a extra version of arbitrators' voronoi diagram
        map<id_t, AggNode>               _aggregators;          // list of known aggregators, and nodes they represented
        map<id_t, map<id_t, Node> >      _aggregators_n;        // list of managing nodes of known aggregators
        map<id_t, id_t>                  _med2ag;               // mapping from managed nodes to its aggregator
        int                              _migration_cd;         // migration count down, the time waiting for MIGRATE_ACK

        //vector<pair<id_t, event *> >     _unforward_event;      // list of all un-forwarded event
        vector<ExtendedEvent>            _unforward_event;

        // _event_queue, _arbitrator_lasttick is useless on optimistic model
        /*
        map<id_t, 
            map<timestamp_t,
                vector<event *> > >      _event_queue;          // list of all received event
        map<id_t, timestamp_t>           _arbitrator_lasttick;  // last tick timer for enclosing arbitrators
        */
        map<id_t, Node>                  _peers;                // list of peers managed by me
        map<id_t, int>                   _peers_countdown;      // a countdown for avatar object deletion
        map<id_t, object *>              _peer2obj;             // mapping from peer ID to object ID
        
        map<id_t, map<obj_id_t, version_pair_t> > _nodes_knowledge;  
                                                                // nodes' (peers and arbs) knowledge about objects I owned (mapped to pos_version and version pair
        //map<id_t, map<obj_id_t, version_t> > _peers_knowledge;  // peer knows which objects and its version
        //map<obj_id_t, map<id_t, version_t> *> _interested;      // which nodes (arbitrators/peers) know a particular object (for update purpose)
        map<obj_id_t, int>               _owner_countdown;      // countdown to claim ownership of unowned objects

        map<id_t, Msg_SNODE>              _join_peers;           // a list of peers that have sent in join requests

        // aggregator data members
        ///////////////////////////////////
        AggNode                          _aggnode;
        map<id_t, ArbRecord>             _managing_nodes;
        map<id_t, ArbRecord>             _managing_nodes_intrans;
        bool                             _aggnode_dirty;

        // buffers
        char _buf[VASTATE_BUFSIZ];

        // counters
        int _obj_id_count;

        timestamp_t _overload_time;                         // last timestep when overload () is called
        int  _overload_count;                               // counter for # of timesteps overloaded
        timestamp_t _underload_time;                        // last timestep when underload () is called
        int  _underload_count;                              // counter for # of timesteps underloaded

        // states
        bool _is_demoted;                                   // whether this arbitrator is demoted
        
        Node _newpos;                                       // new AOI & position to be updated

        //
        // server data
        //
        std::vector<Msg_SNODE> _potential_arbitrators;
        std::vector<int>      _promotion_count;

        std::list<Node>       _promoted_positions;          // Promoted node's promoting timestamp and position

        // for debug purpose
        static errout _eo;
        static char   _str [VASTATE_BUFSIZ];
    };

} // namespace VAST

#endif // #define VASTATE_ARBITRATOR_IMPL_H

