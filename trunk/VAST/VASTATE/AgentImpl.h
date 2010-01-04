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
 *  Agent.h -- VASTATE Agent implementation
 *
 *  ver 0.1 (2006/07/21)
 *   
 */

#ifndef VASTATE_AGENT_IMPL_H
#define VASTATE_AGENT_IMPL_H

#include "Agent.h"
#include "VASTTypes.h"
#include "VASTUtil.h"

namespace Vast 
{  

    class AgentImpl : public Agent
    {
    public:

        AgentImpl (AgentLogic *logic, VAST *vastnode);
        ~AgentImpl (); 

        //
        // Agent interface
        //
        
        // login/logout the system
        bool        login (char *URL, const char *auth = 0, size_t auth_size = 0);
        bool        logout ();

        // send a message to gateway arbitrator (i.e., server)
        bool        send (Message &msg);

        // AOI related functions
        void        setAOI (length_t radius);
        length_t    getAOI ();

        // join & leaves the system
        bool        join (Position &pos);
        void        leave ();
                        
        // moves to a new position
        void        move (Position &pos);
        
        // obtain a reference to an event object
        Event *     createEvent (msgtype_t event_type);

        // send an Event to the current managing arbitrator
        bool        act (Event *e);

        //
        //  MessageHandler methods
        //

        // returns whether the message has been handled successfully
        bool        handleMessage (Message &in_msg);

        // perform some tasks after all messages have been handled (default does nothing)        
        void        postHandling ();

        Node *getSelf ()
        {
            return &_self;
        }

        bool isJoined()
        {            
            return (_state == JOINED);
        }

        const char *toString ()
        {
            _str_desc[0] = 0;

            return _str_desc;
        }

        // check if the login is successful
        bool isAdmitted ()
        {
            return _admitted;
        }

        AgentLogic *getLogic ()
        {
            return _logic;
        }

        VAST *getVAST ()
        {
            return _vastnode;
        }

        // learn of the current arbitrator managing this region
        Node *getCurrentArbitrator ()
        {
            if (isJoined () == false)
                return NULL;

            return &_arbitrators[0];
        }

    private:

        // check if the size of a received message is right
        inline bool validateSize (int size, int variable_size, int item_size)
        {
            return ((size - variable_size - 1) % item_size == 0);
        }

        inline timestamp_t getTimestamp ()
        {
            return (timestamp_t)((int) _net->getTimestamp ());
        }

        // deliver an event to current arbitrator (might also include enclosing, depend on method)
        inline bool sendEvent (Message &msg);
        
        // check to see if the Agent needs to remove any non-AOI objects
        void updateAOI ();

        // send request to arbitrator for full object states (when POSITION or STATE is received for unknown objects)
        void requestObject (id_t arbitrator, obj_id_t &obj_id);

        // check if my own avatar object still exists
        void checkConsistency ();

            
        AgentLogic *_logic;
        VAST       *_vastnode;            
                
        Node        _self;                          // agent as exists logically (not vastnode's self)
        NodeState   _state;                         // current state of the node
        obj_id_t    _self_objid;                    // avatar object ID for myself

        bool        _admitted;                      // whether login is successul for this agent

        vector<Node> _arbitrators;                  // a list of enclosing arbitrators, current arbitrator is the first

        id_t        _sub_no;                        // subscription # for AOI

        map<obj_id_t, Object *> _obj_store;         // repository of all known objects
        map<obj_id_t, byte_t>   _attr_sizes;        // number of attributes for each object

        map<obj_id_t, id_t>     _obj_requested;     // objects & arbitrators where OBJECT_R have been sent

        map<obj_id_t, int>      _remove_countdown;  // countdown for removing non-AOI objects

        // timeouts
        int                 _join_timeout;          // timeout for joining a relay
        int                 _join_attempts;         // attempts to send out JOIN request to arbitrator

        // misc & debug
        char    _str_desc[VASTATE_BUFSIZ];        
        errout  _eo;
        char    _str[VASTATE_BUFSIZ];
    };
    
} // end namespace Vast

#endif // #define VASTATE_AGENT_IMPL_H

