/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005 Shun-Yun Hu (syhu@yahoo.com)
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

#include "vor_SF.h"

namespace VAST
{

// insert a new site, the first inserted is myself
void 
vor_SF::
insert (id_t id, Position &pt)
{
	// avoid duplicate insert
	if (get_idx (id) == -1)
	{
		invalidated = true;
		_sites.push_back (site_t (id, pt));
	}
}

// remove a site
void 
vor_SF::
remove (id_t id)
{    
    int idx = get_idx (id);
    if (idx != -1)
    {
        invalidated = true;
        _sites.erase (_sites.begin () + idx);
    }
}

// modify the coordinates of a site
void 
vor_SF::
update (id_t id, Position &pt)
{
    int idx = get_idx (id);
    if (idx != -1)
    {
        invalidated = true;
        _sites[idx].second = pt;
    }
}

// get the point of a site
Position
vor_SF::
get (id_t id)
{
    return _sites[get_idx (id)].second;
}

// check if a point lies inside a particular region
bool 
vor_SF::
contains (id_t id, Position &pt)
{    
    int idx = get_idx (id);
    if (idx == -1) 
        return false;
    
    recompute();
    bool result = _voronoi.isPointInsideSiteRegion (idx, point2d (pt.x, pt.y));
    
    return result;
}

// check if the node is a boundary neighbor
bool 
vor_SF::
is_boundary (id_t id, Position &pt, aoi_t radius)
{   
    int idx = get_idx (id);
    if (idx == -1) 
        return false;
    
    recompute();
    point2d center (pt.x, pt.y); 
    bool result = _voronoi.enclosed (idx, center, (long)radius);

    return !result;
}

// check if the node 'id' is an enclosing neighbor of 'center_node_id'
bool 
vor_SF::
is_enclosing (id_t id, id_t center_node_id)
{   
    recompute();    
    std::set<int> temp;

    int idx = (center_node_id == ((unsigned)-1) ? 0 : get_idx (center_node_id));
    _voronoi.getNeighborSet (idx, temp);
    
    for (set<int>::iterator it = temp.begin(); it != temp.end(); ++it)
    {
        //id_t en_id = _sites[*it].first;
        if (_sites[*it].first == id) 
            return true;
    }

    return false;
}


// get a list of enclosing neighbors
/*
vector<id_t> &
vor_SF::
get_en (id_t id, int level)
{
    recompute();
    int idx = get_idx (id);
    
    std::set<int> idxlist;
    _voronoi.getNeighborSet (idx, idxlist);
    
    _en_list.clear ();
    
    for (set<int>::iterator it = idxlist.begin(); it != idxlist.end(); ++it)
    {
        _en_list.push_back (_sites[(*it)].first);
    }

    //return list.size();
    return _en_list;
}
*/

/*
vector<id_t> &
vor_SF::
get_en (id_t id, int level)
{
    _en_list.clear ();
    
    int idx;
    
    std::set<int> idxlist;
    std::vector<Position> pos_list;
    std::vector<id_t> remove_list;

    int remove_count = 0;
    int j;

    while (true)
    {
        recompute();
        remove_list.clear ();
        idx = get_idx (id);
        _voronoi.getNeighborSet (idx, idxlist);        
        
        for (set<int>::iterator it = idxlist.begin(); it != idxlist.end(); ++it)
        {
            _en_list.push_back (_sites[(*it)].first);
            pos_list.push_back (_sites[(*it)].second);
            remove_list.push_back (_sites[(*it)].first);
        }            
        
        // check to see if we're done
        if (--level <= 0)
            break;

        // remove the enclosing neighbors
        for (j=0; j<(int)remove_list.size (); j++)
        {
            remove (remove_list[j]);
            remove_count++;
        }
    }

    // add back the removed neighbors
    if (remove_count > 0)
    {
        for (j=0; j<(int)_en_list.size (); j++)
            insert (_en_list[j], pos_list[j]);
    }
    
    //return list.size();
    return _en_list;
}
*/

// a more efficient version (avoids voronoi rebuilt unless sites are removed by necessity)
vector<id_t> &
vor_SF::
get_en (id_t id, int level)
{
    _en_list.clear ();
        
    std::set<int> idxlist;
    std::vector<Position> pos_list;

    int remove_count = 0;
    int j;

    while (level > 0)
    {
        // remove enclosing neighbors already recorded, if any
        for (j = remove_count; j<(int)_en_list.size (); j++)
        {
            remove (_en_list[j]);
            remove_count++;
        }

        recompute();

        _voronoi.getNeighborSet (get_idx (id), idxlist);        
        
        // get nearest neighbors given the current center
        for (set<int>::iterator it = idxlist.begin(); it != idxlist.end(); ++it)
        {
            _en_list.push_back (_sites[(*it)].first);
            pos_list.push_back (_sites[(*it)].second);
        }       

        level--;
    }

    // add back the removed neighbors (when query beyond 1st-level neighbors are made)
    for (j=0; j < remove_count; j++)
        insert (_en_list[j], pos_list[j]);
    
    return _en_list;
}


// check if a circle overlaps with a particular region
bool 
vor_SF::
overlaps (id_t id, Position &pt, aoi_t radius, bool accuracy_mode)
{    
    int idx = get_idx (id);
    if (idx == -1) 
        return false;

    if (accuracy_mode)
    {
        // version 1: more & more slow, syhu: accuracy mode ! 
        recompute();
        point2d center (pt.x, pt.y); 
        return _voronoi.collides (idx, center, (int)(radius+5));
    }
    else
    {
        // version 2: simply check if it's within AOI
        Position &pt2 = _sites[idx].second;    
        return (pt2.dist (pt) <= (double)(radius) ? true : false);
    }
}

// returns the closest node to a point
id_t
vor_SF::
closest_to (Position &pt)
{
    id_t closest = _sites[0].first;
    double min = pt.dist (_sites[0].second);
    double d;

    id_t id;
    int n = _sites.size ();
    for (int i = 0; i<n; i++)
    {
        id          = _sites[i].first;
        Position &p = _sites[i].second;

        //ACE_DEBUG ((LM_DEBUG, "(%5t) closest_to() checking idx:%d id:%d\n", i, id));
        if ((d = pt.dist (p)) < min)
        {
            min = d;
            closest = id;
        }
    }
    
    return closest;
}

/*
sfvoronoi &
vor_SF::
get_voronoi ()
{
    recompute ();
    return _voronoi;
}
*/

void 
vor_SF::
recompute()
{
    if (invalidated == false)
        return;

    _voronoi.clearAll();
    int n = _sites.size ();
    for (int i = 0; i < n; i++)
    {
        point2d pt (_sites[i].second.x, _sites[i].second.y);
        _voronoi.mSites.push_back (pt);
    }
    _voronoi.calsfv();
    invalidated = false;
}

int 
vor_SF::
get_idx(id_t h)
{        
    int n = _sites.size ();
    for (int i = 0; i < n; i++)
    {
        if (_sites[i].first == h)
            return i;
    }    
    return -1;
}


vector<line2d> & 
vor_SF::getedges()
{
    recompute ();
    return _voronoi.mLines;
}

// get edges of sites with ID = id
std::set<int> &
vor_SF::get_site_edges (int id)
{
    static std::set<int> empty_set;

    int idx = get_idx (id);
    if (idx == -1) 
        return empty_set;
    
    recompute();
    return _voronoi.mSites[idx].mEdgeIndexSet;
}


} // namespace VAST


