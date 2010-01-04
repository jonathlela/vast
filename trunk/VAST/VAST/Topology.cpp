


#include "Topology.h"

using namespace Vast;

namespace Vast
{   

    // constructor for physical topology class, may supply an artifical physical coordinate
    Topology::Topology (id_t host_id, Position *phys_coord)
            :MessageHandler (MSG_GROUP_TOPOLOGY), _counter (0)
    {        
        _host_id = host_id;		        

        // create randomized temp coord for coordinate estimation
        _temp_coord.x = (coord_t)((rand () % 10) + 1);
        _temp_coord.y = (coord_t)((rand () % 10) + 1);

        // if it's the gateway then it's default position is at origin
        if (_host_id == NET_ID_GATEWAY)
        {
            _phys_coord = new Position (0, 0, 0);
            _temp_coord = Position (0, 0, 0);
        }
        // store default physical coordinate, if exist
        else if (phys_coord != NULL)
            _phys_coord = new Position (*phys_coord);
        else
            _phys_coord = NULL;
        
        // default error value is 1.0f
        _error = TOPOLOGY_DEFAULT_ERROR;

        _request_times = 0;
    }

    Topology::~Topology ()
	{
        if (_phys_coord != NULL)
            delete _phys_coord;
	}

    // obtain my physical coordinate
    Position *
    Topology::getPhysicalCoordinate ()
    {   
        // if physical coordinate doesn't exist, need to query & estimate one
        //if (_phys_coord == NULL && _counter-- <= 0)
        if (_phys_coord == NULL)
        {
            // send query to gateway if we know no neighbors
            if (_neighbors.size () == 0)
            {
                Node gateway;

                gateway.id = NET_ID_GATEWAY;
                gateway.addr = getAddress (NET_ID_GATEWAY);

                _neighbors[gateway.id] = gateway;
            }
                           
            // re-send PING to all known neighbors
            vector<id_t> failed_landmarks;
            map<id_t, Node>::iterator it = _neighbors.begin ();
            for (; it != _neighbors.end (); it++)
                if (ping (it->first) == false)
                    failed_landmarks.push_back (it->first);
            
            // reset re-try countdown
            //_counter = COORD_QUERY_RETRY_COUNTDOWN;

            // notify gateway of failed landmarks
            listsize_t n = (listsize_t)failed_landmarks.size ();
            if (n > 0)
            {
                Message msg (BADLANDMARK);
                msg.priority = 1;
                           
                msg.store (n);
                for (size_t i=0; i < n; i++)
                {
                    msg.store (failed_landmarks[i]);
                    _neighbors.erase (failed_landmarks[i]);
                    printf ("(%ld) ", failed_landmarks[i]);
                }

                msg.addTarget (NET_ID_GATEWAY);
                sendMessage (msg);

                printf ("[%ld] notify gateway of %d failed landmarks\n", _host_id, n);
            }
        }

        return _phys_coord;
    }

    // send a message to a remote host in order to obtain round-trip time
    bool 
    Topology::ping (id_t target)
    {
        // check if redundent PING has been sent
        if (_pending.find (target) != _pending.end ())
            return true;

        Message msg (PING);
        msg.priority = 1;

        timestamp_t current_time = _net->getTimestamp ();
        msg.store (current_time);        

        msg.addTarget (target);

        vector<id_t> failed;
        sendMessage (msg, &failed);

        if (failed.size () > 0)            
            return false;
            
        _request_times++;
        _pending[target] = true;

#ifdef DEBUG_DETAIL
        printf ("Topology: [%ld] pinging [%ld]..\n", _host_id, target);
#endif

        return true;
    }

    // send a message to a remote host in order to obtain round-trip time
    bool 
    Topology::pong (id_t target, timestamp_t querytime, bool first)
    {
        // send back the time & my own coordinate, error
        Message msg (PONG);
        msg.priority = 1;

        msg.store (querytime);
        msg.store (_temp_coord);
        msg.store (_error);
        msg.addTarget (target);
                   
        // also request a response if it's responding to a PING
        if (first == true)
        {
            timestamp_t current_time = _net->getTimestamp ();
            msg.store (current_time);       
        }
        else
        {
            msg.msgtype = PONG_2;

            // remove record of sending a PING
            _pending.erase (target);
        }

        sendMessage (msg);

        return true;
    }

    // perform initialization tasks for this handler (optional)
    // NOTE that all internal variables (such as handler_no) have been set at this point
    void 
    Topology::initHandler ()
    {
		// TODO: should send query to locate physical coordinate here
    }

    // returns whether the message was successfully handled
    bool 
    Topology::handleMessage (Message &in_msg)
    {
        switch (in_msg.msgtype)
        {
        case LOCATE:
            break;

        case COORDINATE:
            break;

        case PING:
            {
                // extract the time & coordinate
                timestamp_t senttime;
                in_msg.extract (senttime);

                // respond the PING message
                pong (in_msg.from, senttime, true);

                // send back a list of known landmarks
                listsize_t n = (listsize_t)_neighbors.size ();

                if (n > 0)
                {
                    // max out at 5 landmarks
                    if (n > 5)
                        n = 5;

                    Message msg (LANDMARK);
                    msg.priority = 1;
                    msg.store (n);
                
                    // select some neighbors
                    // TODO: randomize if more than needed
                    map<id_t, Node>::iterator it = _neighbors.begin ();
                    for (; it != _neighbors.end (); it++)
                    {
                        printf ("(%ld) ", it->second.id);
                        msg.store (it->second);   
                    }
                    printf ("\n[%ld] Topology responds with %d landmarks\n", _host_id, n);
                
                    msg.addTarget (in_msg.from);                
                    sendMessage (msg);
                }
            }
            break;

        case PONG:
        case PONG_2:
            {
                // tolerance for error to determine stabilization
                static float tolerance = TOPOLOGY_TOLERANCE;
                                                   
                timestamp_t querytime;
                Position    xj;             // remote node's physical coord
                float       ej;             // remote nodes' error value                
                in_msg.extract (querytime);
                in_msg.extract (xj);
                in_msg.extract (ej);

                // calculate RTT
                timestamp_t current  = _net->getTimestamp ();
                float rtt = (float)(current - querytime);
                if (rtt == 0)
                {
                    printf ("[%ld] processing PONG: RTT = 0 error, removing neighbor [%ld] currtime: %lu querytime: %lu\n", _host_id, in_msg.from, current, querytime);
                    _neighbors.erase (in_msg.from);
                    break;
                }
                
                // call the Vivaldi algorithm to update my physical coordinate
                vivaldi (rtt, _temp_coord, xj, _error, ej);

#ifdef DEBUG_DETAIL
                printf ("[%ld] physcoord (%.3f, %.3f) rtt to [%ld]: %.3f error: %.3f requests: %d\n", 
                         _host_id, _temp_coord.x, _temp_coord.y, in_msg.from, rtt, _error, _request_times);
#endif

                // check if the local error value is small enough, 
                // if so we've got our physical coordinate
                if (_error < tolerance)
                {
                    // print a small message to show it
                    if (_request_times > 0)
                        printf ("[%ld] physcoord (%.3f, %.3f) rtt to [%ld]: %.3f error: %.3f requests: %d\n", 
                                _host_id, _temp_coord.x, _temp_coord.y, in_msg.from, rtt, _error, _request_times);

                    // update into actual physical coordinate
                    if (_phys_coord == NULL)
                        _phys_coord = new Position (_temp_coord);
                    else
                        *_phys_coord = _temp_coord;

                    // register with server if I'm public & I'm an arbitrator
                    if (isGateway (_host_id) == false && 
                        _net->isPublic () && _host_id % 2 == 1)
                    {
                        Message msg (REGISTER);
                        msg.priority = 1;

                        Node self;
                        self.id = _host_id;
                        self.addr = getAddress (self.id);
                        msg.store (self);

                        msg.addTarget (NET_ID_GATEWAY);
                        sendMessage (msg);
                    }
                }

                // send back PONG_2 if I'm an initial requester
                if (in_msg.msgtype == PONG)
                {
                    timestamp_t senttime;
                    in_msg.extract (senttime);
                    pong (in_msg.from, senttime);
                }                               
            }
            break;

        case LANDMARK:
            {
                listsize_t n;
                in_msg.extract (n);

                Node neighbor;

                // extract each neighbor and store them
                for (int i=0; i < n; i++)
                {                    
                    in_msg.extract (neighbor);

                    // check if it's not host on same node
                    if (neighbor.id != (_host_id-1))
                    {
                        // store ID to address mapping
                        notifyMapping (neighbor.id, &neighbor.addr);

                        _neighbors[neighbor.id] = neighbor;
                    }
                }
            }
            break;

        case BADLANDMARK:
            if (_host_id == NET_ID_GATEWAY)
            {
                listsize_t n;
                in_msg.extract (n);

                id_t id;
                if (n > 0)
                {
                    // remove each reported bad landmark
                    // TODO: check validity of the reporting first
                    for (size_t i=0; i < n; i++)
                    {
                        in_msg.extract (id);
                        _neighbors.erase (id);
                    }
                }
            }
            break;

        // a remote node tries to register as a potential landmark
        case REGISTER:
            if (_host_id == NET_ID_GATEWAY)
            {
                Node neighbor;
                in_msg.extract (neighbor);

                _neighbors[neighbor.id] = neighbor;
            }
            break;

        case DISCONNECT:
            {
                // if a known landmark leaves, remove it
                if (_neighbors.find (in_msg.from) != _neighbors.end ())
                {
                    printf ("[%ld] Topology::handleMessage () removes disconnected landmark [%ld]\n", _host_id, in_msg.from);
                    _neighbors.erase (in_msg.from);
                }
            }
            break;

        default:
            return false;
        }

        return true;
    }

    // performs some tasks the need to be done after all messages are handled
    // such as neighbor discovery checks
    void 
    Topology::postHandling ()
    {

    }

    // recalculate my physical coordinate estimation (using Vivaldi)
    // given a neighbor j's position xj & error estimate ej
    void 
    Topology::vivaldi (float rtt, Position &xi, Position &xj, float &ei, float &ej)   
    {
        // constant error
        static float C_e = TOPOLOGY_CONSTANT_ERROR;

        // constant fraction
        static float C_c = TOPOLOGY_CONSTANT_FRACTION;

		// weight = err_i / (err_i + err_j)
		float weight;
        if (ei + ej == 0)
            weight = 0;
        else
            weight = ei / (ei + ej);
       
        coord_t dist = xi.distance (xj);

        // compute relative error
		// e_s = | || phy_coord(i) - phy_coord(j) || - rtt | / rtt
		float relative_error = fabs (dist - rtt) / rtt;

		// update weighted moving average of local error
		ei = relative_error * C_e * weight + ei * (1 - C_e * weight);

		// update local coordinates using delta time * force
		float dtime = C_c * weight;
        Position force = xi - xj;
        force.makeUnit ();
		
		xi += ((force * (rtt - dist)) * dtime);        
    }

    /*
	Vivaldi *Topology::getVivaldi()
	{
		return _vivaldi;
	}
	
    // TODO: should send query to locate physical coordinate here
    // Simulation only
	void Topology::setVivaldi (Vivaldi *val)
	{		
		_vivaldi = val;		

		if (_vivaldi != NULL)
		{
#ifdef ENABLE_TOPOLOGY_AWARE
            if (_phys_coord != NULL)
                delete _phys_coord;

            _phys_coord = new Position (_vivaldi->get_phys_coord (_host_id));
#endif
        }
	}
    */

} // end namespace Vast
