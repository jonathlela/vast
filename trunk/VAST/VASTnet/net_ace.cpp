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

#include "net_ace.h"
#include "net_ace_handler.h"
#include "VASTUtil.h"

namespace Vast {

    // constructor
    net_ace::net_ace (uint16_t port)
        :_port_self (port), 
         _up_cond (NULL), 
         _down_cond (NULL), 
         _acceptor (NULL)
    {
        // necessary to avoid crash when using ACE with WinMain
        ACE::init ();
   
        _udphandler = NULL;
        
        printf ("net_ace::net_ace(): Host IP: %s\n", getIPFromHost ());

        ACE_INET_Addr addr (_port_self, getIPFromHost ());

        // TODO: necessary here? actual port might be different and correct one
        //       is set in svc ()
        //       reason is that when VASTVerse is created, it needs to find out / resolve
        //       actual address for gateway whose IP is "127.0.0.1"
        _addr.setPublic ((uint32_t)addr.get_ip_address (), 
                         (uint16_t)addr.get_port_number ());

        // set the conversion rate between seconds and timestamp unit
        // for net_ace it's 1000 timestamp units = 1 second
        _sec2timestamp = 1000;
    }

    //
    //  Standard ACE_Task methods
    //
    
    // service initialization method
    int 
    net_ace::
    open (void *p) 
    {        
        // we need to use a 'ACE_Condition' to wait for the thread to properly comes up
        // otherwise there might be conflict in data access by two threads
        
        ACE_Thread_Mutex mutex;
        _up_cond = new ACE_Condition<ACE_Thread_Mutex>(mutex);
        
        printf ("ace_net::open (), before activate thread count: %lu\n", this->thr_count ()); 
        
        // activate the ACE network layer as a thread
        // NOTE: _active should be set true by now, so that the loop in svc () may proceed correctly
        _active = true;
        this->activate (); 
            
        // wait until server is up and running (e.g. svc() is executing)
        mutex.acquire ();
        _up_cond->wait ();
        mutex.release ();
        
        delete _up_cond;
        
        printf ("ace_net::open (), after activate thread count: %lu\n", this->thr_count ()); 

        return 0;
    }
    
    // service termination method;
    int 
    net_ace::
    close (u_long i)
    {       
        if (_active == true)
        {
            ACE_Thread_Mutex mutex;
            _down_cond = new ACE_Condition<ACE_Thread_Mutex>(mutex);
                   
            printf ("ace_net::close () thread count: %lu (before closing reactor)\n", this->thr_count ()); 
            
            // allow the reactor to leave its event handling loop
            _active = false;
            _reactor->end_reactor_event_loop ();
                            
            // wait until the svc() thread terminates
            mutex.acquire ();
            _down_cond->wait ();
            mutex.release ();
            
            delete _down_cond;

            printf ("ace_net::close (), thread count: %lu (after closing reactor)\n", this->thr_count ()); 
        }

        return 0;
    }
    
    // service method
    int 
    net_ace::
    svc (void)
    {        
        _reactor = new ACE_Reactor;                
        _acceptor = new net_ace_acceptor (this);
        
        ACE_INET_Addr addr (_port_self, getIPFromHost ());
       
        printf ("net_ace::svc () default port: %d\n", _port_self);

        // obtain a valid server TCP listen port        
        while (true) 
        {
            ACE_DEBUG ((LM_DEBUG, "(%5t) attempting to start server at %s:%d\n", addr.get_host_addr (), addr.get_port_number ()));
            if (_acceptor->open (addr, _reactor) == 0)
                break;
            
            _port_self++;
            addr.set_port_number (_port_self);
        }        
        
        ACE_DEBUG ((LM_DEBUG, "net_ace::svc() called. actual port binded: %d\n", _port_self));
        
        //addr.set (addr.get_port_number (), getIP ());

        //ACE_DEBUG ((LM_DEBUG, "(%5t) server at %s:%d\n", addr.get_host_addr (), addr.get_port_number ()));
        
        // create new handler for listening to UDP packets        
        ACE_NEW_RETURN (_udphandler, net_ace_handler, -1);
        _udp = _udphandler->openUDP (addr);
        _udphandler->open (_reactor, this);

        // TODO: this should really be the private IP, public should be obtained from a server
        // register my own address        
        _addr.setPublic ((uint32_t)addr.get_ip_address (), 
                          (uint16_t)addr.get_port_number ());
         
        // wait a bit to avoid signalling before the main thread tries to wait
        ACE_Time_Value tv (0, 200000);
        ACE_OS::sleep (tv);        

        _binded = true;

        // continue execution of original thread in open()
        _up_cond->signal ();
        
        // enter into event handling state           
        while (_active) 
        {        
            _reactor->handle_events();
        }        
 
        ACE_DEBUG ((LM_DEBUG, "(%5t) net_ace::svc () leaving event handling loop\n"));
        
        _reactor->remove_handler (_acceptor, ACE_Event_Handler::DONT_CALL);
        _binded = false;

        // NOTE: _acceptor will be deleted when reactor is deleted as one of its
        //       event handlers
        if (_reactor != NULL)
        {
            delete _reactor;
            _reactor = NULL;
            _acceptor = NULL;
        }

        // continue execution of original thread in close()
        // to ensure that svc () will exit
        if (_down_cond != NULL)
            _down_cond->signal ();
                                     
        return 0;
    }    


    //
    // inherent methods from class 'VASTnet'
    //


    // get current physical timestamp, unit is milliseconds
    timestamp_t
    net_ace::
    getTimestamp ()
    {
        ACE_Time_Value time = ACE_OS::gettimeofday();

        timestamp_t now = (timestamp_t)((time.sec ()  - _start_time.sec ()) * 1000 + 
                                        (time.usec () - _start_time.usec ()) / 1000);        
        return now;
    }

    // get IP address from host name
    const char *
    net_ace::getIPFromHost (const char *host)
    {
        char hostname[255];
        
        // if host is NULL then we use our own hostname
        if (host == NULL)
        {            
            ACE_OS::hostname (hostname, 255);
        }
        else
            strcpy (hostname, host);
 
        //printf ("hostname=%s, calling gethostbyname ()\n", hostname);
        hostent *remoteHost = ACE_OS::gethostbyname (hostname);
        //printf ("net_ace::getIPFromHost (): gethostbyname () success!\n");

        //printf("\tOfficial name: %s\n", remoteHost->h_name);

        if (remoteHost == NULL)
            return NULL;

        // BUG NOTE: IPv6 will not work...
        struct in_addr addr;        

        // return multiple IPs if exist
        /*
        int i = 0;
        while (remoteHost->h_addr_list[i] != 0) {
            addr.s_addr = *(u_long *) remoteHost->h_addr_list[i++];
            printf("\tIP Address #%d: %s\n", i, inet_ntoa(addr));
        }
        */
        
        // return the first IP found
        if (remoteHost->h_addr_list[0] != 0)
        {
            addr.s_addr = *(u_long *) remoteHost->h_addr_list[0];
            return ACE_OS::inet_ntoa (addr);
        }        
        else
            return NULL;
    }

    // check the validity of an IP address, modify it if necessary
    // (for example, translate "127.0.0.1" to actual IP)
    bool 
    net_ace::validateIPAddress (IPaddr &addr)
    {
        // if address is localhost (127.0.0.1), replace with my detected IP 
        if (addr.host == 0 || addr.host == 2130706433)
            addr.host = getHostAddress ().publicIP.host;

        // TODO: perform other actual checks

        return true;
    }

    int 
    net_ace::
    connect (id_t target)
    {
        if (_active == false)
            return (-1);

        // avoid self-connection
        if (target == _id)
            return 0;

        // lookup address
        if (_id2addr.find (target) == _id2addr.end ())
        {
            printf ("net_ace::connect (): cannot find address for target: %llu\n", target);
            return (-1);           
        }
        Addr &addr = _id2addr[target];

        // convert target address to ACE format
        ACE_INET_Addr target_addr ((u_short)addr.publicIP.port, (ACE_UINT32)addr.publicIP.host);
        
        if (isConnected (target) == true)
        {
#ifdef DEBUG_DETAIL
            //ACE_SOCK_Stream &stream = *((net_ace_handler *)_id2conn[target]);
            //ACE_DEBUG ((LM_DEBUG, "existing connection to (%s:%d)\n", ));
#endif            
            ACE_ERROR_RETURN ((LM_ERROR, "(%5t) connect(): connection already exists for [%d] (%s:%d)\n", target, target_addr.get_host_addr (), target_addr.get_port_number ()), 0);
        }
        
        // create new handler
        net_ace_handler *handler;
        ACE_NEW_RETURN (handler, net_ace_handler, (-1));    

        // initialize a connection, note that the handler object
        // is treated as a SOCK_Stream
        // NOTE 2nd parameter is in microsecond (not milli)        
        int attempt_count = 3;
        ACE_Time_Value tv (0, 100000);
        ACE_DEBUG ((LM_DEBUG, "connecting to %s:%d\n", target_addr.get_host_addr (), target_addr.get_port_number ()));
        ACE_Time_Value timeout (2, 0);

        while (_connector.connect (*handler, target_addr, &timeout) == -1)         
        {  
            if (attempt_count >= RECONNECT_ATTEMPT)
            {
                _id2addr.erase (target);
                ACE_ERROR_RETURN ((LM_ERROR, "connect to %s:%d failed after %d re-attempts\n", target_addr.get_host_addr (), target_addr.get_port_number (), attempt_count), (-1));
            }
            
            attempt_count++;
            
            ACE_DEBUG ((LM_DEBUG, "connect %s:%d failed (try %d):\n", target_addr.get_host_addr (), target_addr.get_port_number (), attempt_count));
            ACE_OS::sleep (tv);
        }
        
        // open the handler object
        if (handler->open (_reactor, this, target) == -1) 
        {
            handler->close();
            return (-1);
        }
        
        ACE_DEBUG ((LM_DEBUG, "(%5t) connect(): connected to (%s:%d)\n", target_addr.get_host_addr (), target_addr.get_port_number ()));

        return 0;
    }

    int 
    net_ace::
    disconnect (id_t target)
    {
        if (isConnected (target) == false)
            return false;
        
        // BUG: if mutex is used here the program will stale under Linux (reason unknown)
        //_conn_mutex.acquire ();
        net_ace_handler *handler = (net_ace_handler *)_id2conn[target];       
        handler->close();
        //_conn_mutex.release ();
        
        ACE_DEBUG ((LM_DEBUG, "(%5t) disconnect(): [%d] disconnected\n", (int)target));
        
        return 0;
    }

    // send an outgoing message to a remote host
    // return the number of bytes sent
    size_t
    net_ace::
    send (id_t target, char const *msg, size_t size, bool reliable)
    {
        // TODO: too much checking? earlier / collective checking?
        if (_active == false || isConnected (target) == false)
            return 0;

        // a TCP message
        if (reliable)
        {
            _conn_mutex.acquire ();
            ACE_SOCK_Stream &stream = *((net_ace_handler *)_id2conn[target]);
            size = stream.send_n (msg, size);
            _conn_mutex.release ();
        }
        // a UDP message
        else
        {
            Addr addr = _id2addr[target];
            ACE_INET_Addr target_addr ((u_short)addr.publicIP.port, (ACE_UINT32)addr.publicIP.host);
            size = _udp->send (msg, size, target_addr);
        }

        return size;
    }

    // receive an incoming message
    // return pointer to next QMSG structure or NULL for no more message
    QMSG *
    net_ace::receive ()
    {
        // if no time is left in current timeslot, then return immediately
        //if (TimeMonitor::instance ()->available () == 0)
        if (TimeMonitor::getInstance ().available () == 0)
        {
            //printf ("no time available\n");
            return NULL;
        }

        // we simply return the next message in queue, sorted by priority
        if (_msgqueue.size () == 0)
            return NULL;

        QMSG *qmsg;

        _msg_mutex.acquire ();
        qmsg = _msgqueue.begin ()->second;
        _msgqueue.erase (_msgqueue.begin ());
        _msg_mutex.release ();

        return qmsg;
    }
    
    // store a message into priority queue
    // returns success or not
    bool 
    net_ace::
    store (QMSG *qmsg)
    {
        // we store message according to message priority
        _msg_mutex.acquire ();
        _msgqueue.insert (std::multimap<byte_t, QMSG *>::value_type (qmsg->msg->priority, qmsg));        
        _msg_mutex.release ();

        return true;
    }

    //
    // net_ace specific methods
    //

    // methods to keep track of active connections
    // returns the id being assigned 
    id_t
    net_ace::
    register_conn (id_t id, void *stream)
    {
        // if remote connection is without HostID, assign a new one
        if (id == NET_ID_UNASSIGNED)
        {
            printf ("net_ace::register_conn () empty id given\n");
            return NET_ID_UNASSIGNED;
        }
        else if (isConnected (id) == true)
        {
            printf ("net_ace::register_conn () TCP stream already registered\n");
            return NET_ID_UNASSIGNED;
        }

        // store the connection stream
        _conn_mutex.acquire ();
        _id2conn[id] = stream;
        _id2time[id] = this->getTimestamp ();
        _conn_mutex.release ();

        // store address mapping for remote node (but only if it's a new node)
        ACE_SOCK_Stream &s = *((net_ace_handler *)stream);
        ACE_INET_Addr remote_addr;
        s.get_remote_addr (remote_addr);
        IPaddr ip (remote_addr.get_ip_address (), remote_addr.get_port_number ()); 
        Addr addr (id, &ip);

        storeMapping (addr);

#ifdef DEBUG_DETAIL        
        printf ("register_conn [%d]\n", (int)id);
#endif
        
        return id;
    }

    id_t 
    net_ace::
    unregister_conn (id_t id)
    {
        if (_active == false)
            return NET_ID_UNASSIGNED;

        if (isConnected (id) == false)
            return NET_ID_UNASSIGNED;

#ifdef DEBUG_DETAIL
        printf ("unregister_conn [%d]\n", (int)id);
#endif
        _conn_mutex.acquire ();
        _id2conn.erase (id);
        //_id2addr.erase (id);
        _id2time.erase (id);
        _conn_mutex.release ();
                        
        // send a DISCONNECT notification
        // NOTE that storing 0 msg and 0 size won't conflict with normal messages with no parameters sent
        //      as those messages will at least have some content sent to storeMessage
        //timestamp_t t = getTimestamp ();
        Message *msg = new Message (DISCONNECT);
        storeRawMessage (id, msg);
        
        return id;
    }

    /*
    id_t 
    net_ace::
    update_conn (id_t prev_id, id_t curr_id)
    {
        if (isConnected (prev_id) == false || curr_id == NET_ID_UNASSIGNED)
            return NET_ID_UNASSIGNED;
        
        _conn_mutex.acquire ();
        void *stream = _id2conn[prev_id];
        _id2conn.erase (prev_id);
        _id2conn[curr_id] = stream;

        Addr a = _id2addr[prev_id];
        _id2addr.erase (prev_id);
        _id2addr[curr_id] = a; 
        _conn_mutex.release ();

#ifdef DEBUG_DETAIL
        printf ("update_conn [%d] changed to [%d]\n", (int)prev_id, (int)curr_id);
#endif
        
        return curr_id;
    }
    */

} // end namespace Vast
