/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2011 Shun-Yun Hu (syhu@ieee.org)
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
 *  VASTClient.h    Implementation class for the VAST interface. 
 *              
 *      history:    2009/04/28  separated from VAST.h
 */

#ifndef _VAST_THREAD_H
#define _VAST_THREAD_H

// for starting separate thread
#include "ace/ACE.h"
//#include "ace/OS.h"
#include "ace/OS_NS_unistd.h"       // ACE_OS::sleep
#include "ace/Task.h"
#include "ace/Reactor.h"
#include "ace/Condition_T.h"        // ACE_Condition

#include "VAST.h"
#include "VASTUtil.h"               // TimeMonitor


using namespace std;

namespace Vast
{

    //
    //  Thread class for starting & stopping periodic ticking for one VAST node
    //

    class VASTThread : public ACE_Task<ACE_MT_SYNCH>
    {
    
    public:
        VASTThread (int tick_persec)
        {
            _ticks_persec = tick_persec;
            _active = false;
            _state  = ABSENT;
            _world  = NULL;
            _sub_id = 0;

            _up_cond = _down_cond = NULL;
        }

        ~VASTThread ()
        {       
            this->close (0);
        }

        //
        //  Standard ACE_Task methods (must implement)
        //

        // service initialization method
        int open (void *p);
        
        // service termination method;
        int close (u_long i);
        
        // service method
        int svc (void);

    private:

        int         _ticks_persec;      // # of ticks this thread should execute per second

        bool        _active;            // whether the currnet play thread is alive.
        NodeState   _state;             // the join state of this node
        VAST       *_vastnode;          // pointer to vastnode
        void       *_world;             // pointer to world
        id_t        _sub_id;            // client's subscription ID

        // condition to ensure thread is running before proceeding
        ACE_Condition<ACE_Thread_Mutex> *_up_cond;    
        ACE_Condition<ACE_Thread_Mutex> *_down_cond;    
    };

} // end namespace Vast

#endif // VAST_THREAD_H
