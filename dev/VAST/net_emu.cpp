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

#include "net_emu.h"

namespace VAST
{   
    // static member variables
    Compressor net_emu::_compressor;

    void 
    net_emu::
    register_id (id_t my_id)
    {
        id_t temp_id = _id;

        _bridge.register_id (_id, my_id);
        _id = my_id;

        // register new id in _id2addr structure
        _id2addr[_id] = _id2addr[temp_id];
        _id2addr[_id].id = _id;
        _id2addr.erase (temp_id);
    }

    int 
    net_emu::
    _send_ipquery (id_t target)
    {
        timestamp_t ct = get_curr_timestamp ();

        // check if the record is already known
        if (_id2queryip.find (target) == _id2queryip.end ()
            || _id2queryip[target].size () == 0)
        {
            printf ("[%3d] net_emu: _send_idquery: Found no IP info. Node id: %d  Nei info count: %d\n", _id, target, _id2queryip[target].size ());
            return -1;
        }

        // for all nodes in knowledge of quering target
        vector<pair<id_t,timestamp_t> > & q_list = _id2queryip[target];
        vector<pair<id_t,timestamp_t> >::iterator it;
        for (it = q_list.end (); it != q_list.begin (); it --)
        {
            id_t& responder = (it - 1)->first;

            // if has no connection to inquirer, skip it
            if (!is_connected (responder))
            {
                printf ("[%lu] net_emu: _send_idquery: can't fetch info from disconnected node %d\n", _id, responder);
                continue;
            }

            // send ip query
            net_emu *responder_ptr = (net_emu *) _bridge.get_netptr (responder);
            if (responder_ptr == NULL)
            {
                printf ("net_emu: _send_ipquery (): found NULL net pointer from bridge of id %d\n", responder);
                continue;
            }

            // 2 trip (round trip) delay will be caused by each query
            ct += 2;

            // IPQUERY msg = timestamp_t + msgtype_t (IPQUERY) + id_t (ID)
            count_message (responder_ptr, IPQUERY, sizeof_packetize (sizeof(id_t)));

            Addr answer;
            if (responder_ptr->_query_ip (target, answer) != 0)
            {
                // IPQUERY_FAIL msg = timestamp_t + msgtype_t (IPQUERY_FAIL) + id_t (ID)
                responder_ptr->count_message (this, IPQUERY, sizeof_packetize (sizeof (id_t)));

                continue;
            }

            // IPQUERY_ACK msg = timestamp_t + msgtype_t (QUERY_ACK) + Addr (target's address)
            responder_ptr->count_message (this, IPQUERY, sizeof_packetize (sizeof (Addr)));

            _id2addr[target] = answer;
            break;
        }

        // remove all knowledge record if found no answer
        if (it == q_list.begin ())
        {
            printf ("[%lu] net_emu: _send_idquery: Found no IP info. Node id: %d  Nei info count: %d\n", _id, target, _id2queryip[target].size ());
            _id2queryip.erase (target);
            return -1;
        }

        // remove queried but no connection or response nodes
        if (it != q_list.end ())
            q_list.erase (it, q_list.end ());

        // assign connection delay to target node
        if (ct >= get_curr_timestamp ())
            _id2conndelay [target] = ct;

        return 0;
    }

    int 
    net_emu::
    _query_ip (id_t target, Addr & ret)
    {
        if (_id2addr.find (target) == _id2addr.end ())
            return -1;

        ret = _id2addr[target];
        return 0;
    }

    int 
    net_emu::
    connect (id_t target)
    {        
        // avoid self-connection
        if (target == _id)
            return 0;
        
        // check if 
        //   network is actived
        //   connection is already established
        if (_active == false ||
            _id2conn.find (target) != _id2conn.end ())
            return -1;

        // check if waiting for quering address
        if (_id2conndelay.find (target) != _id2conndelay.end ())
            return 0;

        // check if the nodes address I already know
        if (_id2addr.find (target) == _id2addr.end ())
        {
            int ret = _send_ipquery (target);
            if (ret != 0)
                return ret;
        }

        // create connection record
        _id2conn[target] = 0;

        // notify remote node of the connection
        net_emu *receiver = (net_emu *)_bridge.get_netptr (target);

        if (receiver == NULL)
            return (-1);

        receiver->remote_connect (_id, _id2addr[_id]);
        if (_id2conndelay.find (target) != _id2conndelay.end ())
        {
            timestamp_t ts = _id2conndelay[target];
            if (receiver->_id2conndelay.find (_id) != receiver->_id2conndelay.end () &&
                ts > receiver->_id2conndelay [_id])
                ts = receiver->_id2conndelay [_id];
            receiver->_id2conndelay [_id] = ts;
            _id2conndelay[target] = ts;
        }

        return 0;
    }

    // Notice::
    //    while connecting to an address by connect(Addr& addr) calling, if addr.id is
    //        NET_ID_UNASSIGNED: means connects to a outside node, network layer should allocating a
    //                           temp&private id and return by addr structure
    //        any non-private id: connects to the node, and register addr.id to the address
    int 
    net_emu::
    connect (Addr & addr)
    {
        if (_active == false)
            return (-1);

        if (addr.id == NET_ID_UNASSIGNED)
        {
            /*
            // check for ID-less connections (should not supported by current emulation network)
            id_t pri_id;
            for (pri_id = NET_ID_PRIVATEBEGIN; NET_ID_ISPRIVATE(pri_id); pri_id=NET_ID_PRIVATENEXT(pri_id))
                if (_id2addr.find (pri_id) == _id2addr.end ())
                    break;
            if (NET_ID_ISPRIVATE(pri_id))
            {
                addr.id = pri_id;
                _id2addr[pri_id] = addr;
                return 0;
            }

            printf ("net_emu: connect(): Run out of private ids.\n");
            return -1;
            */
            printf ("net_emu: connect(): ID-less connection are not supported.\n");
            return -1;
        }

        // store the address if unknown
        if (_id2addr.find (addr.id) == _id2addr.end ())
            _id2addr[addr.id] = addr;

        return connect (addr.id);
    }

    int 
    net_emu::
    disconnect (id_t target)
    {
        // check if connection exists
        if (_id2conn.find (target) == _id2conn.end ())
            return -1;

#ifdef DEBUG_DETAIL
        printf ("[%d] disconnection success\n", (int)target);
#endif
        
        // update the connection relationship
        _id2conn.erase (target);
        _id2conndelay.erase (target);

        // do a remote disconnect
        net_emu *receiver = (net_emu *)_bridge.get_netptr (target);
        
        if (receiver == NULL)
            return -1;

        receiver->remote_disconnect (_id);   
        return 0;
    }

    int
    net_emu::
    notify_id_mapper (id_t src_id, const std::vector<id_t> & map_to)
    {
        vector<id_t>::const_iterator it = map_to.begin ();
        for (; it != map_to.end (); it ++)
        {
            vector<pair<id_t, timestamp_t> > & this_list = _id2queryip[*it];

            // remove duplicate record
            vector<pair<id_t, timestamp_t> >::iterator it = this_list.begin ();
            for (; it != this_list.end (); it ++)
                if (it->first == src_id)
                {
                    this_list.erase (it);
                    break;
                }

            this_list.push_back (pair<id_t,timestamp_t> (src_id, get_curr_timestamp ()));
        }

        return 0;
    }

    // send message to a node
    int 
    net_emu::
    sendmsg (id_t target, msgtype_t msgtype, char const *msg, size_t len, bool reliable, bool buffered)
    {
#ifdef DEBUG_DETAIL
        //printf ("%4d [%3d]    sendmsg   to: %3d type: %-8s size:%3d\n", time, (int)_id, target, VAST_MESSAGE[msgtype], len);
#endif
        if (_active == false)
            return 0;
        
        // check for connection
        if (_id2conn.find (target) == _id2conn.end ())
            return (-1);

        // find the receiver network record
        net_emu *receiver = (net_emu *)_bridge.get_netptr (target);

        if (receiver == NULL)
            return (-1);

        // make a time correction, or do not send at all
        timestamp_t time;
        if (target == _id)
            time = 0;
        else if ((time = _bridge.get_timestamp (_id, target, len, reliable)) == (timestamp_t)(-1))
            return (-1);

        // check if connection delay exists
        if (_id2conndelay.find (target) != _id2conndelay.end ())
        {
            //if (_id2conndelay[target] > get_curr_timestamp ())
            if (_id2conndelay[target] > time)
                time = _id2conndelay[target];
            else
                _id2conndelay.erase (target);
        }

        // update network flow statistics
        count_message (receiver, msgtype, sizeof_packetize (len));

		// added by yuli ==================================================================================================

        if (buffered == true)
        {
		    // to buffer all the messages this step for deflating in flush ()
		    size_t size = sizeof (msgtype_t) + sizeof (timestamp_t) + sizeof (size_t) + len;

            // syhu: TODO: need to buffer all messages even when there's no compression?
            
		    if (_all_msg_buf.find (target) != _all_msg_buf.end ())
		    {
                /*
                char *tmp = new char[size];
			    char *pt;
			    pt = tmp;
			    memcpy (pt, &size, sizeof (size_t));
			    pt += sizeof (size_t);
			    memcpy (pt, &msgtype, sizeof (msgtype_t));
			    pt += sizeof (msgtype_t);
			    memcpy (pt, &time, sizeof (timestamp_t));
			    pt += sizeof (timestamp_t);
			    memcpy (pt, msg, len);
			    pt += len;
                */

                vastbuf *msgbuf = _all_msg_buf[target];
                msgbuf->expand (size);
                msgbuf->add (&size, sizeof (size_t));
			    msgbuf->add (&msgtype, sizeof (msgtype_t));
			    msgbuf->add (&time, sizeof (timestamp_t));
			    msgbuf->add ((void *)msg, len);

			    //vastbuf *test;
			    //test = _all_msg_buf[target];
			    //_all_msg_buf[target]->add ((void *)tmp, size);
			    //delete[] tmp;
		    }
		    else
		    {
			    vastbuf *msgbuf = new vastbuf;
			    msgbuf->reserve (size);
			    msgbuf->add (&size, sizeof (size_t));
			    //msgbuf.add (&_id, sizeof (id_t));
			    msgbuf->add (&msgtype, sizeof (msgtype_t));
			    msgbuf->add (&time, sizeof (timestamp_t));
			    msgbuf->add ((void *)msg, len);
			    _all_msg_buf[target] = msgbuf;
		    }
            
		    //=================================================================================================================
        }

        return receiver->storemsg (_id, msgtype, msg, len, time);
    }
    
    // obtain the next message removed from queue
    // return size of message, or 0 for no more message
    int 
    net_emu::
    recvmsg (id_t &from, msgtype_t &msgtype, timestamp_t &recvtime, char **msg)
    {
        if (_active == false)
            return -1;

        // record time for use by remote_disconnect
        //_curr_time = time;
        timestamp_t time = get_curr_timestamp ();

        // return if no more message in queue 
        // or all messages are beyond current logical time
        //
        // NOTE: if we do not do logical time check, then it'll be possible
        // for a node to process messages from another node that had sent
        // a message earlier in the same time-step (this makes 0 latency). 
        
        if (_msgqueue.size () == 0 || (time > 0 && time <= _msgqueue.begin ()->second->time))
            return -1;

        netmsg *curr_msg;
        curr_msg = _msgqueue.begin ()->second;
        _msgqueue.erase (_msgqueue.begin ());
        
        // prepare the return data        
        from     = curr_msg->from;
        msgtype  = curr_msg->msgtype;
        recvtime = curr_msg->time;        
        size_t size = curr_msg->size;

        // allocate receive buffer
        _recv_buf.reserve (size);

        memcpy (_recv_buf.data, curr_msg->msg, size);
        *msg = _recv_buf.data;

        // de-allocate memory
        delete curr_msg;
        return size;
    }


    // insert the new message as a node in the double linklist
    // NOTE: the list should be sorted by timestamp when inserting
    int
    net_emu::
    storemsg (id_t from, msgtype_t msgtype, char const *msg, size_t len, timestamp_t time)
    {                    
        if (_active == false)
            return 0;

        // store the new message
        netmsg *newnode = new netmsg (from, msg, len, msgtype, time, NULL, NULL);
        
        // failure likely due to memory allocation problem
        if (newnode->size < 0)
            return 0;

        _msgqueue.insert (std::multimap<timestamp_t, netmsg *>::value_type (time, newnode));
        
        return len;
    }

    // remote host has connected to me
    bool
    net_emu::
    remote_connect (id_t remote_id, const Addr &addr)
    {
        if (_active == false)
            return false;

        if (_id2conn.find (remote_id) == _id2conn.end ())
        {
            // make a record of connection
            _id2conn[remote_id] = 0;

            // check for information consistency
            if (addr.id != remote_id)
                printf ("net_emu: remote_connect (): remote_id and addr.id inconsistent.\n");
            if (_id2addr.find (remote_id) != _id2addr.end () &&
                _id2addr[remote_id] != addr)
                printf ("net_emu: remote_connect (): remote address and local address remote mismatch.\n");

            _id2addr[remote_id] = addr;
        }

        return true;
    }
    
    // remote host has disconnected me
    void 
    net_emu::
    remote_disconnect (id_t remote_id)
    {
        // cut connection
        if (_id2conn.find (remote_id) != _id2conn.end ())
        {
            _id2conn.erase (remote_id);
            _id2conndelay.erase (remote_id);
        }

        // send a DISCONNECT notification
        char msg[1+sizeof (id_t)];
        msg[0] = 1;
        memcpy (msg+1, &_id, sizeof (id_t));
        storemsg (remote_id, DISCONNECT, msg, 1+sizeof (id_t), 0);
    }

	int net_emu::flush (bool compress)
	{
		// added by yuli ==================================================================================================

		if (compress == true)
		{
			for (std::map<id_t, vastbuf *>::iterator it = _all_msg_buf.begin (); it != _all_msg_buf.end (); it++)
			{
				size_t after_def_size = 0;
				size_t bufsize = it->second->size;

				unsigned char *source_buf = new unsigned char[bufsize];
				memcpy (source_buf, it->second->data, bufsize);

				// to deflate the buffered messages
                after_def_size = _compressor.compress (source_buf, (unsigned char *)&_def_buf, bufsize);
				delete[] source_buf;

				// passing the deflated length to emu bridge and record the length
				//_bridge.pass_def_data (_id, it->first, after_def_size);
                count_compressed_message ((net_emu *) _bridge.get_netptr (it->first), after_def_size);
			}
        }

		// delete the allocate memory
		for (std::map<id_t, vastbuf *>::iterator it = _all_msg_buf.begin (); it != _all_msg_buf.end (); it++)
			delete it->second;

		// clear the msg buf for next time usage
		_all_msg_buf.clear ();
		
        // clean expired address records
        // TODO: csc 20080310
        //      any other method to prevent to check every step
        _refresh_database ();

        // debug 
#ifdef DEBUG_DETAIL
        /*
        printf ("%4d [%lu] connected with ", get_curr_timestamp(), _id);
        for (map<id_t,int>::iterator it = _id2conn.begin (); it != _id2conn.end (); it ++)
            printf ("%lu ", it->first);
        printf ("\n");
        printf ("%4d [%lu] has address of ", get_curr_timestamp(), _id);
        for (map<id_t,Addr>::iterator it = _id2addr.begin (); it != _id2addr.end (); it ++)
            printf ("%lu ", it->first);
        printf ("\n");
        printf ("%4d [%lu] connection delay of ", get_curr_timestamp(), _id);
        for (map<id_t,timestamp_t>::iterator it = _id2conndelay.begin (); it != _id2conndelay.end (); it ++)
            printf ("[%lu]->%u-%u=%u ", it->first, it->second, get_curr_timestamp (), (it->second - get_curr_timestamp ()));
        printf ("\n");
        */
#endif

        return 0;

		//=================================================================================================================
	}

    void 
    net_emu::
    _refresh_database ()
    {
        vector<id_t> remove_list;
        vector<id_t>::iterator itr;

        // check expiration of _id2addr
        map<id_t, Addr>::iterator it = _id2addr.begin ();
        for (; it != _id2addr.end (); it ++)
        {
            const id_t & this_id = it->first;
            // skip my address record
            if (this_id == _id)
                continue;

            // if connected, keep it
            else if (_id2conn.find (this_id) != _id2conn.end ())
            {
                _id2addr_add.erase (this_id);
                continue;
            }

            // else start a countdown timer
            else if (_id2addr_add.find (this_id) == _id2addr_add.end ())
            {
                _id2addr_add[this_id] = get_curr_timestamp ();
                continue;
            }

            // time-up! remove the record
            else if (get_curr_timestamp () - _id2addr_add[this_id] > EXPIRE_ADDRESS_RECORD)
                remove_list.push_back (this_id);
        }

        for (itr = remove_list.begin ();
            itr != remove_list.end (); itr ++)
            _id2addr.erase (*itr);

        remove_list.clear ();

        // check expiration of _id2queryip
        std::map<id_t, vector<pair<id_t,timestamp_t> > >::iterator it2 = _id2queryip.begin ();
        for (; it2 != _id2queryip.end (); it2 ++)
        {
            vector<pair<id_t,timestamp_t> >::iterator itv = it2->second.end ();
            for (; itv != it2->second.end (); itv --)
            {
                if (get_curr_timestamp () - itv->second > 2 * EXPIRE_ADDRESS_RECORD)
                {
                    itv ++;
                    it2->second.erase (it2->second.begin (), itv);
                    break;
                }
            }

            if (it2->second.size () == 0)
                remove_list.push_back (it2->first);
        }

        for (itr = remove_list.begin (); itr != remove_list.end (); itr ++)
            _id2queryip.erase (*itr);
    }

	//=================================================================================================================
} // end namespace VAST

