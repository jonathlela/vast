

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

void arbitrator_reg::update_food_image (map<id_t, object *> & obj_store)
{
    AttributeBuilder a;
    map<id_t, object *>::iterator it = obj_store.begin ();
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

void arbitrator_reg::update_object (id_t arbitrator_id, object * the_object)
{
    Position pos = the_object->get_pos ();
    if (the_object != NULL && arb_voronoi->contains (arbitrator_id, pos))
    {
        god_store[the_object->get_id ()].id          = the_object->get_id ();
        god_store[the_object->get_id ()].peer        = the_object->peer;
        god_store[the_object->get_id ()].pos         = pos;
        god_store[the_object->get_id ()].pos_version = the_object->pos_version;
        god_store[the_object->get_id ()].version     = the_object->version;
    }
}

void arbitrator_reg::delete_object (id_t arbitrator_id, object * the_object)
{
    if (the_object != NULL && arb_voronoi->contains (arbitrator_id, the_object->get_pos ()))
    {
        if (god_store.find (the_object->get_id ()) != god_store.end ())
            god_store.erase (the_object->get_id ());
    }
}

