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


/*
 *  SimNode.h -- basic node entity used for simulations with bookkeeping capability
 *
 */

#ifndef VASTSIM_SIMNODE_H
#define VASTSIM_SIMNODE_H

#include "VASTUtil.h"
#include "Movement.h"
#include "VASTVerse.h"
#include "VASTsim.h"

#define LAYER_EVENT     1       // which message layer should events be delivered
#define LAYER_UPDATE    2

using namespace Vast;

class SimNode
{
public:
    
    SimNode (int id, MovementGenerator *move_model, const char *GWstr, SimPara &para, VASTPara_Net &netpara); //, bool as_relay);
    
    ~SimNode ();

    void clearVariables ();
    
    void move ();

    // stop this node 
    void fail ();

    void adjustAOI ();
    
    void processMessage ();

    void recordStat ();

    // get stat
    StatType &getSendStat (bool interval_only = false);
    StatType &getRecvStat (bool interval_only = false);

    // record per node per second transmission 
    //void recordStatPerSecond ();

    length_t getAOI ();

    Position &get_pos ();

    Node *getSelf ();

    // get unique ID
    Vast::id_t getID ();

    // get subscription ID
    Vast::id_t getHostID ();
    
    //size_t accumulated_send ();
    //size_t accumulated_recv ();

    long min_aoi ();

    float avg_aoi ();

    int max_CN ();

    float avg_CN ();

    //float avg_send ();

    //float avg_recv ();

	// distance to a point
    bool in_view (SimNode *remote_node);
    
    // returns the Node pointer if known, otherwise returns NULL
    // TODO:
    Node *knows (SimNode *node);

    // whether I've joined successfully
    bool isJoined ();

    // whether I'm a failed node
    bool isFailed ();

    // return the ID for this Peer
    Vast::id_t getPeerID ();

    // get the Voronoi from the matcher of this node
    Voronoi *getVoronoi ();

    VAST            *vnode;
    SimNodeState    state;
    //size_t          max_send_persec, max_recv_persec;

private:
    MovementGenerator   *_move_model;   // movement model (to provide a series of position updates)
    VASTVerse           *_world;        // factory class for VAST    

    bool                _is_gateway;    // whether I'm the first node

    SimPara             _para;
    VASTPara_Net        _netpara;       // network parameters sent to VASTVerse
    VASTPara_Sim        _simpara;       // simulation parameters sent to VASTVerse

    Node                _self;          // info about myself   
    int                 _nodeindex;     // index number of the node created
    Vast::id_t          _sub_no;        // subscription number (for my AOI)    
    bool                _as_relay;      // whether to join as relay
    map<Vast::id_t, Node *>   _neighbors;     // known neighbors

    Area                _last_aoi;      // last AOI info

    // stat
    int         _steps;
    int         _steps_recorded;
    
    long        _min_aoi, _total_aoi;    
    
    int         _max_CN, _total_CN;

    //int         _seconds_recorded;

    //size_t         _last_send;
    //size_t         _last_recv;

    //size_t         _total_send;
    //size_t         _total_recv;

};

#endif
