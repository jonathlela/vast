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
 *  arbitrator.h -- VASTATE arbitrator class (everything related to an arbitrator)
 *
 *  ver 0.1 (2006/07/18)
 *   
 */

#ifndef VASTATE_ARBITRATOR_H
#define VASTATE_ARBITRATOR_H

#include "shared.h"
#include "arbitrator_logic.h"

namespace VAST 
{  
    class arbitrator : public msghandler
    {
    public:
        // initialize an arbitrator
        arbitrator (id_t my_parent, VASTATE::system_parameter_t * sp)
            : self (NULL), parent (my_parent), sysparm (sp)
        {
        }

        virtual ~arbitrator ()
        {
        }

        //
        // arbitrator interface
        //        

        virtual bool    join (id_t id, Position &pos) = 0;
        
        // process messages (send new object states to neighbors)
        virtual int     process_msg () = 0;

        // obtain any request to demote from arbitrator
        virtual bool    is_demoted (Node &info) = 0;

        //
        //  msghandler methods
        //

        // handle messages sent by vastnode
        virtual bool    handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size) = 0;

        // do things after messages are all handled
        virtual void    post_processmsg () = 0;

        //
        // for use mostly by arbitrator_logic
        //

        // create or delete a new object (can only delete if I'm the owner)
        // peer_id indicates if it's an avatar object
        virtual object *create_obj (Position &pos, id_t peer_id = 0, void *p = NULL, size_t size = 0) = 0;
        virtual bool    delete_obj (object *obj) = 0;

        // updating an existing object
        virtual void    update_obj (object *obj, int index, int type, void *value) = 0;
        virtual void    change_pos (object *obj, Position &newpos) = 0;
                
        // NOTE: overload & underload should be called continously as long as the 
        //       condition still exist as viewed by the application

        // arbitrator overloaded, call for help
        virtual bool    overload (int level) = 0;

        // arbitrator underloaded, will possibly depart as arbitrator
        virtual bool    underload (int level) = 0;

        // called by a managing arbitrator to continue the process of admitting a peer
        // attaching an app-specific initialization message
        virtual bool    insert_peer (id_t peer_id, void *initmsg, size_t size) = 0;

        // send to particular peers an app-specific message
        virtual bool    send_peermsg (vector<id_t> &peers, char *msg, size_t size) = 0;

        virtual bool    is_joined () = 0;

        //virtual bool    

        //
        // for statistical purposes
        //

        // obtain a copy of VAST node
        virtual vast *  get_vnode () = 0;

        // get a list of objects I own
        virtual map<obj_id_t, bool> &get_owned_objs () = 0;

        // get a list of neighboring arbitrators
        virtual map<id_t, Node> &get_arbitrators () = 0;
        
        // info about myself
        Node * self;
        id_t   parent;

        // pointer to system parameter hold by vastate
        VASTATE::system_parameter_t * sysparm;

        /*
        // debugging a bytestring
        void show (char *p, int len)
        {
            for (int i=0; i<len; i++)
            {
                printf ("%u ", (unsigned char)p[i]);            
            }
            printf ("\n");
        }
        */
        virtual const char * to_string () = 0;
        
    };

} // end namespace VAST

#endif // #define VASTATE_ARBITRATOR_H
