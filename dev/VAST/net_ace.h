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

#include "typedef.h"
#include "network.h"
#include "net_msg.h"
#include "net_ace_acceptor.h"
#include "vastbuf.h"
#include <map>


namespace VAST {

    // TODO: export temporarily for debug purpose
    class EXPORT net_ace : public network, public ACE_Task<ACE_MT_SYNCH>
    {
    public:
        // pass in IP for gateway
        net_ace (int port)
            :_port_self(port), _msgqueue_in (NULL), _lastmsg (NULL),// _curr_time (0), 
             _active (false), _acceptor (NULL)
        {
            // necessary to avoid crash when using ACE with WinMain
            ACE::init ();

            // init clock
            clock ();

            _id = NET_ID_UNASSIGNED;       
            _temp_id = NET_ID_UNASSIGNED-1;
            _udphandler = NULL;
        }

        ~net_ace ()
        {
        }

        //
        // inherent methods from class 'network'
        //

        void register_id (id_t my_id);

        void start ()
        {            
            this->open (0);
        }
        
        void stop ()
        {            
            this->close (0);
        }

        // connect or disconnect a remote node (should check for redundency)
        int connect (id_t target, Addr addr);
        int connect (id_t target);
        int connect (Addr & addr);
        int disconnect (id_t target);        

        Addr &getaddr (id_t id);

        // get a list of currently active connections' remote id and IP addresses
        std::map<id_t, Addr> &getconn ();

        // send message to a node
        int sendmsg (id_t target, msgtype_t msgtype, char const *msg, size_t len, /*timestamp_t time, */bool reliable = true, bool buffered = false);
        
        // obtain next message in queue before a given timestamp
        // returns size of message, or -1 for no more message
        int recvmsg (id_t &from, msgtype_t &msgtype, timestamp_t &recvtime, char **msg/*, timestamp_t time*/);

        // send out all pending reliable message in a single packet to each target
        int flush (bool compress = false);

        // notify ip mapper to create a series of mapping from src_id to every one in the list map_to
        int notify_id_mapper (id_t src_id, const std::vector<id_t> & map_to);

        // get current physical timestamp
        timestamp_t get_curr_timestamp ();

        // method to store an incoming message
        int storemsg (id_t from, msgtype_t msgtype, char const *msg, size_t len, timestamp_t time);

        //
        //  Standard ACE_Task methods (must implement)
        //

        // service initialization method
        int open (void *);
        
        // service termination method;
        int close (u_long);
        
        // service method
        int svc (void);
        
        // methods to keep track of active connections
        // returns NET_ID_UNASSIGNED if failed
        id_t register_conn (id_t id, void *stream);
        id_t unregister_conn (id_t id);
        id_t update_conn (id_t prev_id, id_t curr_id);

        // helper methods
        inline bool is_connected (id_t id)
        {
            return (_id2conn.find (id) != _id2conn.end ());
        }

    private:
      
        // address & connection mapping
        std::map<id_t, Addr>        _id2addr;
        std::map<id_t, void *>      _id2conn;

        // unique id for the vast class that uses this network interface
        id_t                        _id;
        Addr                        _addr;
        id_t                        _temp_id;

        int                         _port_self;

        // queue for incoming messages
        netmsg *                    _msgqueue_in;

        // queue for outgoing messages
        std::map<id_t, netmsg *>    _msgqueue_out;
        std::map<id_t, size_t>      _msgqueue_out_length;

        // the last message retrived
        netmsg *                    _lastmsg;        

        // last timestamp
        //timestamp_t                 _curr_time;

        bool                        _active;

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
        
        // buffer for sending/receiving messages
        vastbuf                     _send_buf;
        vastbuf                     _recv_buf;
    };

} // end namespace VAST

#endif // VAST_NET_ACE_H
