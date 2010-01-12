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
 *  Arbitrator.h -- VASTATE Arbitrator class actual implementation
 *
 *  ver 0.1 (2006/07/21)
 *   
 */


#ifndef VASTATE_ARBITRATOR_IMPL_H
#define VASTATE_ARBITRATOR_IMPL_H

#include "VASTTypes.h"
#include "VASTUtil.h"
#include "Arbitrator.h"
#include "VONPeer.h"
#include "VONNetwork.h"

#include <list>

namespace Vast 
{      

    class ArbitratorImpl : public Arbitrator, public VONNetwork        
    {
    public:
        
        // initialize an arbitrator
        ArbitratorImpl (ArbitratorLogic *logic, VAST *vastnode, VASTATEPara &para);
        
        ~ArbitratorImpl ();

        //
        // Arbitrator interface
        //        
        
        // join the arbitrator mesh network with a given arbitrator ID and logical position
        bool    join (const Position &pos);

        // leave the arbitrator overlay
        bool    leave ();
        
        // sends the results of a join request to a joining Agent
        void    admit (id_t agent, char *status, size_t size);

        // send a message to a given agent
        bool    send (Message &msg);

        // create or delete a new Object (can only delete if I'm the owner)
        // agent_id specifies if the object is an avatar object
        Object *createObject (byte_t type, const Position &pos, obj_id_t *obj_id = NULL, id_t agent_id = 0);
        bool    destroyObject (const obj_id_t &obj_id);
        
        // updating an existing object
        bool    updateObject (const obj_id_t &obj_id, int index, int type, void *value);
        bool    moveObject (const obj_id_t &obj_id, const Position &newpos);
        
        // NOTE: overload & underload should be called continously as long as the 
        //       condition still exist as viewed by the application

        // Arbitrator overloaded, call for help
        void    notifyLoading (int status);

        //
        //  MessageHandler methods
        //

        // process incoming messages
        bool handleMessage (Message &in_msg);

        // perform some tasks after all messages have been handled (default does nothing)        
        void postHandling ();


        //
        //  VONNetwork methods
        //

        // send messages to some target nodes
        // returns number of bytes sent
        size_t sendVONMessage (Message &msg, bool is_reliable = true, vector<id_t> *failed_targets = NULL);

        // obtain next message in queue
        // return pointer to Message, or NULL for no more message
        Message* receiveVONMessage (timestamp_t &senttime);

        // notify the network layer of nodeID -> Address mapping        
        bool notifyAddressMapping (id_t node_id, Addr &addr);

        // get the IP address of current host machine
        Addr &getHostAddress ();

        // get current physical timestamp
        timestamp_t getTimestamp ();

        /*
        // obtain a copy of VAST node
        vast *get_vastnode () 
        {
            return _vastnode;
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
        */

        Node *getSelf ()
        {
            return &_self;
        }

        bool isJoined()
        {
            return (_state == JOINED);
        }

        const char * toString ();

        ArbitratorLogic *getLogic ()
        {
            return _logic;
        }

        VAST *getVAST ()
        {
            return _vastnode;
        }

        Position *getPosition ()
        {
            if (isJoined () == false)
                return NULL;
            
            return &_self.aoi.center;
        }

        // get the arbitrator's boundaries (edges with neighbor arbitrators)
        vector<Vast::line2d> *getEdges ()
        {
            if (_VONpeer == NULL || _VONpeer->isJoined () == false)
                return NULL;

            return &_VONpeer->getVoronoi ()->getedges ();
        }

        // get the boundary box for the arbitrator's Voronoi diagram
        bool getBoundingBox (point2d& min, point2d& max)
        {
            if (_VONpeer == NULL || _VONpeer->isJoined () == false)
                return false;

            return _VONpeer->getVoronoi ()->get_bounding_box (min, max);
        }

    private:
                
        //
        // helper methods
        //

        inline bool validateSize (int size, int variable_size, int item_size)
        {
            return ((size - variable_size - 1) % item_size == 0);
        }

        inline bool isOwner (const obj_id_t &obj_id, bool check_transit = false)
        {
            //return (_obj_owned.find (obj_id) != _obj_owned.end ());
            map<obj_id_t, StoredObject>::iterator it = _obj_store.find (obj_id);
            if (it == _obj_store.end ())
                return false;

            return (it->second.is_owner == true || (check_transit && it->second.in_transit != 0));
        }

        // whether a sending agent is in my region
        bool isWithinRegion (const Object *obj);        
        bool isWithinRegion (obj_id_t &obj_id);
        bool isWithinRegion (const Position &pos);
       
        // obtain the position to insert a new arbitrator
        Position findArbitratorInsertion (Voronoi *voronoi, id_t self_id);

        bool isLegalPosition (const Position & pos, bool include_self = true);

        // get a list of my enclosing arbitrators
        bool getEnclosingArbitrators (vector<id_t> &list);

        // get the center of all current agents I maintain
        bool getAgentCenter (Position &center);

        // get the center of all neighbor arbitrators
        bool getArbitratorCenter (Position &center);

        //
        // maintenance methods
        //

        // insert a new agent and create its avatar object
        bool addAgent (id_t from, Node &info, obj_id_t *obj_id = NULL);
       
        // handle the departure of an agent, due to request or connection failure
        bool removeAgent (id_t from, bool remove_obj = true);
        
        void storeObject (Object *obj, bool isOwner);
        void unstoreObject (obj_id_t obj_id);
        inline Object *getObject (const obj_id_t &obj_id);
        inline StoredObject *getStoredObject (const obj_id_t &obj_id);


        //
        // functional methods (tasks performed during each tick)
        //

        // check whether to take in a joining Agent as its managing arbitrator
        //bool checkAcceptance (id_t from_id, Msg_NODE &Agent, char *msg, size_t size);

        // see if any of my objects should transfer ownership
        // or if i should claim ownership to any new objects (if neighboring arbitrators fail)
        // returns number of ownership changes
        int transferOwnership ();
        
        // change position of this arbitrator in response to overload signals
        void moveArbitrator ();

        // check with VON to refresh current connected arbitrators
        void updateArbitrators ();

        // see we've properly joined & subscribed in a VON
        void checkVONJoin ();

        // remove any non-AOI or invalid avatar objects
        void validateObjects ();

        // do Agent's Object discovery, default is for all agents, optional for only a specified agent
        void updateAOI (id_t agent = 0);

        // find a suitable new Arbitrator given a certain need/stress level
        bool findArbitrator (int level, Node &new_arb);

        // process Event in Event queue in sequence
        // returns the number of events processed
        int processEvent ();

        // process ownership transfer notification
        void processTransfer (Message &in_msg);

        // check to call additional additional arbitrators for load balancing
        void checkOverload ();

        // perform statistics collection
        void reportStat ();

        // send loading to neighbors
        void reportLoading ();

        //
        // sending methods
        //
        
        // send updated states of objects I own to affected nodes (called once per timestep)
        bool sendUpdates ();

        // notify another arbitrator(s) of objects that I own, or notify only a single object (optional)
        bool notifyOwnership (vector<id_t> &targets, obj_id_t *obj_id = NULL);

        // notify an agent that I'm the current arbitrator
        bool notifyArbitratorship (id_t agent);

        // same effect as sendMessage (), but checks and remove failed agents (if any)
        int sendAgent (Message &msg);

        // send all objects I known
        bool sendObjects (id_t target, bool owned, bool send_delete = false);

        // send full object states to specified target(s)
        bool sendFullObject (Object *obj, vector<id_t> &targets, byte_t msggroup, bool to_delete = false);

        // send an Object update request to the approprite node (checks for target ownership)
        bool forwardRequest (obj_id_t obj_id, msgtype_t msgtype, char *buf, int size);
             
        // send request to arbitrator for full object states (when POSITION or STATE is received for unknown objects)
        void requestObject (id_t arbitrator, vector<obj_id_t> &obj_list);

        //
        // self info & references
        //

        Node                _self;          // basic arbitrator info as exists in a VON
        NodeState           _state;         // state of this arbitrator 

        VONPeer *           _VONpeer;       // interface as a participant in a VON
        VAST *              _vastnode;      // interface to perform SPS
        id_t                _sub_no;        // the arbitrator's subscription number in VAST

        ArbitratorLogic *   _logic;         // to reference app-specific callbacks                

        Addr                _gateway;       // gateway IP        

        // 
        // objects & events
        //

        map<obj_id_t, StoredObject>     _obj_store;             // repository of all known objects                
        map<obj_id_t, byte_t>           _attr_sizes;            // # of expected attributes for a certain object

        //map<obj_id_t, timestamp_t>      _obj_transit;           // objects currently in transfer of ownership (from myself to others)
        map<obj_id_t, Message *>        _transfer_msg;          // unprocessed ownership transfer messages
        map<obj_id_t, id_t>             _obj_requested;         // objects & arbitrators where OBJECT_R have been sent

        map<obj_id_t, bool>             _new_objs;              // newly created objects by this arbitrator

        /*
        map<id_t, 
            map<timestamp_t,
                vector<Event *> > >     _event_queue;           // list of all received event
        vector<pair<id_t, Event *> >    _unforward_event;       // list of all un-forwarded event
        */        
        multimap<timestamp_t, Event *>  _events;                // event queue
        
        //
        // agents & arbitrators
        //

        map<id_t, Node>                 _agents;                // list of Agents managed by me
        map<id_t, int>                  _agents_expire;         // a countdown for avatar Object deletion
        map<id_t, Object *>             _agent2obj;             // mapping from Agent ID to Object ID
        
        map<id_t, id_t>                 _host2agent;            // mapping from hostID (from) to agent

        map<id_t, map<obj_id_t, version_t> > _known_objs;       // objects known by an agent and their versions
        
        map<obj_id_t, int>              _transfer_countdown;    // countdown to transfer ownership
        map<obj_id_t, int>              _reclaim_countdown;     // countdown to reclaim ownership to unowned objects
        
        map<id_t, Node>                 _admitted;              // a list of Agents that have sent in join requests

        map<id_t, Node>                 _arbitrators;           // list of neighbor arbitrators 
                                                                // (NOTE: both index and Node info refer VONpeer id not host ID)
        
        map<id_t, int>                  _arb_loading;           // current loading of neighbor arbitrators

        // buffers
        char _buf[VASTATE_BUFSIZ];

        // used in sendFullObject ()
        Message obj_msg;        // update message about object creation / deletion
        Message pos_msg;        // update message about position update
        Message state_msg;      // update message about attributes 

        //
        // counters
        //

        int         _obj_id_count;                          // counter for assigning / creating object id
        int         _load_counter;                          // counter for # of timesteps overloaded (positive) or underloaded (negative)
        int         _overload_requests;                     // counter for # of times a OVERLOAD_M request is sent

        timestamp_t _tick;                                  // # of ticks since the execution of arbitrator
        
        // position states
        Node _newpos;                                       // new AOI & position to be updated

        vector<Position> _legal_pos;                        // legal positions for inserting new arbitrator

        //
        // server data (Gateway arbitrator)
        //

        std::vector<Node>     _potentials;                  // list of potential arbitrators      
        std::map<id_t, Node>  _promotion_requests;          // node requesting help's timestamp of promotion and position, index is the promoted node
        std::map<unsigned long, int> _promoted_hosts;      // hosts already promoted as arbitrator

        //
        // stat collection
        //
        StatType        _stat_agents;                   // statistics on the active agents at this arbitrator
        size_t          _last_send;                     // send stat last time                     
        size_t          _last_recv;                     // send stat last time
        FILE           *_statfile;                      // file pointer to store statistics

        //
        // system parameter used by VASTATE
        //
        VASTATEPara     _para;        

        // debug
        errout  _eo;
        char    _str[VASTATE_BUFSIZ];
    };

} // namespace Vast

#endif // #define VASTATE_ARBITRATOR_IMPL_H
