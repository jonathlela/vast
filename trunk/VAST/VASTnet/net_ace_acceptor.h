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

#ifndef VAST_NETWORK_ACCEPTOR_H
#define VAST_NETWORK_ACCEPTOR_H

#if !defined (ACE_LACKS_PRAGMA_ONCE)
# pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

#include "ace/SOCK_Acceptor.h"
#include "ace/Event_Handler.h"

#include "net_ace_handler.h"

namespace Vast {

class net_ace_acceptor : public ACE_Event_Handler 
{

public:

    net_ace_acceptor (void *handler)
       : _msghandler (handler)
    {
    }

    int open (const ACE_INET_Addr &addr, ACE_Reactor *reactor) 
    {
        // NEW_THREAD acceptor open?
        if (_acceptor.open (addr, 1) == -1)
            return -1;

        _reactor = reactor;
       
        //return 0; 
        return _reactor->register_handler (this, ACE_Event_Handler::ACCEPT_MASK);
    }

    
protected:
    // reactor will need this handle for its internal usage
    ACE_HANDLE get_handle (void) const 
    {
        return this->_acceptor.get_handle();
    }

    // when an incoming connection occurs
    virtual int handle_input (ACE_HANDLE handle) 
    {
        //printf ("acceptor handle_input() called\n");
        //ACE_DEBUG ((LM_DEBUG, "Acceptor handle_input() called for %d\n", handle));
     
        // handle can be used to differentiate if the same handler
        // is used for handling multiple connections, not used here
        ACE_UNUSED_ARG (handle);
        
        // create new handler
        net_ace_handler *handler;
        ACE_NEW_RETURN (handler, net_ace_handler, -1);

        // accept the connection, note that the handler object
        // is treated as a SOCK_Stream
        // NOTE: currently we only take non-SSL incoming connections, note the handler used
        if (this->_acceptor.accept (*handler) == -1)
            ACE_ERROR_RETURN ((LM_ERROR,
                               "%p",
                               "accept failed"),
                               -1);
        //ACE_DEBUG ((LM_DEBUG, "handler created for %d\n", handle));
 
        // open the handler object, default id is NET_ID_UNASSIGNED
        if (handler->open (_reactor, _msghandler) == -1)
        {
            ACE_ERROR ((LM_ERROR, "net_ace_acceptor: cannot open handler for fd: %d\n", handle));
            handler->close();
        }

        ACE_SOCK_Stream &stream = *handler;

        // display message
        ACE_INET_Addr remote_addr;
        stream.get_remote_addr (remote_addr);
        printf ("connection accepted from remote addr: (%s, %d)\n", remote_addr.get_host_addr (), remote_addr.get_port_number ());

        return 0;
    }

    // if handle_input() returns -1, reactor would call handle_close()
    int handle_close (ACE_HANDLE, ACE_Reactor_Mask mask)
    {
        // IMPORTANT: close socket so port can be re-use
        _acceptor.close ();

        _reactor->remove_handler (this, mask | ACE_Event_Handler::DONT_CALL);
                     
        ACE_DEBUG ((LM_DEBUG, "(%5t) acceptor handle_close()\n"));
        delete this;
        return 0;
    }

private:
    ACE_SOCK_Acceptor _acceptor;
    ACE_Reactor      *_reactor;

    // a pointer to a net_ace object (passed to net_ace_handler when handling incoming connections)
    void             *_msghandler;

};

} // end namespace Vast

#endif // VAST_NETWORK_ACCEPTOR_H

