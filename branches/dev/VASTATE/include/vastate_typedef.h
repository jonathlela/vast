
/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007 Shun-Yun Hu (syhu@yahoo.com)
 *               2008 Shao-Jhen Chang (cscxcs at gmail.com)
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
 *  vastate_typedef.h -- VASTATE - typedef for VASTATE
 *  Splitted from shared.h
 *
 */

#ifndef _VASTATE_TYPEDEF_H
#define _VASTATE_TYPEDEF_H

namespace VASTATE {
    typedef unsigned long id_t;
    //typedef unsigned long long_id_t;
} /* namespace VASTATE */

namespace VAST {

    // type definations
    typedef unsigned char  uchar_t;

    typedef unsigned char  sindex_t;
    typedef unsigned short  index_t;

    typedef VASTATE::id_t  obj_id_t;
    //typedef id_t          event_id_t;
    typedef unsigned char  obj_type_t;

} /* namespace VAST */

#endif /* _VASTATE_TYPEDEF_H */

