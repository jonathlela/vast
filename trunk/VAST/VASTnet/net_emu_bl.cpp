/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007 Shao-Chen Chang (cscxcs at gmail.com)       
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
 * net_emu_bl.h -- network layer implementation for emulation
 *                 bandwidth limited version (revolution 2)
 *
 */

/*
 * TODO:
 *      Csc 20080317: enable compression in flush ()
 */

#include "net_emu_bl.h"
#define VAST_NET_EMULATED_BL_SIZED_QUEUE
// Send buffer size (NETBL_SENDBUFFER_MULTIPLIER * peer upload bandwidth)
#define NETBL_SENDBUFFER_MULTIPLIER (2)
// Send buffer minimumu size (send buffer will not smaller than this size 
//                            in case peer has a small upload bandwidth)
#define NETBL_SENDBUFFER_MIN    (4096)
#define NET_MSGSTATE_UNIMPLEMENT (0)
#define NET_MSGSTATE_UNKNOWN     (1)
#define NET_MSGSTATE_PENDING     (2)
#define NET_MSGSTATE_SENDING     (3)

// Return value for 
#define VAST_ERR_ERROR          (-1)
#define VAST_ERR_BUFFERISFULL   (-2)
#define BLOCK_SIZE              (100)

#define MIN(x,y) ((x) < (y) ? (x) : (y)) 

namespace Vast
{       
    int 
    net_emu_bl::
    disconnect (id_t target)
    {
        int ret;
        if ((ret = net_emu::disconnect (target)) != 0)
            return ret;

        // cleanup sendqueue
        std::map<id_t, netmsg *>::iterator it;
        if ((it = _sendqueue.find (target)) != _sendqueue.end ())
        {
            netmsg * msg;

            // free all stored message(s)
            while (it->second != NULL)
            {
                it->second = it->second->getnext (&msg);
                _msg2state.erase (msg);
				
                delete msg;
            }

            // clear the queue
            _sendqueue.erase (target);
            _sendqueue_size.erase (target);
            _sendspace.erase (target);
        }

        return 0;
    }

	size_t 
	net_emu_bl::send (id_t target, char const *msg, size_t size, bool reliable)
	{
		if (_active == false || isConnected (target) == false)
			return 0;

		// find the receiver network record
		net_emu_bl *receiver = (net_emu_bl *)_bridge.getNetworkInterface (target);

		if (receiver == NULL)
			return ((size_t)(-1));

		//(1) put the message in the sending queue
		//(2) flush the sending queue

#ifdef VAST_NET_EMULATED_BL_SIZED_QUEUE
		
/*
		if (_sendqueue_size.find(target) != _sendqueue_size.end () &&
			_sendqueue_size[target] + packetsize (size) > mycap)
		{// sendqueue is overflow..
#ifdef DEBUG_DETAIL
			// TODO: has any other side effects?
			printf ("[%lu][net_emu_bl] send queue is full {target=%lu, msgtype=%d, size=%u}.\n", _id, target, msgtype, size);
#endif
			// simply return a sending failure
			return VAST_ERR_BUFFERISFULL;
		}
*/
		// create netmsg
		netmsg *newm = new netmsg (_id, msg, size, this->getTimestamp (), NULL, NULL);
		
        // push msg into _sendqueue (send will be done when flush called in near future)
		if (_sendqueue.find (target) == _sendqueue.end ())
		{
			_sendqueue[target] = newm;
			_sendqueue_size [target] = packetsize(size);
		}
		else
		{
			_sendqueue[target]->append (newm, this->getTimestamp ());
			_sendqueue_size [target] += packetsize (size);
		}
		_msg2state[newm] = NET_MSGSTATE_PENDING;
		

		// Potential BUG: always returns a success, whatever the message sends out in this step or not

#else

		// create the message receive time, do not send if dropped (arrival time is -1)
		timestamp_t recvtime;

		if ((recvtime = _bridge.getArrivalTime (_id, target, size, reliable)) == (timestamp_t)(-1))
			return (-1);

		// loop through it, as this msg could contain several 
		// distinct messages to different target logical nodes
		char *p = (char *)msg;
		size_t      msg_size;
		id_t        sendhost_id;
		timestamp_t senttime;

		while (p < (msg + size))
		{// detect this message has been sent or not
			memcpy (&msg_size, p, sizeof (size_t));
			p += sizeof (size_t);

			msg_size -= (sizeof (id_t) + sizeof (timestamp_t));

			memcpy (&sendhost_id, p, sizeof (id_t));
			p += sizeof (id_t);

			memcpy (&senttime, p, sizeof (timestamp_t));
			p += sizeof (timestamp_t);

			receiver->storeRawMessage (sendhost_id, p, msg_size, senttime, recvtime);
			p += msg_size;
		}
#endif
		
		return 0;
	}
  
    // send all pending reliable messages in a single packet to each target
    size_t 
    net_emu_bl::
    clearQueue ()
    {
        // nothing to send
        if (_sendqueue.size () <= 0)
            return 0;

        std::vector <id_t> remove_list;

        // get send quota first
        size_t mycap = _bridgebl.get_quota (BW_UPLOAD, _id);
		
		// get the number of active connections
		size_t myconns = _id2conn.size();
		size_t sendTarget = _sendqueue.size();
		if (myconns == 0 && sendTarget == 0)
		{
			return 0;
		}

		std::map<id_t, netmsg *>::iterator it = _sendqueue.begin ();
		std::map<id_t, netmsg *>::const_iterator end_it = _sendqueue.end();
		
		// each target peer's max quota per step
		const uint32_t max_peer_quota = mycap/ (min(myconns, sendTarget)); 
        if (max_peer_quota <= 0)
            return 0;

        const static size_t min_block_size = 32; //rquota / 10;
        //size_t dest_full_count = 0;

        it = _sendqueue.begin ();

        // find last active queue (check end of _sendqueue for safety) (why??? by CH)
        for (; (it->first < _last_actqueue_id) && (it != _sendqueue.end ()); it ++)
            ;

        // if really reach the end of queue, restart from begin (this shouldn't happen)
        if (it == _sendqueue.end ())
            it = _sendqueue.begin ();

        // reset last active queue id
        _last_actqueue_id = 0;

        // continue send if have send quota and data need to be send
		it = _sendqueue.begin();
		end_it = _sendqueue.end();

		size_t left_quota = 0, total_send_size = 0; // left quota from other peers
		while ((it != end_it) 
			   && (_sendqueue.size () > remove_list.size () /* + dest_full_count */))
		{
			const id_t & dest_id = it->first;
			//netmsg* msg = it->second;
			//   send success iff 
			//   there has data need to send
			//   connection successfully established
			//   *I have quota to send*(checked before) and *destination has free space to receive*
			if (it->second!= NULL 
				&& (_id2conn.find (dest_id) != _id2conn.end ()) 
				/*&& (_bridgebl.get_quota (BW_DOWNLOAD, dest_id) >= min_block_size)*/)
			{							
				size_t sent_size = 0;					
				size_t rquota = MIN (max_peer_quota, (mycap - total_send_size));

				//if (_sendspace[dest_id] + min_block_size >= packetsize (msg->size))
				if (packetsize(it->second->size) - _sendspace[dest_id] <=  rquota)
				{   // message will be send at this round
					sent_size = packetsize (it->second->size) - _sendspace[dest_id];
					// reset send counter
					_sendspace [dest_id] = 0;

					// get next message
					netmsg * tmsg;
					it->second = it->second->getnext (&tmsg);
					if (it->second == NULL)
					{ // no message will be sent to this peer at this ronud
						remove_list.push_back (dest_id);						
					}
					// check if has next message in queue, if yes, update state of the message
					else if (_msg2state.find (it->second) != _msg2state.end ())
						_msg2state [it->second] = NET_MSGSTATE_SENDING;

					// remove msgstate
					if (_msg2state.find (tmsg) != _msg2state.end ())
						_msg2state.erase (tmsg);

					// decrease send queue size
					_sendqueue_size [dest_id] -= sent_size;
					
					// store message to destination's recv queue						
					// call NET_EMU::send();					
					{
						net_emu::send (dest_id, tmsg->msg, tmsg->size, true);
					}
					delete tmsg;				
				}
				else
				{ // this message cannot be sent at this round
					sent_size = rquota + left_quota;
					_sendspace [dest_id] += sent_size;					
				}
				total_send_size += sent_size;

				if (min_block_size > mycap - total_send_size)
				{// run out of my all upload bandwidth
					break;
				}
			}
			//next peer			
			it ++;
			if (it == _sendqueue.end ())
				it = _sendqueue.begin ();
		}

        // remove empty send queue
        std::vector<id_t>::iterator itd = remove_list.begin ();
        for (; itd != remove_list.end (); itd ++)
        {
            id_t &r_id = *itd;
            if (_sendqueue.find (r_id) != _sendqueue.end () && 
                _sendqueue[r_id] != NULL)
            {
                printf ("[%llu][net_emu_bl] error of removing sendqueue\n", _id);
                while (_sendqueue[r_id]->msg != NULL && _sendqueue[r_id]->next != NULL)
                {
                    netmsg * tmsg;
					_sendqueue[r_id] = _sendqueue[r_id]->getnext (&tmsg);
                    delete tmsg;
                }
            }
            _sendqueue.erase (r_id);
            _sendqueue_size.erase (r_id);
        }
		
        return total_send_size;
    }

    // remote host has disconnected me
    void 
    net_emu_bl::
    remoteDisconnect (id_t remote_id)
    {		
        net_emu::remoteDisconnect (remote_id);

        // clear up sendqueue
        std::map<id_t, netmsg *>::iterator it;
        if ((it = _sendqueue.find (remote_id)) != _sendqueue.end ())
        {
            netmsg *msg;
            while (it->second != NULL)
            {
                it->second = it->second->getnext (&msg);
                _msg2state.erase (msg);
                delete msg;
            }

            _sendqueue.erase (remote_id);
            _sendqueue_size.erase (remote_id);
            _sendspace.erase (remote_id);
        }
		
    }

} // end namespace Vast

