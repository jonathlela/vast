

/*
 *  SimNode.cpp -- basic node entity used for simulations with bookkeeping capability
 *
 */


#include "SimNode.h"

    
    SimNode::SimNode (int id, MovementGenerator *move_model, SimPara &para, bool as_relay, id_t beh_id)
        :_move_model (move_model), _para (para), _as_relay (as_relay), _beh_id(beh_id)
    {            
        _self.id = NET_ID_UNASSIGNED;
        _nodeindex = id;
        if (_para.AOI_RADIUS < 0)
            _self.aoi.radius = (-1)*_para.AOI_RADIUS + (rand () % 3-1) * (rand () % _para.AOI_RADIUS);

        else
            _self.aoi.radius = _para.AOI_RADIUS;

        // obtain initial position
        _steps = 0;

		//setJoinStep(_steps);
        Position *pos = _move_model->getPos (beh_id, _steps);
        _self.aoi.center = *pos;

        // note there's no need to assign the gateway ID as it'll be found automatically
        _netpara.gateway    = Addr (0, IPaddr ("127.0.0.1", 3737));        
        _netpara.is_gateway = (_nodeindex == 1);
        _netpara.model      = (VAST_NetModel)_para.NET_MODEL;
        _netpara.port       = 3737;
        _netpara.phys_coord = _self.aoi.center;     // use our virtual coord as physical coordinate
        _netpara.peer_limit = _para.PEER_LIMIT;
        _netpara.relay_limit = _para.RELAY_LIMIT;
		                
        _simpara.conn_limit = _para.CONNECT_LIMIT;
        _simpara.fail_rate  = _para.FAIL_RATE;
        _simpara.loss_rate  = _para.LOSS_RATE;
        _simpara.recv_quota = _para.DOWNLOAD_LIMIT;
        _simpara.send_quota = _para.UPLOAD_LIMIT;
		_simpara.step_per_sec = _para.STEPS_PERSEC;       
		_netpara.is_relay   = _as_relay;
        _world = new VASTVerse (&_netpara, &_simpara);	

        state = IDLE;
		
        _last_recv = _last_send = 0;        
        clear_variables ();
    }

	SimNode::SimNode (int id, MovementGenerator *move_model, SimPara &para, bool as_relay)
		:_move_model (move_model), _para (para), _as_relay (as_relay)
	{            
		_self.id = NET_ID_UNASSIGNED;
		_nodeindex = id;
		if (_para.AOI_RADIUS < 0)
			_self.aoi.radius = (-1)*_para.AOI_RADIUS + (rand () % 3-1) * (rand () % _para.AOI_RADIUS);

		else
			_self.aoi.radius = _para.AOI_RADIUS;

		// obtain initial position
		_steps = 0;
		Position *pos = _move_model->getPos (_nodeindex-1, _steps);
		_self.aoi.center = *pos;

		// note there's no need to assign the gateway ID as it'll be found automatically
		_netpara.gateway    = Addr (0, IPaddr ("127.0.0.1", 3737));        
		_netpara.is_gateway = (_nodeindex == 1);
		_netpara.model      = (VAST_NetModel)_para.NET_MODEL;
		_netpara.port       = 3737;
		{
			//_netpara.phys_coord = _self.aoi.center;     // use our virtual coord as physical coordinate
			_netpara.phys_coord = Position();

		}
		

		_simpara.conn_limit = _para.CONNECT_LIMIT;
		_simpara.fail_rate  = _para.FAIL_RATE;
		_simpara.loss_rate  = _para.LOSS_RATE;
		_simpara.recv_quota = _para.DOWNLOAD_LIMIT;
		_simpara.send_quota = _para.UPLOAD_LIMIT;
		_simpara.send_quota = _para.STEPS_PERSEC;
		_world = new VASTVerse (&_netpara, &_simpara);

		state = IDLE;

		_last_recv = _last_send = 0;        
		clear_variables ();
	}

    SimNode::~SimNode ()
    {   
		if (vnode != NULL && state != IDLE)
		{
			vnode->leave ();
		}		        
        _world->destroyClient (vnode);
        delete _world;
    }

    void SimNode::clear_variables ()
    {
        _steps_recorded = _seconds_recorded = 0;
        _min_aoi = _para.WORLD_WIDTH;
        _total_aoi = 0;

        _max_CN = _total_CN = 0;
                
        max_send_persec = max_recv_persec = 0;
        _total_send = _total_recv = 0;
    }	

    void SimNode::move ()
    {        
        if (state == NORMAL)
        {
            _steps++;
            Position* pos = _move_model->getPos (getBehId(), _steps);
			if (pos == NULL || *pos == Position(0,0,-1))
			{
				printf ("failing [%d]..\n", get_id());
				fail();
				return;
			}

			_self.aoi.center = *pos;
            if (_para.CONNECT_LIMIT > 0)
                adjust_AOI ();

            // set the new position (NOTE: aoi could change)
            vnode->move (_sub_no, _self.aoi);

            //
            // send a test message to a random neighbor
            //
            /*
            if (_neighbors.size () == 0)
                return;

            int i = rand () % _neighbors.size ();
            map<id_t, Node *>::iterator it = _neighbors.begin ();

            for (;i > 0; i--)
                it++;

            id_t neighbor = it->first;
            Message msg (99);
            msg.addTarget (neighbor);
            char buf[80];
            sprintf (buf, "hello from [%d]!\0", vnode->getSelf ()->id);
            
            msg.store (buf, strlen(buf), true);
            vnode->send (msg);
            */

            //
            // get messages from neighbors
            //
            Message *recvmsg;
            char inbuf[80];
            size_t size;
            while ((recvmsg = vnode->receive ()) != NULL)
            {
                size = recvmsg->extract (inbuf, 0);
                inbuf[size] = 0;
                printf ("[%d] got '%s'\n", vnode->getSelf ()->id, inbuf);
            }
        }
    }

    // stop this node 
    void SimNode::fail ()
    {
        if (state == NORMAL)
        {
            _world->pause ();
			vnode->leave ();
			clear_variables();
            state = FAILED;
        }
    }

    void SimNode::adjust_AOI ()
    {
        // NOTE: the following code is small but produces very interesting behaviors. :) 

        static int radius_delta = -2;

        // do a little AOI-radius adjustment
        //if ((_self.aoi.radius < (length_t)(_para.AOI_RADIUS * 0.6)) || (_self.aoi.radius > (length_t)_para.AOI_RADIUS))
        if ((_self.aoi.radius < 100) || (_self.aoi.radius > 200))
        {
            // change sign for multiplier
            radius_delta *= -1;
        }
        _self.aoi.radius += radius_delta;

        if (_self.aoi.radius < 5)    
            _self.aoi.radius = 5;
    }
    
    void SimNode::processmsg ()
    {        
        // check if we have properly joined the overlay, and should initiate
        // subscription
        if (state == IDLE)
        {
            if ((vnode = _world->createClient ()) != NULL)
            {
                // TODO: we should obtain a physical coordinate point for joining
                //       right now we just use the logical coord as the physical coordinate			   
               vnode->join (*_world->getTopology ()->getPhysicalCoordinate (), _as_relay);     
               // vnode->join (_self.aoi.center, _as_relay);           
                state = WAITING;
            }
        }

        else if (state == WAITING)
        {
            if (vnode->isJoined ())
            {
                // after the VASTnode has successfully joined the overlay, 
                // get assigned unique ID & initiate subscription
                
                state = NORMAL;
                _sub_no = vnode->subscribe (_self.aoi, LAYER_EVENT);
                
                // update my peer ID (equals subscription number)
                _self.id = _sub_no;
            }
        }

        // NOTE: we assume that message process will occur after some logical time progression
        //       that is, events are allowed some timesteps (e.g. 100ms) before they're being 
        //       processed and calculated for respective stats
        _world->tick ();
    }

    void SimNode::record_stat ()
    {
        // record keeping
        long aoi = _self.aoi.radius;
        _total_aoi += aoi;
        
        if (aoi < _min_aoi)
            _min_aoi = aoi;
        
        int CN = vnode->list ().size ();
		
        _total_CN += CN;
        if (_max_CN < CN)
            _max_CN = CN;

        // set up neighbor view
        _neighbors.clear ();
        vector<Node *> &nodes = vnode->list ();
        for (size_t i=1; i < nodes.size (); ++i)
            if (nodes[i]->id != 0)
                _neighbors[nodes[i]->id] = nodes[i];
            else
                printf ("zero ID\n");

        _steps_recorded++;
    }

    // record per node per second transmission 
    void SimNode::record_stat_persec ()
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

		_seconds_recorded++;
    }

    length_t SimNode::get_aoi ()
    {
        return _self.aoi.radius;
    }

    Position &SimNode::get_pos ()
    {
        return _self.aoi.center;
    }

    Vast::id_t SimNode::get_id ()
    {
        return _self.id;
    }
    
    inline size_t SimNode::accumulated_send () 
    {
        return _world->getSendSize ();
    }

    inline size_t SimNode::accumulated_recv () 
    {        
        return _world->getReceiveSize ();
    }

	long SimNode::min_aoi ()
    {
        return _min_aoi;
    }

    float SimNode::avg_aoi ()
    {
        return (float)_total_aoi / (float)_steps_recorded;
    }

    int 
    SimNode::max_CN ()
    {
        return _max_CN;
    }

    float 
    SimNode::avg_CN ()
    {
        return (float)_total_CN / (float)_steps_recorded;
    }

    float 
    SimNode::avg_send ()
    {
        return (float)_total_send / (float)_seconds_recorded;
    }

    float 
    SimNode::avg_recv ()
    {
        return (float)_total_recv / (float)_seconds_recorded;
    }    

	// distance to a point
    bool 
    SimNode::in_view (SimNode *remote_node)
    {
        return (_self.aoi.center.distance (remote_node->get_pos()) < (double)_self.aoi.radius);
    }
    
    // returns the Node pointer if known, otherwise returns NULL
    // TODO:
    Node *
    SimNode::knows (SimNode *node)
    {
        id_t id = node->get_id ();
        
        // see if 'node' is a known neighbor of me
        if (_neighbors.find (id) != _neighbors.end ())
            return _neighbors[id];
        else
            return NULL;
    }

    // whether I've joined successfully
    bool 
    SimNode::isJoined ()
    {
        return (state == NORMAL);
    }

    // whether I'm a failed node
    bool 
    SimNode::isFailed ()
    {
        return (state == FAILED);
    }

    // return the ID for this Peer
    id_t 
    SimNode::getPeerID ()
    {
        return _sub_no;
    }

	void SimNode::setBehId( const id_t bid )
	{
		_beh_id = bid;
	}

	inline Vast::id_t SimNode::getBehId()
	{
		return _beh_id;
	}

	void SimNode::setJoinStep( const int s )
	{
		_join_step = s;
	}

	inline int SimNode::getJoinStep()
	{
		return _join_step;
	}
