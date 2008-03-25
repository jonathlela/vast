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

namespace VAST {

    // in vast_dc.cpp, for debug display purposes
    //extern char VAST_MESSAGE[][20];

    //
    // inherent methods from class 'network'
    //

    void 
    net_ace::
    register_id (id_t my_id)
    {
        _id = my_id;  
        _id2addr[_id] = _addr;
    }

    int 
    net_ace::
    connect (id_t target)
    {
        printf ("net_ace: connect (): Not implemented.\n");
        return 0;
    }

    int 
    net_ace::
    connect (Addr & addr)
    {
        printf ("net_ace: connect (): Not implemented.\n");
        return 0;
    }

    int 
    net_ace::
    connect (id_t target_id, Addr addr)
    {
        if (_active == false)
            return (-1);

        // avoid self-connection
        if (target_id == _id)
            return 0;

        // convert target address to ACE format
        ACE_INET_Addr target_addr ((u_short)addr.publicIP.port, (ACE_UINT32)addr.publicIP.host);

        // record address first
        _id2addr[target_id] = addr;
        
        if (is_connected (target_id) == true)
        {
#ifdef DEBUG_DETAIL
            //ACE_SOCK_Stream &stream = *((net_ace_handler *)_id2conn[target_id]);
            //ACE_DEBUG ((LM_DEBUG, "existing connection to (%s:%d)\n", ));
#endif            
            ACE_ERROR_RETURN ((LM_ERROR, "(%5t) connect(): connection already exists for [%d] (%s:%d)\n", target_id, target_addr.get_host_addr (), target_addr.get_port_number ()), 0);
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
                _id2addr.erase (target_id);
                ACE_ERROR_RETURN ((LM_ERROR, "connect to %s:%d failed after 3 attempts", target_addr.get_host_addr (), target_addr.get_port_number ()), (-1));
            }
            
            attempt_count++;
            ACE_ERROR ((LM_ERROR, "connect %s:%d failed (try %d): %p\n", target_addr.get_host_addr (), target_addr.get_port_number (), attempt_count));
            ACE_OS::sleep (tv);
        }
        
        // open the handler object
        if (handler->open (_reactor, this, target_id) == -1) 
        {
            handler->close();
            return (-1);
        }
        
        ACE_DEBUG ((LM_DEBUG, "(%5t) connect(): connected to [%d] (%s:%d)\n", target_id, target_addr.get_host_addr (), target_addr.get_port_number ()));

        // record address first 
        // BUG: put it here will have weird/incorrect connection behavior for chatva (but why?)
        //_id2addr[target_id] = addr;

        return 0;
    }

    int 
    net_ace::
    disconnect (id_t target)
    {
        if (is_connected (target) == false)
            return false;
        
        // clean up the out message queue for this connection first
        if (_msgqueue_out.find (target) != _msgqueue_out.end ())
        {
            netmsg *queue = _msgqueue_out[target];
            netmsg *curr;
            while (queue != NULL)
            {
                queue = queue->getnext (&curr);
                delete curr;
            }
            
            // clean up queue content
            _msgqueue_out.erase (target);
            _msgqueue_out_length.erase (target);   
        }

        // BUG: if mutex is used here the program will stale under Linux (reason unknown)
        //_conn_mutex.acquire ();
        net_ace_handler *handler = (net_ace_handler *)_id2conn[target];       
        handler->close();
        //_conn_mutex.release ();
        
        ACE_DEBUG ((LM_DEBUG, "(%5t) disconnect(): [%d] disconnected\n", (int)target));
        
        return 0;
    }

    Addr &
    net_ace::
    getaddr (id_t id)
    {
#ifdef DEBUG_DETAIL
        if (_id2addr.find (id) == _id2addr.end ())
            printf ("get_addr (): address not found for [%d]\n", (int)id);
#endif
        return _id2addr[id];
    }

    // get a list of currently active connections' remote id and IP addresses
    std::map<id_t, Addr> &
    net_ace::getconn ()
    {
        return _id2addr;
    }

    // send message to a node
    int 
    net_ace::
    sendmsg (id_t target, msgtype_t msgtype, char const *msg, size_t len, bool reliable, bool buffered)
    {
#ifdef DEBUG_DETAIL
        printf ("[%3d]    sendmsg   to: %3d type: (%d) size:%3d\n", (int)_id, target, (int)msgtype, len);
#endif  
        // if it's a loop-back message we simply store it
        if (target == _id)
        {
            return storemsg (target, msgtype, msg, len, get_curr_timestamp ());
        }

        // cannot send unreliable message exceedings system buffer size
        if (len > VAST_BUFSIZ && reliable == false)
        {
            ACE_DEBUG ((LM_NOTICE, "(%5t) ERROR: sendmsg(): sendsize exceeds VAST_BUFSIZ for unreliable message at time %d\n", time));
            return (-1);
        }

        if (is_connected (target) == false) 
        {            
            ACE_DEBUG ((LM_NOTICE, "(%5t) ERROR: sendmsg(): target id [%d] mapping not found at time %d\n", target, time));
            return (-1);
        }

        // TODO: more efficient way to implement message buffer?
        size_t size = sizeof (id_t) + sizeof (msgtype_t) + sizeof (timestamp_t) + len;
        _send_buf.reserve (size);

        //
        // prepare bytestring
        //

        timestamp_t curr_time = get_curr_timestamp ();
        // append size, id, message type & timestamp in front of the bytestring  
        _send_buf.add (&size, sizeof (size_t));
        _send_buf.add (&_id, sizeof (id_t));        
        _send_buf.add (&msgtype, sizeof (msgtype_t));
        _send_buf.add ((void *)&curr_time, sizeof (timestamp_t));
        _send_buf.add ((void *)msg, len);

        // store to output queue if it's reliable message
        if (reliable == true)                        
        {    
            // get the TCP stream and send right away
            if (buffered == false)
            {
                _conn_mutex.acquire ();
                ACE_SOCK_Stream &stream = *((net_ace_handler *)_id2conn[target]);
                stream.send_n (_send_buf.data, _send_buf.size);
                _conn_mutex.release ();
            }
            // otherwise we put into a queue first
            else
            {
                // create new message
                netmsg *newmsg = new netmsg (_id, _send_buf.data, _send_buf.size, msgtype, get_curr_timestamp (), NULL, NULL);
            
                // failure likely due to memory allocation problem
                if (newmsg->size < 0)
                    return 0;
            
                // obtain msgqueue for this target, create one if necessary             
                if (_msgqueue_out.find (target) != _msgqueue_out.end ())            
                {
                    netmsg *out_queue = _msgqueue_out[target];
                    out_queue->append (newmsg, get_curr_timestamp ());
                    _msgqueue_out_length[target]  = _msgqueue_out_length[target] + newmsg->size;
                }
                else
                {
                    _msgqueue_out[target] = newmsg;
                    _msgqueue_out_length[target] = newmsg->size;
                }                 
            }                 
        }
        else
        {
            Addr addr = _id2addr[target];
            ACE_INET_Addr target_addr ((u_short)addr.publicIP.port, (ACE_UINT32)addr.publicIP.host);
            _udp->send (_send_buf.data, _send_buf.size, target_addr);
        }

        return len;
    }
    
    // obtain next message in queue before a given timestamp
    // returns size of message, or -1 for no more message
    //
    // (copied verbatiam from net_emu.cpp) TODO: consider to combine them?
    int
    net_ace::
    recvmsg (id_t &from, msgtype_t &msgtype, timestamp_t &recvtime, char **msg/*, timestamp_t time*/)
    {
        
        // return if no more message in queue 
        // or all messages are beyond current logical time
        //
        // NOTE: we simply return the next message in queue, sorted by timestamp
        if (_msgqueue_in == NULL) // || (time > 0 && time <= _msgqueue_in->time))
            return -1;
        
        netmsg *curr_msg;

        // TODO: should place critical section around _msgqueue access
        _msg_mutex.acquire ();
        _msgqueue_in = _msgqueue_in->getnext (&curr_msg); 
        _msg_mutex.release ();
        
        // prepare the return data
        from     = curr_msg->from;
        msgtype  = curr_msg->msgtype;
        recvtime = curr_msg->time;        
        size_t size = curr_msg->size;

        // allocate receive buffer
        _recv_buf.reserve (size);

        memcpy (_recv_buf.data, curr_msg->msg, size);
        *msg = _recv_buf.data;
        
        // de-allocate memory
        delete curr_msg;
        return size;
    }
    
    // insert the new message as a node in the double linklist
    // NOTE: the list should be sorted by timestamp when inserting
    //
    // (copied verbatiam from net_emu.cpp) TODO: consider to combine them?    
    int
    net_ace::
    storemsg (id_t from, msgtype_t msgtype, char const *msg, size_t len, timestamp_t time)
    {                            
        // store the new message
        netmsg *newnode = new netmsg (from, msg, len, msgtype, time, NULL, NULL);
        
        // failure likely due to memory allocation problem
        if (newnode->size < 0)
            return 0;
        
        // TODO: should place critical section around _msgqueue_in
        _msg_mutex.acquire ();
        // insert unconditionally if the queue is empty
        if (_msgqueue_in == NULL)
            _msgqueue_in = newnode;
        else
            _msgqueue_in->append (newnode, time);
        _msg_mutex.release ();
        
        return len;
    }

    // send out all pending reliable message in a single packet to each target
    int 
    net_ace::
    flush (bool compress)
    {
        int flush_size = 0;     // number of total bytes sent this time

        // check if there are any pending send queues
        std::map<id_t, netmsg *>::iterator it = _msgqueue_out.begin ();

        //int pos = 0;
        vastbuf buf;
        while (it != _msgqueue_out.end ())
        {
            id_t target     = it->first;        
            netmsg *queue   = it->second;

            // extract all messages from the queue into one buffer
            size_t len = _msgqueue_out_length[target];
            buf.reserve (len);

            netmsg *curr_msg;
            while (queue != NULL)
            {
                queue = queue->getnext (&curr_msg);

                // check if we've exceed allocated memory
                if (buf.size + curr_msg->size > len)
                {
                    ACE_ERROR ((LM_ERROR, "flush (): actual content & recorded size differ in send queue\n"));
                    return flush_size;
                }

                buf.add (curr_msg->msg, curr_msg->size);
                delete curr_msg;
            }

            // check for connection
            if (is_connected (target) == false)
            {            
                ACE_DEBUG ((LM_NOTICE, "(%5t) ERROR: flush(): target id [%d] mapping not found at time %d\n", target, time));
            }
            else
            {
                // get the TCP stream and send away
                _conn_mutex.acquire ();
                ACE_SOCK_Stream &stream = *((net_ace_handler *)_id2conn[target]);
                stream.send_n (buf.data, buf.size);
                _conn_mutex.release ();
                flush_size += buf.size;
            }
            
            it++;
        }

        // clear contents in send queue & send queue size
        _msgqueue_out.clear ();
        _msgqueue_out_length.clear ();

        return flush_size;
    }

    // notify ip mapper to create a series of mapping from src_id to every one in the list map_to
    int
    net_ace::
    notify_id_mapper (id_t src_id, const std::vector<id_t> & map_to)
    {
        printf ("net_ace: notify_id_mapper (): Not implemented.\n");
        return 0;
    }

    // get current physical timestamp
    timestamp_t
    net_ace::
    get_curr_timestamp ()
    {
        return (timestamp_t) clock ();
    }

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
        _addr.set_public ((unsigned long)addr.get_ip_address (), 
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
    // net_ace specific methods
    //

    // methods to keep track of active connections
    // returns the id being assigned (temp id may be assigned if unassigned id is received)
    id_t
    net_ace::
    register_conn (id_t id, void *stream)
    {
        if (is_connected (id) == true)
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
        if (is_connected (id) == false)
            return NET_ID_UNASSIGNED;

#ifdef DEBUG_DETAIL
        printf ("unregister_conn [%d]\n", (int)id);
#endif
        _conn_mutex.acquire ();
        _id2conn.erase (id);
        _id2addr.erase (id);
        _conn_mutex.release ();
                        
        // send a DISCONNECT notification?
        char msg[1+sizeof (id_t)];
        msg[0] = 1;
        memcpy (msg+1, &_id, sizeof (id_t));
        storemsg (id, DISCONNECT, msg, 1+sizeof (id_t), get_curr_timestamp ()/*_curr_time*/);
        
        return id;
    }    

    id_t 
    net_ace::
    update_conn (id_t prev_id, id_t curr_id)
    {
        if (is_connected (prev_id) == false)
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

} // end namespace VAST
