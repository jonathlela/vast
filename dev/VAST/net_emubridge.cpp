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

#include "net_emubridge.h"

namespace VAST
{
    id_t 
    net_emubridge::
    obtain_id (void *pointer)
    {
        id_t new_id = _id_count--;
        _id2ptr[new_id] = pointer;
            
        return new_id;
    }

    // replace the temp id with a new id
    void 
    net_emubridge::
    register_id (id_t temp_id, id_t id)
    {
        // print out warning or error for ids already in use?
        if (_id2ptr.find (id) != _id2ptr.end () ||
            _id2ptr.find (temp_id) != _id2ptr.end ())
        {
            // printf
        }
        _id2ptr[id] = _id2ptr[temp_id];
        if (id != temp_id)
            _id2ptr.erase (temp_id);
    }

    void 
    net_emubridge::
    release_id (id_t id)
    {
        if (_id2ptr.find (id) != _id2ptr.end ())
            _id2ptr.erase (id);
    }
           
    timestamp_t 
    net_emubridge::
    get_timestamp (id_t sender, id_t receiver, size_t length, bool reliable)
    {
        // delay 1 step by default
        timestamp_t time = get_curr_timestamp ();

        /*
        // TODO: not a clean way to do stat collection
        // csc 20080316: network size statistics are all collected by network
        if (_send_size.find (sender) == _send_size.end ())
            _send_size[sender] = 0;

        //_send_size[sender] += length;
        _send_size[sender] += (length + sizeof (msgtype_t) + sizeof (timestamp_t));
        */

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

        /*
        // TODO: not a clean way to do stat collection
        // csc 20080316: network size statistics are all collected by network
        if (_recv_size.find (receiver) == _recv_size.end ())
            _recv_size[receiver] = 0;

        //_recv_size[receiver] += length;
        _recv_size[receiver] += (length + sizeof (msgtype_t) + sizeof (timestamp_t));
        */

        return time;
    }

    void *
    net_emubridge::
    get_netptr (id_t target)
    {
        if (_id2ptr.find (target) == _id2ptr.end ())
            return NULL;
        return _id2ptr[target];
    }

    // csc 20080316: network size statistics are all collected by network
    /*
    size_t 
    net_emubridge::
    sendsize (id_t node)
    {
        if (_send_size.find (node) == _send_size.end ())
            return 0;
        else
            return _send_size[node];
    }

    size_t 
    net_emubridge::
    recvsize (id_t node)
    {
        if (_recv_size.find (node) == _recv_size.end ())
            return 0;
        else
            return _recv_size[node];
    }
    */

	// added by yuli ==================================================================================================
    // csc 20080316: network size statistics are all collected by network
    /*
	void net_emubridge::pass_def_data (id_t sender, id_t receiver, size_t length)
	{
		if (_send_def_size.find (sender) == _send_def_size.end ())
			_send_def_size[sender] = length;
		else
			_send_def_size[sender] = _send_def_size[sender] + length;

		if (_recv_def_size.find (receiver) == _recv_def_size.end ())
			_recv_def_size[receiver] = length;
		else
			_recv_def_size[receiver] = _recv_def_size[receiver] + length;
	}

	size_t net_emubridge::send_def_size (id_t node)
	{
		if (_send_def_size.find (node) == _send_def_size.end ())
			return 0;
		else
			return _send_def_size[node];
	}

	size_t net_emubridge::recv_def_size (id_t node)
	{
		if (_recv_def_size.find (node) == _recv_def_size.end ())
			return 0;
		else
			return _recv_def_size[node];
	}
    */
	//=================================================================================================================

	void 
    net_emubridge::
    tick (int tickvalue)
    {
        _time += tickvalue;
    }

} // end namespace VAST

