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
        

        // obtain remote host's detected IP for public IP check                                            
        ACE_INET_Addr remote_addr;
        this->_stream.get_remote_addr (remote_addr);
        _remote_IP.host = remote_addr.get_ip_address ();
        _remote_IP.port = remote_addr.get_port_number ();

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
        VASTHeader header;      // message header
        
        // for TCP connection
        if (_udp == NULL) 
        {
            // get size of message            
            size_t bytes_transferred = 0;
            switch (n = this->_stream.recv_n (&header, sizeof (VASTHeader), 0, &bytes_transferred))
            {
            case -1:
                ACE_ERROR_RETURN ((LM_ERROR, "(%5t) [tcp-size] bad read due to (%p) received_bytes: %d\n", "net_ace_handler", bytes_transferred), -1);
            case 0:
                ACE_ERROR_RETURN ((LM_DEBUG, "(%5t) [tcp-size] remote close (fd = %d)\n", this->get_handle() ), -1);
            }
            
            //ACE_DEBUG( (LM_DEBUG, "(%5t) handle_input: header.msgsize: %d, bytes_transferred: %u\n", header.msg_size, bytes_transferred) );
            
            // check buffer size
            _buf.reserve (header.msg_size);

            // get message body
            switch (n = this->_stream.recv_n (_buf.data, header.msg_size, 0, &bytes_transferred)) 
            {
            case -1:
                //ACE_ERROR_RETURN ((LM_ERROR, "(%5t) [tcp-body] bad read due to (%p) \n", "net_ace_handler"), -1);
                ACE_ERROR_RETURN ((LM_ERROR, "(%5t) [tcp-body] bad read due to (%p) received_bytes: %d\n", "net_ace_handler", bytes_transferred), -1);
            case 0:            
                ACE_ERROR_RETURN ((LM_DEBUG, "(%5t) [tcp-body] remote close (fd = %d)\n", fd), -1);
            default:
                if (n != header.msg_size)
                    ACE_ERROR_RETURN ((LM_ERROR, "(%5t) [tcp-body] size mismatch (expected:%u actual:%u)\n", header.msg_size, n), -1);
            }
           
            //printf ("msgsize: %lu bytes_transferred: %lu\n", n, bytes_transferred);        
            
            // handle raw message
            id_t id = ((VASTnet *)_msghandler)->processRawMessage (header, _buf.data, _remote_id, &_remote_IP, this);

            if (id == NET_ID_UNASSIGNED)
                return (-1);
            else
                _remote_id = id;                            
        }
        // for UDP packets
        else 
        {
            // TODO: check if this will occur
            if (_remote_id == NET_ID_UNASSIGNED)
            {
                printf ("net_ace_handler (): UDP message received, but handler's remote_id not yet known\n");
                return 0;
            }

            switch (n = _udp->recv (_buf.data, VAST_BUFSIZ, _local_addr)) 
            {
            case -1:
                //ACE_ERROR_RETURN ( (LM_ERROR, "(%5t) [udp-size] %p bad read\n", "net_ace_handler"), -1 );
                ACE_DEBUG ((LM_DEBUG, "(%5t) [udp-size] bad read due to (%p) \n", "net_ace_handler"));
                return 0;
            case 0:
                ACE_ERROR_RETURN ((LM_ERROR, "(%5t) [udp-size] remote close (fd = %d)\n", fd), -1);
            default:
                break;
            }

            char *p = _buf.data;

            // NOTE that there may be several valid UDP messages received at once            
            while (n > sizeof (size_t))
            {            
                // extract message header
                memcpy (&header, p, sizeof (VASTHeader));
                n -= sizeof (VASTHeader);
                p += sizeof (VASTHeader);

                // NOTE UDP size mismatch is not critical, just drop this packet
                //      if actual payload is not long enough as expected               
                if (n < header.msg_size)
                {                    
                    ACE_DEBUG ((LM_DEBUG, "(%5t) [udp-body] (%p) size error (expected:%u actual:%u)\n", "net_ace_handler", header.msg_size, n));
                    return 0;
                }
            
                // NOTE: no error checking is performed
                //processmsg (header, p);
                ((VASTnet *)_msghandler)->processRawMessage (header, p, _remote_id);

                // next message
                p += header.msg_size;
                n -= header.msg_size;
            }
        }
        
        return 0;
    }

    // if handle_input() returns -1, reactor would call handle_close()
    int 
    net_ace_handler::
    handle_close (ACE_HANDLE, ACE_Reactor_Mask mask)
    {
        // NOTE: it's possible reactor may already be deleted if handle_close is caused by
        //       net_ace object deletion
        if (_reactor != NULL)
            _reactor->remove_handler (this, mask | ACE_Event_Handler::DONT_CALL);
     
        // unregister from message handler, do this first to avoid message sending attempt
        // to the stream object
        if (_remote_id != NET_ID_UNASSIGNED)
            ((net_ace *)_msghandler)->unregister_conn (_remote_id);

        // close the socket
        _stream.close();
     
        ACE_DEBUG ((LM_DEBUG, "(%5t) handle_close (): [%d]\n", _remote_id));
        delete this;
        
        return 0;
    }

    /*
    int processmsg (VASTHeader &header, const char *msg)
    {    
        // if the remote node doesn't have an id yet, we should
        //  1. assign a new id if I'm the relay
        //  2. reject connection if I'm not relay or the message isn't a request for ID

        // store the incoming message
        size_t n = size - (sizeof (id_t) + sizeof (timestamp_t));
        id_t             fromhost;
        timestamp_t      senttime;        
        char *p = (char *)msg;

        // extract id
        memcpy (&fromhost, p, sizeof (id_t));
        p += sizeof (id_t);
        
        // extract time stamp
        memcpy (&senttime, p, sizeof (timestamp_t));
        p += sizeof (timestamp_t);
        
        // if the message comes from a reliable (TCP) connection
        if (_udp == NULL) 
        {
            // check if we're getting a new ID
            if (fromhost == NET_ID_NEWASSIGNED)
            {
                ((VASTnet *)_msghandler)->processHandshake (p, n);
                return;
            }

            // check if it's a new incoming connection
            else if (_remote_id == NET_ID_UNASSIGNED)
            {
                // simply a new connection with valid ID
                if (fromhost != NET_ID_UNASSIGNED)
                {
                    _remote_id = fromhost;
                    ((net_ace *)_msghandler)->register_conn (_remote_id, this); 
                }
                // the remote host just joins the network and needs an ID
                else
                {
                    ((VASTnet *)_msghandler)->extractIDRequest (p, n);
                    // we do not process any message further
                    return;
                }
            }        
            // remote node has obtained a new id (or corrupted?)
            else if (_remote_id != fromhost)
            {
                printf ("net_ace_handler::processmsg () remote HostID has changed from [%lld] to [%lld]\n", _remote_id, fromhost);                
                //_remote_id = ((net_ace *)_msghandler)->update_conn (_remote_id, fromhost);

                return;
            }
        }
        else
            // NOTE we assume that any UDP connection has an accompanying TCP connection
            //      already made
            _remote_id = fromhost;

        // store to message queue
        if (_remote_id != NET_ID_UNASSIGNED)        
            ((net_ace *)_msghandler)->storeRawMessage (_remote_id, p, n, senttime, ((net_ace *)_msghandler)->getTimestamp ());
    }
    */
        

} // end namespace Vast



