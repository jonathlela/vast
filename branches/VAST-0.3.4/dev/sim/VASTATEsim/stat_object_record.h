
/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007-2008 Shao-Chen Chang (cscxcs at gmail.com)
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

#ifndef _VASTATESIM_STAT_OBJECT_RECORD_H
#define _VASTATESIM_STAT_OBJECT_RECORD_H


class stat_object_record
{
public:
    stat_object_record ()
        : pos_version(0), version (0), pos(Position(0,0)), player_aoi(0), seem (0)
    {
    }

    ~stat_object_record ()
    {
    }

public:
    timestamp_t pos_version;
	timestamp_t version;

	Position pos;
	int player_aoi;

    vector<VAST::id_t> k_peers;
	int seem;
};


#endif /* _VASTATESIM_STAT_OBJECT_RECORD_H */

