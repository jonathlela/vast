
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

/*
 *  arbitrator_reg.cpp (VASTATE simulation arbitrator register class header)
 *
 *
 */

#ifndef _VASTATESIM_ARBITRATOR_REGISTER_H
#define _VASTATESIM_ARBITRATOR_REGISTER_H

#include "shared.h"
#include <map>

using namespace std;
using namespace VAST;

struct object_signature
{
    obj_id_t  id;
    VAST::id_t      peer;
    Position  pos;
    //VAST::id_t      owner;
    version_t pos_version;
    version_t version;
};

class arbitrator_reg
{
public:
    arbitrator_reg (vastverse * world);
	~arbitrator_reg ();

    int food_count;
    inline void inc_food () { food_count ++; };
    inline void dec_food () { food_count --; };
    inline int getFoodCount () { return food_count; }

    map<VAST::id_t, food_reg> _food_image;
    void food_image_clear ();
    void update_food_image (map<VAST::id_t, object *> & obj_store);
    
    map<VAST::id_t, unsigned int> node_transmitted;

    voronoi *arb_voronoi;
    map<obj_id_t, object_signature> god_store;
    map<VAST::id_t, Node> arbitrators;
    void update_arbitrator (const arbitrator_info & the_arb);
    void delete_arbitrator (const arbitrator_info & the_arb);
    void update_object (VAST::id_t arbitrator_id, object * the_object);
    void delete_object (VAST::id_t arbitrator_id, object * the_object);

private:
    vastverse * _world;
};

#endif /* _VASTATESIM_ARBITRATOR_REGISTER_H */

