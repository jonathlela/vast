/*
 * VON - forwarding model (based on VAST)
 * Copyright (C) 2006 Tsu-Han Chen (bkyo0829@yahoo.com.tw)
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


#ifndef VAST_FO_H
#define VAST_FO_H

#define CHUNK 128*1024
#include "vast.h"
#include "net_msg.h"
#include <time.h>       // for time ()
#include <map>

namespace VAST {
    
    typedef enum VAST_FO_Message
    {
        FO_DISCONNECT,         // disconnection without action: leaving overlay or no longer overlapped
        FO_ID,                 // id for the host
        FO_QUERY,              // find out host to contact for initial neighbor list
        FO_HELLO,              // initial connection request
        FO_EN,                 // query for missing enclosing neighbors
        FO_MOVE,               // position update to normal peers 
        FO_ZIPACK,             // used for compression
        FO_UNKNOWN
    };
    

    class msg_Node
    {
    public:
        msg_Node ()
        {
            
        }
        msg_Node (Node _node , Addr _addr )
          : node(_node), addr(_addr)
        {
           
        }
        Node        node;
        Addr        addr;
        
    };
    
    struct ForwardRecord
    {
        id_t id;
        bool newly;
        bool flag_c;            // to alert Target changed
        vector<id_t> from;      // this message sent from whom
        vector<id_t> Target;    // forward to nodes
  
    };
    
    struct Keep
    {
        id_t node;
        id_t To;
        int counter;
        char FCount;
        bool OutOfAOI;
        bool boundary;
        bool forwarded;
        bool renew;             // updated or not
        bool connected;
        unsigned char serial;   
    };
    
    struct Buffer
    {
        id_t id;
        int lifetime;
        char frame_id;
        vector<string > msg;
        vector<int> size_of_msg;
        vector<msgtype_t> type_of_msg;
        vector<id_t> OldEN;
        vector<id_t> OldAN;
    };
    struct Forwarded_Record
    {
        id_t who;
        id_t to;
        vector<id_t> f_ids;
    };
  
    class vast_fo : public vast
    {
    public:

        vast_fo (network *netlayer, aoi_t detect_buffer, int conn_limit = 0);
        ~vast_fo ();
  
        // join VON to obtain an initial set of AOI neighbors
        int     join (id_t id, aoi_t AOI, Position &pos, Addr &gateway);
  
        // quit VON
        void    leave ();
        
        // AOI related functions
        void    setAOI (aoi_t radius);
        aoi_t   getAOI ();
        
        // move to a new position, returns actual position
        Position & setpos (Position &pos);

        char *getstat (bool clear = false);

        void  op_switch(bool op_bool);           //optimization switch
        void  loss_rate_setting(int lr);         //set loss rate
   
    private:
          
        // returns whether the message has been handled successfully
        bool handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size);

        // do things after messages are all handled
        void post_processmsg ();              

        Addr _gateway;
        // neighbor maintainence
        inline bool insert_node (Node &node, Addr &addr);
        inline bool delete_node (id_t id);
        inline bool update_node (Node &node);
  
        // send node infos to a particular node
        void send_nodes (id_t target, vector<id_t> &list);
  
        bool is_neighbor (id_t id);
        bool is_new_nbr (id_t id);
          
        inline bool over_connected ()
        {
            return (_connlimit != 0 && (int)_id2node.size () > _connlimit);
        }
        
        inline bool is_gateway ()
        {
            return (_id_count != 0);
        }
        //
        // internal data structures
        //
  
        int     _connlimit;
        aoi_t   _original_aoi;      // initial AOI
        id_t    _id_count;
        
        bool    passing_node;       // when it is true, passing nodes is allowed.
        bool    move_start;         // bool to send MOVE before setpos called. 
        int     mode;               // mode1 forwarding model, mode2 direct-connected model(not work in this version).
        int     fo_conn_limit;      // forwarding limit setting
        int     conn_count;         // limit count
        bool    sendmsg_bool;       // when it's true, starting send msg, when msg sent compeletly, it will turn to false automatically.
        int     net_latency;        // for simulation
        int     computing_count;    // if computing_count==1,send only forwarding msg.
        
        int     initial_count;
        
        bool    path_optimize;       //bool to switch optimization
        int     loss_rate_p;
  
        Addr    myaddr;              // Buffer to Record Node's Msg
        char    Package_id,frame_id; //avoid old message update
        vector<id_t> MyOldEN;
        vector<id_t> MyOldENrt;
  
  
        //Buffer BufferFlow[100];
        vector<Buffer> BufferFlow;     // keep EN's msg 
  
        // mapping from id to node
        map<id_t, msg_Node> _id2node;    
  
        // record for each connected neighbor's knowledge of my ENs
        map<id_t, map<id_t, int> *> _EN_lists;
  
        // record of current time, progress with each call to processmsg
        timestamp_t _time;
  
        // record for path and nodes' lifetime
        vector<Keep> Refresher;
  
        // record new nbr
        vector<id_t> new_nbr;
  
        //record for the node to forward
        vector<ForwardRecord> FMsg; 
  
        Position  try_to_walk(Position pt);             // avoid passing node 
        void   findPos(msg_Node New);             // accept QUERY or forward QUERY to EN
        void   SayHelloToNewNbr(void);            // send HELLO to new EN            
        void   SaveMessage(id_t,int,char*,int);   // keep Message 
        void   AdjustPath(void);                  // forwarding path adjust
        void   sendmsg(void);                     // send new position to EN
  
        string SaveOverlap(int BFNum,int type);   // refresh overlapping EN
        void   MakeNewBuffer(id_t id);            // make a new buffer
        int    MakeNewRefresher(msg_Node Current,int serial,int life_time);  // make a new refresher
        
        int    return_CN(void);                   // return connected EN
        bool   accept_initial;                    // when accept first HELLO, it will be set true.
        int   net_sendmsg(id_t to, msgtype_t type,char *msg,size_t size,timestamp_t a_time);
        int   get_Buffer_pos(id_t);
        // mapping from id to payload message queue
        map<id_t, netmsg *> _id2msg;
  
        int inf(unsigned char *source,unsigned char *dest,int size);
        int def(unsigned char *source,unsigned char *dest,int size);
        void zip_test(void);
    };
  
} // end namespace VAST

#endif // VAST_FO_H
