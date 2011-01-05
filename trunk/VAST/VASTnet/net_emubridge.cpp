/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2011 Shun-Yun Hu (syhu@ieee.org)
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

#include "net_emubridge.h"
//#include <cmath.h>
#define  SEC_PER_STEP (100)

namespace Vast
{	

	int round (float F)
	{   
		bool minus = (F < 0.0 ? true : false);  
		
        if (minus)   
            F*=-1;  
		int a = (int)F;  
		int b = (int)((F-(float)a)*10);  
		if (b >= 5)   
            ++a;  
		if (minus)   
            a*=-1;  
		return a;  
	}   

    id_t 
    net_emubridge::
    obtain_id (void *pointer)
    {
        //id_t new_id = _id_count--;
        
        id_t new_id = _id_count++;
        _id2ptr[new_id] = pointer;
            
        return new_id;
    }

    
    // replace the temp id with a new id
    void 
    net_emubridge::
    replaceHostID (id_t temp_id, id_t id)
    {
        // print out warning or error for ids already in use?
        if (_id2ptr.find (id) != _id2ptr.end () ||
            _id2ptr.find (temp_id) == _id2ptr.end ())
        {
            printf ("net_emubridge::replaceHostID () warning, tempID not found or ID already in use\n");      
        }

        _id2ptr[id] = _id2ptr[temp_id];
        
        if (id != temp_id)
		{
            _id2ptr.erase (temp_id);
		}
    }
    

    void 
    net_emubridge::
    releaseHostID (id_t id)
    {
        if (_id2ptr.find (id) != _id2ptr.end ())
		{         
			_id2ptr.erase (id);
		}
    }
           
    // obtain the arrived timestamp of a packet, subject to packet loss or latency
    // (-1) indicates a failed send
    timestamp_t 
    net_emubridge::
    getArrivalTime (id_t sender, id_t receiver, size_t length, bool reliable)
    {                   
        sender = 0;
        receiver = 0;
        length = 0;
        reliable = true;

        timestamp_t time;

#ifdef ENABLE_LATENCY
		if (_vivaldi != NULL)
		{//calculate the receiving time
			float latency_time = _vivaldi->get_latency (sender, receiver);			
			//time = getTimestamp () + (timestamp_t)ceil((latency_time/SEC_PER_STEP));
			float d_time = (latency_time/SEC_PER_STEP);
			int r = round (d_time);

			time = getTimestamp() + (timestamp_t)r;
		}
		else
		{
			// delay 1 step by default
			time = getTimestamp () + 1;
		}
#else
		// delay 1 step by default
		time = getTimestamp () + 1;
#endif
		
/*
		
        // TODO: consider fail_rate and block potential sends between sender & receivers
        if (_loss_rate > 0)
        {
            srand (_last_seed);
            _last_seed = rand ();

            int r = rand()%100;                                
            if (r < _loss_rate)
            {
                //printf ("time: %d [%d] -> [%d] type: %d gets dropped at %d:%d\n", _curr_time, (int)from, (int)_id, msgtype, r, _loss_rate);
                if (reliable == false)
                    return (-1);                
                // for reliable delivery, we assume it'll arrive in the next time-stamp
                // TODO: more realistic delay model?
                else
                    time++;                
            }
        }
*/

        return time;
    }

    void *
    net_emubridge::
    getNetworkInterface (id_t target)
    {
        if (_id2ptr.find (target) == _id2ptr.end ())
            return NULL;
        return _id2ptr[target];
    }

	void 
    net_emubridge::
    tick (int tickvalue)
    {
        _time += tickvalue;
    }

} // end namespace Vast

