/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2009 Shun-Yun Hu  (syhu@yahoo.com)
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
 *  Relay.h
 *
 *  Super-peers that serve as entry points of the overlay
 *
 *  history 2010/01/21  1st implementation
 *
 */



#ifndef _VAST_Relay_H
#define _VAST_Relay_H

#include "Config.h"
#include "VASTTypes.h"
#include "MessageHandler.h"

using namespace std;

namespace Vast
{

    typedef enum 
    {
        QUERY = VON_MAX_MSG,    // find the closet node to a particular physical coordinate
        QUERY_REPLY,            // the closest node

    } RELAY_Message;

    class Relay : public MessageHandler
    {

    public:

        // initialize a Relay node with a number of potential entries to the overlay
        Relay ();

        ~Relay ();

    private:

        // perform initialization tasks for this handler (optional)
        // NOTE that all internal variables (such as handler_no) have been set at this point
        void initHandler ();

        // returns whether the message was successfully handled
        bool handleMessage (Message &in_msg);

        // performs tasks after all messages are handled
        void postHandling ();

	};

} // namespace Vast

#endif
