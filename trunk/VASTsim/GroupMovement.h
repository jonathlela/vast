/*
    Group Movement Model

    adopted from OverSim sourcecode (http://www.oversim.org/)
        \Applications\SimpleClient\groupRoaming.h    
        \Applications\SimpleClient\groupRoaming.cc

*/



#ifndef _VAST_UTIL_GROUPMOVEMENT_H
#define _VAST_UTIL_GROUPMOVEMENT_H

namespace Vast
{  


//
// available parameters:
//    double _x_min, _x_max, _y_min, _y_max;      // boundary for the movement space
//    int    _num_nodes;                          // total number of nodes
//    double _speed;                              // speed of node movmenet

#define GROUPMOVEMENT_GROUPSIZE (12)
#define GROUPMOVEMENT_FLOCKDIST (40)

class GroupMovement : public MovementModel
{
public:
    GroupMovement (Coord &p1, Coord &p2, int num_nodes, double speed) 
        :MovementModel (p1, p2, num_nodes, speed)
    {
        _num_groups = ((num_nodes - 1) / GROUPMOVEMENT_GROUPSIZE) + 1;
        _target = new Coord[_num_groups];
    }

    ~GroupMovement () 
    {
        delete[] _target;
    }

    bool init ()
    {
        // random group target
        for (int g=0; g<_num_groups; ++g)
        {
            rand_pos (_target[g], _topleft, _bottomright);
        }

        // create new positions & destination waypoint targets
        for (int i=0; i<_num_nodes; ++i)
        {
            Coord tl, br, area(20.0, 20.0);
            tl = br = _target[ (i / GROUPMOVEMENT_GROUPSIZE) ];
            tl -= area;
            br += area;

            rand_pos (_pos[i], tl, br);
        }
        return true;
    }

    bool move ()
    {
        double dist, ratio;
        for (int i=0; i<_num_nodes; ++i)
        {
            Coord dest = _target[ (i / GROUPMOVEMENT_GROUPSIZE) ];
            dist = dest.dist (_pos[i]);

            // move towards the destination one step
            Coord delta = dest - _pos[i];
            
            // adjust deltas for constant velocity            
            if ((ratio = dist / _speed) > 1.0)
                delta /= ratio;

            _pos[i] += delta;

            flock (i);

            bound (_pos[i]);

            // new position needed if destination already reached
            if (_pos[i].distsqr (dest) < 4.0 * _speed * _speed)
                rand_pos (_target[ (i / GROUPMOVEMENT_GROUPSIZE) ], _topleft, _bottomright);
        }
        
        return true;
    }

    void flock (int ind)
    {
        Coord cshift (0, 0);
        for (int i=0; i<_num_nodes; ++i)
        {
            if (i == ind)
                continue;

            if (_pos[i].distsqr (_pos[ind]) < GROUPMOVEMENT_FLOCKDIST * GROUPMOVEMENT_FLOCKDIST) //4.0 * _speed * _speed)
            {
                Coord temp = _pos[i] - _pos[ind];
                coord_normalize (temp);
                cshift += temp;
            }
        }
        coord_normalize (cshift);
        cshift *= (_speed / 2.0);
        _pos[ind] -= cshift;

        bound (_pos[ind]);
    }

    void coord_normalize (Coord& co)
    {
        double temp;
        temp = sqrt((co.x*co.x + co.y*co.y));
        if(temp != 0.0)
        {
            co.x /= temp;
            co.y /= temp;
        }
    }

    void bound (Coord& co)
    {
        // necessary?
        if (co.x < _topleft.x) co.x = _topleft.x;
        if (co.x > _bottomright.x) co.x = _bottomright.x;
        if (co.y < _topleft.y) co.y = _topleft.y;
        if (co.y > _bottomright.y) co.y = _bottomright.y;
    }

private:
    Coord *_target;                        // group target for all groups
    int   _num_groups;                   // group number
};

} // namespace Vast

#endif /* VASTUTIL_RANDOMMOVEMENT */
