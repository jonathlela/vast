
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

#include "precompile.h"
#include "rawdata.h"

namespace VAST {
    bool RawData::push_array (const uchar_t * data, size_t dsize)
    {
        this->insert (end (), data, data + dsize);
        return true;
    }

    bool RawData::push_sized_array (const uchar_t * data, size_t dsize)
    {
        push_array ((uchar_t *) &dsize, sizeof(size_t));
        push_array (data, dsize);
        return true;
    }

    bool RawData::pop_array  (uchar_t * data_b, size_t dsize)
    {
        if (size () < dsize)
            return false;

        //memcpy (data_b, &(this->operator[](0)), dsize);
        memcpy (data_b, &((*this)[0]), dsize);

        this->erase (begin (), begin () + dsize);

        return true;
    }

    bool RawData::pop_sized_array  (uchar_t * data_b, size_t& dsize)
    {
        size_t s;
        if (!pop_array ((uchar_t *) &s, sizeof(size_t)) || s > dsize)
            return false;

        return pop_array (data_b, (dsize = s));
    }

    void RawData::pop_front (size_t size)
    {
        this->erase (begin (), begin() + size);
    }
} /* namespace VAST */

