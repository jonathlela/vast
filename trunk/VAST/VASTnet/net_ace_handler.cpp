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


#include "net_ace_handler.h"
#include "net_ace.h"

namespace Vast {

    // default constructor
    net_ace_handler::
    net_ace_handler () 
    {
        _udp = NULL;
        _remote_id = NET_ID_UNASSIGNED;
        _reactor = NULL;
        _msghandler = NULL;
        _secure = false;
    }

    int 
    net_ace_handler::
    open (ACE_Reactor *reactor, void *msghandler, id_t remote_id) 
    {        
        // if this handler is used to init a new connection out
        // remote_id should be given, otherwise, this is an incoming connection
        // and we can determine remote host's IP & port as the remote_id

        // obtain remote host's detected IP for public IP check
        // TODO: should move this function elsewhere?
        ACE_INET_Addr remote_addr;

#ifdef VAST_USE_SSL
        if (_secure)
        {
            printf ("\nnet_ace_handler::open () getting remote addr from secure stream, handle: %llu\n", _ssl_stream.get_handle ());
            this->_ssl_stream.get_remote_addr (remote_addr);
        }
        else
#endif
            this->_stream.get_remote_addr (remote_addr);
        _remote_addr.host = remote_addr.get_ip_address ();
        _remote_addr.port = remote_addr.get_port_number ();

        char remote_host[80];
        _remote_addr.getString (remote_host);
        printf ("\nnet_ace_handler::open () remote_addr: %s\n", remote_host);

        // if socket connection indeed exists, try to determine remote ID & register socket
        // otherwise (a UDP handler) simply skip this part
        if (_remote_addr.host != 0 && _remote_addr.port != 0)
        {
            // determine remoteID based on detected IP & port
            if (remote_id == NET_ID_UNASSIGNED)
            {
                remote_id = ((net_manager *)msghandler)->resolveHostID (&_remote_addr);
                //ACE_DEBUG ((LM_DEBUG, "\nnet_ace_handler::open () detecting remote_id as: [%llu]\n", remote_id));
                printf ("\nnet_ace_handler::open () detecting remote_id as: [%llu]\n", remote_id);
            }
        
            if (((net_ace *)msghandler)->socket_connected (remote_id, this, _secure) == false)
            {
                // possible a remote connection is already established (remote connect)
                ACE_ERROR_RETURN ((LM_ERROR,
                               "(%5t) [%llu] socket registeration failed, maybe connection already exists\n", remote_id), 
                               -1);            
            }
        }
        else
            ACE_DEBUG ((LM_DEBUG, "net_ace_handler::open () creating UDP listener, remote_id [%d]\n", remote_id));

        // hook this handler to reactors to listen for events
        if (reactor->register_handler (this, ACE_Event_Handler::READ_MASK) == -1)
            ACE_ERROR_RETURN ((LM_ERROR,
                               "(%5t) [%l] cannot register with reactor\n", remote_id), 
                               -1 );

        _reactor    = reactor;
        _msghandler = msghandler;
        _remote_id  = remote_id;            
        
        return 0;
    }

    // close connection & unregister from reactor
    int
    net_ace_handler::
    close (void)
    {
        return this->handle_close (ACE_INVALID_HANDLE, ACE_Event_Handler::RWE_MASK);
    }

    // open UDP listen port
    ACE_SOCK_Dgram *
    net_ace_handler::openUDP (ACE_INET_Addr addr)
    {
        // initialize UDP socket
        _udp = new ACE_SOCK_Dgram;
        _udp->open (addr);
        _local_addr = addr;

        return _udp;
    }

    // obtain address of remote host
    IPaddr &
    net_ace_handler::getRemoteAddress ()
    {
        return _remote_addr;
    }

    // swtich remote ID to a new one
    bool 
    net_ace_handler::switchRemoteID (id_t oldID, id_t newID)
    {
        if (oldID != _remote_id)
            return false;

        _remote_id = newID;
        return true;
    }

    //
    // handling incoming message
    // here we just extract the bytestring, and store to msghandler for later processing
    //
    // the general rule is we'll read until linefeed (LF) '\n' is met
    // there are two cases: 
    //
    //      1) VAST messages
    //              there's a 4-byte VAST message header right before the LF
    //              in such case, the header is read, and "message size" extracted
    //              the handler will keep reading "message size". 
    //              The "VASTHeader + message content" is considered a single message & stored
    //
    //      2) Socket message
    //              if whatever before the LF cannot be identified as a VASTHeader
    //              then everything before the LF is considered a single message & stored
    //
    int 
    net_ace_handler::
    handle_input (ACE_HANDLE fd) 
    {    
        //ACE_DEBUG ((LM_DEBUG, "\nhandle_input() fd: %d\n", fd));
                 
        // return values are:
        //  -1: error
        //   0: connection closed
        //  >0: bytes read
        size_t n = 0;
        //VASTHeader header;      // message header for VAST messages
        
        // for TCP connection        
        if (_udp == NULL) 
        {                        
            // get message body 
            // TODO: make buffer to vary in size?
#ifdef VAST_USE_SSL
            switch (n = (_secure ? this->_ssl_stream.recv (_buf.data, VAST_BUFSIZ) : this->_stream.recv (_buf.data, VAST_BUFSIZ)))
#else
            switch (n = this->_stream.recv (_buf.data, VAST_BUFSIZ))
#endif
            {
            case -1:
                ACE_ERROR_RETURN ((LM_ERROR, "(%5t) [tcp-body] bad read due to (%p) \n", "net_ace_handler"), -1);
            case 0:            
                ACE_ERROR_RETURN ((LM_DEBUG, "(%5t) [tcp-body] remote close (fd = %d)\n", fd), -1);
            }
        }
        // for UDP packets
        else 
        {
            // NOTE: As a packet may contain more than one message
            //       VASTHeader extraction is performed later
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

            // store UDP messages

            /*
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
            
                // BUG: processVASTMessage may make mistake as _remote_id is empty
                // NOTE: no error checking is performed
                id_t id = ((net_ace *)_msghandler)->processVASTMessage (header, p, _remote_id);
                
                // NOTE: _remote_id is not set because this handler may receive / process
                //       messages from multiple senders 
                   
                // next message
                p += header.msg_size;
                n -= header.msg_size;
            }
            */

        }

        // store message
        ((net_ace *)_msghandler)->msg_received (_remote_id, _buf.data, n);
        
        return 0;
    }

    // if handle_input() returns -1, reactor would call handle_close()
    int 
    net_ace_handler::
    handle_close (ACE_HANDLE, ACE_Reactor_Mask mask)
    {
        bool disconnect_success = true;

        // unregister from message handler, do this first to avoid message sending attempt
        // to the stream object
        if (_remote_id != NET_ID_UNASSIGNED)
        {
            if (((net_ace *)_msghandler)->socket_disconnected (_remote_id) == false)
                disconnect_success = false;
        }

        // NOTE: if reactor is already invalid (if handle_close () is caused by
        //       net_ace object deletion, may crash, but here we cannot know for sure
        //       so success after calling socket_disconnected () is a better indicator
        if (disconnect_success && _reactor != NULL)
            _reactor->remove_handler (this, mask | ACE_Event_Handler::DONT_CALL);
     
        // IMPORTANT: close the socket so port can be released for re-use
#ifdef VAST_USE_SSL
        if (_secure)
            _ssl_stream.close ();
        else
#endif
            _stream.close ();
     
        ACE_DEBUG ((LM_DEBUG, "(%5t) handle_close (): [%d]\n", _remote_id));
        delete this;
        
        return 0;
    }
       
} // end namespace Vast



