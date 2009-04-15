
#ifndef _VAST_UTIL_CLUSTERMOVEMENT_H
#define _VAST_UTIL_CLUSTERMOVEMENT_H

namespace Vast
{  


//
//  cluster movement (originally written by Jiun-Shiang Chiou, adopted by Shun-Yun Hu)
//

// % of chance of going to a random attractor
#define PROB_RANDOM_ATTRACTOR       20
#define ATTRACTOR_SPACE_MULTIPLIER  3

class ClusterMovement : public MovementModel
{
public:
    ClusterMovement (Coord &p1, Coord &p2, int num_nodes, double speed) 
        :MovementModel (p1, p2, num_nodes, speed)
    {
        _dest = new Coord[num_nodes];
        _num_attractors = (int)(1.5 * log ((double)num_nodes));
        _attractors = new Coord[_num_attractors];

        // setup attractor ranges
        _range.x = _range.y = sqrt ((_dim.x * _dim.y) / (double)_num_attractors) / ATTRACTOR_SPACE_MULTIPLIER;
    }

    ~ClusterMovement () 
    {
        delete[] _dest;
        delete[] _attractors;
    }

    void reset_attractors ()
    {
        // setup attractor locations
	int i,j;
        for (i = 0; i<_num_attractors; ++i)
        {
            while (1)
            {
                rand_pos (_attractors[i], _topleft, _bottomright);

                // check if the new attractor is far apart enough from existing ones
                for (j=0; j<i; ++j)
                {
                    if (_attractors[i].dist (_attractors[j]) < (_range.x * ATTRACTOR_SPACE_MULTIPLIER))
                        break;
                }
                if (i == j)
                    break;
            }                            
        }
    }

    bool init ()
    {
        // create new positions & destination waypoint targets
        for (int i = 0; i<_num_nodes; ++i)
        {
            rand_pos (_pos[i], _topleft, _bottomright);
            rand_pos (_dest[i], _topleft, _bottomright);
        }

        reset_attractors ();

        return true;
    }

    bool move ()
    {
        // move towards desintations
        double dist, ratio;
        for (int i=0; i<_num_nodes; ++i)
        {            
            dist = _dest[i].dist (_pos[i]);

            // move towards the destination one step
            Coord delta = _dest[i] - _pos[i];
            
            // adjust deltas for constant velocity            
            if ((ratio = dist / _speed) > 1.0)
                delta /= ratio;

            _pos[i] += delta;

            // set new destinations if already reached
            if (dist < 0.1)
            {
                // random waypoint
                // rand_pos (_dest[i]);
                int nearest;

                // 10% chance to go to another attractor
                if (rand () % 100 < PROB_RANDOM_ATTRACTOR)
                    nearest = rand () % _num_attractors;
                else
                    nearest = find_nearest (_pos[i]);

                Coord topleft     = _attractors[nearest] - _range;
		bound (topleft);
                Coord bottomright = _attractors[nearest] + _range;
		bound (bottomright);

                rand_pos (_dest[i], topleft, bottomright);			
            }
        }
        
        return true;
    }

private:
    Coord *_dest;                           // destination coordinates of all nodes
    Coord *_attractors;                     // positions for attractors
    int    _num_attractors;
    Coord  _range;                          // sphere in which the attractor is effective

    // put adjust a coordinate to be within boundary
    Coord &bound (Coord &c)
    {
        if (c.x < _topleft.x)       c.x = _topleft.x;
        if (c.y < _topleft.y)       c.y = _topleft.y;
        if (c.x > _bottomright.x)   c.x = _bottomright.x;
        if (c.y > _bottomright.y)   c.y = _bottomright.y;
        return c;
    }

    // find nearest attractors
    int find_nearest (const Coord &c)
    {
		// set up new destination around the nearest attractors
        double min_dist = _dim.x + _dim.y;
        double curr_dist;
		int index = 0;
		
		// find the nearest attractor
		for (int j=0; j<_num_attractors; j++)
		{
            curr_dist = c.dist (_attractors[j]);
			if (curr_dist < min_dist)
			{
				min_dist = curr_dist;
				index = j;
			}
		}
        return index;
    }
};

} // namespace Vast

#endif /* _VAST_UTIL_CLUSTERMOVEMENT_H */
