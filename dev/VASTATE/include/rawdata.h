
/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2008 Shao-Jhen Chang (cscxcs at gmail.com)
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

#ifndef _RAWDATABUFFER_H
#define _RAWDATABUFFER_H

//#include "precompile.h"
#include <vector>
#include "vastate_typedef.h"

namespace VAST
{

#define rawdata_p(x) ((const uchar_t *)&(x))

    class RawData : public std::vector<uchar_t>
    {
    public:
        RawData ()
        {
        }

        ~RawData ()
        {
        }

        // push an array to back of buffer
        bool push_array       (const uchar_t * data, size_t dsize);

        // push size of array and the array to back of buffer
        bool push_sized_array (const uchar_t * data, size_t dsize);

        // concat two RawData
        inline 
        void push_raw         (const RawData & rw)
        {
            if (rw.size () > 0)
                this->insert (end (), rw.begin (), rw.end ());
        }

        // fetch a data from front of buffer
        bool pop_array        (uchar_t * data_b, size_t dsize);

        // fetch a size-marking data
        bool pop_sized_array  (uchar_t * data_b, size_t& dsize);

        // pop size bytes out from front of buffer
        void pop_front (size_t size = sizeof(uchar_t));
    };

} /* namespace VAST */
#endif /* _RAWDATABUFFER_H */

