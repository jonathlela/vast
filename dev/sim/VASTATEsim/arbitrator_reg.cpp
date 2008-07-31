
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
 *  arbitrator_reg.cpp (VASTATE simulation arbitrator register class)
 *
 *
 */

#include "vastverse.h"
#include "shared.h"
using namespace VAST;
#include "vastatesim.h"
#include "attributebuilder.h"
#include "arbitrator_reg.h"

///////////////////////////////////////////////////////////////////////////////
//
//		arbitrator_reg
//
///////////////////////////////////////////////////////////////////////////////
arbitrator_reg::arbitrator_reg (vastverse * world)
: food_count (0), arb_voronoi (NULL), _world (world)
{
    arb_voronoi = _world->create_voronoi ();
}

arbitrator_reg::~arbitrator_reg ()
{
    if (arb_voronoi != NULL)
    {
        _world->destroy_voronoi (arb_voronoi);
    }
}

void arbitrator_reg::food_image_clear ()
{
    _food_image.clear ();
}

void arbitrator_reg::update_food_image (map<VAST::id_t, object *> & obj_store)
{
    AttributeBuilder a;
    map<VAST::id_t, object *>::iterator it = obj_store.begin ();
    for (; it != obj_store.end (); it ++)
    {
        if (a.checkType (*(it->second), SimGame::OT_FOOD) == true)
        {
            object * tobj = it->second;
            if (_food_image.find (tobj->get_id ()) == _food_image.end ())
            {
                _food_image [tobj->get_id ()].id = tobj->get_id ();
                _food_image [tobj->get_id ()].pos = tobj->get_pos ();
                _food_image [tobj->get_id ()].count = a.getFoodCount (*tobj);
                _food_image [tobj->get_id ()].version = tobj->version;
            }
            else if (tobj->version > _food_image[tobj->get_id()].version)
            {
                _food_image [tobj->get_id ()].pos = tobj->get_pos ();
                _food_image [tobj->get_id ()].count = a.getFoodCount (*tobj);
                _food_image [tobj->get_id ()].version = tobj->version;
            }
        }
    }
}

void arbitrator_reg::update_arbitrator (const arbitrator_info & the_arb)
{
    arbitrator_info the_arb_c = the_arb;
    if (arbitrators.find (the_arb.id) == arbitrators.end ())
    {
        arbitrators[the_arb.id] = Node (the_arb.id, 0, the_arb_c.pos);
        arb_voronoi->insert (the_arb_c.id, the_arb_c.pos);
    }
    else
    {
        arbitrators[the_arb.id].pos = the_arb.pos;
        arb_voronoi->update (the_arb_c.id, the_arb_c.pos);
    }
}

void arbitrator_reg::delete_arbitrator (const arbitrator_info & the_arb)
{
    if (arbitrators.find (the_arb.id) != arbitrators.end ())
    {
        arbitrators.erase (the_arb.id);
        arb_voronoi->remove (the_arb.id);
    }
}

void arbitrator_reg::update_object (VAST::id_t arbitrator_id, object * the_object)
{
    const obj_id_t & objid = the_object->get_id ();
    Position pos = the_object->get_pos ();
//    if (the_object != NULL && arb_voronoi->contains (arbitrator_id, pos))
    if (the_object != NULL)
    {
        god_store[objid].id          = the_object->get_id ();
        god_store[objid].peer        = the_object->peer;
        god_store[objid].pos         = pos;
        god_store[objid].pos_version = the_object->pos_version;
        god_store[objid].version     = the_object->version;
    }
}

void arbitrator_reg::delete_object (VAST::id_t arbitrator_id, object * the_object)
{
    //if (the_object != NULL && arb_voronoi->contains (arbitrator_id, the_object->get_pos ()))
    if (the_object != NULL)
    {
        if (god_store.find (the_object->get_id ()) != god_store.end ())
            god_store.erase (the_object->get_id ());
    }
}

