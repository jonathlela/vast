

/*
 *  SimPeer.cpp -- basic node entity used for simulations with bookkeeping capability
 *

    Steps for joining:
        1. create VASTATE "_factory"            (constructor)
        2. new AgentLogic and ArbitratorLogic   (constructor)
        3. create Arbitrator                    (isJoined () state = IDLE)
        4. create Agent                         (isJoined () state = IDLE)
        5. login Agent                          (isJoined () state = IDLE)
        6. join Arbitrator                      (isJoined () state = INIT)    
        7. join Agent                           (isJoined () state = INIT)
        8. both Arbitrator and Agent joined     (isJoined () state = JOINED)

 */

#include "SimPeer.h"
#include "SimAgent.h"
#include "SimArbitrator.h"
    
    SimPeer::SimPeer (int id, MovementGenerator *move_model, SimPara &para, bool as_relay)
        :_move_model (move_model), _para (para)
    {
        vnode = NULL;
        _agent = NULL;
        _arbitrator = NULL;

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
        IPaddr ip ("127.0.0.1", GATEWAY_DEFAULT_PORT);
        Addr addr (0, ip);
        _netpara.gateway     = addr;        
        _netpara.is_gateway  = (_nodeindex == 1);
        _netpara.model       = (VAST_NetModel)_para.NET_MODEL;
        _netpara.port        = GATEWAY_DEFAULT_PORT;      
        _netpara.phys_coord  = _self.aoi.center;         // use virtual coord as temp physical coord
        _netpara.is_relay    = as_relay;
        _netpara.peer_limit  = _para.PEER_LIMIT;
        _netpara.relay_limit = _para.RELAY_LIMIT;    

        _netpara.conn_limit  = _para.CONNECT_LIMIT;
        _netpara.recv_quota  = _para.DOWNLOAD_LIMIT;
        _netpara.send_quota  = _para.UPLOAD_LIMIT;
        _netpara.step_persec = _para.STEPS_PERSEC;

        _simpara.fail_rate   = _para.FAIL_RATE;
        _simpara.loss_rate   = _para.LOSS_RATE;

        VASTATEPara vpara;        

        vpara.default_aoi    = _para.AOI_RADIUS;
        vpara.world_height   = _para.WORLD_HEIGHT;
        vpara.world_width    = _para.WORLD_WIDTH;
        vpara.overload_limit = _para.OVERLOAD_LIMIT;
        
        _factory = new VASTATE (vpara, _netpara, &_simpara);

        // create logic classes
        _agent_logic = new SimAgent ();
        _arb_logic = new SimArbitrator ();

        state = IDLE;

        _last_recv = _last_send = 0;        
        clear_variables ();
    }
    
    SimPeer::~SimPeer ()
    {   
        if (vnode != NULL)
            vnode->leave ();

        if (_factory != NULL)
        {
            _factory->destroyNode ();                
            delete _factory;
        }
        if (_agent_logic != NULL)
            delete _agent_logic;
        if (_arb_logic != NULL)
            delete _arb_logic;
    }

    void SimPeer::clear_variables ()
    {
        _steps_recorded = _seconds_recorded = 0;
        _min_aoi = _para.WORLD_WIDTH;
        _total_aoi = 0;

        _max_CN = _total_CN = 0;
                
        max_send_persec = max_recv_persec = 0;
        _total_send = _total_recv = 0;
    }
    
    void SimPeer::move ()
    {        
        if (state == NORMAL)
        {
            _steps++;

            Position old_pos = _self.aoi.center;
            _self.aoi.center = *_move_model->getPos (_nodeindex-1, _steps);
        
            if (_para.CONNECT_LIMIT > 0)
                adjust_AOI ();

            //
            // send the new position (NOTE: aoi could change)
            // (here we test both event-based and direct command)
            //

            // option 1:
            // Event-based, use SimPeerAction's number to denote movement
            /*
            SimPeerAction action = MOVE;
            Event *e = _agent->createEvent (action);
            e->add (_self.aoi.center);
            _agent->act (e);            
            */
            
            // option 2:
            // direct command

            // if new position is too far from old, indicates a teleport
            if (old_pos.distance (_self.aoi.center) > _self.aoi.radius)
            {
                // join at new location
                _agent->leave ();
                _agent->join (_self.aoi.center);
            }
            // regular move
            else
                _agent->move (_self.aoi.center);

            /*
            //
            // send a test message to a random neighbor
            //
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
            */

            //
            // test object creation
            //

            /*
            // we will try to create a few objects here and there
            if (_netpara.is_gateway)
            {
                Area area;
                area.radius = _para.WORLD_WIDTH;
                area.height = _para.WORLD_HEIGHT;

                SimPeer::createRandomObjects (_arbitrator, 100, 1000, area);
            }
            */
        }
    }

    // stop this node 
    void SimPeer::fail ()
    {
        if (state == NORMAL)
        {
            vnode->leave ();

            // disconnect arbitrator & agent's network
            _factory->pause ();

            state = FAILED;
        }
    }

    void SimPeer::adjust_AOI ()
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
    
    void SimPeer::processmsg ()
    {        
        // NOTE: we assume that message process will occur after some logical time progression
        //       that is, events are allowed some timesteps (e.g. 100ms) before they're being 
        //       processed and calculated for respective stats

        // NOTE: that in normal operations, _arbitrator->createObject () may 
        //       be called to create certain game objects

        // move VASTATE forward one time-step
        _factory->tick (0);
    }

    void SimPeer::record_stat ()
    {
        // record keeping
        long aoi = _self.aoi.radius;
        _total_aoi += aoi;
        
        if (aoi < _min_aoi)
            _min_aoi = aoi;
        
        int CN = _agent_logic->getNeighbors ().size ();
		
        _total_CN += CN;
        if (_max_CN < CN)
            _max_CN = CN;

        // set up neighbor view
        _neighbors.clear ();
        vector<Node *> &nodes = _agent_logic->getNeighbors ();
        for (size_t i=1; i < nodes.size (); ++i)
            if (nodes[i]->id != 0)
                _neighbors[nodes[i]->id] = nodes[i];
            else
                printf ("zero ID\n");

        _steps_recorded++;
    }

    // record per node per second transmission 
    void SimPeer::record_stat_persec ()
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

    length_t SimPeer::get_aoi ()
    {
        return _self.aoi.radius;
    }

    Position &SimPeer::get_pos ()
    {
        return _self.aoi.center;
    }

    Vast::id_t SimPeer::get_id ()
    {
        return _self.id;
    }
    
    inline size_t SimPeer::accumulated_send () 
    {
        return _factory->getSendSize ();
        //return 0;
    }

    inline size_t SimPeer::accumulated_recv () 
    {        
        return _factory->getReceiveSize ();
        //return 0;
    }

	long SimPeer::min_aoi ()
    {
        return _min_aoi;
    }

    float SimPeer::avg_aoi ()
    {
        return (float)_total_aoi / (float)_steps_recorded;
    }

    int 
    SimPeer::max_CN ()
    {
        return _max_CN;
    }

    float 
    SimPeer::avg_CN ()
    {
        return (float)_total_CN / (float)_steps_recorded;
    }

    float 
    SimPeer::avg_send ()
    {
        return (float)_total_send / (float)_seconds_recorded;
    }

    float 
    SimPeer::avg_recv ()
    {
        return (float)_total_recv / (float)_seconds_recorded;
    }    

	// distance to a point
    bool 
    SimPeer::in_view (SimPeer *remote_node)
    {
        return (_self.aoi.center.distance (remote_node->get_pos()) < (double)_self.aoi.radius);
    }
    
    // returns the Node pointer if known, otherwise returns NULL
    // TODO:
    Node *
    SimPeer::knows (SimPeer *node)
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
    SimPeer::isJoined ()
    {
        // create the VASTATE node from factory, note that both arbitrator & agents are created at once
        if (state == IDLE)
        {
            string password ("abc\0");


            // case 1: only gateway is added as first arbitrator, but others are potential arbitrators
            _factory->createNode (_self.aoi, _arb_logic, _agent_logic, password, (_nodeindex == 1 ? &_self.aoi.center : NULL));            
            
            /*
            // case 2: any node that is relay is added as arbitrator, other nodes are strictly agents
            if (_netpara.is_relay)
                _factory->createNode (_self.aoi, _arb_logic, _agent_logic, password, &_self.aoi.center);
            else
                _factory->createNode (_self.aoi, NULL, _agent_logic, password, NULL);                        
            */

            state = WAITING;
        }

        else if (state == WAITING && _factory->isLogined ())
        {
            _agent = _factory->getAgent ();
            _arbitrator = _factory->getArbitrator ();

            // get a reference to the VAST node
            vnode = _agent->getVAST ();

            // update my peer ID
            _self.id = _agent->getSelf ()->id;
            //_agent_logic->setSelf (&_self);

            state = NORMAL;
        }

        return (state == NORMAL);
    }

    // whether I've failed
    bool 
    SimPeer::isFailed ()
    {
        return state == FAILED;
    }

    Node *
    SimPeer::getSelf ()
    {   
        if (_agent == NULL)
            return NULL;

        return _agent->getSelf ();
    }

    vector<Node *> &
    SimPeer::getNeighbors ()
    {
        return _agent_logic->getNeighbors ();
    }

    vector<line2d> *
    SimPeer::getArbitratorBoundaries ()
    {
        if (_arbitrator != NULL)
            return _arbitrator->getEdges ();
        else
            return NULL;
    }

    // get the boundary box for the arbitrator's Voronoi diagram
    bool 
    SimPeer::getBoundingBox (point2d& min, point2d& max)
    {
        if (_arbitrator != NULL)
            return _arbitrator->getBoundingBox (min, max);
        else
            return false;
    }

    void 
    SimPeer::createRandomObjects (Arbitrator *arb, int num_objects, int num_attributes, Area area)
    {
        size_t total_size = 0;
        float array[100];
        string float_array ((char *)array, 100 * sizeof(float));

        // create 100 random objects
        int i, j=0;
        for (i=0; i < num_objects; i++)
        {
            Position pos ((coord_t)(rand () % area.radius), (coord_t)(rand () % area.height));
            Object *obj = arb->createObject (2, pos);

            // insert attributes                   
            for (j=0; j < num_attributes; j++)
            {
                switch ((rand () % 5)+1)
                {
                case VASTATE_ATTRIBUTE_TYPE_BOOL:
                    obj->add (true);
                    break;

                case VASTATE_ATTRIBUTE_TYPE_INT:
                    obj->add (99);
                    break;

                case VASTATE_ATTRIBUTE_TYPE_FLOAT:
                    obj->add (3.7f);
                    break;

                case VASTATE_ATTRIBUTE_TYPE_STRING:
                    //obj->add (string ("hello world!! this is great!! :)"));
                    obj->add (float_array);
                    break;

                case VASTATE_ATTRIBUTE_TYPE_VEC3:
                    obj->add (Position (100, 200));
                    break;                            
                }                        
            }
            //printf ("objsize: %d\n", obj->pack (NULL));
            total_size += obj->pack (NULL);
        }

        printf ("%d obj created with %d attributes, size avg: %u, total: %u\n", i, j, total_size / i, total_size);
    }
