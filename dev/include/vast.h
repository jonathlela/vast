/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2007 Shun-Yun Hu (syhu@yahoo.com)
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
 *  vast.h -- main VAST header, used by application     
 *
 *  history 2005/04/11  ver 0.1
 *          2007/01/11  ver 0.2     simplify interface   
 */

#ifndef VAST_H
#define VAST_H

#include "config.h"
#include "msghandler.h"
#include "voronoi.h"
#include <vector>

using namespace std;

namespace VAST 
{
    // msghandler is inherented to provide support for chained message handling,
    // as a result, any subclass of vast must implement handlemsg ()
    class vast : public msghandler
    {
    public:

		//
		// main VAST methods
		//

        vast () 
            :_joined (false)//, _time(0)
        {
        }

        // set destructor as 'virtual' to allow more approprite destructor to be called
        virtual ~vast ()
        {
        }

        // join VON to obtain an initial set of AOI neighbors
        virtual bool        join (id_t id, aoi_t AOI, Position &pos, Addr &gateway) = 0;

        // quit VON
        virtual void        leave () = 0;
        
        // AOI related functions
        virtual void        setAOI (aoi_t radius) = 0;
        virtual aoi_t       getAOI () = 0;
        
        // move to a new position, returns actual position
        virtual Position &  setpos (Position &pos) = 0;

        // get current statistics about this node (a NULL-terminated string)
        virtual char *      getstat (bool clear = false) = 0;    

        // get a list of currently known AOI neighbors
        vector<Node *>& getnodes ()
        {
            return _neighbors;
        }

        // get the current node's information
        Node * getself ()
        {
            return &_self;
        }
	
		// ==================================================================

		// let logical time progress & process all currently received messages
        // necessary to call once every application loop
        void tick (bool advance_time = true)
        {
            //if (advance_time)
            //    _time++;

            // NOTE: processmsg () is a method of msghandler
            this->processmsg (/*_time*/);
        }

		//
		//	service methods
		//

        bool is_joined ()
        {
            return _joined;
        }

        voronoi *getvoronoi ()
        {
            return _voronoi;
        }

    protected:

        Node                _self;   
        vector<Node *>      _neighbors;
        voronoi             *_voronoi;
        bool                _joined;
        //timestamp_t         _time;          // current logical time        
	
    };

} // end namespace VAST

#endif // VAST_H
