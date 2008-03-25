/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007 Shun-Yun Hu (syhu@yahoo.com)
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
 *  vastid.h -- unique ID generator for VAST
 *
 *  history     2007/01/13
 *   
 */


#ifndef VASTID_H
#define VASTID_H

#include "typedef.h"
#include "msghandler.h"

namespace VAST
{
    class vastid : public msghandler
    {
    public:

        vastid ()
        {
        }

        // allow more appropriate destrcutor to be called
        virtual ~vastid ()
        {
        }

        // obtain a unique ID, if not yet obtained then returns NET_ID_UNASSIGNED
        virtual id_t getid () = 0;

        //
        // msghandler methods
        //

        // returns whether the message has been handled successfully
        //virtual bool handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size) = 0;

        // do neighbor discovery check for AOI neighbors
        //virtual void post_processmsg () = 0;
    };

} // end namespace VAST

#endif // VASTID_H
