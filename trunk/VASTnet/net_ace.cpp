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

namespace Vast {



    //
    //  Standard ACE_Task methods
    //
    
    // service initialization method
    int 
    net_ace::
    open (void *p) 
    {        
        printf ("getIP(): %s\n", getIP ());

        // we need to use a 'ACE_Condition' to wait for the thread to properly comes up
        // otherwise there might be conflict in data access by two threads
        
        ACE_Thread_Mutex mutex;
        _up_cond = new ACE_Condition<ACE_Thread_Mutex>(mutex);
        
        // activate the ACE network layer as a thread
        this->activate ();   
        _active = true;
        
        // wait until server is up and running (e.g. svc() is executing)
        mutex.acquire ();
        _up_cond->wait ();
        mutex.release ();
        
        delete _up_cond;
        
        return 0;
    }
    
    // service termination method;
    int 
    net_ace::
    close (u_long i)
    {
        _active = false;

        // wait for the listening thread to close
        ACE_Time_Value tv (0, 500);
        ACE_OS::sleep (tv);        

        return 0;
    }
    
    // service method
    int 
    net_ace::
    svc (void)
    {        
        _reactor = new ACE_Reactor;                
        _acceptor = new net_ace_acceptor (this);
        
        ACE_INET_Addr addr;
        
        // obtain a valid server listen port        
        while (true) 
        {
            addr.set_port_number (_port_self);
            //ACE_DEBUG ((LM_DEBUG, "(%5t) attempting to start server at %s:%d\n", addr.get_host_addr (), addr.get_port_number ()));
            if (_acceptor->open (addr, _reactor) == 0)
                break;
            
            _port_self++;
        }        
        
        ACE_DEBUG ((LM_DEBUG, "net_ace::svc() called. port_self: %d\n", _port_self));
        
        addr.set (addr.get_port_number (), getIP ());

        //ACE_DEBUG ((LM_DEBUG, "(%5t) server at %s:%d\n", addr.get_host_addr (), addr.get_port_number ()));
        
        // create new handler for listening to UDP packets        
        ACE_NEW_RETURN (_udphandler, net_ace_handler, -1);
        _udp = _udphandler->openUDP (addr);
        _udphandler->open (_reactor, this);

        // TODO: this should really be the private IP, public should be obtained from a server
        // register my own address
        _addr.setPublic ((unsigned long)addr.get_ip_address (), 
                          (unsigned short)addr.get_port_number ());
               
        // wait a bit to avoid signalling before the main thread tries to wait
        ACE_Time_Value tv (0, 500);
        ACE_OS::sleep (tv);        

        // continue execution of original thread in open()
        _up_cond->signal ();
        
        // enter into event handling state           
        while (_active) 
        {        
            _reactor->handle_events();
        }        

        _reactor->remove_handler (_acceptor, ACE_Event_Handler::DONT_CALL);

        // NOTE: _acceptor will be deleted when reactor is deleted as one of its
        //       event handlers
        if (_reactor != NULL)
        {
            delete _reactor;
            _reactor = NULL;
            _acceptor = NULL;
        }
                                     
        return 0;
    }    


    //
    // inherent methods from class 'network'
    //

    void 
    net_ace::
    registerID (id_t my_id)
    {
        _id = my_id;  
        _id2addr[_id] = _addr;
    }

    // get current physical timestamp
    timestamp_t
    net_ace::
    getTimestamp ()
    {
        return (timestamp_t) clock ();
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
            printf ("net_ace::connect (): cannot find address for target:%d\n", target);
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
                
        int attempt_count = 0;
        ACE_Time_Value tv (0, 100);
        ACE_DEBUG ((LM_DEBUG, "connecting to %s:%d\n", target_addr.get_host_addr (), target_addr.get_port_number ()));
        while (_connector.connect (*handler, target_addr) == -1) 
        {  
            if (attempt_count >= 3)
            {
                _id2addr.erase (target);
                ACE_ERROR_RETURN ((LM_ERROR, "connect to %s:%d failed after 3 attempts", target_addr.get_host_addr (), target_addr.get_port_number ()), (-1));
            }
            
            attempt_count++;
            ACE_ERROR ((LM_ERROR, "connect %s:%d failed (try %d): %p\n", target_addr.get_host_addr (), target_addr.get_port_number (), attempt_count));
            ACE_OS::sleep (tv);
        }
        
        // open the handler object
        if (handler->open (_reactor, this, target) == -1) 
        {
            handler->close();
            return (-1);
        }
        
        ACE_DEBUG ((LM_DEBUG, "(%5t) connect(): connected to [%d] (%s:%d)\n", target, target_addr.get_host_addr (), target_addr.get_port_number ()));

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
    // return success or not
    bool 
    net_ace::
    receive (netmsg **msg)
    {
        // return if no more message in queue 
        // or all messages are beyond current logical time
        //
        // NOTE: we simply return the next message in queue, sorted by timestamp        
        if (_msgqueue_in == NULL) // || (time > 0 && time <= _msgqueue_in->time))
            return false;

        _msg_mutex.acquire ();
        _msgqueue_in = _msgqueue_in->getnext (msg); 
        _msg_mutex.release ();

        return true;
    }
    
    // insert the new message as a node in the double linklist
    // NOTE: the list should be sorted by timestamp when inserting
    //
    // (copied verbatiam from net_emu.cpp) TODO: consider to combine them?    
    int
    net_ace::
    storeMessage (id_t from, char const *msg, size_t len, timestamp_t time)
    {                 
        if (_active == false)
            return 0;

        // store the new message
        netmsg *newnode = new netmsg (from, msg, len, time, NULL, NULL);
        
        // failure likely due to memory allocation problem
        if (newnode == NULL || newnode->size < 0)
            return 0;
               
        _msg_mutex.acquire ();
        // insert unconditionally if the queue is empty
        if (_msgqueue_in == NULL)
            _msgqueue_in = newnode;
        else
            _msgqueue_in->append (newnode, time);
        _msg_mutex.release ();
        
        return len;
    }




    //
    // net_ace specific methods
    //

    // methods to keep track of active connections
    // returns the id being assigned (temp id may be assigned if unassigned id is received)
    id_t
    net_ace::
    register_conn (id_t id, void *stream)
    {
        if (isConnected (id) == true)
            return NET_ID_UNASSIGNED;

        if (id == NET_ID_UNASSIGNED)
            id = _temp_id--;

        _conn_mutex.acquire ();
        _id2conn[id] = stream;
        _conn_mutex.release ();

#ifdef DEBUG_DETAIL        
        printf ("register_conn [%d]\n", (int)id);
#endif
        return id;
    }

    id_t 
    net_ace::
    unregister_conn (id_t id)
    {
        if (isConnected (id) == false)
            return NET_ID_UNASSIGNED;

#ifdef DEBUG_DETAIL
        printf ("unregister_conn [%d]\n", (int)id);
#endif
        _conn_mutex.acquire ();
        _id2conn.erase (id);
        _id2addr.erase (id);
        _id2time.erase (id);
        _conn_mutex.release ();
                        
        // send a DISCONNECT notification
        storeMessage (id, 0, 0, getTimestamp ());
        
        return id;
    }    

    id_t 
    net_ace::
    update_conn (id_t prev_id, id_t curr_id)
    {
        if (isConnected (prev_id) == false)
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

    const char *
    net_ace::
    getIP () 
    {        
        char hostname[80];
        ACE_OS::hostname (hostname, 80);
        struct hostent *p = ACE_OS::gethostbyname (hostname);

        // BUG NOTE: IPv6 will not work...
        static struct in_addr addr;
        static char local_IP[16];
        
        // only get the first IP
        memcpy (&addr, p->h_addr_list[0], sizeof(struct in_addr));
        //printf( "first IP: %s second IP: %s\n", ACE_OS::inet_ntoa(p->h_addr_list[0]), ACE_OS::inet_ntoa (p->h_addr_list[1]));
        
        ACE_OS::strcpy (local_IP, ACE_OS::inet_ntoa (addr));
        //printf ("IP local: %s\n", local_IP);
        
        return local_IP;
    }    

} // end namespace Vast
