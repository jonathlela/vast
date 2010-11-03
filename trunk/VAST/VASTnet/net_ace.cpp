/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2010 Shun-Yun Hu (syhu@ieee.org)
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
        // necessary when using ACE within WinMain to avoid crash 
        ACE::init ();

        // get hostname
        _hostname[0] = 0;
        _IPaddr[0] = 0;

        // NEW_THREAD will be created (ACE_OS called for 1st time?)
        ACE_OS::hostname (_hostname, 255);
           
        _udphandler = NULL;
        
        printf ("net_ace::net_ace(): Host IP: %s\n", getIPFromHost ());

        ACE_INET_Addr addr (_port_self, getIPFromHost ());

        
        // TODO: necessary here? actual port might be different and correct one
        //       is set in svc ()
        //       reason is that when VASTVerse is created, it needs to find out / resolve
        //       actual address for gateway whose IP is "127.0.0.1"
        _self_addr.setPublic ((uint32_t)addr.get_ip_address (), (uint16_t)addr.get_port_number ());

        //_self_addr.setPublic ((uint32_t)addr.get_ip_address (), 0);

        /*
        // self-determine preliminary hostID first
        _self_addr.host_id = this->resolveHostID (&_self_addr.publicIP);
        */

        // set the conversion rate between seconds and timestamp unit
        // for net_ace it's 1000 timestamp units = 1 second (each step is 1 ms)
        _sec2timestamp = 1000;
    }

    // destructor
    net_ace::~net_ace ()
    {          
        ACE::fini ();
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
        // NOTE: this works because we expect svc () will first take sometime to execute,
        //       during which the _up_cond->wait () is first called.
        //       if wait () is called *after* svc () has called _up_cond->signal ();
        //       we will face long, infinite waiting.
        mutex.acquire ();
        _up_cond->wait ();
        mutex.release ();
        
        delete _up_cond;
        _up_cond = NULL;
        
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
            _down_cond = NULL;

            printf ("ace_net::close (), thread count: %lu (after closing reactor)\n", this->thr_count ()); 
        }

        return 0;
    }
    
    // service method
    int 
    net_ace::
    svc (void)
    {        
        // NEW_THREAD net_ace runs as ACE_TASK
        _reactor = new ACE_Reactor;                
        _acceptor = new net_ace_acceptor (this);
        
        ACE_INET_Addr addr (_port_self, getIPFromHost ());
       
        printf ("net_ace::svc () default port: %d\n", _port_self);

        // obtain a valid server TCP listen port        
        while (true) 
        {
            // NEW_THREAD (ACE_DEBUG called for 1st time?)
            ACE_DEBUG ((LM_DEBUG, "(%5t) attempting to start server at %s:%d\n", addr.get_host_addr (), addr.get_port_number ()));

            if (_acceptor->open (addr, _reactor) == 0)
                break;
            
            _port_self++;
            addr.set_port_number (_port_self);
        }        
                
        ACE_DEBUG ((LM_DEBUG, "net_ace::svc () called. actual port binded: %d\n", _port_self));
                
        // create new handler for listening to UDP packets        
        // NEW_THREAD will be created (new handler that will listen to port?)
        ACE_NEW_RETURN (_udphandler, net_ace_handler, -1);
        _udp = _udphandler->openUDP (addr);
        _udphandler->open (_reactor, this);

        // NOTE: this is a private IP, publicIP is obtained from server
        // register my own address        
        _self_addr.setPublic ((uint32_t)addr.get_ip_address (), 
                              (uint16_t)addr.get_port_number ());

        // self-determine preliminary hostID first
        _self_addr.host_id = this->resolveHostID (&_self_addr.publicIP);
         
        // wait a bit to avoid signalling before the main thread tries to wait
        ACE_Time_Value sleep_interval (0, 200000);
        ACE_OS::sleep (sleep_interval);        

        // continue execution of original thread in open()
        _up_cond->signal ();
        
        // enter into event handling state           
        while (_active) 
        {        
            _reactor->handle_events();
        }        
 
        ACE_DEBUG ((LM_DEBUG, "(%5t) net_ace::svc () leaving event handling loop\n"));
        
        _reactor->remove_handler (_acceptor, ACE_Event_Handler::DONT_CALL);

        // NOTE: _acceptor will be deleted when reactor is deleted as one of its
        //       event handlers
        if (_reactor != NULL)
        {
            _reactor->close ();
            delete _reactor;
            _reactor = NULL;            

            // NOTE: acceptor is self deleted when its handle_close () is called by reactor, 
            //       so no need to delete again here
            _acceptor = NULL;
        }

        // wait a bit to avoid signalling before the main thread tries to wait
        ACE_OS::sleep (sleep_interval);

        // continue execution of original thread in close()
        // to ensure that svc () will exit
        if (_down_cond != NULL)
            _down_cond->signal ();
                                     
        return 0;
    }    


    //
    // inherent methods from 'net_manager'
    //

    void 
    net_ace::start ()
    {               
        net_manager::start ();

        // open the listening thread, _active will be true after calling
        this->open (0);
    }

    void 
    net_ace::stop ()
    {                
        net_manager::stop ();

        // close the listening thread, _active will set to false after calling
        this->close (0);
    }


    // get current physical timestamp, unit is milliseconds since 1970 (system clock)
    timestamp_t
    net_ace::
    getTimestamp ()
    {
        ACE_Time_Value time = ACE_OS::gettimeofday();

        timestamp_t now = (timestamp_t)(time.sec () * 1000 + time.usec () / 1000);        
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
            // if we've already looked up, return previous record
            if (_IPaddr[0] != 0)
                return _IPaddr;

            strcpy (hostname, _hostname);
        }
        else
            strcpy (hostname, host);
 
        hostent *remoteHost = ACE_OS::gethostbyname (hostname);

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
            strcpy (_IPaddr, ACE_OS::inet_ntoa (addr));

            return _IPaddr;
        }        
        else
            return NULL;
    }

    // obtain the IP / port of a remotely connected host
    // returns NULL if not available
    bool 
    net_ace::getRemoteAddress (id_t host_id, IPaddr &addr)
    {
        std::map<id_t, ConnectInfo>::iterator it;
        bool result = false;

        _conn_mutex.acquire ();
        it = _id2conn.find (host_id);
        if (it != _id2conn.end ())
        {
            result = true;
            addr = ((net_ace_handler *)(it->second.stream))->getRemoteAddress ();
        }
        _conn_mutex.release ();             

        return result;
    }

    bool
    net_ace::
    connect (id_t target, unsigned int host, unsigned short port)
    {
        if (_active == false)
            return false;

        // we're always connected to self
        if (target == _id || isConnected (target))
        {
            printf ("net_ace::connect () connection for [%llu] already exists\n", target);
            return true;
        }

        // convert target address to ACE format
        ACE_INET_Addr target_addr (port, (ACE_UINT32)host);
                
        // create new handler
        net_ace_handler *handler;
        ACE_NEW_RETURN (handler, net_ace_handler, false);    

        // initialize a connection, note that handler is treated as a SOCK_Stream object
        // NOTE 2nd parameter in ACE_Time_Value is in microsecond (not milli)        
        int attempt_count = 0;
        ACE_Time_Value sleeptime (0, 100000);
        ACE_DEBUG ((LM_DEBUG, "connecting to %s:%u\n", target_addr.get_host_addr (), port));
        
        // three-second TCP connection timeout
        ACE_Time_Value timeout (3, 0);

        while (_connector.connect (*handler, target_addr, &timeout) == -1)
        {  
            attempt_count++;
            if (attempt_count >= RECONNECT_ATTEMPT)
            {
                ACE_ERROR_RETURN ((LM_ERROR, "connect to %s:%u failed after %d re-attempts\n", target_addr.get_host_addr (), target_addr.get_port_number (), attempt_count), false);
            }
                        
            ACE_DEBUG ((LM_DEBUG, "connect %s:%d failed (try %d):\n", target_addr.get_host_addr (), target_addr.get_port_number (), attempt_count));
            ACE_OS::sleep (sleeptime);
        }
        
        // open the handler object, this will 
        // 1) register handler with reactor 2) cause socket_connected () be called
        if (handler->open (_reactor, this, target) == -1)
        {
            handler->close();
            return false;
        }
        
        ACE_DEBUG ((LM_DEBUG, "(%5t) connect(): connected to (%s:%d)\n", target_addr.get_host_addr (), target_addr.get_port_number ()));

        return true;
    }

    bool
    net_ace::
    disconnect (id_t target)
    {
        // we need to use mutex to access the handler object, because it's
        // possible that remote disconnection occurs at the same time when disconnect () 
        // is called. Then, it'll be possible the id2conn object is already gone,
        // when the following line is still called (will cause crash)
        
        net_ace_handler *handler = NULL;
        std::map<id_t, ConnectInfo>::iterator it;
        
        _conn_mutex.acquire ();
        if ((it = _id2conn.find (target)) != _id2conn.end ())
            handler = (net_ace_handler *)(it->second.stream);
        _conn_mutex.release ();

        // check if connection is already close (possibly due to remote disconnect)
        if (handler == NULL)
            return false;

        // NOTE: socket_disconnected () will be called when handler->close () is called
        //       mutex will be used again to protect access to _id2conn
        handler->close ();
        
        ACE_DEBUG ((LM_DEBUG, "(%5t) net_ace::disconnect(): [%llu] disconnected\n", target));
        
        return true;
    }

    // send an outgoing message to a remote host, if addr is given, then message is sent via UDP
    // return the number of bytes sent
    size_t
    net_ace::
    send (id_t target, char const *msg, size_t size, const Addr *addr)
    {        
        if (_active == false)
            return 0;

        // NOTE: it's possible that connection is already broken, yet still try to access connection
        //       (remote disconnect + local send), may crash. So mutex has to be used

        // a TCP message
        if (addr == NULL)
        {
            std::map<id_t, ConnectInfo>::iterator it;
            timestamp_t now = this->getTimestamp ();

            _conn_mutex.acquire ();

            // need to check if stream object still exists, otherwise may crash
            // (remote disconnection could already remove the connection)
            if ((it = _id2conn.find (target)) != _id2conn.end ())
            {
                // TODO: perhaps actual send can be done outside of mutex (avoid locking too long?)
                ACE_SOCK_Stream &stream = *((net_ace_handler *)(it->second.stream));
                size = stream.send_n (msg, size);

                // update the last accessed time for this connection
                it->second.lasttime = now;
            }
            else
                size = 0;

            _conn_mutex.release ();
        }
        // a UDP message
        else
        {
            // TODO: check if two threads accessing same data structure can occur
            ACE_INET_Addr target_addr ((u_short)addr->publicIP.port, (ACE_UINT32)addr->publicIP.host);
            size = _udp->send (msg, size, target_addr);

            // NOTE: there's no time update for UDP connection, as there's no need for connection timeout
        }

        return size;
    }

    // receive an incoming message
    // return pointer to next NetSocketMsg structure or NULL for no more message
    NetSocketMsg *
    net_ace::
    receive ()
    {
        // clear last received message
        if (_recvmsg != NULL)
        {            
            delete _recvmsg;
            _recvmsg = NULL;
        }

        // we simply return the next message in queue
        if (_recv_queue.size () == 0)
            return NULL;
        
        // FIFO, return first message extracted
        _msg_mutex.acquire ();
        _recvmsg = _recv_queue[0];
        _recv_queue.erase (_recv_queue.begin ());
        _msg_mutex.release ();

        return _recvmsg;
    }
    
    // change the ID for a remote host
    bool 
    net_ace::switchID (id_t prevID, id_t newID)
    {
        bool result = false;

        _conn_mutex.acquire ();

        // new ID already in use
        if (_id2conn.find (newID) != _id2conn.end () || _id2conn.find (prevID) == _id2conn.end ())
        {
            printf ("[%llu] net_ace::switchID () old ID not found or new ID already exists\n", _id);
        }
        else
        {
            printf ("[%llu] net_ace::switchID () replace [%llu] with [%llu]\n", _id, prevID, newID);

            // copy to new ID
            _id2conn[newID] = _id2conn[prevID];

            // erase old ID mapping
            _id2conn.erase (prevID);

            // change remote ID knowledge at stream
            result = ((net_ace_handler *)(_id2conn[newID].stream))->switchRemoteID (prevID, newID);
        }

        _conn_mutex.release ();

        return result;
    }

    // store a message into priority queue
    // returns success or not
    bool 
    net_ace::
    msg_received (id_t fromhost, const char *message, size_t size, timestamp_t recvtime, bool in_front)
    {
        // TODO: more space-efficient method?
        // make a copy and store internally
        NetSocketMsg *newmsg = new NetSocketMsg ();

        newmsg->fromhost = fromhost;
        newmsg->recvtime = recvtime;
        newmsg->size     = size;

        if (size > 0)
        {
            newmsg->msg      = new char[size];
            memcpy (newmsg->msg, message, size);
        }

        // assign a timestamp if necessary
        if (newmsg->recvtime == 0)
            newmsg->recvtime = this->getTimestamp ();

        // we store message according to message priority
        _msg_mutex.acquire ();
        //_msgqueue.insert (std::multimap<byte_t, NetSocketMsg *>::value_type (qmsg-> qmsg->msg->priority, qmsg));
        if (in_front)
            _recv_queue.insert (_recv_queue.begin (), newmsg);
        else
            _recv_queue.push_back (newmsg);
        _msg_mutex.release ();

        // update last accessed time of the connection
        _conn_mutex.acquire ();
        std::map<id_t, ConnectInfo>::iterator it = _id2conn.find (fromhost);
        if (it != _id2conn.end ())
        {
            if (newmsg->recvtime > it->second.lasttime)
                it->second.lasttime = newmsg->recvtime; 
        }
        _conn_mutex.release ();

        return true;
    }

    //
    // net_ace specific methods
    //

    // methods to keep track of active connections
    // returns the id being assigned 
    bool
    net_ace::
    socket_connected (id_t id, void *stream)
    {
        // if remote connection is without HostID, cannot record
        if (id == NET_ID_UNASSIGNED)
        {
            printf ("net_ace::socket_connected () empty id given\n");
            return false;
        }

        // store the connection info
        ConnectInfo conn (stream, this->getTimestamp ());        
        bool stored = false;

        _conn_mutex.acquire ();
        if (_id2conn.find (id) == _id2conn.end ())
        {
            _id2conn.insert (std::map<id_t, ConnectInfo>::value_type (id, conn));
            stored = true;
        }
        _conn_mutex.release ();

        // if connection wasn't stored successfully (possibly connection already established)
        if (stored == false)
        {
            printf ("net_ace::socket_connected () stream already registered\n");            
        }

        /* TO-DO: check why is storing mapping necessary. it's probably not
        //        or, possible that when we try to reply to sender, the mapping is needed?

        // store address mapping for remote node (but only if it's a new node)
        ACE_SOCK_Stream &s = *((net_ace_handler *)stream);
        ACE_INET_Addr remote_addr;
        s.get_remote_addr (remote_addr);

        // port is destinated as '0' because it's a send port, not a listen port
        // so is only valid for this connection
        //IPaddr ip (remote_addr.get_ip_address (), remote_addr.get_port_number ()); 
        IPaddr ip (remote_addr.get_ip_address (), 0); 
        Addr addr (id, &ip);

        storeMapping (addr);
        */
        
        return stored;
    }

    bool
    net_ace::
    socket_disconnected (id_t id)
    {
        if (_active == false)
            return false;

        bool success = false;

        _conn_mutex.acquire ();
        if (_id2conn.find (id) != _id2conn.end ())
        {
            _id2conn.erase (id);
            success = true;
        }
        _conn_mutex.release ();
                    
        // TODO: store mesasge 
        // send a DISCONNECT notification
        // NOTE that storing a message with size 0 won't conflict with normal messages 
        //      with no parameters sent, as those messages will 
        //      at least have some content sent to storeMessage
        
        if (success)
        {
            /*
            // store a NULL message to indicate disconnection to message queue
            NetSocketMsg *msg = new NetSocketMsg;

            msg->fromhost = id;
            msg->recvtime = this->getTimestamp ();
            msg->msg = NULL;
            msg->size = 0;
            */

            this->msg_received (id, NULL, 0, this->getTimestamp ());
            //Message *msg = new Message (DISCONNECT);
            //storeRawMessage (id, msg);
        }
        
        return success;
    }

} // end namespace Vast
