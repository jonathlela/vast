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
 * net_ace.h -- network implementation using ACE
 *   
 *  
 */

#ifndef VAST_NET_ACE_H
#define VAST_NET_ACE_H

#ifdef WIN32

// disable warning about C4996: 'sprintf' was declared deprecated
#pragma warning(disable: 4996)

// disable unreferenced formal parameter warning
#pragma warning(disable: 4100)

#endif

// NOTE: turning this on will cause serious linker error under Win32 Release Mode
//#define ACE_NO_INLINE 1

#include "ace/ACE.h"
#include "ace/OS_NS_unistd.h"       // ACE_OS::sleep
#include "ace/OS_NS_netdb.h"        // gethostbyname
#include "ace/OS_NS_arpa_inet.h"    // inet_ntoa
#include "ace/Task.h"
#include "ace/Reactor.h"
#include "ace/Condition_T.h"        // ACE_Condition

//#include "ace/SOCK_Acceptor.h"
#include "ace/SOCK_Stream.h"        // ACE_INET_Addr
#include "ace/SOCK_Dgram.h"         // ACE_INET_Dgram
#include "ace/SOCK_Connector.h"

#include "VASTnet.h"
#include "net_ace_acceptor.h"

#ifdef VAST_USE_SSL
#include "ace/SSL/SSL_SOCK_Connector.h" // ACE_SSL_SOCK_Connector
#include "ace/SSL/SSL_SOCK_Stream.h"    // ACE_SSL_SOCK_Stream
#endif

using namespace ACE;

// attempts to try to re-connect to a node
const int RECONNECT_ATTEMPT = 0;

namespace Vast {
    
    class net_ace : public Vast::net_manager, public ACE_Task<ACE_MT_SYNCH>
    {
    friend class net_ace_handler;

    public:
        
        net_ace (uint16_t port);
        ~net_ace ();

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

        void start ();        
        void stop ();

        // get current physical timestamp (in millisecond, 10^-6)
        timestamp_t getTimestamp ();

        // get IP address from host name
        const char *getIPFromHost (const char *hostname = NULL);
          
        // obtain the IP / port of a remotely connected host
        bool getRemoteAddress (id_t host_id, IPaddr &addr);

        // connect or disconnect a remote node
        bool connect (id_t target, unsigned int host, unsigned short port, bool is_secure = false);
        bool disconnect (id_t target);      

        // send an outgoing message to a remote host, if addr is specified, message is UDP
        // return the number of bytes sent
        size_t send (id_t target, char const *msg, size_t size, const Addr *addr = NULL);
        
        // receive an incoming message
        // return pointer to valid NetSocketMsg structure or NULL for no messages
        NetSocketMsg *receive ();

        // change the ID for a remote host
        bool switchID (id_t prevID, id_t newID);

        // perform a tick of the logical clock 
        void tickLogicalClock () {}

        // store a message into priority queue
        // returns success or not
        bool msg_received (id_t fromhost, const char *message, size_t size, timestamp_t recvtime = 0, bool in_front = false);

        // methods to keep track of active connections
        bool socket_connected (id_t id, void *stream, bool is_secure);
        bool socket_disconnected (id_t id);

    private:

        // bind port for this node
        uint16_t              _port_self;

        // condition to ensure server thread is running before proceeding
        // to avoid remote nodes not able to connect
        ACE_Condition<ACE_Thread_Mutex> *_up_cond;    
        ACE_Condition<ACE_Thread_Mutex> *_down_cond; 
                
        // ACE reactor for handling both accept and incoming message events
        ACE_Reactor                *_reactor;

        // acceptor for listen for incoming connections
        net_ace_acceptor           *_acceptor;

        // conncector initiates connections to remote hosts        
        ACE_SOCK_Connector          _connector;
#ifdef VAST_USE_SSL
        ACE_SSL_SOCK_Connector      _SSL_connector;
#endif

        // UDP datagram wrapper
        ACE_SOCK_Dgram              *_udp;
        net_ace_handler             *_udphandler;

        // for critical section access control 
        ACE_Thread_Mutex            _msg_mutex;
        ACE_Thread_Mutex            _conn_mutex;        // connection mutex

        // hostname & IP of current host
        char                        _hostname[255];
        char                        _IPaddr[17];       
    };

} // end namespace Vast

#endif // VAST_NET_ACE_H
