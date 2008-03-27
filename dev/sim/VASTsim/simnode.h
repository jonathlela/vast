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

#include "vastutil.h"
#include "vastverse.h"

class SimNode
{
public:
    
    SimNode (int id, MovementGenerator *move_model, vastverse *world, VASTsimPara &para)
        :_id (id), _move_model (move_model), _world (world)
    {
        //bool is_gateway = (_id == NET_ID_GATEWAY ? true : false);                
        Addr gateway;
        // NOTE: this must be done for network layer is check the id to connect
        gateway.id = NET_ID_GATEWAY;
        
        // NOTE: we assume default local listen port is the same as gateway's port
        vnode = _world->create_node (gateway.publicIP.port, (aoi_t)para.AOI_BUFFER);

        Coord *pos = _move_model->getPos (_id-1, 0);
        Position pt (pos->x, pos->y);
        vnode->join (_id, (aoi_t)para.AOI_RADIUS, pt, gateway);

        _self = vnode->getself ();
        _original_aoi = para.AOI_RADIUS;

        _last_recv = _last_send = 0;
        _steps = 0;
        clear_variables ();
    }
    
    ~SimNode ()
    {
        _world->destroy_node (vnode);
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
        Position pt (pos->x, pos->y);
        vnode->setpos (pt);
        vnode->getnet ()->flush ();
    }
    
    void processmsg ()
    {        
        // NOTE: we assume that message process will occur after one logical time progression
        //       that is, events are allowed one timestep (e.g. 100ms) before they're being 
        //       processed and calculated for respective stats
        vnode->tick ();
        vnode->getnet ()->flush ();
    }

    void record_stat ()
    {
        // record keeping
        long aoi = _self->aoi;
        _total_aoi += aoi;
        
        if (aoi < _min_aoi)
            _min_aoi = aoi;
        
        //int CN = vnode->getnodes ().size ()-1;
        int CN = vnode->getnet ()->getconn ().size ();
		
        _total_CN += CN;
        if (_max_CN < CN)
            _max_CN = CN;

        // set up neighbor view
        _neighbors.clear ();
        vector<Node *> &nodes = vnode->getnodes ();
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

    aoi_t get_aoi ()
    {
        return _self->aoi;
    }

    Position &get_pos ()
    {
        return _self->pos;
    }

    VAST::id_t get_id ()
    {
        return _self->id;
    }
    
    inline size_t accumulated_send () 
    {
        //return _world->sendsize (_self->id);
        return vnode->getnet ()->sendsize ();
    }

    inline size_t accumulated_recv () 
    {
        //return _world->recvsize (_self->id);
        return vnode->getnet ()->recvsize ();
    }

	// added by yuli ======================================

	inline size_t accumulated_def_send ()
	{
		//return _world->send_def_size (_self->id);
        return vnode->getnet ()->sendsize_compressed ();
	}

	inline size_t accumulated_def_recv ()
	{
		//return _world->recv_def_size (_self->id);
        return vnode->getnet ()->recvsize_compressed ();
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
        return (_self->pos.dist (remote_node->get_pos()) < (double)_self->aoi);
    }
    
    // returns the Node pointer if known, otherwise returns NULL
    // TODO:
    Node *knows (SimNode *node)
    {
        VAST::id_t id = node->get_id ();
        
        // see if 'node' is a known neighbor of me
        if (_neighbors.find (id) != _neighbors.end ())
            return _neighbors[id];
        else
            return NULL;
    }

    vast        *vnode;
    NodeState   state;
    size_t      max_send_persec, max_recv_persec;

	// added by yuli
	size_t      max_defsend_persec, max_defrecv_persec;

private:
    int         _id;
    MovementGenerator *_move_model;
    vastverse   *_world;
    Node        *_self;    
    aoi_t       _original_aoi;
    map<VAST::id_t, Node *> _neighbors;

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
