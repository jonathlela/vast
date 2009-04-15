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
 *  vastbuf.h -- generic buffer class with expandable size
 *
 *  history     2007/03/22
 *   
 */


#ifndef VASTBUF_H
#define VASTBUF_H

#include "config.h"

namespace VAST
{
    class vastbuf
    {
    public:

        vastbuf ()
            :size (0), _bufsize (0)
        {
            data = new char[VAST_BUFSIZ];
        }

        ~vastbuf ()
        {
            delete[] data;
        }

        void expand (size_t len)
        {
            // estimate size for new buffer
            _bufsize = (((size + len) / VAST_BUFSIZ) + 1) * VAST_BUFSIZ;
            char *temp = new char[_bufsize];

            // copy old data to new buffer
            memcpy (temp, data, size);

            // remove old buffer
            delete[] data;
            data = temp;
        }

        void reserve (size_t len)
        {
            size = 0;
            expand (len);
        }

        bool add (void *stuff, size_t len)
        {
            // check if the buffer has enough space to add new stuff
            if (size + len > _bufsize)
                expand (len);
            
            memcpy (data+size, stuff, len);
            size += len;

            return true;
        }

        size_t size;
        char  *data;

    private:
        size_t _bufsize;
    };

} // end namespace VAST

#endif // VASTBUF_H
