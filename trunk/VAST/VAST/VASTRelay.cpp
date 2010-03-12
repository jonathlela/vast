


#include "VASTRelay.h"
#include "MessageQueue.h"


using namespace Vast;

namespace Vast
{   

    // constructor for physical topology class, may supply an artifical physical coordinate
    VASTRelay::VASTRelay (size_t client_limit, size_t relay_limit, Position *phys_coord)
            :MessageHandler (MSG_GROUP_VAST_RELAY), 
             _curr_relay (NULL),
             _state (ABSENT),
             _timeout_query (0),
             _timeout_ping (0),
             _timeout_join (0)
    {                     

        _client_limit = client_limit;     // if 0 means unlimited
        _relay_limit  = relay_limit;      // if 0 means unlimited

        // create randomized temp coord for coordinate estimation
        _temp_coord.x = (coord_t)((rand () % 1000) + 1);
        _temp_coord.y = (coord_t)((rand () % 1000) + 1);

        // store default physical coordinate, if exist
        if (phys_coord != NULL)
            _temp_coord = _self.aoi.center = *phys_coord;
        
        // default error value is 1.0f
        _error = RELAY_DEFAULT_ERROR;

        _request_times = 0;
    }

    VASTRelay::~VASTRelay ()
	{
	}

    // obtain my physical coordinate
    Position *
    VASTRelay::getPhysicalCoordinate ()
    {   
        // if self coordinate equals determined coord
        if (_self.aoi.center == _temp_coord)
            return &_self.aoi.center;
        else
            return NULL;
    }

    // send a message to a remote host in order to obtain round-trip time
    bool 
    VASTRelay::ping ()
    {
        // prepare PING message
        Message msg (PING);
        msg.priority = 1;

        timestamp_t current_time = _net->getTimestamp ();
        msg.store (current_time);

        // set how many we send PING each time
        int ping_num = (_relays.size () > MAX_CONCURRENT_PING ? MAX_CONCURRENT_PING : _relays.size ());

        // obtain list of targets (checking if redundent PING has been sent)
        vector<bool> random_set;
        randomSet (ping_num, _relays.size (), random_set);

        // TODO: should we keep PINGing the same set of hosts, or different?
        //       which converges faster?
        map<id_t, Node>::iterator it = _relays.begin ();
        int i=0;
        for (; it != _relays.end (); it++)
        {
            // skip relays not chosen this time
            // NOTE: actual contact relays may be less than desired, if the target is already contacted
            if (random_set[i++] == false)
                continue;

            // check for redundency
            id_t target = it->first;
            if (_pending.find (target) == _pending.end ())
            {
                msg.addTarget (target);
                _pending[target] = true;                
            }
        }

        // send out messages
        vector<id_t> failed;

        if (msg.targets.size () > 0)
        {
            sendMessage (msg, &failed);
                
            // remove failed relays
            for (size_t i=0; i < failed.size (); i++)
            {
                printf ("removing failed relay (%lld) ", failed[i]);
                removeRelay (failed[i]);
                //_relays.erase (failed[i]);                       
            }
        }
                        
        // if no PING is sent
        if ((msg.targets.size () - failed.size ()) == 0)
        {
            // error if cannot contact any relays
            if (_net->getEntries ().size () != 0)                        
                printf ("VASTRelay::ping () no known relays to contact\n cannot determine physical coordinate, check if relays are valid\n");
            return false;
        }
        else
        {
            // success means at least a few PING requests are sent
            _request_times++;
            return true;
        }
    }

    // send a message to a remote host in order to obtain round-trip time
    bool 
    VASTRelay::pong (id_t target, timestamp_t querytime, bool first)
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

    // whether the current node is joined (find & connect to closest relay)
    // we need to:
    //      1. obtain physical coordinate
    //      2. greedy-find the closest relay and connect to it
    bool 
    VASTRelay::isJoined ()
    {
        if (getPhysicalCoordinate () == NULL || _curr_relay == NULL)
            return false;

        return (_state == JOINED);
    }

    // obtain the ID of my relay node
    id_t 
    VASTRelay::getRelayID ()
    {
        if (_curr_relay == NULL)
            return NET_ID_UNASSIGNED;
        else
            return _curr_relay->id;
    }

    // perform initialization tasks for this handler (optional)
    // NOTE that all internal variables (such as handler_no) have been set at this point
    void 
    VASTRelay::initHandler ()
    {
        _self.id    = _net->getHostID ();
        _self.addr  = _net->getHostAddress ();
                
        // register self mapping
        notifyMapping (_self.id, &_self.addr);
    }

    // returns whether the message was successfully handled
    bool 
    VASTRelay::handleMessage (Message &in_msg)
    {
        switch (in_msg.msgtype)
        {

        case REQUEST:
            {
                // send back a list of known relays
                sendRelays (in_msg.from, MAX_CONCURRENT_PING);
            }
            break;

        case PING:
            {
                // extract the time & coordinate
                timestamp_t senttime;
                in_msg.extract (senttime);

                // respond the PING message
                pong (in_msg.from, senttime, true);
            }
            break;

        case PONG:
        case PONG_2:
            {                
                // tolerance for error to determine stabilization
                static float tolerance = RELAY_TOLERANCE;
                                                   
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
                    printf ("[%ld] processing PONG: RTT = 0 error, removing neighbor [%ld] currtime: %lu querytime: %lu\n", _self.id, in_msg.from, current, querytime);
                    removeRelay (in_msg.from);
                    //_relays.erase (in_msg.from);
                    break;
                }
                
                // call the Vivaldi algorithm to update my physical coordinate
                vivaldi (rtt, _temp_coord, xj, _error, ej);

//#ifdef DEBUG_DETAIL
                printf ("[%llu] physcoord (%.3f, %.3f) rtt to [%llu]: %.3f error: %.3f requests: %d\n", 
                         _self.id, _temp_coord.x, _temp_coord.y, in_msg.from, rtt, _error, _request_times);
//#endif

                // if remote host is a relay, record its coordinates                
                if (_relays.find (in_msg.from) != _relays.end ())
                    _relays[in_msg.from].aoi.center = xj;

                // check if the local error value is small enough, 
                // if so we've got our physical coordinate
                if (_error < tolerance)
                {
                    // print a small message to show it
                    if (_request_times > 0)
                    {
                        printf ("[%llu] physcoord (%.3f, %.3f) rtt to [%llu]: %.3f error: %.3f requests: %d\n", 
                                _self.id, _temp_coord.x, _temp_coord.y, in_msg.from, rtt, _error, _request_times);

                        // reset
                        _request_times = 0;
                    }

                    // if we've modified our coordinate and is a working relay
                    // should let neighbors to know
                    if (_self.aoi.center != _temp_coord && _net->isPublic () && _relays.size () > 0)
                    {                        
                        // notify existing known relays of myself as a new relay
                        Message msg (RELAY);
                        msg.priority = 1;
                        listsize_t n = 1;
                        msg.store (n);
                        msg.store (_self);

                        // obtain list of targets (check if redundent PING has been sent)
                        map<id_t, Node>::iterator it = _relays.begin ();

                        for (;it != _relays.end (); it++)
                            msg.addTarget (it->first);

                        sendMessage (msg);
                    }

                    // update into actual physical coordinate
                    _self.aoi.center = _temp_coord;
                }
                
                // if we've not yet determined a small error, should keep sending ping requests
                else
                {
                    _timeout_ping = 0;
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

        case RELAY:
            {
                listsize_t n;
                in_msg.extract (n);

                Node relay;

                // extract each neighbor and store them
                for (int i=0; i < n; i++)
                {                    
                    in_msg.extract (relay);

                    // check if it's not from differnet nodes from the same host 
                    if (VASTnet::extractHost (relay.id) != VASTnet::extractHost (_self.id))
                    {
                        // add a new relay (while storing ID->address mapping)
                        addRelay (relay);
                    }
                }
            }
            break;

        case RELAY_QUERY:
            {
                Node joiner;
                Addr relay;
                in_msg.extract (joiner);
                in_msg.extract (relay);

                bool success = false;
                while (!success)
                {
                    // find the closet relay
                    Node *closest = closestRelay (joiner.aoi.center);
                
                    // respond the query if I'm closest 
                    if (closest->id == _self.id)
                    {
                        Message msg (RELAY_QUERY_R);
                        msg.priority = 1;
                        msg.store (joiner);
                        msg.store (*closest);
                        msg.addTarget (relay.host_id);
                                        
                        notifyMapping (relay.host_id, &relay);
                        sendMessage (msg);
                        success = true;
                    }
                    // otherwise greedy-forward the message to the next closest, valid relay
                    else
                    {
                        in_msg.targets.clear ();
                        in_msg.addTarget (closest->id);
                        success = (sendMessage (in_msg) > 0);

                        // remove the invalid relay, this occurs if the neighbor relay has failed
                        if (!success)
                        {
                            removeRelay (closest->id);
                            //_relays.erase (closest->id);
                        }
                    }          
                }
            }
            break;

        case RELAY_QUERY_R:
            {
                Node requester;
                Node relay;

                // extract requester
                in_msg.extract (requester);

                // extract the responding relay
                in_msg.extract (relay);

                // if I was the requester, record the relay
                if (requester.id == _self.id)
                {
                    addRelay (relay);

                    if (_net->isPublic () == false)
                        _curr_relay = &_relays[relay.id];
                    else
                        _curr_relay = &_self;

                    joinRelay (_curr_relay);
                }
                // if I was the forwarder, forward the response to the requester
                else
                {
                    in_msg.reset ();
                    in_msg.targets.clear ();
                    in_msg.addTarget (requester.id);

                    sendMessage (in_msg);
                }
            }
            break;

        // handles a JOIN request of a new node to the overlay, 
        // responds with a list of known relay neighbors
        case RELAY_JOIN:
            {
                Node joiner;
                bool as_relay;                
                in_msg.extract (joiner);
                in_msg.extract ((char *)&as_relay, sizeof (bool));
                
                // check if I can take in this new node, default is NO 
                // (because joiner is itself a relay, or no more peer space available)
                bool join_reply = false;

                if (as_relay == false && 
                    (_client_limit == 0 || _accepted.size () < _client_limit))
                {
                    join_reply = true;
                    
                    // record the clients this relay has accepted
                    _accepted[joiner.id] = joiner;
                }                                

                // compose return message
                Message msg (RELAY_JOIN_R);
                msg.priority = 1;
                msg.store ((char *)&join_reply, sizeof (bool));                                    
                msg.addTarget (in_msg.from);
                sendMessage (msg);

                // also send the joiner some of my known relays
                sendRelays (in_msg.from);
            }
            break;

        // response from previous JOIN request
        case RELAY_JOIN_R:
            {
                // only update if we're currently seeking to join
                if (_state == JOINED)
                    break;

                bool as_relay = _net->isPublic ();
                bool join_success;

                in_msg.extract ((char *)&join_success, sizeof (bool));

                // if a non-relay client fails to join, then find another relay to join
                if (!as_relay && join_success == false)
                {
                    joinRelay ();
                }
                // otherwise, we've successfully joined the relay mesh 
                else 
                {                   
                    // if I'm a relay, let each newly learned relay also know
                    if (as_relay)
                    {
                        Message msg (RELAY);
                        msg.priority = 1;
                        listsize_t n = 1;
                        msg.store (n);
                        msg.store (_self);
                        
                        map<id_t, Node>::iterator it = _relays.begin ();
                        
                        // notify up to _relay_limit
                        msg.targets.clear ();
                        for (size_t i=0; it != _relays.end () && (_relay_limit == 0 || i < _relay_limit); it++)
                            msg.addTarget (it->first);

                        sendMessage (msg);

                        // set myself as relay
                        setJoined ();
                    }
                    // set the sender as my relay
                    else
                        setJoined (in_msg.from);

                    /* TODO: Move to VASTClient
                    // re-subscribe current subscriptions
                    else if (_subscriptions.size () > 0)
                    {
                        // send out subscription request
                        Message msg (SUBSCRIBE);
                        msg.priority = 1;

                        map<id_t, Subscription>::iterator it = _subscriptions.begin ();
                        for (; it != _subscriptions.end (); it++)
                        {
                            Subscription &sub = it->second;

                            msg.clear (SUBSCRIBE);
                            msg.store (sub.sub_id);
                            msg.store (sub.aoi);
                            msg.store (sub.layer);
                            msg.addTarget (_relay_id);
                                    
                            sendMessage (msg);
                        }
                    }
                    */
                }
            }
            break;
        
        case DISCONNECT:
            {
                // if a known relay leaves, remove it
                if (_relays.find (in_msg.from) != _relays.end ())
                {
                    printf ("[%llu] VASTRelay::handleMessage () removes disconnected relay [%llu]\n", _self.id, in_msg.from);
                    
                    // remove the disconnecting relay from relay list
                    removeRelay (in_msg.from);
                   
                    // if disconnected from current relay, re-find closest relay
                    if (_curr_relay != NULL && 
                        in_msg.from == _curr_relay->id && 
                        in_msg.from != _self.id)
                    {
                        // invalidate relay ID, re-initiate query for closest relay
                        _curr_relay = NULL;                        
                        _timeout_query = 0;
                        _state = QUERYING;
                    }
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
    VASTRelay::postHandling ()
    {
		// send periodic query to locate & update physical coordinate here
        // also to ensure aliveness of relays

        if (_timeout_ping-- <= 0)
        {
            // reset re-try countdown
            _timeout_ping = TIMEOUT_COORD_QUERY * _net->getTickPerSecond ();

            // obtain some relays from network layer if no known relays
            if (_relays.size () == 0)
            {
                printf ("VASTRelay::postHandling () no relays known, try to get some from network layer\n");

                // using network entry points as initial relays 
                vector<IPaddr> &entries = _net->getEntries ();
                Node relay;

                for (size_t i=0; i < entries.size (); i++)
                {            
                    relay.id = relay.addr.host_id  = _net->resolveHostID (&entries[i]);
                    relay.addr.publicIP = entries[i];
        
                    addRelay (relay);                    
                }
            }

            // if no relays are known and no physical coordinate specified, 
            // we *assume* that we're the very first node on the network
            if (_relays.size () == 0)
            {
                if (_state == ABSENT)
                {
                    _self.aoi.center = _temp_coord = Position (0, 0, 0);
                    setJoined ();
                }
            }

            // send ping if some relays are known
            else 
            {
                // re-send PING to known relays
                printf ("VASTRelay::postHandling () pinging relays to determine physical coord\n");
                ping ();
            
                // if not yet joined, try to request more relays
                if (_state != JOINED)
                {
                    Message msg (REQUEST);
            
                    map<id_t, Node>::iterator it = _relays.begin ();
                    for (; it != _relays.end (); it++)
                        msg.addTarget (it->first);
            
                    sendMessage (msg);
                }
            }
        }

        if (_state == ABSENT)
        {
            // first check if we've gotten physical coordinate, 
            // if so then we start to query the closest relay 
            if (getPhysicalCoordinate () != NULL)
                _state = QUERYING;
        }
        else if (_state == QUERYING)
        {
            // if the closest relay is found, start joining it
            if (_curr_relay != NULL)
                _state = JOINING;

            // query the closest relay to join
            else if (_timeout_query-- <= 0)
            {                          
                // set a timeout of re-querying
                _timeout_query = TIMEOUT_RELAY_QUERY * _net->getTickPerSecond ();
        
                printf ("VASTRelay::postHandling () sending query to find closest relay\n");

                Node *relay = nextRelay ();

                // send query to one existing relay to find physically closest relay to join
                Message msg (RELAY_QUERY);
                msg.priority = 1;
        
                // parameter 1: requester physical coord & address
                msg.store (_self);
        
                Addr addr = relay->addr;
                // parameter 2: forwarder's address 
                msg.store (addr);
        
                // send to the selected relay
                msg.addTarget (relay->id);
        
                sendMessage (msg);
            }
        }
        else if (_state == JOINING)
        {
            if (_timeout_join-- <= 0)
            {                       
                // TODO: should we remove unresponding relay, or simply re-attempt?
                //       should we avoid making same requests to unresponding relay?

                // re-attempt join
                joinRelay ();                                                
            }
        }

        // remove non-essential relays
        // TODO: implement & check this very carefully, as it could disrupt normal operations
        //       if not done correctly
        //cleanupRelays ();
    }

    // find the relay closest to a position, which can be myself
    // returns the relay's Node info
    Node * 
    VASTRelay::closestRelay (Position &pos)
    {
        // set self as default
        Node *closest   = &_self;

        double min_dist = _self.aoi.center.distance (pos);

        // find closest among known neighbors
        multimap<double, Node *>::iterator it = _dist2relay.begin ();
        for (; it != _dist2relay.end (); it++)
        {
            // NOTE: it's important if distance is equal or very close, then 
            //       there's a second way to determine ordering (i.e., by ID)
            //       otherwise the query may be thrown in circles

            double dist = it->second->aoi.center.distance (pos);
            if (dist < min_dist || 
                ((dist - min_dist < EQUAL_DISTANCE) && (it->second->id < closest->id)))
            {
                closest     = it->second;
                min_dist    = dist;
            }            
        }

        // reply the closest
        return closest;
    }

    // find next available relay
    Node *
    VASTRelay::nextRelay ()
    {
        // if no existing relay exists, pick first known
                                                                                                                      if (_curr_relay == NULL)
        {
            if (_relays.size () == 0)
            {
                printf ("VASTRelay::nextRelay () relay list is empty, no next relay available\n");
                return NULL;
            }
            _curr_relay = &(_relays.begin ()->second);
        }

        // begin looping to find the next available relay in terms of distance to self
        multimap<double, Node *>::iterator it = _dist2relay.begin ();
        
        // find the next closest
        for (; it != _dist2relay.end (); it++)
        {
            if (it->second->id == _curr_relay->id)
                break;
        }

        // find next available
        if (it != _dist2relay.end ())
            it++;

        // loop from beginning
        if (it == _dist2relay.end ())
        {
            printf ("VASTRelay::nextRelay () - last known relay reached, re-try first\n");
            it = _dist2relay.begin ();
        }

        return it->second;
    }
    
    // add a newly learned relay
    void
    VASTRelay::addRelay (Node &relay)
    {
        if (relay.id == NET_ID_UNASSIGNED)
        {
            printf ("VASTRelay::addRelay () relay's ID is unassigned\n");
            return;
        }

        // we first determine a temp distance between our current estimated coord & the relay's coord
        double dist = relay.aoi.center.distance (_temp_coord);

        // avoid inserting the same relay, but note that we accept two relays have the same distance
        if (_relays.find (relay.id) != _relays.end ())
            return;

        printf ("[%llu] VASTRelay::addRelay () adding relay [%llu]..\n", _self.id, relay.id);

        _relays[relay.id] = relay;
        _dist2relay.insert (multimap<double, Node *>::value_type (dist, &(_relays[relay.id])));

        notifyMapping (relay.id, &relay.addr);
    }
    
    void
    VASTRelay::removeRelay (id_t id)
    {
        map<id_t, Node>::iterator it_relay;
        if ((it_relay = _relays.find (id)) == _relays.end ())
            return;

        double dist = it_relay->second.aoi.center.distance (_self.aoi.center);

        multimap<double, Node *>::iterator it = _dist2relay.find (dist);
        for (; it != _dist2relay.end (); it++)
        {
            if (it->second->id == id)
            {
                printf ("[%lld] VASTRelay::removeRelay () removing relay [%lld]..\n", _self.id, id);
                
                _dist2relay.erase (it);                
                _relays.erase (id);
                break;
            }
        }

        // also erase the pending tracker
        _pending.erase (id);
    }

    // join the relay mesh by connecting to the closest known relay
    bool
    VASTRelay::joinRelay (Node *relay)
    {
        // check if relay info exists
        if (relay == NULL && _relays.size () == 0)
        {
            printf ("VASTRelay::joinRelay (): no known relay to contact\n");
            return false;
        }

        // see if I'm also a relay when contacting the relay mesh
        bool as_relay = _net->isPublic ();

        // if no preferred relay is provided, find next available (distance sorted)
        if (relay == NULL)
            relay = nextRelay ();

        // set current relay as the next closest relay
        _curr_relay = relay;

        // send JOIN request to the closest relay
        Message msg (RELAY_JOIN);
        msg.priority = 1;
        msg.store (_self);
        msg.store ((char *)&as_relay, sizeof (bool));
        msg.addTarget (relay->id);
        sendMessage (msg);
        
        //_state = JOINING;
        //_timeout_join = TIMEOUT_JOIN;

        /* TODO: move to VASTClient
        // invalidate all current subscriptions (necessary during relay re-join after relay failure)
        map<id_t, Subscription>::iterator it = _subscriptions.begin ();
        for (; it != _subscriptions.end (); it++)
            it->second.active = false;
        */

        // reset countdown
        _timeout_join = TIMEOUT_RELAY_JOIN * _net->getTickPerSecond ();

        return true;
    }

    // remove non-useful relays
    void 
    VASTRelay::cleanupRelays ()
    {
        // no limit
        if (_relay_limit == 0)
            return;

        // remove known relays that are too far, but only after we've JOINED
        if (_state == JOINED && _dist2relay.size () > _relay_limit)
        {
            // remove the most distant ones, assuming those further down the map are larger values
            int num_removals = _dist2relay.size () - _relay_limit;

            vector<id_t> remove_list;

            multimap<double, Node *>::iterator it = _dist2relay.end ();
       
            while (num_removals > 0)
            {
                remove_list.push_back (it->second->id);
                it--;
            }

            for (int i=0; i < num_removals; i++)      
                removeRelay (remove_list[i]);
        }
    }

    int 
    VASTRelay::sendRelays (id_t target, int limit)
    {                
         listsize_t n = (listsize_t)_relays.size ();

         if (n == 0)
             return 0;

         // limit at max ping size
         if (n > limit)
             n = (listsize_t)limit;

         Message msg (RELAY);
         msg.priority = 1;
         msg.store (n);
         
         // select some neighbors
         vector<bool> selected;
         randomSet (n, _relays.size (), selected);

         map<id_t, Node>::iterator it = _relays.begin ();
         int i=0;
         for (; it != _relays.end (); it++)
         {
             if (selected[i++] == true)
             {
                 printf ("(%lld) ", it->first);
                 msg.store (it->second);
             }
         }

         printf ("\n[%llu] responds with %d relays\n", _self.id, n);
         
         msg.addTarget (target);                
         sendMessage (msg);

         return n;
    }

    // specify we've joined at a given relay.
    bool 
    VASTRelay::setJoined (id_t relay_id)
    {
        // if no ID is specified, I myself is relay
        if (relay_id == NET_ID_UNASSIGNED)
        {
            _curr_relay = &_self;
        }
        else
        {
            // if the specify ID is not found
            map<id_t, Node>::iterator it = _relays.find (relay_id);
            if (it == _relays.end ())
                return false;

            _curr_relay = &it->second;
        }

        _state = JOINED;

        // notify network of default host to route messages
        // TODO: not a good idea to touch such lower-level stuff here, better way?                        
        ((MessageQueue *)_msgqueue)->setDefaultHost (_curr_relay->id);

        return true;
    }


    // recalculate my physical coordinate estimation (using Vivaldi)
    // given a neighbor j's position xj & error estimate ej
    void 
    VASTRelay::vivaldi (float rtt, Position &xi, Position &xj, float &ei, float &ej)   
    {
        // constant error
        static float C_e = RELAY_CONSTANT_ERROR;

        // constant fraction
        static float C_c = RELAY_CONSTANT_FRACTION;

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

    // randomly select some items from a set
    bool
    VASTRelay::randomSet (int size, int total, vector<bool> &random_set)
    {        
        if (size > total)
        {
            printf ("randomSet () # needed to select is greater than available\n");
            return false;
        }

        random_set.clear ();

        // empty out
        for (int i=0; i < total; i++)
            random_set.push_back (false);
       
        int selected = 0;

        while (selected < size)
        {
            int index = rand () % total;

            if (random_set[index] == false)
            {
                random_set[index] = true;
                selected++;
            }
        }

        return true;
    }

} // end namespace Vast
