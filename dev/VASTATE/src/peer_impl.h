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
 *  peer.h -- VASTATE peer implementation
 *
 *  ver 0.1 (2006/07/21)
 *   
 */

#ifndef VASTATE_PEER_IMPL_H
#define VASTATE_PEER_IMPL_H

#include "peer.h"
#include "vastutil.h"

namespace VAST 
{  

    class peer_impl : public peer
    {
    public:
        peer_impl (peer_logic *logic, network *net, int capacity, Addr &gateway);
        ~peer_impl () 
        {            
            // cannot delete ?
            /*
            if (_logic != NULL)
                delete _logic;           
            */

            if (_joinmsg != NULL)
                delete _joinmsg;
        }

        //
        // peer interface
        //
        
        // process messages (send new object states to neighbors)
        int     process_msg ();

        // join VSP
        bool    join (id_t id, Position &pt, aoi_t radius, char *auth = NULL, size_t size = 0, Addr *entrance = NULL);
        
        // quit VSP
        void    leave (bool notify);
        
        // AOI related functions
        void    set_aoi (aoi_t radius);
        aoi_t   get_aoi ();
        
        // get self object (necessary?)
        //object *get_self ();
        
        event *create_event ();

        // send an event to the current managing arbitrator
        bool send_event (event *e);

        // obtain any request to promote as arbitrator
        bool is_promoted (Node &info);

        //
        //  msghandler methods
        //

        // process messages sent by vastnode
        bool handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size);

        // do things after messages are all handled
        void post_processmsg ()
        {
        }

        Node &get_self ()
        {
            return _self;
        }

        bool is_joined ()
        {
            return _joined;
        }

        char *to_string ()
        {
            char buf[80];
            _str_desc[0] = 0;

            if (_curr_arbitrator.node.id != NET_ID_UNASSIGNED)
            {
                Node & n = _curr_arbitrator.node;

                sprintf (buf, "curr_arb [%lu] (%d, %d)\n", n.id, (int)n.pos.x, (int)n.pos.y);
                strcat (_str_desc, buf);
            }

            for (map<id_t, Msg_SNODE>::iterator it = _arbitrators.begin (); it != _arbitrators.end (); it++)
            {
                Node &n = it->second.node;
                sprintf (buf, "arb [%lu] (%d, %d)\n", n.id, (int)n.pos.x, (int)n.pos.y);
                strcat (_str_desc, buf);
            }

            return _str_desc;
        }

    private:

        inline bool validate_size (int size, int variable_size, int item_size)
        {
            return ((size - variable_size - 1) % item_size == 0);
        }

        inline timestamp_t get_timestamp ()
        {
            return (timestamp_t)((int) _net->get_curr_timestamp () + _time_diff);
        }
        
        // see if I should switch to another arbitrator
        void check_handover ();

        // check to see if the peer needs to remove any non-AOI objects
        void update_interests ();

		// tell peer_logic any updated states
		//void notify_updates ();

        // check if need to resync clock
        void update_time ();
    
        peer_logic *_logic;
        //network    *_net;
        Addr        _gateway;
        int         _capacity;
        // time difference between physical clock of this node and time source
        int         _time_diff;
        
        Node        _self;
        bool        _joined;

        Msg_SNODE    _curr_arbitrator;
        int         _arbitrator_error;
        
        map<id_t, Msg_SNODE> _arbitrators;
        map<obj_id_t, object *>          _obj_store;       // repository of all known objects

        // counters
        event_id_t  _event_id_count;

        // buffers
        char    *_joinmsg;          // app-specific join message 
        size_t   _joinmsg_size;     // (TODO: something cleaner?)
        char     _buf[VASTATE_BUFSIZ];

        // arbitrator info to be promoted
        Node     _arbitrator_request;

        // misc & debug
        char _str_desc[VASTATE_BUFSIZ];
        
        errout _eo;
        char _str[VASTATE_BUFSIZ];

    };
    
} // end namespace VAST

#endif // #define VASTATE_PEER_IMPL_H

