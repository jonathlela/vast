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


#include "net_ace_handler.h"
#include "net_ace.h"

namespace Vast {

    int 
    net_ace_handler::
    open (ACE_Reactor *reactor, void *msghandler, id_t remote_id) 
    {
        _remote_id = NET_ID_UNASSIGNED;
        _reactor = reactor;
        _msghandler = msghandler;

        if (_reactor->register_handler (this, ACE_Event_Handler::READ_MASK) == -1)
            ACE_ERROR_RETURN ((LM_ERROR,
                               "(%5t) [%d] cannot register with reactor\n", 0), 
                               -1 );

        // by default remote id is unknown, but should be known
        // if this handler is used to initiate a new connection out
        if (remote_id != NET_ID_UNASSIGNED)
            _remote_id = ((net_ace *)_msghandler)->register_conn (remote_id, this);
        
        return 0;
    }

    // close connection & unregister from reactor
    int
    net_ace_handler::
    close (void)
    {
        return this->handle_close (ACE_INVALID_HANDLE, ACE_Event_Handler::RWE_MASK);
    }

    ACE_SOCK_Dgram *
    net_ace_handler::openUDP (ACE_INET_Addr addr)
    {
        // initialize UDP socket
        _udp = new ACE_SOCK_Dgram;
        _udp->open (addr);        
        _local_addr = addr;

        return _udp;
    }

    // handling incoming message
    // here we just extract the bytestring, and let process_msg () handles the rest
    int 
    net_ace_handler::
    handle_input (ACE_HANDLE fd) 
    {    

        //ACE_DEBUG ((LM_DEBUG, "\nhandle_input() fd: %d\n", fd));
                 
        // return values are:
        //  -1: error
        //   0: connection closed
        //  >0: bytes read
        size_t n;
        size_t msg_size;
        
        // for TCP connection
        if (_udp == NULL) 
        {
            // get size of message
            switch (n = this->_stream.recv_n (&msg_size, sizeof(size_t))) 
            {
            case -1:
                ACE_ERROR_RETURN( (LM_ERROR, "(%5t) [tcp-size] %p bad read\n", "net_ace_handler"), -1 );
            case 0:
                ACE_ERROR_RETURN( (LM_DEBUG, "(%5t) [tcp-size] remote close (fd = %d)\n", this->get_handle() ), -1 );
            }
            
            //ACE_DEBUG( (LM_DEBUG, "(%5t) handle_input: msgsize: %d\n", msg_size ) );
            
            // check buffer size
            _buf.reserve (msg_size);

            // get message body
            switch (n = this->_stream.recv_n (_buf.data, msg_size)) 
            {
            case -1:
                ACE_ERROR_RETURN( (LM_ERROR, "(%5t) [tcp-body] %p bad read\n", "net_ace_handler"), -1 );
            case 0:            
                ACE_ERROR_RETURN( (LM_DEBUG, "(%5t) [tcp-body] remote close (fd = %d)\n", this->get_handle() ), -1 );
            default:
                if (n != msg_size)
                    ACE_ERROR_RETURN( (LM_ERROR, "(%5t) [tcp-body] size mismatch (expected:%d actual:%d)\n", msg_size, n ), -1 );
            }

            processmsg (_buf.data, n);
        }
        // for UDP packets
        else 
        {
            switch (n = _udp->recv (_buf.data, VAST_BUFSIZ, _local_addr)) 
            {
            case -1:
                ACE_ERROR_RETURN( (LM_ERROR, "(%5t) [udp-size] %p bad read\n", "net_ace_handler"), -1 );
            case 0:
                ACE_ERROR_RETURN( (LM_DEBUG, "(%5t) [udp-size] remote close (fd = %d)\n", this->get_handle() ), -1 );
            default:
                // extract message size
                memcpy (&msg_size, _buf.data, sizeof (size_t));
            }
            
            if (msg_size != (n - sizeof(size_t)))
                ACE_ERROR_RETURN ((LM_ERROR, "(%5t) [udp-body] size mismatch (expected:%d actual:%d)\n", msg_size, n - sizeof(size_t)), -1);
            
            processmsg (_buf.data + sizeof (size_t), msg_size);
        }
        
        return 0;
    }

    // if handle_input() returns -1, reactor would call handle_close()
    int 
    net_ace_handler::
    handle_close (ACE_HANDLE, ACE_Reactor_Mask mask)
    {
        _reactor->remove_handler (this, mask | ACE_Event_Handler::DONT_CALL);
     
        // unregister from message handler, do this first to avoid message sending attempt
        // to the stream object
        if (_remote_id != NET_ID_UNASSIGNED)
            ((net_ace *)_msghandler)->unregister_conn (_remote_id);

        // close the socket
        _stream.close();
     
        ACE_DEBUG ((LM_DEBUG, "(%5t) handle_close(): [%d]\n", _remote_id));
        delete this;
        return 0;
    }

    // whatever bytestring received is processed here in its entirety
    void
    net_ace_handler::
    processmsg (const char *msg, size_t size)
    {            
        // if the remote node doesn't have an id yet, we should
        //  1. assign an internal temp id if I'm the gateway (and relay the request for ID)
        //  2. reject connection if I'm not gateway or the message isn't a request for ID

        // store the incoming message
        size_t n = size - (sizeof (id_t) + sizeof (timestamp_t));
        timestamp_t      senttime;
        id_t             id;
        char *p = (char *)msg;

        // extract id
        memcpy (&id, p, sizeof (id_t));
        p += sizeof (id_t);
        
        // extract time stamp
        memcpy (&senttime, p, sizeof (timestamp_t));
        p += sizeof (timestamp_t);
        
        // if the message comes from a reliable (TCP) connection
        if (_udp == NULL) 
        {
            // check if id is valid
            if (_remote_id == NET_ID_UNASSIGNED)
            {
                if (id != NET_ID_UNASSIGNED)
                    _remote_id = id;                  
                
                _remote_id = ((net_ace *)_msghandler)->register_conn (_remote_id, this);
            }        
            // remote node has obtained a new id (or corrupted?)
            else if (_remote_id != id)
                _remote_id = ((net_ace *)_msghandler)->update_conn (_remote_id, id);                        
        }
        else
            _remote_id = id;

        // store to message queue
        if (_remote_id != NET_ID_UNASSIGNED)        
            ((net_ace *)_msghandler)->storeMessage (_remote_id, p, n, senttime);
    }

} // end namespace Vast



