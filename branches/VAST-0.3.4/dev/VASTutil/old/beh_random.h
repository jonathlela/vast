/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2006 Shun-Yun Hu (syhu@yahoo.com)
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

class beh_random : public behavior
{
public:
    beh_random (int size, long dim_x, long dim_y, int velocity, bool new_steps = false, bool record_steps = true)
        :behavior (size, dim_x, dim_y, velocity, new_steps, record_steps)
    {        
    }

    // changing to a new position
    void resetpos (int num)
    {
        srand (_last_seed);
        
        _nodes[num].x = (int_t)(rand ()%_dim_x);
        _nodes[num].y = (int_t)(rand ()%_dim_y);
        
        _last_seed = rand ();
    }
    
    bool moveall (void *para)
    {
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
                _nodes[i].dest_x = (int_t)(rand ()%_dim_x);
                _nodes[i].dest_y = (int_t)(rand ()%_dim_y);
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
    
};


