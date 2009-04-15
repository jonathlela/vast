/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2006 Jiun-Shiang Chiou (junchiou@acnlab.csie.ncu.edu.tw)
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

#include "behavior.h"

#define DEFAULT_ATTRACTOR_SIZE      3


class beh_clustered : public behavior
{
public:
    beh_clustered (int size, long dim_x, long dim_y, int velocity, bool new_steps = false, bool record_steps = true, int num_attractors = DEFAULT_ATTRACTOR_SIZE)
        :behavior (size, dim_x, dim_y, velocity, new_steps, record_steps)
    {        
        _num_attractors = num_attractors;
        _att = new Position[_num_attractors];

        // create the positions of attractors
        // set up the coordinate of first attractor
        _att[0].x = rand () % _dim_x;
        _att[0].y = rand () % _dim_y;

        // the minimum distance of any pair attracters
        // the distance of any pair of attractors must be bigger than or equal to it
        //double min_dist = sqrt ((double)(_dim_x * _dim_y) / _num_attractors);
        double min_dist = _dim_x / 2;

        // determine a zone where the attractor is its center
        // when nodes move to the neareast attractor, they will move to the zone
        _range_x = 200;
        _range_y = 200;
        //_range_x = (int_t)sqrt ((double)(_dim_x * _dim_x) / _num_attractors);
        //_range_y = (int_t)sqrt ((double)(_dim_y * _dim_y) / _num_attractors);

        
        // set up coordinates of remainder attractors
        for (int i=1; i<_num_attractors; i++)
        {
            _att[i].x   = rand ()%_dim_x;
            _att[i].y   = rand ()%_dim_y;
            bool correct = false;
            
            // make sure that the distance between any pairs of attractors > min_dist
            while (!correct)
            {
                int check = 0;
                for (int j=0; j<i; j++)
                {
                    if (_att[i].dist (_att[j]) >= min_dist)
                        check++;
                }
                if (check == i)
                    correct = true;
                else
                {
                    // reassign attractor position
                    _att[i].x   = rand ()%_dim_x;
                    _att[i].y   = rand ()%_dim_y;
                }
            }
        }
    }

    ~beh_clustered ()
    {
        delete[] _att;
    }

    // changing to a new position
    void resetpos (int num)
    {
        srand (_last_seed);
        
        _nodes[num].x = (int_t)(rand ()%_dim_x);
        _nodes[num].y = (int_t)(rand ()%_dim_y);
        
        _last_seed = rand ();
    }
    
    // can move cluster or random
    bool moveall (void *para)
    {        
        bool cluster = (para == NULL ? true : *(bool *)para);

        srand (_last_seed);
        
        for (int i=0; i<_size; ++i)
        {
            // in playback mode, no need to worry creating new positions
            if (_playback == true)
            {
                if (readpos (_nodes[i]) == false)
                    return false;
                continue;
            }

            // new position needed if destination already reached
            if (_nodes[i].dest_x == _nodes[i].x && _nodes[i].dest_y == _nodes[i].y)
            {
                // Jun: nodes setup the destination around the neareast attractors
                if (cluster == true)
                {
                    double distance_2 = sqrt (pow ((double)_dim_x, 2) + pow ((double)_dim_y, 2));
                    int p_att = 0;
                    
                    // Jun: to find which attractor is the neareast
                    for (int j=0;j<_num_attractors;j++)
                    {
                        Position pos (_nodes[i].x, _nodes[i].y);
                        double distance = pos.dist (_att[j]);
                        if (distance < distance_2)
                        {
                            distance_2 = distance;
                            p_att = j;
                        }
                    }
                    
                    // Jun: check if the zone of attractor is under the boundary of the world
                    if (_att[p_att].x - _range_x < 0)
                        _nodes[i].dest_x = (int_t)(rand ()% (int)(_att[p_att].x + _range_x));
                    else if (_att[p_att].x + _range_x > _dim_x)
                        _nodes[i].dest_x = (int_t)(rand ()% (int)(_dim_x - (_att[p_att].x - _range_x) ) + (_att[p_att].x - _range_x));
                    else
                        _nodes[i].dest_x = (int_t)(rand ()% (int)(2*_range_x) + (_att[p_att].x - _range_x));

                    if (_att[p_att].y - _range_y < 0)
                        _nodes[i].dest_y = rand ()% (int)(_att[p_att].y + _range_y);
                    else if (_att[p_att].y + _range_y > _dim_y)
                        _nodes[i].dest_y = (int_t)(rand ()% (int)( _dim_y - (_att[p_att].y - _range_y) ) + (_att[p_att].y - _range_y));
                    else
                        _nodes[i].dest_y = (int_t)(rand ()% (int)(2*_range_y) + (_att[p_att].y - _range_y));
                }
                else
                {
                    _nodes[i].dest_x = (int_t)(rand ()%_dim_x);
                    _nodes[i].dest_y = (int_t)(rand ()%_dim_y);
                }
                //printf ("[%d] oldpos: (%d, %d) newpos: (%d, %d)\n", i+1, _nodes[i].x, _nodes[i].x, _nodes[i].dest_x, _nodes[i].dest_y);
            }                
            
            // move towards the destination one step
            long dx = _nodes[i].dest_x - _nodes[i].x;
            long dy = _nodes[i].dest_y - _nodes[i].y;
            //printf ("[%d] distance left (%d, %d) ", i+1, (int)dx, (int)dy); 

            // adjust deltas for constant velocity
            double ratio = sqrt((double)((dx*dx) + (dy*dy))) / (double)_velocity;

            if (ratio > 1)
            {
                // note that this may cause actual velocity to be less than desired
                // as any decimal places are dropped
                dx = (long)((double)dx / ratio);
                dy = (long)((double)dy / ratio);
            }
            
            //printf (" actual movements: dx=%d dy=%d\n", (int)dx, (int)dy);

            _nodes[i].x += (int_t)dx;
            _nodes[i].y += (int_t)dy;

            // record positions
            if (_record)
                writepos (_nodes[i]);
        }

        // note we won't write if it's playback mode
        if (_playback == false && _record == true)
            writeEOL ();
            
        _last_seed = rand ();        
        
        return true;        
    }

private:
    Position    *_att; // Jun: use to record the coordinates of ATTRACTORS

    // determine a zone where the attractor is its center
    // when nodes move to the neareast attractor, they will move to the zone
    int_t       _range_x;
    int_t       _range_y;

    // the number of attracters
    int         _num_attractors;
};


