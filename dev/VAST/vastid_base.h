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
 *  vastid_base.h -- unique ID generator for VAST (default implementation)
 *
 *  history     2007/03/15
 *   
 */


#ifndef VASTID_BASE_H
#define VASTID_BASE_H

#include "vastid.h"

namespace VAST
{
    class vastid_base : public vastid
    {
    public:
        vastid_base (network *net, Addr &gateway, bool is_gateway = false)
            :_gateway (gateway), _request_counter (0)
        {
            this->setnet (net);

            _id = (is_gateway ? NET_ID_GATEWAY : NET_ID_UNASSIGNED);
            _id_count = (is_gateway ? NET_ID_GATEWAY + 1 : 0);
        }

        ~vastid_base ()
        {
        }

        // obtain a unique ID
        id_t getid ();

        //
        // msghandler methods
        //

        // returns whether the message has been handled successfully
        bool handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size);

        // do neighbor discovery check for AOI neighbors
        void post_processmsg ()
        {
        }

    private:

        inline bool is_gateway ()
        {
            return (_id_count != 0);
        }

        id_t    _id;
        Addr    _gateway;
        id_t    _id_count;      // record for current id assignment, used only by gateway
        int     _request_counter;
    };

} // end namespace VAST

#endif // VASTID_BASE_H
