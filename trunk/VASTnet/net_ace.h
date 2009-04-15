/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2006 Shun-Yun Hu (syhu@yahoo.com)
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
 * net_ace.h -- network implementation using ACE
 *   
 *  
 */

#ifndef VAST_NET_ACE_H
#define VAST_NET_ACE_H

#include "ace/ACE.h"
#include "ace/OS.h"
#include "ace/Task.h"
#include "ace/Reactor.h"
#include "ace/Condition_T.h"        // ACE_Condition
//#include "ace/streams.h"
//#include "ace/Singleton.h"
//#include "ace/Synch.h"
//#include "ace/Log_Msg.h"


//#include "ace/SOCK_Acceptor.h"
#include "ace/SOCK_Stream.h"        // ACE_INET_Addr
#include "ace/SOCK_Dgram.h"         // ACE_INET_Dgram
#include "ace/SOCK_Connector.h"

#include "VASTnet.h"
#include "net_ace_acceptor.h"

using namespace ACE;

namespace Vast {

    // TODO: export temporarily for debug purpose
    class EXPORT net_ace : public Vast::VASTnet, public ACE_Task<ACE_MT_SYNCH>
    {
    friend class net_ace_handler;

    public:
        // pass in IP for gateway
        net_ace (int port)
            :_port_self(port), _msgqueue_in (NULL), _acceptor (NULL)
        {
            // necessary to avoid crash when using ACE with WinMain
            ACE::init ();

            // init clock
            clock ();
   
            _temp_id = NET_ID_UNASSIGNED-1;
            _udphandler = NULL;
        }

        ~net_ace ()
        {
        }

        //
        //  Standard ACE_Task methods (must implement)
        //

        // service initialization method
        int open (void *);
        
        // service termination method;
        int close (u_long);
        
        // service method
        int svc (void);

        //
        // inherent methods from class 'VASTnet'
        //

        void start ()
        {            
            this->open (0);
        }
        
        void stop ()
        {                
            this->close (0);
            VASTnet::stop ();
        }

        // replace unique ID for current VASTnet instance
        void registerID (id_t my_id);
               
        // get current physical timestamp
        timestamp_t getTimestamp ();
        
    private:

        // connect or disconnect a remote node (should check for redundency)
        int connect (id_t target);
        int disconnect (id_t target);      

        // send an outgoing message to a remote host
        // return the number of bytes sent
        size_t send (id_t target, char const *msg, size_t size, bool reliable = true);

        // receive an incoming message
        // return success or not
        bool receive (netmsg **msg);
        
        // methods to keep track of active connections
        // returns NET_ID_UNASSIGNED if failed
        id_t register_conn (id_t id, void *stream);
        id_t unregister_conn (id_t id);
        id_t update_conn (id_t prev_id, id_t curr_id);

        // store an incoming message to buffer
        int storeMessage (id_t from, char const *msg, size_t len, timestamp_t time);
        
        Addr                        _addr;
        id_t                        _temp_id;

        int                         _port_self;

        // queue for incoming messages
        netmsg *                    _msgqueue_in;

        // condition to ensure server thread is running before proceeding
        // to avoid remote nodes not able to connect
        ACE_Condition<ACE_Thread_Mutex> *_up_cond;    
                
        // ACE reactor for handling both accept and incoming message events
        ACE_Reactor                *_reactor;

        // acceptor for listen for incoming connections
        net_ace_acceptor           *_acceptor;

        // conncector initiates connections to remote hosts
        ACE_SOCK_Connector          _connector;

        // UDP datagram wrapper
        ACE_SOCK_Dgram              *_udp;
        net_ace_handler             *_udphandler;

        // for critical section access control 
        ACE_Thread_Mutex            _msg_mutex;
        ACE_Thread_Mutex            _conn_mutex;        // connection mutex

        // get local IP
        const char *getIP ();        
    };

} // end namespace Vast

#endif // VAST_NET_ACE_H
