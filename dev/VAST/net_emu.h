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

/*
 *  TODO
 */

/*
 * net_emu.h -- network implementation interface (virtual class)
 *   
 *  
 */

#ifndef VAST_NET_EMU_H
#define VAST_NET_EMU_H

#include "typedef.h"
#include "network.h"
#include "net_msg.h"
#include "net_emubridge.h"
#include "vastbuf.h"
#include "vastutil.h"
#include <map>
#include <vector>

namespace VAST {

    // default address record will be expired in (steps)
    const timestamp_t EXPIRE_ADDRESS_RECORD = 20;
    // note: _id2queryip record will keeps two times to EXPIRE_ADDRESS_RECORD from inserting

    class net_emu : public network
    {
    public:
        net_emu (net_emubridge &b)
            :_bridge (b), _active (false)
        {
            // obtain a temporary id first
            _id = _bridge.obtain_id (this);
            
            //printf ("tempid: %d\n", _id);
            Addr addr;
            addr.id = _id;
            _id2addr[_id] = addr;
        }

        ~net_emu ()
        {
            // remove from bridge so that others can't find me
            _bridge.release_id (_id);

            // free up message queue
            std::multimap<timestamp_t, netmsg *>::iterator it = _msgqueue.begin ();
            for (; it != _msgqueue.end (); it++)
                free (it->second);
        }

        //
        // inherent methods from class 'network'
        //

        virtual void register_id (id_t my_id);

        virtual void start ()
        {
            _active = true;
        }

        virtual void stop ()
        {
            _active = false;
            
            // remove all existing connections
            std::vector<id_t> remove_list;
            std::map<id_t, int>::iterator it = _id2conn.begin ();
            for (; it != _id2conn.end (); it++)
                remove_list.push_back (it->first);

            for (unsigned int i=0; i<remove_list.size (); i++)
                disconnect (remove_list[i]);            
        }

        // connect or disconnect a remote node (should check for redundency)
        // returns (-1) for error, (0) for success
        virtual int connect (id_t target);
        virtual int connect (Addr & addr);
        virtual int disconnect (id_t target);

        Addr &getaddr (id_t id)
        {
            static Addr s_addr;
            if (_id2addr.find (id) != _id2addr.end ())
                return _id2addr[id];

            return s_addr;
        }

        // get a list of currently active connections' remote id and IP addresses
        std::map<id_t, Addr> &getconn ()
        {
            static std::map<id_t, Addr> ret;
            ret.clear ();

            for (std::map<id_t, int>::iterator it = _id2conn.begin (); it != _id2conn.end (); it ++)
            {
                if (_id2addr.find (it->first) == _id2addr.end ())
                {
                    printf ("net_emu: getconn (): Can't find IP record of id %d\n", it->first);
                    ret.clear ();
                    return ret;
                }

                ret[it->first] = _id2addr[it->first];
            }

            return ret;
        }

        // send message to a node
        virtual int sendmsg (id_t target, msgtype_t msgtype, char const *msg, size_t len, bool reliable = true, bool buffered = false);
        
        // obtain next message in queue before a given timestamp
        // returns size of message, or -1 for no more message
        virtual int recvmsg (id_t &from, msgtype_t &msgtype, timestamp_t &recvtime, char **msg);

        // send out all pending reliable message in a single packet to each target
        virtual int flush (bool compress = false);

        // notify ip mapper to create a series of mapping from src_id to every one in the list map_to
        virtual int notify_id_mapper (id_t src_id, const std::vector<id_t> & map_to);

        // get current physical timestamp
        timestamp_t get_curr_timestamp ()
        {
            return _bridge.get_curr_timestamp ();
        }
        
        //
        // emulator-specific methods
        //
        virtual int  storemsg (id_t from, msgtype_t msgtype, char const *msg, size_t len, timestamp_t time);
        virtual bool remote_connect (id_t remote_id, Addr const &addr);
        virtual void remote_disconnect (id_t remote_id);


        // get if is connected with the node
        inline bool is_connected (id_t id)
        {
            if (_id2conn.find (id) == _id2conn.end ())
                return false;

            return true;
        }

    protected:
        int _send_ipquery (id_t target);
        int _query_ip (id_t target, Addr & ret);
        void _refresh_database ();

    protected:
        // unique id for the vast class that uses this network interface
        id_t                        _id;

        // mapping from node ids to addresses
        std::map<id_t, Addr>        _id2addr;
        std::map<id_t, timestamp_t> _id2addr_add;

        // connection list
        // (following int has no meaning, using map for fast insert/remove)
        std::map<id_t, int>         _id2conn;

        // connection delay (absolute time while connection has been established)
        std::map<id_t, timestamp_t> _id2conndelay;

        // waiting response ip for estibilish connection (no need in emulation network)
        //std::map<id_t, timestamp_t> _id2waitconn;

        // nodes to query ip about id
        std::map<id_t, vector<pair<id_t,timestamp_t> > > _id2queryip;

        // a shared object among net_emu class 
        // to discover class pointer via unique id
        net_emubridge &             _bridge;

        // queue for incoming messages
        std::multimap<timestamp_t, netmsg *> _msgqueue;

        // last timestamp
        //timestamp_t                 _curr_time;

        // network characteristics
        bool                        _active;
        
        // buffer for sending/receiving messages
        vastbuf                     _recv_buf;

		// added by yuli ====================================================
        // Csc 20080225: change to 'static'
        //   it's just a namespace 'Compressor', so declare as static maybe wastes less memory?
        static Compressor           _compressor;

		// be compression source buffer
		char _def_buf[VAST_BUFSIZ];
		std::map<id_t, vastbuf *>			_all_msg_buf;		

		double _step_original_size;
		double _step_compressed_size;

		// ==================================================================

    };

} // end namespace VAST

#endif // VAST_NET_EMU_H
