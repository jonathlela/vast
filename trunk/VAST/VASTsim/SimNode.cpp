

/*
 *  SimNode.cpp -- basic node entity used for simulations with bookkeeping capability
 *
 */


#include "SimNode.h"

    
    SimNode::SimNode (int id, MovementGenerator *move_model, Addr &gateway, SimPara &para, VASTPara_Net &netpara, bool as_relay)
        :_move_model (move_model), _gateway (gateway), _para (para), _netpara (netpara), _as_relay (as_relay)
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

        _netpara.phys_coord   = _self.aoi.center;     // use our virtual coord as physical coordinate

        _simpara.fail_rate    = _para.FAIL_RATE;
        _simpara.loss_rate    = _para.LOSS_RATE;

        // create fake entry points
        vector<IPaddr> entries;
                
        // if not gateway, store some IPs to home
        if (id != 1)
            entries.push_back (_gateway.publicIP);
        
        _world = new VASTVerse (entries, &_netpara, &_simpara);
        _world->createVASTNode (_gateway.publicIP, _self.aoi, LAYER_UPDATE);

        state = WAITING;

        _last_recv = _last_send = 0;        
        clear_variables ();
    }
    
    SimNode::~SimNode ()
    {   
        if (vnode != NULL)
            vnode->leave ();
        _world->destroyVASTNode (vnode);
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
            Position *pos = _move_model->getPos (_nodeindex-1, _steps);

            if (pos == NULL)
                return;

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
                //printf ("[%d] got '%s'\n", vnode->getSelf ()->id, inbuf);
            }
        }
    }

    // stop this node 
    void SimNode::fail ()
    {
        if (state == NORMAL)
        {
            vnode->leave ();
            _world->pause ();
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
        // check if we have properly joined the overlay and subscribed
        if (state == WAITING)
        {            
            if ((vnode = _world->getVASTNode ()) != NULL)
            {
                _sub_no = vnode->getSubscriptionID ();
                _self.id = _sub_no;
                state = NORMAL;
            }
        }

        // NOTE: we assume that message process will occur after some logical time progression
        //       that is, events are allowed some timesteps (e.g. 100ms) before they're being 
        //       processed and calculated for respective stats
        _world->tick ();
    }

    void SimNode::record_stat ()
    {
        if (vnode == NULL)
            return;

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
        Vast::id_t id = node->get_id ();
        
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
    Vast::id_t 
    SimNode::getPeerID ()
    {
        return _sub_no;
    }

    // get the Voronoi from the matcher of this node
    Voronoi *
    SimNode::getVoronoi ()
    {
        if (_world != NULL)
            return _world->getMatcherVoronoi ();
        else
            return NULL;
    }

