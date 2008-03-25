
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
    id_t      peer;
    Position  pos;
    //id_t      owner;
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

    map<id_t, food_reg> _food_image;
    void food_image_clear ();
    void update_food_image (map<id_t, object *> & obj_store);
    
    map<id_t, unsigned int> node_transmitted;

    voronoi *arb_voronoi;
    map<obj_id_t, object_signature> god_store;
    map<id_t, Node> arbitrators;
    void update_arbitrator (const arbitrator_info & the_arb);
    void delete_arbitrator (const arbitrator_info & the_arb);
    void update_object (id_t arbitrator_id, object * the_object);
    void delete_object (id_t arbitrator_id, object * the_object);

private:
    vastverse * _world;
};

#endif /* _VASTATESIM_ARBITRATOR_REGISTER_H */