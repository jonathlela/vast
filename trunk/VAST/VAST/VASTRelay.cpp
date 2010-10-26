


#include "VASTRelay.h"
#include "MessageQueue.h"


using namespace Vast;

namespace Vast
{   

    // constructor for physical topology class, may supply an artifical physical coordinate
    VASTRelay::VASTRelay (bool as_relay, size_t client_limit, size_t relay_limit, Position *phys_coord)
            :MessageHandler (MSG_GROUP_VAST_RELAY), 
             _curr_relay (NULL),
             _contact_relay (NULL),
             _as_relay (as_relay),
             _state (ABSENT),
             _timeout_ping (0),
             _timeout_query (0),
             _timeout_join (0),
             _ping_all_count (0)
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
    VASTRelay::ping (bool curr_relay_only)
    {
        // prepare PING message
        Message msg (PING);
        msg.priority = 1;

        timestamp_t current_time = _net->getTimestamp ();
        msg.store (current_time);

        // ping all known relays
        if (curr_relay_only == false)
        {
            // set how many we send PING each time
            int ping_num = ((int)_relays.size () > MAX_CONCURRENT_PING ? MAX_CONCURRENT_PING : (int)_relays.size ());
        
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
        
                // avoid self & check for redundency
                id_t target = it->first;
                if (_self.id != target && _pending.find (target) == _pending.end ())
                {
                    msg.addTarget (target);
                    _pending[target] = current_time;             
                }
            }
        }

        // *must* ping current relay (to make sure it doesn't disconnect me)
        if (isRelay () == false && _curr_relay != NULL)
            msg.addTarget (_curr_relay->id);

        // if no targets exist
        if (msg.targets.size () == 0)
            return false;

        // send out messages        
        int num_success = sendRelay (msg);

        // if no PING is sent
        if (num_success == 0)
        {
            // error if cannot contact any relays
            if (_net->getEntries ().size () != 0)                        
                printf ("VASTRelay::ping () no known relays to contact\n cannot determine physical coordinate, check if relays are valid\n");
            return false;
        }
        
        if (curr_relay_only == false)            
            _request_times++;

        // success means at least a few PING requests are sent
        return true;
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

        sendRelay (msg);

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

    // whether the current node is a relay
    bool 
    VASTRelay::isRelay ()
    {
        return (_as_relay && _net->isPublic ());
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

    // get # of clients connected
    int
    VASTRelay::getClientSize ()
    {
        return (int)_clients.size ();
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
        // store the app-specific message type if exists
        msgtype_t app_msgtype = APP_MSGTYPE(in_msg.msgtype);
        in_msg.msgtype = VAST_MSGTYPE(in_msg.msgtype);

        switch (in_msg.msgtype)
        {

        // combined into PING
        /*
        case REQUEST:
            {
                // send back a list of known relays
                printf ("REQUEST received from [%llu]\n", in_msg.from);
                sendRelayList (in_msg.from, MAX_CONCURRENT_PING);
            }
            break;
        */

        case PING:
            {
                // extract the time & coordinate
                timestamp_t senttime;
                in_msg.extract (senttime);

                // respond the PING message
                pong (in_msg.from, senttime, true);

                // send back a list of known relays
                //printf ("REQUEST received from [%llu]\n", in_msg.from);
                sendRelayList (in_msg.from, MAX_CONCURRENT_PING);
            }
            break;

        case PONG:
        case PONG_2:
            {                
                // tolerance for error to determine stabilization
                //static float tolerance = RELAY_TOLERANCE;
                                                   
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
                    printf ("[%llu] processing PONG: RTT = 0 error, removing neighbor [%llu] currtime: %lu querytime: %lu\n", _self.id, in_msg.from, current, querytime);
                    removeRelay (in_msg.from);
                    //_relays.erase (in_msg.from);
                    break;
                }
                
                // call the Vivaldi algorithm to update my physical coordinate
                vivaldi (rtt, _temp_coord, xj, _error, ej);

#ifdef DEBUG_DETAIL
                printf ("[%llu] physcoord (%.3f, %.3f) rtt to [%llu]: %.3f error: %.3f requests: %d\n", 
                         _self.id, _temp_coord.x, _temp_coord.y, in_msg.from, rtt, _error, _request_times);
#endif

                // if remote host is a relay, record its coordinates                
                if (_relays.find (in_msg.from) != _relays.end ())
                    _relays[in_msg.from].aoi.center = xj;

                // if the local error value is small enough, we've got our physical coordinate
                // or we force the convergence if too many queries are sent
                if (_error < RELAY_TOLERANCE || _request_times > MAX_RELAY_QUERIES)
                {
                    // print a small message to show it
                    if (_request_times > 0)
                    {
                        printf ("[%llu] physcoord (%.3f, %.3f) rtt to [%llu]: %.3f error: %.3f requests: %d\n", 
                                _self.id, _temp_coord.x, _temp_coord.y, in_msg.from, rtt, _error, _request_times);

                        // reset
                        _request_times = 0;
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
                    //if (VASTnet::extractHost (relay.id) != VASTnet::extractHost (_self.id))
                    //{
                        // add a new relay (while storing ID->address mapping)
                        addRelay (relay);
                    //}
                }
            }
            break;

        // response to a query of the closest relay by a joining node
        case RELAY_QUERY:
            {
                Node joiner;
                Addr relay;
                in_msg.extract (joiner);
                in_msg.extract (relay);

                bool success = false;

                // Eventually I will reply myself, unless the relay provided is invalid,
                // in which case I should still break
                while (!success)
                {
                    // find the closet relay
                    Node *closest = closestRelay (joiner.aoi.center);
                
                    // respond the query if I'm the closest 
                    if (closest->id == _self.id)
                    {
                        Message msg (RELAY_QUERY_R);
                        msg.priority = 1;
                        msg.store (joiner);
                        msg.store (*closest);
                        msg.addTarget (relay.host_id);
                                        
                        notifyMapping (relay.host_id, &relay);

                        // TODO: if the relay provided is not valid, the joining node may wait for longer time before another RELAY_QUERY is sent
                        if (sendRelay (msg) == 0)
                        {
                            // if the relay provided cannot be reached, drop this request
                            printf ("[%llu] VASTRelay::handleMessage () RELAY_QUERY relay supplied [%llu] is no longer valid\n", _self.id, relay.host_id);
                        }
                        // whether success or not, we have to leave the loop
                        break;
                    }
                    // otherwise greedy-forward the message to the next closest, valid relay
                    else
                    {
                        in_msg.targets.clear ();
                        in_msg.addTarget (closest->id);
                        success = (sendRelay (in_msg) > 0);
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

                    // add myself as initial relay
                    if (isRelay () == true)
                        addRelay (_self);

                    joinRelay ();
                }
                // if I was the forwarder, forward the response to the requester
                else
                {
                    in_msg.reset ();
                    in_msg.targets.clear ();
                    in_msg.addTarget (requester.id);

                    sendRelay (in_msg);
                }
            }
            break;

        // handles a JOIN request of a new node to the overlay, 
        // responds with a list of known relay neighbors
        case RELAY_JOIN:
            {
                Node joiner;
                bool is_relay;                
                in_msg.extract (joiner);
                in_msg.extract ((char *)&is_relay, sizeof (bool));
                
                // check if I can take in this new node, default is NO 
                // (because joiner is itself a relay, or no more peer space available)
                bool join_reply = false;

                // conditions for accepting a joiner:
                //  1. I myself is joining  or
                //  
                //  2a. I'm an accepting relay & still have space (or no limit)
                //  2b. the joiner must not be relay
                
                if (joiner.id == _self.id ||
                    (_as_relay == true && is_relay == false && (_client_limit == 0 || _clients.size () < _client_limit)))
                {
                    join_reply = true;                    
                    addClient (in_msg.from, joiner);
                }                                
                
                // compose & send return message
                Message msg (RELAY_JOIN_R);
                msg.priority = 1;
                msg.store ((char *)&join_reply, sizeof (bool));                                                    
                msg.addTarget (joiner.id);
                sendMessage (msg);

                // also send the joiner some of my known relays, not too many though
                sendRelayList (joiner.id, MAX_CONCURRENT_PING);
            }
            break;

        // response from previous JOIN request
        case RELAY_JOIN_R:
            {
                // only update if we're currently seeking to join
                if (_state == JOINED)
                    break;

                // I must be both willing & have public IP to be an accepting relay
                bool join_success;

                in_msg.extract ((char *)&join_success, sizeof (bool));

                // NOTE that as long as we have public IP, I can use myself as relay
                //      and there's no need to seek join acceptance from an existing relay
                if (isRelay () == false && join_success == false)
                {
                    // if a non-relay client fails to join, then find another relay to join
                    joinRelay ();
                }
                // otherwise, we've successfully joined the relay mesh 
                else 
                {                   
                    // if I'm an accepting relay, let existing relays also know
                    if (isRelay ())
                    {
                        // let other relays know me
                        notifyPhysicalCoordinate ();

                        // set myself as relay
                        setJoined ();
                    }
                    else
                    {
                        // set the sender as my relay 
                        // NOTE: if I join myself, then I could receive RELAY_JOIN_R from myself as well
                        setJoined (in_msg.from);
                    }
                }
            }
            break;

        // notification from matcher about mapping from subscription ID to host ID
        case SUBSCRIBE_NOTIFY:
            {
                id_t sub_id;
                id_t client_host_id;
                in_msg.extract (sub_id);
                in_msg.extract (client_host_id);

                //_sub2client[sub_id] = in_msg.from;
                _sub2client[sub_id] = client_host_id;

                // check if there are unprocessed forwarded messages due to this client
                //forwardMessage (sub_id, in_msg.from);
                forwardMessage (sub_id, client_host_id);

                // NOTE: we'll periodically remove expired unforwarded messages
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
                   
                }

                else
                {
                    // otherwise we simply remove a client connected to us
                    // NOTE: that a host can only be either a client or a relay
                    removeClient (in_msg.from);
                }
            }
            break;

        // assume all other messages are forwardwd messages 
        // receiving a forwarded message (source can be either PUBLISH or SEND) or NEIGHBOR
        default:
        //case MESSAGE:
        //case NEIGHBOR:
            {
                // forward message to clients 

                // NOTE must send to remote first before local, as sendtime will be extracted by local host
                //      and the message structure will get changed
                
                // store back app-specific message type
                if (in_msg.msgtype == MESSAGE)
                    in_msg.msgtype = (app_msgtype << VAST_MSGTYPE_RESERVED) | MESSAGE;
                                                
                // translate targets to actual client hostID 
                vector<id_t> clients;
                vector<id_t> unknown_clients;
                
                for (size_t i=0; i < in_msg.targets.size (); i++)
                {
                    id_t target = in_msg.targets[i];

                    // mapping of subscription to clientID found
                    if (_sub2client.find (target) != _sub2client.end ())
                        clients.push_back (_sub2client[target]);

                    // check for message to self 
                    else if (target == _net->getHostID ())
                    {
                        clients.push_back (target);
                    }
                    else
                    {
                        // record unresolved client targets
                        printf ("VASTRelay: cannot translate received subscriptionID [%llu] to clientID\n", target);
                        unknown_clients.push_back (target);                        
                    }
                }

                // all forward messages are addressed to clients
                in_msg.msggroup = MSG_GROUP_VAST_CLIENT;
                
                // store unknown clients
                timestamp_t now = _net->getTimestamp ();                
                for (size_t i=0; i < unknown_clients.size (); i++)
                {
                    in_msg.targets.clear ();
                    in_msg.addTarget (unknown_clients[i]);

                    // store a copy of the message into queue, also record the time
                    _queue.insert (multimap<id_t, Message *>::value_type (unknown_clients[i], new Message (in_msg)));
                    _queue_time[unknown_clients[i]] = now + (KEEPALIVE_RELAY * _net->getTimestampPerSecond ());
                }
                                
                // use the converted client hostID
                if (clients.size () > 0)
                {
                    in_msg.targets = clients;
                    
                    sendMessage (in_msg);
                }
                else
                {
                    printf ("VASTRelay: cannot forward message, message type: %d\n", in_msg.msgtype);
                    return false;                        
                }
            }
            break;
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
        timestamp_t now = _net->getTimestamp ();

        if (now >= _timeout_ping)
        {
            // reset re-try countdown
            _timeout_ping = now + (KEEPALIVE_RELAY * _net->getTimestampPerSecond ());

            // obtain some relays from network layer if no known relays
            if (_relays.size () == 0)
            {                
                // using network entry points as initial relays 
                vector<IPaddr> &entries = _net->getEntries ();
                Node relay;

                for (size_t i=0; i < entries.size (); i++)
                {            
                    relay.id = relay.addr.host_id  = net_manager::resolveHostID (&entries[i]);
                    relay.addr.publicIP = entries[i];
        
                    addRelay (relay);
                }

                // if still no relays found
                if (_relays.size () == 0)
                {
                    printf ("VASTRelay: relay list is empty, got none from network layer\n");
                }
            }

            // send ping if some relays are known
            if (_relays.size () > 0)
            {
                /*
                // re-send PING to known relays
                bool send_maintain = (_ping_all_count++ != (TIMEOUT_COORD_QUERY / KEEPALIVE_RELAY));
                if (_ping_all_count == (TIMEOUT_COORD_QUERY / KEEPALIVE_RELAY))
                    _ping_all_count = 0;
                */

                //if (isJoined () == false)
                //    curr_relay_only = false;

                //ping (curr_relay_only);

                // we only ping current relay once joined (to reduce PING traffic)
                ping (isJoined ());
                               
                /*
                else if (send_maintain)
                {
                    if (getPhysicalCoordinate () != NULL && isRelay ())
                        notifyPhysicalCoordinate ();
                } 
                */

                // remove unforwarded messages that are expired
                vector<id_t> remove_list;
                map<id_t, timestamp_t>::iterator it = _queue_time.begin ();
                for (; it != _queue_time.end (); it++)
                    if (now > it->second)
                        remove_list.push_back (it->first);

                // remove each unforwarded messages
                for (size_t i=0; i < remove_list.size (); i++)
                    forwardMessage (remove_list[i]);
            }
        }
        
        if (_state == ABSENT)
        {
            // if no relays are known and no physical coordinate specified, 
            // we *assume* that we're the very first node on the network
            if (_relays.size () == 0)
            {
                _self.aoi.center = _temp_coord;
                setJoined ();
            }

            // first check if we've gotten physical coordinate, 
            // if so then we start to query the closest relay 
            else if (getPhysicalCoordinate () != NULL)
                _state = QUERYING;
        }
        else if (_state == QUERYING)
        {
            // if the closest relay is found, start joining it
            if (_curr_relay != NULL)
                _state = JOINING;

            // query the closest relay to join
            else if (now >= _timeout_query)
            {                          
                // set a timeout of re-querying
                _timeout_query = now + (TIMEOUT_RELAY_QUERY * _net->getTimestampPerSecond ());
        
                printf ("VASTRelay::postHandling () sending query to find closest relay\n");

                Node *relay = nextRelay ();

                if (relay == NULL)
                {
                    // need to re-ping for relays
                    _timeout_ping = 0;
                }
                else
                {
                    // send query to one existing relay to find physically closest relay to join
                    Message msg (RELAY_QUERY);
                    msg.priority = 1;
                
                    // parameter 1: requester physical coord & address
                    msg.store (_self);
                                    
                    // parameter 2: forwarder's address 
                    Addr addr = relay->addr;
                    msg.store (addr);
                
                    // send to the selected relay
                    msg.addTarget (relay->id);
                
                    // if the relay has failed, we'll try again next tick 
                    if (sendRelay (msg) == 0)
                        _timeout_query = 0;
                }
            }
        }
        else if (_state == JOINING)
        {
            if (now >= _timeout_join)
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
        cleanupRelays ();
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
        // NOTE: that we could join the mesh without accepting clients
        bool as_relay = isRelay ();

        // if no preferred relay is provided, find next available (distance sorted)
        if (relay == NULL)
            relay = nextRelay ();

        // send JOIN request to the closest relay
        Message msg (RELAY_JOIN);
        msg.priority = 1;
        msg.store (_self);
        msg.store ((char *)&as_relay, sizeof (bool));
        msg.addTarget (relay->id);
        
        // reset countdown, if the send failed, try again next tick
        if (sendRelay (msg) > 0)        
            _timeout_join = _net->getTimestamp () + (TIMEOUT_RELAY_JOIN * _net->getTimestampPerSecond ());
        else
            _timeout_join = 0;

        return true;
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
        // if no existing contact relay exists, pick first closest
        if (_contact_relay == NULL)
        {
            if (_relays.size () == 0)
            {
                printf ("VASTRelay::nextRelay () relay list is empty, no next relay available\n");
                return NULL;
            }

            _contact_relay = _dist2relay.begin ()->second;
        }
        else
        {
            // begin looping to find the next available relay in terms of distance to self
            multimap<double, Node *>::iterator it = _dist2relay.begin ();
            
            // find the next closest
            for (; it != _dist2relay.end (); it++)
            {
                if (it->second->id == _contact_relay->id)
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
        
            // set current relay as the next closest relay
            _contact_relay = it->second;
        }

        return _contact_relay;
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
        double dist = relay.aoi.center.distance (_self.aoi.center);

        map<id_t, Node>::iterator it = _relays.find (relay.id);

        // avoid inserting the same relay, but note that we accept two relays have the same distance
        if (it != _relays.end ())
        {
#ifdef DEBUG_DETAIL
            printf ("[%llu] VASTRelay::addRelay () updating relay [%llu]..\n", _self.id, relay.id);
#endif

            it->second = relay;

            // erase distance record            
            for (multimap<double, Node *>::iterator it = _dist2relay.begin (); it != _dist2relay.end (); it++)
            {
                if (it->second->id == relay.id)
                {                
                    _dist2relay.erase (it);                
                    break;
                }
            }           
        }
        else
        {
#ifdef DEBUG_DETAIL
            printf ("[%llu] VASTRelay::addRelay () adding relay [%llu]..\n", _self.id, relay.id);
#endif
            _relays[relay.id] = relay;            
        }

        _relays[relay.id].time = _net->getTimestamp ();

        _dist2relay.insert (multimap<double, Node *>::value_type (dist, &(_relays[relay.id])));
        notifyMapping (relay.id, &relay.addr);
    }
    
    void
    VASTRelay::removeRelay (id_t id)
    {
        map<id_t, Node>::iterator it_relay;
        if ((it_relay = _relays.find (id)) == _relays.end ())
            return;

        // if disconnected from current relay, re-find closest relay
        if (_curr_relay != NULL && id == _curr_relay->id)
        {
            // invalidate relay ID, re-initiate query for closest relay
            _curr_relay = NULL;   
            _contact_relay = NULL;
            _timeout_query = 0;
            _state = QUERYING;
        }

        if (_contact_relay != NULL && id == _contact_relay->id)
            _contact_relay = NULL;

        //double dist = it_relay->second.aoi.center.distance (_self.aoi.center);

        multimap<double, Node *>::iterator it = _dist2relay.begin ();
        for (; it != _dist2relay.end (); it++)
        {
            if (it->second->id == id)
            {
#ifdef DEBUG_DETAIL
                printf ("[%lld] VASTRelay::removeRelay () removing relay [%lld]..\n", _self.id, id);
#endif
                
                _dist2relay.erase (it);                
                _relays.erase (id);
                break;
            }
        }

        // also erase the pending tracker
        _pending.erase (id);

        // if no relays exist, need to re-create relays
        if (_relays.size () == 0)
        {
            printf ("VASTRelay::removeRelay () no more relays known, need to re-query network\n");
            _timeout_ping = 0;
        }
    }

    // send message to a relay, remove the relay if send isn't successful
    // return # of targets sent successfully
    int
    VASTRelay::sendRelay (Message &msg)
    {               
        size_t num_success = msg.targets.size ();
        
        if (num_success > 0)
        {
            vector<id_t> failed;
            sendMessage (msg, &failed);
                
            // remove failed relays
            for (size_t i=0; i < failed.size (); i++)
            {
                printf ("removing failed relay (%lld) ", failed[i]);
                removeRelay (failed[i]);
            }
        
            num_success -= failed.size ();
        }

        return (int)num_success;
    }

    // remove non-useful relays
    void 
    VASTRelay::cleanupRelays ()
    {
        // if there's no limit or we're still in process of joining
        if (_relay_limit == 0 || _curr_relay == NULL)
            return;

        // remove known relays that are too far, but only after we've JOINED
        if (_state == JOINED && _dist2relay.size () > _relay_limit)
        {
            // remove the most distant ones, assuming those further down the map are larger values
            int num_removals = _dist2relay.size () - _relay_limit;

            vector<id_t> remove_list;

            multimap<double, Node *>::reverse_iterator rit = _dist2relay.rbegin ();
       
            while (num_removals > 0)
            {
                // skip current relay
                // TODO: if physical coord is continously adjusted, perhaps we should
                //       allow current relay change?
                if (_curr_relay->id != rit->second->id)
                {                    
                    remove_list.push_back (rit->second->id);                
                    num_removals--;
                }
                rit++;
            }

            printf ("[%llu] VASTRelay::cleanupRelays () remove %lu relays\n", _self.id, remove_list.size ());
            for (size_t i=0; i < remove_list.size (); i++)  
                removeRelay (remove_list[i]);
        }
    }

    int 
    VASTRelay::sendRelayList (id_t target, int limit)
    {                
         listsize_t n = (listsize_t)_relays.size ();

         if (n == 0)
             return 0;

         // limit at max ping size
         if (limit != 0 && n > limit)
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
#ifdef DEBUG_DETAIL
                 printf ("(%lld) ", it->first);
#endif
                 msg.store (it->second);
             }
         }

         printf ("\n[%llu] responds with %d relays to [%llu]\n", _self.id, n, target);
         
         msg.addTarget (target);                
         sendMessage (msg);

         return n;
    }

    // notify neighboring relay of my updated physical coordinate
    bool 
    VASTRelay::notifyPhysicalCoordinate ()
    {
        if (_relays.size () == 0)
            return false;

        // notify existing known relays of myself as a new relay
        Message msg (RELAY);
        msg.priority = 1;
        listsize_t n = 1;
        msg.store (n);
        msg.store (_self);

        // obtain list of targets (check if redundent PING has been sent)
        map<id_t, Node>::iterator it = _relays.begin ();

        //for (;it != _relays.end (); it++)
        // notify up to _relay_limit neighbors
        for (size_t i=0; it != _relays.end () && (_relay_limit == 0 || i < _relay_limit); it++, i++)
            if (it->first != _self.id)
                msg.addTarget (it->first);

        sendRelay (msg);
        
        return true;
    }

    // specify we've joined at a given relay.
    bool 
    VASTRelay::setJoined (id_t relay_id)
    {
        // if no ID is specified, I myself is relay
        if (relay_id == NET_ID_UNASSIGNED)
            _curr_relay = &_self;
        else
        {
            // if the specify ID is not found
            map<id_t, Node>::iterator it = _relays.find (relay_id);
            if (it == _relays.end ())
                return false;

            _curr_relay = &it->second;
        }

        // no longer needs this, reset so next time calls to nextRelay will return closest
        _contact_relay = NULL;

        _state = JOINED;

        // notify network of default host to route messages
        // TODO: not a good idea to touch such lower-level stuff here, better way?                        
        ((MessageQueue *)_msgqueue)->setDefaultHost (_curr_relay->id);

        return true;
    }

    // accept a client
    void 
    VASTRelay::addClient (id_t from_host, Node &client)
    {
        // record the clients this relay has accepted
        _clients[client.id] = client;

        // estblish link to the joining client
        notifyMapping (client.id, &client.addr);

        printf ("\n[%llu] VASTRelay accepts join request from [%llu]\n\n", _self.id, from_host); 
    }

    // remove a client no longer connected
    void 
    VASTRelay::removeClient (id_t id)
    {
        map<id_t, Node>::iterator it = _clients.find (id);

        if (it != _clients.end ())
            _clients.erase (it);

        // remove all mappings from subscription to client host ID
        map<id_t, id_t>::iterator itr = _sub2client.begin ();

        vector<id_t> remove_list;
        for (; itr != _sub2client.end (); itr++)
            if (itr->second == id)
                remove_list.push_back (itr->first);
        
        for (size_t i=0; i < remove_list.size (); i++)
        {
            _sub2client.erase (remove_list[i]);
        }
    }

    // forward a message meant for client 'sub_id', to an actual client host
    // if host_id is not provided then the message is simply removed
    int
    VASTRelay::forwardMessage (id_t sub_id, id_t host_id)
    {
        multimap<id_t, Message *>::iterator it; 
        int processed = 0;
        
        while ((it = _queue.find (sub_id)) != _queue.end ())
        {
            // if client host is provided, forward it
            if (host_id != NET_ID_UNASSIGNED)
            {
                Message *msg = it->second;
                msg->targets.clear ();
                msg->addTarget (host_id);
                msg->msggroup = MSG_GROUP_VAST_CLIENT;

                sendMessage (*msg);
            }

            _queue_time.erase (sub_id);

            delete it->second;                                        
            _queue.erase (it);

            processed++;
        }

        return processed;
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
        // if local error is large relative to remote error, weight will be heavier
		float weight;

        // no error exists, we're perfect (possible to achieve?)
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
