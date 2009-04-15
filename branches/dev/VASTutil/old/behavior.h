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


/*
 *  behavior.h -- particular behavior model	for a simulation
 *
 */

#ifndef VASTSIM_BEHAVIOR_H
#define VASTSIM_BEHAVIOR_H

#include <stdlib.h>         // srand
#include <time.h>           // time
//#include "typedef.h"        // for 'Position'

typedef short int int_t;	// internal data-type used to store the coordinates of a point


typedef struct
{
    int_t x, y;              // current x & y positions
    int_t dest_x, dest_y;    // destination x & y positions    
} node;

class behavior
{
public:
    behavior (int size, long dim_x, long dim_y, int velocity, bool new_steps = false, bool record_steps = true)
        :_size (size), _dim_x (dim_x), _dim_y (dim_y), _velocity (velocity), _record (record_steps)
    {        
        _nodes      = new node[size];                       
        _fp         = NULL;

        srand ((unsigned int)time (NULL));

        // open previous step file if exists
        char filename[80];
        sprintf (filename, "N%04dW%dx%d.pos", _size, (int)_dim_x, (int)_dim_y);
                
        if (new_steps == true || fopen (filename, "rb") == NULL)
        {
            _playback = false;
            if (record_steps == true)
            {
                printf ("creating new file: %s\n", filename);
                _fp = fopen (filename, "wb");                
            }
        }
        else
        {
            printf ("reading from old file: %s\n", filename);
            _fp = fopen (filename, "rb");            
            _playback = true;
        }

        // set up initial positions
        if (_playback == false)
        {
            // create new positions
            for (int i=0; i<size; ++i)
            {
                _nodes[i].dest_x = _nodes[i].x = (int_t)(rand ()%_dim_x);
                _nodes[i].dest_y = _nodes[i].y = (int_t)(rand ()%_dim_y);
                if (_record == true)
                    writepos (_nodes[i]);					
            }
            if (_record == true)
                writeEOL ();
        }
        else
        {
            // load up positions from file
            for (int i=0; i<size; ++i)
				readpos (_nodes[i]);                
        }
        
        _last_seed = rand ();
    }

    virtual ~behavior ()
    {
        delete[] _nodes;
        if (_fp != NULL)
            fclose (_fp);
    }

    node &getpos (int num)
    {
        return _nodes[num];
    }

    // changing to a new position
    virtual void resetpos (int num) = 0;

    virtual bool moveall (void *para = NULL) = 0;
        

protected:

	inline bool readpos (node &n)
	{
		//return (fscanf (_fp, "%d,%d ", &n.x, &n.y) < 0 ? false : true);
		return (fread (&n, sizeof (int_t), 2, _fp) == 2 ? true : false);
	}

	inline void writepos (node &n)
	{
		//fprintf (_fp, "%d,%d ", n.x, n.y);
		fwrite (&n, sizeof (int_t), 2, _fp);
	}

	inline void writeEOL ()
	{
		//fprintf (_fp, "\n");
	}


    int     _size;              // model (node) size
    long    _dim_x, _dim_y;     // dimensions of the model
    long    _velocity;          // velocity of each node
    node    *_nodes;            // pointer to all nodes
    FILE    *_fp;
    bool    _playback;
    bool    _record;
    
    int     _last_seed;
};

#endif // VASTSIM_BEHAVIOR_H

