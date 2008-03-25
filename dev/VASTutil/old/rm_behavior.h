
/*
 * FLoD, a peer-to-peer 3D streaming library
 * Copyright (C) 2006 Wei-Lun Sung    (kergy at acnlab.csie.ncu.edu.tw)
 *                    Shao-Chen Chang (cscxcs at gmail.com)
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
 *  rm_behavior class (Random Move Behavior model class)
 *
 *
 */

#ifndef _FLODSIM_RM_BEHAVIOR_H
#define _FLODSIM_RM_BEHAVIOR_H

#include <string>
#include <map>
#include <vector>
#include "vastutil.h"

class rm_behavior
{
public:
    rm_behavior (SimPara * para, int mode, SectionedFile * record_file);
    ~rm_behavior ();

    Position & get_init_pos (VAST::id_t peer_id);
    Position & get_next_pos (VAST::id_t peer_id);

    void start ()
    {
        if (!m_bStart)
        {
            m_bStart = true;
            m_startstep = _para->current_timestamp;
        }
    }
    /// Management member functions ///

private:
    SimPara * _para;

    map<VAST::id_t, vector<Position> > _peer_pos_list;
    bool m_bStart;
    int  m_startstep;
    //map<id_t, Position> _peer_dest_list;
};

#endif /* _FLODSIM_RM_BEHAVIOR_H */

