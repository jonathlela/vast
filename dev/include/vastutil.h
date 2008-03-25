/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007 Shao-Chen Chang (cscxcs at gmail.com)
 *                    Shun-Yun Hu     (syhu at yahoo.com)
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
    Utility classes for VAST: 
    
    NOTE: these classes should be self-sufficent, and should not rely on 
          any other VAST-related classes or type definitions except C++ standard includes
*/

#ifndef _VAST_UTIL_H
#define _VAST_UTIL_H

#include "config.h"

#include <string>
#include <map>
#include <vector>
#include <math.h>
//#include <stdio.h>

using namespace std;

// forward declaration

//class EXPORT errout;

// 
// errout: Error output utility
//

class EXPORT errout
{
public:
	errout ();
	virtual ~errout ();
	void setout (errout * out);
	void output (const char * str);
	virtual void outputHappened (const char * str);
private:
	static errout * _actout;
};

//
//  SectionedFile: store/load data onto file in sections
//

enum SFOpenMode
{
    SFMode_NULL,
    SFMode_Read,
    SFMode_Write
};

class EXPORT SectionedFile
{
public:
    SectionedFile ()
    {
    }

    virtual ~SectionedFile ()
    {
    }

    virtual bool open    (const std::string & file_name, SFOpenMode mode) = 0;
    virtual int  read    (unsigned int section_id, void * buffer, int record_size, int record_count) = 0;
    virtual int  write   (unsigned int section_id, void * buffer, int record_size, int record_count) = 0;
    virtual bool close   () = 0;
    virtual bool refresh () = 0;
};

class EXPORT FileClassFactory
{
public:
    FileClassFactory ()
    {
    }

    ~FileClassFactory ()
    {
    }

    SectionedFile * CreateFileClass (int type);
    bool            DestroyFileClass (SectionedFile * filec);
};


//
//  StdIO_SectionedFile: store/load data onto StdIO
//

class StdIO_SectionedFile : public SectionedFile
{
public:
    StdIO_SectionedFile ()
        : _fp (NULL), _mode (SFMode_NULL)
    {
    }

    ~StdIO_SectionedFile ()
    {
        close ();
    }

    bool open  (const string & filename, SFOpenMode mode);
    int  read  (unsigned int section_id, void * buffer, int record_size, int record_count);
    int  write (unsigned int section_id, void * buffer, int record_size, int record_count);
    bool close ();
    bool refresh ();
    void error (const char * msg);

private:
    FILE * _fp;
    map<unsigned int,vector<unsigned char> > section;
    SFOpenMode _mode;
};


//
// MovementModel: abstract class to create various movement patterns
//

#define VAST_MOVEMENT_RANDOM      1
#define VAST_MOVEMENT_CLUSTER     2
#define VAST_MOVEMENT_GROUP       3

typedef double coord_t;
//typedef short coord_t;

class Coord
{
public:
    coord_t x,y;

    Coord () 
        :x (0), y (0)
    {}

    Coord (const Coord &c)
        :x(c.x), y(c.y) {}

    Coord (int x_coord, int y_coord)
        :x((coord_t)x_coord), y((coord_t)y_coord) {}

    Coord (coord_t x_coord, coord_t y_coord)
        :x(x_coord), y(y_coord) {}

    Coord &operator=(const Coord& c)
    {
        x = c.x;
        y = c.y;
        return *this;
    }

    bool operator==(const Coord& c)
    {
        return (this->x == c.x && this->y == c.y);
    }

    Coord &operator+=(const Coord& c)
    {
        x += c.x;
        y += c.y;
        return *this;
    }

    Coord &operator-=(const Coord& c)
    {
        x -= c.x;
        y -= c.y;
        return *this;
    }

    Coord &operator*=(double value)
    {
        x *= value;
        y *= value;
        return *this;
    }

    Coord &operator/=(double c)
    {
        x = (coord_t)((double)x / c);
        y = (coord_t)((double)y / c);
        return *this;
    }

    Coord operator+(const Coord& c)
    {
        return Coord (x + c.x, y + c.y);
    }

    Coord operator-(const Coord& c)
    {
        return Coord (x - c.x, y - c.y);
    }

    double dist (const Coord &c) const
    {        
        double dx = (c.x - x);
        double dy = (c.y - y);
        return sqrt (pow (dx, 2) + pow (dy, 2));
    }

    inline double distsqr (const Coord &c) const
    {
        return (pow (c.x - x, 2) + pow (c.y - y, 2));
    }
};

class MovementModel
{
public:
    MovementModel (Coord &world_p1, Coord &world_p2, int num_nodes, double speed);
    virtual ~MovementModel ();

    virtual bool init () = 0;                                   // initialize positions
    virtual bool move () = 0;                                   // make one move for all nodes

    // get position for a node
    Coord &getpos (int node)
    {
        return _pos[node];
    }

protected:
    // create a random position
    void rand_pos (Coord &pos, Coord &topleft, Coord &bottomright);

    Coord  _topleft, _bottomright;              // boundary for the movement space
    Coord  _dim;                                // dimension of the world
    int    _num_nodes;                          // total number of nodes
    double _speed;                              // speed of node movmenet
    Coord *_pos;                                // coordinates of all nodes
};

//
//  MovementGenerator:  factory class to create actual movements, 
//                      also able to store/retrieve movement records
//                          
class EXPORT MovementGenerator 
{
public:
    MovementGenerator ();

    ~MovementGenerator ();
    
    bool initModel (int model, SectionedFile *record, bool replay, Coord top_left, Coord bottom_right, int num_nodes, int num_steps, double speed);
    Coord *getPos (int node, int step);

private:
    // NOTE: we have to use pointer to avoid warning from VC
    map<int, vector<Coord> > *__pos_list;          // complete positions for all nodes
    int _num_nodes, _num_steps;
};


class EXPORT Compressor
{
public:
    size_t compress (unsigned char *source, unsigned char *dest, size_t size);
};

#endif /* _VAST_UTIL_H */

