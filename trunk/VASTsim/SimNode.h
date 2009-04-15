/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2006 Shun-Yun Hu (syhu@yahoo.com)
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
 *  simnode.h -- basic node entity used for simulations with bookkeeping capability
 *
 */

#ifndef VASTSIM_SIMNODE_H
#define VASTSIM_SIMNODE_H

#include "VASTUtil.h"
#include "Movement.h"
#include "vastverse.h"
#include "MessageQueue.h"

#define LAYER_EVENT     1       // which message layer should events be delivered
#define LAYER_UPDATE    2

class SimNode
{
public:
    
    SimNode (int id, MovementGenerator *move_model, vastverse *world, VASTsimPara &para)
        :_id (id), _move_model (move_model), _world (world), _para (para)
    {        
        Addr gateway;
        // NOTE: this must be done for network layer to check the id to connect
        gateway.id = NET_ID_GATEWAY;
        
        // create network layer & message Queue
        // NOTE: we assume default local listen port is the same as gateway's port        
        _net      = _world->create_net (gateway.publicIP.port);
        _msgqueue = _world->create_queue (_net);
        vnode     = _world->create_node (_msgqueue);           

        // register the netID for all
        //if (_id == NET_ID_GATEWAY)
        _net->register_id (_id);
        
        // join the system
        Coord *pos = _move_model->getPos (_id-1, 0);
        _aoi.center.x = pos->x;
        _aoi.center.y = pos->y;
        _original_aoi = _aoi.radius = para.AOI_RADIUS;

        Area area;
        area.width = para.WORLD_WIDTH;
        area.height = para.WORLD_HEIGHT;
        vnode->join (_id, area, para.AOI_RADIUS);

        // make initial subscription
        vector<Area> sublist;
        sublist.push_back (_aoi);
        _sub_no = vnode->subscribe (sublist, LAYER_EVENT);

        // make initial manager movement/joining
        vnode->manage (&_aoi.center);

        _self = vnode->getSelf ();      
        _last_recv = _last_send = 0;
        _steps = 0;
        clear_variables ();
    }
    
    ~SimNode ()
    {   
        vnode->leave ();

        // NOTE: must destory in the reverse order of creation
        _world->destroy_node (_msgqueue, vnode);

        // TODO: delete will cause program crash
        _world->destroy_queue (_msgqueue);
        
        _world->destroy_net (_net);        
    }

    void clear_variables ()
    {
        _steps_recorded = _seconds_recorded = 0;
        _min_aoi = _original_aoi;
        _total_aoi = 0;

        _max_CN = _total_CN = 0;
                
        max_send_persec = max_recv_persec = 0;
        _total_send = _total_recv = 0;
    }
    
    void move () 
    {
        _steps++;
        Coord *pos = _move_model->getPos (_id-1, _steps);
        _aoi.center.x = pos->x;
        _aoi.center.y = pos->y;

        if (_para.CONNECT_LIMIT > 0)
            adjust_AOI ();

        // set the overlay manager's position, also update subscription area as AOI
        vnode->manage (&_aoi.center);

        // If AOI-radius hasn't changed, just use below
        //vnode->move (_sub_no, LAYER_EVENT, _aoi.center);

        // assume that AOI radius has changed, so need to re-subscribe
        vector<Area> sublist;
        sublist.push_back (_aoi);
        _sub_no = vnode->subscribe (sublist, LAYER_EVENT);

        // NOTE that network can only be flushed once per step for a node
        //_net->flush ();
    }

    void adjust_AOI ()
    {
        // NOTE: the following code is small but produces very interesting behaviors. :) 

        static int radius_delta = -2;

        // do a little AOI-radius adjustment
        //if ((_aoi.radius < (length_t)(_para.AOI_RADIUS * 0.6)) || (_aoi.radius > (length_t)_para.AOI_RADIUS))
        if ((_aoi.radius < 100) || (_aoi.radius > 200))
        {
            // change sign for multiplier
            radius_delta *= -1;
        }
        _aoi.radius += radius_delta;

        if (_aoi.radius < 5)    
            _aoi.radius = 5;

    }
    
    void processmsg ()
    {        
        // NOTE: we assume that message process will occur after some logical time progression
        //       that is, events are allowed some timesteps (e.g. 100ms) before they're being 
        //       processed and calculated for respective stats
        _msgqueue->processMessages ();
        _net->flush ();
    }

    void record_stat ()
    {
        // record keeping
        long aoi = _aoi.radius;
        _total_aoi += aoi;
        
        if (aoi < _min_aoi)
            _min_aoi = aoi;
        
        //int CN = vnode->getnodes ().size ()-1;
        int CN = _net->getconn ().size ();
		
        _total_CN += CN;
        if (_max_CN < CN)
            _max_CN = CN;

        // set up neighbor view
        _neighbors.clear ();
        vector<Node *> &nodes = vnode->getManagers ();
        for (int i=1; i<(int)nodes.size (); ++i)
            _neighbors[nodes[i]->id] = nodes[i];

        _steps_recorded++;
    }

    // record per node per second transmission 
    void record_stat_persec ()
    {
        size_t send_size = accumulated_send () - _last_send;
        size_t recv_size = accumulated_recv () - _last_recv;

        _total_send += send_size;
        _total_recv += recv_size;

        if (send_size > max_send_persec)
            max_send_persec = send_size;
        if (recv_size > max_recv_persec)
            max_recv_persec = recv_size;
       
        _last_send = accumulated_send ();
        _last_recv = accumulated_recv ();

		// added by yuli ================================================================

		size_t send_def_size = accumulated_def_send () - _last_def_send;
		size_t recv_def_size = accumulated_def_recv () - _last_def_recv;

		_total_def_send += send_def_size;
		_total_def_recv += recv_def_size;

		if (send_def_size > max_defsend_persec)
			max_defsend_persec = send_def_size;
		if (recv_def_size > max_defrecv_persec)
			max_defrecv_persec = recv_def_size;

		_last_def_send = accumulated_def_send ();
		_last_def_recv = accumulated_def_recv ();

		//===============================================================================

		_seconds_recorded++;
    }

    length_t get_aoi ()
    {
        return _aoi.radius;
    }

    Position &get_pos ()
    {
        return _self->pos;
    }

    Vast::id_t get_id ()
    {
        return _self->id;
    }
    
    inline size_t accumulated_send () 
    {
        return _net->sendsize ();
    }

    inline size_t accumulated_recv () 
    {        
        return _net->recvsize ();
    }

	// added by yuli ======================================

	inline size_t accumulated_def_send ()
	{
		//return _world->send_def_size (_self->id);
        //return _net->sendsize_compressed ();
        // syhu: no compression is supported now
        return 0;
	}

	inline size_t accumulated_def_recv ()
	{
		//return _world->recv_def_size (_self->id);
        // syhu: no compression is supported now
        //return _net->recvsize_compressed ();
        return 0;
        
	}

	//=====================================================

	long min_aoi ()
    {
        return _min_aoi;
    }

    float avg_aoi ()
    {
        return (float)_total_aoi / (float)_steps_recorded;
    }

    int max_CN ()
    {
        return _max_CN;
    }

    float avg_CN ()
    {
        return (float)_total_CN / (float)_steps_recorded;
    }

    float avg_send ()
    {
        return (float)_total_send / (float)_seconds_recorded;
    }

    float avg_recv ()
    {
        return (float)_total_recv / (float)_seconds_recorded;
    }    

	// added by yuli ==========================================================

	float avg_def_send ()
	{
		return (float)_total_def_send / (float)_seconds_recorded;
	}

	float avg_def_recv ()
	{
		return (float)_total_def_recv / (float)_seconds_recorded;
	}

	//=========================================================================

	// distance to a point
    bool in_view (SimNode *remote_node)
    {
        return (_self->pos.dist (remote_node->get_pos()) < (double)_aoi.radius);
    }
    
    // returns the Node pointer if known, otherwise returns NULL
    // TODO:
    Node *knows (SimNode *node)
    {
        Vast::id_t id = node->get_id ();
        
        // see if 'node' is a known neighbor of me
        if (_neighbors.find (id) != _neighbors.end ())
            return _neighbors[id];
        else
            return NULL;
    }

    VAST        *vnode;
    NodeState   state;
    size_t      max_send_persec, max_recv_persec;

	// added by yuli
	size_t      max_defsend_persec, max_defrecv_persec;

private:
    int                 _id;
    MovementGenerator   *_move_model;   // movement model (to provide a series of position updates)
    vastverse           *_world;        // factory class for VAST
    MessageQueue        *_msgqueue;     // message queue for sending / receiving messages for a node
    Node                *_self;         // info about myself
    Network             *_net;          // interface to network layer

    VASTsimPara         _para;

    id_t                _sub_no;        // subscription number (for my AOI)
    Area                _aoi;           // current subscription area
    length_t            _original_aoi;  // original AOI radius
    
    map<Vast::id_t, Node *> _neighbors;

    // stat
    int         _steps;
    int         _steps_recorded;
    int         _seconds_recorded;
    long        _min_aoi, _total_aoi;    
    
    int         _max_CN, _total_CN;

    size_t         _last_send;
    size_t         _last_recv;

    size_t         _total_send;
    size_t         _total_recv;

	// added by yuli ======================================

	size_t			_last_def_send;
	size_t			_last_def_recv;

	size_t			_total_def_send;
	size_t			_total_def_recv;

	//=====================================================
};

#endif
