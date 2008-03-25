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

#include "net_emu_bl.h"

namespace VAST
{   
    void 
    net_emu_bl::
    register_id (id_t my_id)
    {
        _bridge.register_id (_id, my_id);
        _id = my_id;

        // use a dummy self address
        Addr addr;
        addr.zero ();
        _id2addr[_id] = addr;
    }

    int 
    net_emu_bl::
    connect (id_t target)
    {
        printf ("net_emu_bl: connect (): Not implemented.\n");
        return 0;
    }

    int 
    net_emu_bl::
    connect (Addr & addr)
    {
        printf ("net_emu_bl: connect (): Not implemented.\n");
        return 0;
    }

    int 
    net_emu_bl::
    connect (id_t target, Addr addr)
    {
        if (_active == false)
            return (-1);

        // avoid self-connection
        if (target == _id)
            return 0;
        
        /*
        // check if connection is already established
        if (_id2addr.find (target) != _id2addr.end ())
            return -1;
        */

        _id2addr[target] = addr;

        // notify remote node of the connection
        net_emu_bl *receiver = (net_emu_bl *)_bridge.get_netptr (target);
        
        if (receiver == NULL)
            return (-1);
               
        receiver->remote_connect (_id, addr);        
        return 0;
    }
    
    int 
    net_emu_bl::
    disconnect (id_t target)
    {
        // check if connection exists
        if (_id2addr.find (target) == _id2addr.end ())
            return -1;

#ifdef DEBUG_DETAIL
        printf ("[%d] disconnection success\n", (int)target);
#endif
        
        // update the connection relationship
        _id2addr.erase (target);

        // clearup sendqueue
        std::map<id_t, netmsg *>::iterator it;
        if ((it = _sendqueue.find (target)) != _sendqueue.end ())
        {
            netmsg * msg;

            // free all stored message(s)
            while (it->second != NULL)
            {
                it->second = it->second->getnext (&msg);
                _msg2state.erase (msg->msg_id);

                delete msg;
            }

            // clear the queue
            _sendqueue.erase (target);
            _sendqueue_size.erase (target);
            _sendspace.erase (target);
        }

        // do a remote disconnect
        net_emu_bl *receiver = (net_emu_bl *)_bridge.get_netptr (target);
        
        if (receiver == NULL)
            return -1;

        receiver->remote_disconnect (_id);   
        return 0;
    }


    // send message to a node
    int 
    net_emu_bl::
    sendmsg (id_t target, msgtype_t msgtype, char const *msg, size_t len, /*timestamp_t time, */bool reliable, bool buffered)
    {
#ifdef DEBUG_DETAIL
        //printf ("%4d [%3d]    sendmsg   to: %3d type: %-8s size:%3d\n", time, (int)_id, target, VAST_MESSAGE[msgtype], len);
#endif
        if (_active == false)
            return 0;
        
        // check for connection
        if (_id2addr.find (target) == _id2addr.end ())
            return VAST_ERR_ERROR;

        // if is message to myself, send it directy
        if (target == _id)
        {
            _last_msgid ++;
            this->storemsg (_id, msgtype, msg, len, 0);
        }
#ifdef VAST_NET_EMULATED_BL_TYPE_SENDDIRECT
        // if message set as sending directly (controlled by NET_EMU_BL_TYPE_SENDDIRECT defined in config.h)
        else if (NET_EMU_BL_TYPE_SENDDIRECT (msgtype))
        {
            _last_msgid ++;

            // find the receiver network record
            net_emu_bl *receiver = (net_emu_bl *) _bridge.get_netptr (target);

            if (receiver == NULL)
                return VAST_ERR_ERROR;
            else
            {
                size_t real_msgsize = len + sizeof (msgtype_t) + sizeof (timestamp_t);
                _sendsize += real_msgsize;
                receiver->_recvsize += real_msgsize;

                //_msgtype2sendsize [msgtype] = real_msgsize + 
                //    ((_msgtype2sendsize.find (msgtype) == _msgtype2sendsize.end ()) ? 0 : _msgtype2sendsize [msgtype]);
                if (_msgtype2sendsize.find (msgtype) == _msgtype2sendsize.end ())
                    _msgtype2sendsize [msgtype] = 0;
                msgtype2sendsize += real_msgsize;

                //receiver->_msgtype2recvsize [msgtype] = real_msgsize + 
                //    ((receiver->_msgtype2recvsize.find (msgtype) == receiver->_msgtype2recvsize.end ()) ? 0 : receiver->_msgtype2recvsize [msgtype]);
                if (receiver->_msgtype2recvsize.find (msgtype) == receiver->_msgtype2recvsize.end ())
                    receiver->_msgtype2recvsize [msgtype] = 0;
                receiver->_msgtype2recvsize [msgtype] += real_msgsize;

                return receiver->storemsg (_id, msgtype, msg, len, get_curr_timestamp ());
            }
        }
#endif
        // else push message in sendqueue
        else
        {
#ifdef VAST_NET_EMULATED_BL_SIZED_QUEUE
            // First, check if sendqueue is full
            // Default send buffer is setted by twice as upload bandwidth, and has a minimum size with _MIN_BUFFER_SIZE
            size_t mycap = _bridge.default_upcap;
            if (_bridge.node_upcap.find (_id) != _bridge.node_upcap.end ())
                mycap = _bridge.node_upcap [_id];
            mycap *= NETBL_SENDBUFFER_MULTIPLIER;
            if (mycap < NETBL_SENDBUFFER_MIN)
                mycap = NETBL_SENDBUFFER_MIN;
            
            if (_sendqueue_size.find(target) != _sendqueue_size.end () &&
                _sendqueue_size[target] + sizeof_packetize (len) > mycap)
            {
#ifdef DEBUG_DETAIL
                // TODO: has any other side effects?
                printf ("[net_emu_bl][%d] send queue is full {target=%u, msgtype=%d, len=%u}.\n", _id, target, msgtype, len);
#endif
                // simply return a sending failure
                return VAST_ERR_BUFFERISFULL;
            }
            /*
            else
            {
                printf ("[net_emu_bl][%d] queue %d mycap %d\n", _id, _sendqueue_size[target], mycap);
            }
            */
#endif

            id_t msg_id = ++_last_msgid;

            // create netmsg
            netmsg *newm = new netmsg (_id, msg, len, msgtype, this->get_curr_timestamp (), NULL, NULL, msg_id);

            // push msg into _sendqueue (send will be done when flush called in near future)
            if (_sendqueue.find (target) == _sendqueue.end ())
            {             
                _sendqueue[target] = newm;
                _sendqueue_size [target] = sizeof_packetize(len);
            }
            else
            {
                _sendqueue[target]->append (newm, this->get_curr_timestamp ());
                _sendqueue_size [target] += sizeof_packetize(len);
            }

            // set message state
            _msg2state[msg_id] = NET_MSGSTATE_PENDING;
        }

        // Potential BUG: always returns a success
        return len;
    }
    
    // obtain the next message removed from queue
    // return size of message, or 0 for no more message
    int 
    net_emu_bl::
    recvmsg (id_t &from, msgtype_t &msgtype, timestamp_t &recvtime, char **msg/*, timestamp_t time*/)
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

    
    // send out all pending reliable message in a single packet to each target
    int 
    net_emu_bl::
    flush (bool compress)
    {
        if (_sendqueue.size () <= 0)
            return 0;

        std::vector <id_t> remove_list;

        // get send quota first
        size_t rquota = _bridge.get_quota (BW_UPLOAD, _id);
        if (rquota <= 0)
            return 0;

        size_t blocksize = 100;//rquota / 10;
        size_t dest_full_count = 0;

        // TODO: restart transmition from the stop point last time
        std::map<id_t, netmsg *>::iterator it = _sendqueue.begin ();

        // find last active queue (check end of _sendqueue for safety)
        for (; (it->first < _last_actqueue_id) && (it != _sendqueue.end ()); it ++)
            ;

        // if really reach the end of queue, restart from begin (this shouldn't happen)
        if (it == _sendqueue.end ())
            it = _sendqueue.begin ();

        // reset last active queue id
        _last_actqueue_id = 0;

        // continue send if have send quota and data need to be send
        while ((rquota >= blocksize) && 
               (_sendqueue.size () > remove_list.size () + dest_full_count))
        {
			const id_t & dest_id = it->first;

            // send success iff 
            //   there has data need to send
            //   *I have quota to send*(checked before) and *destination has free space to receive*
            if (it->second != NULL &&
                _bridge.get_quota (BW_DOWNLOAD, dest_id) >= blocksize)
            {
                _sendspace [dest_id] += blocksize;
                rquota -= blocksize;

                // TODO: do this here or after entire flush process?
                _bridge.spend_quota (BW_UPLOAD, _id, blocksize);
                _bridge.spend_quota (BW_DOWNLOAD, dest_id, blocksize);

                net_emu_bl * target = (net_emu_bl *) _bridge.get_netptr (dest_id);
                
                // count send/receive size for statistics
                {
                    // add my sendsize
                    _sendsize += blocksize;
                    // add my sendsize by type
                    _type2sendsize [it->second->msgtype] = blocksize + 
                        ((_type2sendsize.find (it->second->msgtype) == _type2sendsize.end ()) ? 0 : _type2sendsize [it->second->msgtype]);

                    // add destination's recvsize
                    
                    if (target != NULL)
                    {
                        target->_recvsize += blocksize;
                        target->_type2recvsize [it->second->msgtype] = blocksize +
                            ((target->_type2recvsize.find (it->second->msgtype) == target->_type2recvsize.end ()) ? 0 : target->_type2recvsize [it->second->msgtype]);
                    }
                }

                // check if sending is finished
                // Pontential BUG: add sizeof msgtype_t and timestamp_t is size of transmission header
                if (_sendspace [dest_id] >= sizeof_packetize (it->second->size))
                {
                    //size_t sizefix = (blocksize - ((it->second->size + sizeof (msgtype_t) + sizeof (timestamp_t)) % blocksize));
                    size_t sizefix = (blocksize - (sizeof_packetize(it->second->size) % blocksize));

                    // revise size counting
                    rquota += sizefix;
                    _sendsize -= sizefix;
                    _type2sendsize [it->second->msgtype] -= sizefix;
                    if (target != NULL)
                    {
                        target->_recvsize -= sizefix;
                        target->_type2recvsize [it->second->msgtype] -= sizefix;
                    }
                    _bridge.spend_quota (BW_UPLOAD, _id, -1 * sizefix);
                    _bridge.spend_quota (BW_DOWNLOAD, dest_id, -1 * sizefix);

                    // reset send counter
                    _sendspace [dest_id] = 0;

                    // get message body
                    netmsg * tmsg;
					it->second = it->second->getnext (&tmsg);
                    if (it->second == NULL)
                        remove_list.push_back (dest_id);
                    // check if has next message in queue, if yes, update state of the message
                    else if (_msg2state.find (it->second->msg_id) != _msg2state.end ())
                        _msg2state [it->second->msg_id] = NET_MSGSTATE_SENDING;

                    // remove msgstate
					if (_msg2state.find (tmsg->msg_id) != _msg2state.end ())
						_msg2state.erase (tmsg->msg_id);

                    // decrease send queue size
                    _sendqueue_size [dest_id] -= sizeof_packetize(tmsg->size);

                    // store message to destination's recv queue
                    if (target != NULL)
                    {
                        target->storemsg (_id, tmsg->msgtype, tmsg->msg, tmsg->size, get_curr_timestamp ());
                    }
#ifdef DEBUG_DETAIL
                    else
                    {
                        fprintf (stderr, "[%d] can't find receiver's [%d] network interface.\n", (int)_id, (int)dest_id);
                    }
#endif

                    delete tmsg;
                }
            }
            // if I have no more send quota
            else if (rquota < blocksize)
            {
                _last_actqueue_id = it->first;
                break;
            }
            // destination has no free space to receive
            else if (it->second != NULL)
            {
                dest_full_count ++;
            }

            it ++;
            if (it == _sendqueue.end ())
                it = _sendqueue.begin ();
        }

        // remove empty send queue
        std::vector<id_t>::iterator itd = remove_list.begin ();
        for (; itd != remove_list.end (); itd ++)
        {
            id_t &r_id = *itd;
            if (_sendqueue[r_id] != NULL)
                printf ("[%d][net_emu_bl] error of removing sendqueue\n", (int) _id);
            _sendqueue.erase (r_id);
            if (_sendqueue_size.find (r_id) == _sendqueue_size.end ())
                printf ("[NET_EMU_BL][%lu] try to remove empty sendqueue_size %lu.\n", _id, r_id);
            else if (_sendqueue_size[r_id] != 0)
                printf ("[NET_EMU_BL][%lu] try to remove non-zero sendqueue_size %lu.\n", _id, r_id);
            _sendqueue_size.erase (r_id);
        }

        return 0;
    }

    // notify ip mapper to create a series of mapping from src_id to every one in the list map_to
    int 
    net_emu_bl::
    notify_id_mapper (id_t src_id, const std::vector<id_t> & map_to)
    {
        printf ("net_emu_bl: notify_id_mapper (): Not implemented.\n");
        return 0;
    }

    // insert the new message as a node in the double linklist
    // NOTE: the list should be sorted by timestamp when inserting
    int
    net_emu_bl::
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
    net_emu_bl::
    remote_connect (id_t remote_id, Addr const &addr)
    {
        if (_active == false)
            return false;

        if (_id2addr.find (remote_id) == _id2addr.end ())
            _id2addr[remote_id] = addr;

        return true;
    }
    
    // remote host has disconnected me
    void 
    net_emu_bl::
    remote_disconnect (id_t remote_id)
    {
        // cut connection
        if (_id2addr.find (remote_id) != _id2addr.end ())        
            _id2addr.erase (remote_id);

        // clear up sendqueue
        std::map<id_t, netmsg *>::iterator it;
        if ((it = _sendqueue.find (remote_id)) != _sendqueue.end ())
        {
            netmsg *msg;
            while (it->second != NULL)
            {
                it->second = it->second->getnext (&msg);
                _msg2state.erase (msg->msg_id);

                delete msg;
            }

            _sendqueue.erase (remote_id);
            _sendqueue_size.erase (remote_id);
            _sendspace.erase (remote_id);
        }

        // send a DISCONNECT notification
        char msg[1+sizeof (id_t)];
        msg[0] = 1;
        memcpy (msg+1, &_id, sizeof (id_t));
        storemsg (remote_id, DISCONNECT, msg, 1+sizeof (id_t), 0);// get_curr_timestamp ());
    }


    // get active connections
    /*
    const std::vector<id_t> & 
    net_emu_bl::
    get_active_connections ()
    {
        // return a empty vector by default
        static std::vector<id_t> conns;
        conns.clear ();

        std::map<VAST::id_t, netmsg *>::iterator it = _sendqueue.begin ();
        for (; it != _sendqueue.end (); it ++)
        {
            conns.push_back (it->first);
        }

        return conns;
    }
    */

} // end namespace VAST

