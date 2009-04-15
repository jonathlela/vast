/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007 Shao-Chen Chang   (cscxcs at gmail.com) 
 *               2006 Jiun-Shiang Chiou
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
 *  MovementGenerator class
 *
 */

#include <stdlib.h>
#include <time.h>

#include "VASTUtil.h"
#include "RandomMovement.h"
#include "ClusterMovement.h"
#include "GroupMovement.h"

using namespace std;

namespace Vast
{  

#define MOVEMENT_SECTION 2

MovementModel::MovementModel (Coord &world_p1, Coord &world_p2, int num_nodes, double speed)
    :_topleft (world_p1), _bottomright (world_p2), _num_nodes (num_nodes), _speed (speed)

{
    _dim.x = _bottomright.x - _topleft.x;
    _dim.y = _bottomright.y - _topleft.y;
    _pos = new Coord[num_nodes];
}

MovementModel::~MovementModel () 
{
    delete[] _pos;
}

// create a random position
void MovementModel::rand_pos (Coord &pos, Coord &topleft, Coord &bottomright)
{
    pos.x = topleft.x + rand () % (int)(bottomright.x - topleft.x);
    pos.y = topleft.y + rand () % (int)(bottomright.y - topleft.y);
}

MovementGenerator::MovementGenerator ()  
        :_num_nodes (0), _num_steps (0)
    {
        __pos_list = new map<int, vector<Coord> >;
    }

    MovementGenerator::~MovementGenerator () 
    {
        delete __pos_list;
    }


bool
MovementGenerator::initModel (int model, SectionedFile *record, bool replay,
                              Coord top_left, Coord bottom_right,
                              int num_nodes, int num_steps, double speed)
{
    errout eo;
    srand ((unsigned int) time (NULL));
    
    map<int, vector<Coord> > &_pos_list = *__pos_list;

    Coord pos;
    if (replay == true)
    {
        for (int p = 0; p < num_nodes; p++)
        {
            // fill in placeholder
            _pos_list[p].insert (_pos_list[p].end (), num_steps + 1, pos);

            // then read all positions from file
            if (record->read (MOVEMENT_SECTION, &_pos_list[p][0], sizeof (Coord) * (num_steps + 1), 1) != 1)
            {
                eo.output ("MovementGenerator: reading position list failed.\n");
            }
        }
    }
    else
    {
        // create the proper movement model
        MovementModel *move_model;
        switch (model)
        {
        case VAST_MOVEMENT_RANDOM:
            move_model = new RandomMovement (top_left, bottom_right, num_nodes, speed);
            break;

        case VAST_MOVEMENT_CLUSTER:
            move_model = new ClusterMovement (top_left, bottom_right, num_nodes, speed);
            break;

        case VAST_MOVEMENT_GROUP:
            move_model = new GroupMovement (top_left, bottom_right, num_nodes, speed);
            break;

        default:
            return false;
        }

        move_model->init ();

        // create all positions at once
        int s,p;
        for (s = 0; s <= num_steps; s++)
        {
            for (p = 0; p < num_nodes; p++)
            {
                _pos_list[p].push_back (move_model->getpos (p));
            }
            move_model->move ();
        }

        delete move_model; 

        // record the positions to file
        if (record != NULL)
        {
            for (int p = 0; p < num_nodes; p++)
            {
                if (record->write (MOVEMENT_SECTION, &_pos_list[p][0], sizeof (Coord) * (num_steps + 1), 1) != 1)
                {
                    eo.output ("MovementGenerator: writing position list to file failed.\n");
                }
            }
        }
    }

    // successful initialization
    _num_nodes = num_nodes;
    _num_steps = num_steps;

    return true;
}

Coord *MovementGenerator::getPos (int node, int step)
{
    int i = 0;
    if (step > 0 && step % 100 == 0)
        i = 100;
    // return failure if the model wasn't initialized
    if (_num_nodes == 0 || node >= _num_nodes || step > _num_steps)
        return NULL;
    else        
        return &((*__pos_list)[node][step]);
}

} // namespace Vast