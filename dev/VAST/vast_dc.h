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
 *  vast_dc.h -- VAST direct connection (DC) model
 *
 *  ver 0.1 (2005/04/11)
 *   
 */

#ifndef VAST_DC_H
#define VAST_DC_H

#include "vast.h"
#include "net_msg.h"
#include "msghandler.h"

#include <map>

namespace VAST 
{    

#define MAX_ADJUST_COUNT            4       // # of time-steps to adjust AOI
#define MAX_DROP_COUNT              4       // # of time-steps to disconnect a non-overlapped neighbor

#define AOI_ADJUST_SIZE             (5)
//#define AOI_ADJUST_SIZE             (_detection_buffer / 3)
//#define AOI_ADJUST_SIZE             (_self.aoi * 0.05)

//#define BUFFER_MULTIPLIER           2     // margin for neighbor discovery detection based on velocity
//#define RATIO_DETECTION_BUFFER      (0.10)  // percentage of default AOI-radius for neighbor discovery check zone
    


// for neighbor discovery 
#define STATE_OVERLAPPED            (0x01)
#define STATE_ENCLOSED              (0x02)

    typedef enum VAST_DC_Message
    {
        DC_QUERY = 10,          // find out host to contact for initial neighbor list
        DC_HELLO,              // initial connection request
        DC_EN,                 // query for missing enclosing neighbors
        DC_MOVE,               // position update to normal peers
        DC_MOVE_B,             // position update to boundary peers
        DC_NODE,               // discovery of new nodes          
        DC_OVERCAP,            // disconnection requiring action: shrink AOI or refuse connection
        DC_UNKNOWN
    };

    class Msg_NODE
    {
    public:
        Msg_NODE () 
        {
        }
        
        Msg_NODE (char const *p)
        {
            memcpy (this, p, sizeof(Msg_NODE));
        }
        
        Msg_NODE (Node &n)//, Addr &a)
            :node(n)//, addr(a)
        {
        }
        
        void set (Node const &node)//, Addr const &addr)
        {
            this->node = node;
            //this->addr = addr;
        }
                
        Node        node;
        //Addr        addr;
    };

    class Msg_QUERY
    {
    public:
        Msg_QUERY () 
        {
        }

        Msg_QUERY (const char *p)
        {
            memcpy (this, p, sizeof (Msg_QUERY));
        }

        Msg_QUERY (Node &n, Addr &a)
            : node (n), addr (a)
        {
        }

        Msg_QUERY& operator= (const Msg_QUERY& m)
        {
            node = m.node;
            addr = m.addr;
        }

        Node node;
        Addr addr;
    };
    
    class vast_dc : public vast
    {
    public:
        vast_dc (network *netlayer, aoi_t detect_buffer, int conn_limit = 0);
        ~vast_dc ();

        // join VON to obtain an initial set of AOI neighbors, 
        // note this only initiates the process but does not guarantee a successful join
        bool    join (id_t id, aoi_t AOI, Position &pos, Addr &gateway);
        
        // quit VON
        void    leave ();
        
        // AOI related functions
        void    setAOI (aoi_t radius);
        aoi_t   getAOI ();
        
        // move to a new position, returns actual position
        Position & setpos (Position &pos);
           
        char *  getstat (bool clear = false);
        
    private:
        
        // returns whether the message has been handled successfully
        bool handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size);

        // do neighbor discovery check for AOI neighbors
        void post_processmsg ();

        // neighbor maintainence
               bool insert_node       (Node &node);
               bool insert_node       (Node &node, Addr &addr);
        inline bool _post_insert_node (Node &node);

        inline bool delete_node (id_t id, bool disconnect = true);
        inline bool update_node (Node &node);

        // helper methods
        inline bool is_neighbor (id_t id)
        {
            return (_id2node.find (id) != _id2node.end ());
        }
        
        inline bool over_connected ()
        {
            return (_connlimit != 0 && (int)_id2node.size () > _connlimit);
        }

        // see if a neighbor to be tested for discovery is to my 'right' (in regard to the moving node)
        inline bool right_of (id_t test_node, id_t moving_node);

        // send node infos to a particular node
        void send_nodes (id_t target, vector<id_t> &list, bool reliable = false);

        // send to target node a list of IDs
        void send_ID (id_t target, msgtype_t msgtype, vector<id_t> &id_list);

        // dynamic AOI adjustment to keep transmission bounded
        void adjust_aoi (Position *invoker = NULL);
        
        // check for disconnection from neighbors no longer in view
        // returns number of neighbors removed
        int  remove_nonoverlapped ();
        
        //
        // internal data structures
        //        
        aoi_t   _original_aoi;          // initial AOI        
        aoi_t   _detection_buffer;      // buffer area for neighbor discovery detection
        int     _connlimit;             // connection limit


        // counters
        int              _count_dAOI;        // counter for adjusting AOI
        map<id_t, int>   _count_drop;        // counter for disconnecting a node

        // mapping from id to node
        map<id_t, Node> _id2node;

        // mapping from id to remote node's velocity
        map<id_t, double> _id2vel;

        // record for each connected neighbor's knowledge of my neighbors
        map<id_t, map<id_t, int> *> _neighbor_states;

        // nodes worth considering to connect
        map<id_t, Msg_NODE> _notified_nodes;  

        // nodes requesting for neighbor discovery check        
        map<id_t, int>      _req_nodes;

        // stat for recording NODE message redundency 
        int _NM_total, _NM_known;

        // generic send buffer
        char _buf[VAST_BUFSIZ];
    };
                     
} // end namespace VAST

#endif // VAST_DC_H

