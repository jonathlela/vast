


#include "VASTMatcher.h"
#include "MessageQueue.h"

#include "VASTUtil.h"   // LogManager


using namespace Vast;

namespace Vast
{   

    VASTMatcher::VASTMatcher (bool is_matcher, int overload_limit, bool is_static, Position *coord)
            :MessageHandler (MSG_GROUP_VAST_MATCHER), 
             _state (ABSENT),
             _VSOpeer (NULL),
             _world_id (0),
             _origin_id (0),
             _is_matcher (is_matcher),
             _is_static (is_static),
             _overload_limit (overload_limit)
    {
        _next_periodic = 0;
        _matcher_keepalive = 0;

        // gateway only: set world_id assignment
        _world_counter = VAST_DEFAULT_WORLD_ID+1;
        
        // initialize initial join location
        if (coord != NULL)
            _self.aoi.center = *coord;            
    }

    VASTMatcher::~VASTMatcher ()
    {
        leave ();

        // release memory
        map<id_t, map<id_t, bool> *>::iterator it = _replicas.begin ();
        for (; it != _replicas.end (); it++)
            delete it->second;

        _replicas.clear ();

        for (size_t i=0; i < _queue.size (); i++)
            delete _queue[i];
        
        _queue.clear ();
    }

    // join the matcher mesh network with a given location
    bool 
    VASTMatcher::join (const IPaddr &gatewayIP)
    {
        // avoid redundent join for a given node
        if (_VSOpeer != NULL)
            return false;

        // if the gatewayIP format is incorrect, also fail the join
        if (setGateway (gatewayIP) == false)
            return false;

        // we use the hostID of the matcher as the ID in the matcher VON
        _VSOpeer = new VSOPeer (_self.id, this, this);
       
        // NOTE: use a small default AOI length as we only need to know enclosing matchers
        //       also, it doesn't matter where our center is, as we will certainly
        //       join at a different location as determined by the workload        
        _self.aoi.radius = 5;
        
        // let gateway know whether I can be a candidate origin matcher
        // NOTE: even the gateway matcher needs to call this
        notifyCandidacy ();

        // matcher is considered joined (initialized complete) once VSOpeer is created
        _state = JOINED; 

        return true;
    }

    // leave the matcher overlay
    bool 
    VASTMatcher::leave ()
    {
        if (isJoined () == false)
            return false;

        // leave the node overlay
        _VSOpeer->leave ();
        _VSOpeer->tick ();

        delete _VSOpeer;
        _VSOpeer = NULL;

        _world_id = 0;

        printf ("VASTMatcher leave () called\n");
        _state = ABSENT;
      
        return true;
    }

    // get the current node's information
    Node * 
    VASTMatcher::getSelf ()
    {
        return &_self;
    }

    bool 
    VASTMatcher::isJoined()
    {
        return (_state == JOINED);
    }

    // whether is particular ID is the gateway node, default (id = 0) is check for myself
    bool 
    VASTMatcher::isGateway (id_t id)
    {
        if (id == 0)
            return (_gateway.host_id == _self.id);
        else
            return (_gateway.host_id == id);
    }

    // whether I'm an active matcher (promoted by gateawy)
    bool
    VASTMatcher::isActive ()
    {
        return (_VSOpeer != NULL && _VSOpeer->isJoined ());
    }

    // obtain access to Voronoi class (usually for drawing purpose)
    // returns NULL if the peer does not exist
    Voronoi *
    VASTMatcher::getVoronoi ()
    {
        if (_VSOpeer != NULL)
            return _VSOpeer->getVoronoi ();
        else
            return NULL;
    }

    // obtain the matcher's adjustable radius (determined by VSOPeer)
    Area *
    VASTMatcher::getMatcherAOI ()
    {
        if (isActive ())
            return &_VSOpeer->getSelf ()->aoi;
        else
            return NULL;
    }

    //
    // VONNetwork
    //

    // send messages to some target nodes
    // returns number of bytes sent
    size_t 
    VASTMatcher::sendVONMessage (Message &msg, bool is_reliable, vector<id_t> *failed_targets)
    {
        // TODO: make return value consistent
        msg.reliable = is_reliable;
        return (size_t)sendMessage (msg, failed_targets);
    }

    // obtain next message in queue
    // return pointer to Message, or NULL for no more message
    Message* 
    VASTMatcher::receiveVONMessage (timestamp_t &senttime)
    {
        return NULL;
    }

    // notify the network layer of nodeID -> Address mapping
    bool 
    VASTMatcher::notifyAddressMapping (id_t node_id, Addr &addr)
    {
        return notifyMapping (node_id, &addr);
    }
    
    // get the IP address of current host machine
    Addr &
    VASTMatcher::getHostAddress ()
    {
        return _net->getHostAddress ();
    }

    // get current physical timestamp
    timestamp_t 
    VASTMatcher::getTimestamp ()
    {
        return _net->getTimestamp ();
    }

    // get the # of ticks in each second;
    timestamp_t 
    VASTMatcher::getTimestampPerSecond ()
    {
        return _net->getTimestampPerSecond ();
    }

    // perform initialization tasks for this handler (optional)
    void 
    VASTMatcher::initHandler ()
    {
        _self.id        = _net->getHostID ();
        _self.addr      = _net->getHostAddress ();
    }

    // handler for various incoming messages
    // returns whether the message was successfully handled
    bool 
    VASTMatcher::handleMessage (Message &in_msg)
    {
        if (_state == ABSENT)
            return false;

        // store the app-specific message type if exists
        msgtype_t app_msgtype = APP_MSGTYPE(in_msg.msgtype);
        in_msg.msgtype = VAST_MSGTYPE(in_msg.msgtype);

        // we need to check if VSOpeer has joined for all messages except DISCONNECT 
        if (in_msg.msgtype != DISCONNECT)
        {
            // check if the message is directed towards the VSOpeer
            if (in_msg.msgtype < VON_MAX_MSG)
            {
                _VSOpeer->handleMessage (in_msg);
                return true;
            }

            // if I have not joined or promoted as a matcher then shouldn't queue up the messages
            if (isJoined () == false || _VSOpeer->isJoined () == false)
            {
                // if VSOPeer is still joining, save the messages for later processing 
                if (_VSOpeer->isJoining ())
                {
                    _queue.push_back (new Message (in_msg));
                    return true;
                }
                else if (!(in_msg.msgtype == MATCHER_CANDIDATE ||
                         in_msg.msgtype == MATCHER_INIT ||
                         //in_msg.msgtype == MATCHER_JOINED ||
                         in_msg.msgtype == MATCHER_ALIVE ||
                         in_msg.msgtype == JOIN))
                {
                    // NOTE: if a matcher just departs, it's possible it will still receive messages from neighbor Matcher temporarily
                    printf ("[%llu] VASTMatcher::handleMessage () non-Matcher receives Matcher-specific message of type %d from [%llu]\n", _self.id, in_msg.msgtype, in_msg.from);
                    return false;
                }
            }                       
        }

        // if we've joined, check if there are pending messages to be processed
        if (_queue.size () > 0)
        {                
            for (size_t i=0; i < _queue.size (); i++)
            {
                sendMessage (*_queue[i]);
                delete _queue[i];
            }
        
            _queue.clear ();
        }        

#ifdef DEBUG_DETAIL
        if (in_msg.msgtype < VON_MAX_MSG)
            ; //printf ("[%d] VASTMatcher::handleMessage from: %d msgtype: %d, to be handled by VONPeer, size: %d\n", _self.id, in_msg.from, in_msg.msgtype, in_msg.size);            
        else
            printf ("[%d] VASTMatcher::handleMessage from: %d msgtype: %d appmsg: %d (%s) size: %d\n", _self.id, in_msg.from, in_msg.msgtype, app_msgtype, VAST_MESSAGE[in_msg.msgtype-VON_MAX_MSG], in_msg.size);
#endif

        switch (in_msg.msgtype)
        {
        // report to gateway that a matcher may be available as candidate origin matcher
        case MATCHER_CANDIDATE:
            {
                if (!isGateway ())
                {
                    LogManager::instance ()->writeLogFile ("[%llu] MATCHER_CANDIDATE received by non-gateway", _self.id);
                    break;
                }

                Addr candidate;            
                in_msg.extract (candidate);

                id_t matcher_id = candidate.host_id;
            
                // store potential nodes to join
                // use the host_id as index
                if (_matchers.find (candidate.host_id) != _matchers.end ())
                {
                    // NOTE: normally this should not happen, unless a timeout matcher has re-join again as matcher,
                    //       in such case we treat it as a new matcher
                    LogManager::instance ()->writeLogFile ("[%llu] MATCHER_CANDIDATE found matcher [%llu] already exists, ignore request", _self.id, matcher_id);
                    break;
                }

                MatcherInfo info;

                info.addr = candidate;
                info.state = CANDIDATE;
                info.world_id = 0;
                info.time = _net->getTimestamp ();

                _matchers[matcher_id] = info;
            }
            break;

        // starting of a new origin matcher by gateway
        // also serves the purpose of re-notifying a matcher its loss of origin matcher status
        case MATCHER_INIT:
            {
                world_t world_id;
                id_t    origin;
                in_msg.extract (world_id);
                in_msg.extract (origin);

                // assign my world_id
                _world_id = world_id;

                // assign origin_id as notified
                // NOTE that it could differ from self
                if (origin != _self.id)
                {
                    LogManager::instance ()->writeLogFile ("[%llu] MATCHER_INIT: lose origin matcher status to [%llu] for world [%u]", _self.id, origin, world_id);

                    // I'm probably outdated, prepare to re-join
                    leave ();
                    join (_gateway.publicIP);
                }
                else if (isActive ())
                    LogManager::instance ()->writeLogFile ("[%llu] MATCHER_INIT: promoted from regular to origin matcher for world [%u]", _self.id, world_id);

                _origin_id = origin;

                // if the VSOpeer is not yet joined
                if (isActive () == false && _VSOpeer)
                {
                    // start up the VSOPeer at the origin matcher, 
                    // set myself is the starting origin node in VSO
                    _VSOpeer->join (_self.aoi, &_self, _is_static);
                }
            }
            break;

        // keepalive messages from matchers to gateway, so backup origin matchers can be kept
        // there are two main categories of cases: origin or regular, new or existing        
        case MATCHER_ALIVE:
            {
                if (!isGateway ())
                {
                    LogManager::instance ()->writeLogFile ("[%llu] MATCHER_ALIVE received by non-gateway", _self.id);
                    break;
                }

                // record matcher info
                bool    request_reply;
                world_t world_id;
                Addr    matcher_addr;
                id_t    origin_id;
                size_t  sendsize;
                size_t  recvsize;

                in_msg.extract ((char *)&request_reply, sizeof (bool));
                in_msg.extract (world_id);
                in_msg.extract (matcher_addr);                
                in_msg.extract (origin_id);
                in_msg.extract (sendsize);
                in_msg.extract (recvsize);
                id_t matcher_id = matcher_addr.host_id;

                // first find out which world this matcher belongs to
                if (world_id == 0)
                {
                    if (_matchers.find (origin_id) == _matchers.end () ||
                        _matchers[origin_id].world_id == 0)
                    {
                        LogManager::instance ()->writeLogFile ("[%llu] MATCHER_ALIVE world_id empty, and world_id for origin [%llu] not found, ignore message", _self.id, origin_id);
                        break;
                    }
                    
                    world_id = _matchers[origin_id].world_id;                    
                }

                // send back world info if requested
                if (request_reply)
                {
                    // notify matcher of its world_id
                    Message msg (MATCHER_WORLD_INFO);
                    msg.priority = 1;
                    msg.store (world_id);
                    msg.store (_matchers[origin_id].addr);
                    msg.addTarget (in_msg.from);
                    sendMessage (msg);
                }

                // check if we know this matcher (we should unless it's keepalive)
                // will occur if a live matcher has lost contact for a while, it needs to re-join
                // in order to get in touch with possibly a new origin matcher for this world
                if (_matchers.find (matcher_id) == _matchers.end ())
                {                    
                    if (_origins.find (world_id) != _origins.end ())
                    {
                        LogManager::instance ()->writeLogFile ("[%llu] MATCHER_ALIVE unknown matcher [%llu], re-init the matcher with its origin [%llu]", _self.id, matcher_id, _origins[world_id]);
                        initMatcher (world_id, _origins[world_id], in_msg.from);
                        break;
                    }
                    else
                    {
                        LogManager::instance ()->writeLogFile ("[%llu] MATCHER_ALIVE unknown matcher [%llu], possibly a lost origin for world [%d], record back", _self.id, matcher_id, world_id);

                        MatcherInfo minfo;
                        minfo.addr      = matcher_addr;
                        minfo.state     = ACTIVE;
                    
                        // insert new record
                        _matchers[matcher_id] = minfo;
                    }
                }
                
                // update info for keepalive record
                MatcherInfo &info = _matchers[matcher_id];
                info.time       = _net->getTimestamp ();                
                info.world_id   = world_id;

                // check for consistency between recorded matcher & sent info
                // NOTE that world_id may be updated for a new origin matcher
                if (info.addr != matcher_addr)
                {
                    LogManager::instance ()->writeLogFile ("[%llu] MATCHER_ALIVE matcher [%llu] sent address not matching with record, update record", _self.id, matcher_id);
                    info.addr = matcher_addr;
                }

                // for origin matchers
                // NOTE: it's possible two or more matchers both claim to be the origin matchers for a world
                if (matcher_id == origin_id)
                {
                    // case 1: new origin matcher
                    if (info.state == PROMOTING)
                    {
                        info.state = ORIGIN;
                        info.world_id = world_id;
                        LogManager::instance ()->writeLogFile ("MATCHER_ALIVE: matcher [%llu] promoted as origin for world [%u]\n", matcher_id, world_id);
                    }
                    // case 2: an unresponding origin matcher responds again
                    //         (equivalent to a regular matcher thinking that it's the origin
                    else if (info.state != ORIGIN)
                    {                        
                        // if mapping doesn't exist, means it's a origin that has lost contact
                        if (_origins.find (world_id) == _origins.end ())
                        {
                            LogManager::instance ()->writeLogFile ("MATCHER_ALIVE: matcher [%llu] re-assigned as origin for world [%u]\n", matcher_id, world_id);
                            info.state = ORIGIN;
                        }
                        // otherwise we should demote this previous origin
                        else
                        {                        
                            LogManager::instance ()->writeLogFile ("MATCHER_ALIVE: matcher [%llu] demoted from origin for world [%u]\n", matcher_id, world_id);                        
                            initMatcher (world_id, _origins[world_id], in_msg.from);
                            break;
                        }
                    }

                    // notify clients of this origin matcher, if any pending
                    map<world_t, vector<id_t> *>::iterator it = _requests.find (world_id);
                    if (it != _requests.end ())
                    {
                        // send reply to joining client
                        // TODO: combine with response in JOIN?
                        // NOTE: we send directly back, as this is a gateway response
                        Message msg (NOTIFY_MATCHER);
                        msg.priority = 1;
                        msg.msggroup = MSG_GROUP_VAST_CLIENT;
                        msg.store (info.addr);
                        msg.targets = *it->second;
                        sendMessage (msg);
                    
                        LogManager::instance ()->writeLogFile ("MATCHER_ALIVE: notify clients with origin matcher [%llu] on world (%u) for clients:\n", matcher_id, world_id);
                        for (size_t i=0; i < msg.targets.size (); i++)
                            LogManager::instance ()->writeLogFile ("[%llu]\n", msg.targets[i]);
                    
                        // erase requesting clients
                        delete it->second;
                        _requests.erase (it);
                    }

                    // record or replace world to origin mapping
                    if (_origins.find (world_id) == _origins.end ())
                    {
                        LogManager::instance ()->writeLogFile ("MATCHER_ALIVE: world [%u]'s origin recorded as [%llu]", world_id, matcher_id);
                        _origins[world_id] = matcher_id;
                    }
                    
                    else if (_origins[world_id] != matcher_id)
                    {
                        // should not happen by current design
                        LogManager::instance ()->writeLogFile ("MATCHER_ALIVE: world [%u]'s origin replaced by [%llu] from [%llu] *shouldn't happen*", world_id, matcher_id, _origins[world_id]);
                        _origins[world_id] = matcher_id;
                    }
                }
                // for regular matchers
                else
                {
                    // case 3: new regular matcher (just directly to ACTIVE mode)
                    if (info.state == CANDIDATE || info.state == PROMOTING)
                    {
                        info.state = ACTIVE;
                        LogManager::instance ()->writeLogFile ("MATCHER_ALIVE: matcher '%llu' now promoted as regular matcher for world [%u]\n", matcher_id, world_id);
                    }
                }
            }
            break;

        // learn of the world_id of my current world & origin matcher's address
        case MATCHER_WORLD_INFO:
            {
                world_t world_id;
                Addr    addr;
                in_msg.extract (world_id);
                in_msg.extract (addr);

                LogManager::instance ()->writeLogFile ("[%llu] MATCHER_WORLD_INFO: new world_id [%u] previous [%u]", _self.id, world_id, _world_id);
                
                _world_id    = world_id;                
                _origin_addr = addr;
            }
            break;

        // a client asks for origin matcher
        case JOIN:
            {
                if (isGateway () == false)
                {
                    LogManager::instance ()->writeLogFile ("JOIN request received by non-gateway\n");
                    break;
                }

                // extract the specific world the client tries to join
                world_t world_id;
                in_msg.extract (world_id);

                LogManager::instance ()->writeLogFile ("Gateway [%llu] JOIN from [%llu] on world (%u)\n", _self.id, in_msg.from, world_id);
                
                // '0' means to let gateway to assign, '1' is the default worldID, others can be assigned
                if (world_id == 0)
                {
                    // TODO: possible infinite loop if all world IDs are used up
                    while (_origins.find (_world_counter) != _origins.end ())
                    {
                        _world_counter++;

                        // wrap over check
                        if (_world_counter == 0)
                            _world_counter = VAST_DEFAULT_WORLD_ID+1;
                    }

                    world_id = _world_counter++;
                }

                // find the address of the origin matcher
                // TODO: send message to start up a new origin matcher, if the world is not instanced yet
                if (_origins.find (world_id) == _origins.end ())
                {
                    printf ("Gateway [%llu] JOIN: creating origin matcher for world (%u)\n", _self.id, world_id);

                    // promote one of the spare potential nodes
                    Addr new_origin;

                    // TODO: if findCandidate () fails, insert new matcher hosted by gateway?
                    // NOTE: we use a loop as it's possible candidates already failed
                    bool promote_success = false;
                    while (findCandidate (new_origin))
                    {                                       
                        // send promotion message
                        notifyMapping (new_origin.host_id, &new_origin);

                        // record the init if send was success
                        if (initMatcher (world_id, new_origin.host_id, new_origin.host_id))
                        {                                                                
                            //_promote_requests[new_node.id] = requester;
                            promote_success = true;

                            // we still specify the world to matcher_id mapping, 
                            // so other clients wishing to join this world can be attached to the request list
                            _origins[world_id] = new_origin.host_id;
                            break;
                        }
                    }

                    if (promote_success == false)
                    {
                        LogManager::instance ()->writeLogFile ("Gateway JOIN: no candidate matchers promoted, for client [%llu] requesting world: %d\n", in_msg.from, world_id);
                    }
                    else
                    {
                        storeRequestingClient (world_id, in_msg.from);
                    }
                }
                // if origin matcher for the requested world exists, reply the client
                else
                {                                    
                    id_t matcher_id = _origins[world_id];

                    if (_matchers.find (matcher_id) == _matchers.end ())
                    {
                        LogManager::instance ()->writeLogFile ("Gateway JOIN: origin matcher [%llu] cannot be found for world [%u]\n", matcher_id, world_id);

                        if (matcher_id == 0)
                        {
                            // TODO: find what causes this
                            LogManager::instance ()->writeLogFile ("Gateway JOIN: matcher_id = 0, should not happen, remove it\n");
                            _origins.erase (world_id);
                        }
                        break;
                    }

                    // if the matcher is still in the process of being promoted, record client request
                    if (_matchers[matcher_id].state == PROMOTING)
                    {
                        storeRequestingClient (world_id, in_msg.from);
                        break;
                    }
                                        
                    // send reply to joining client
                    // NOTE: that we send directly back, as this is a gateway response
                    Message msg (NOTIFY_MATCHER);
                    msg.priority = 1;
                    msg.msggroup = MSG_GROUP_VAST_CLIENT;
                    msg.store (_matchers[matcher_id].addr);
                    msg.addTarget (in_msg.from);
                    sendMessage (msg);

                    printf ("Gateway JOIN replies [%llu] with origin matcher [%llu] on world (%u)\n", in_msg.from, _origins[world_id], world_id);
                }
            }
            break;

        case LEAVE:
            {
                subscriberDisconnected (in_msg.from);
            }
            break;

        // subscription request for an area
        case SUBSCRIBE:
            {               
                // NOTE: we allow sending SUBSCRIBE for existing subscription if
                //       the subscription has updated (for example, the relay has changed)

                printf ("[%llu] VASTMatcher SUBSCRIBE from [%llu]\n", _self.id, in_msg.from);
                Subscription sub;
                in_msg.extract (sub);

                Voronoi *voronoi = _VSOpeer->getVoronoi ();

                // find the closest neighbor and forward
                id_t closest = voronoi->closest_to (sub.aoi.center);

                // check if we should accept this subscription or forward
                if (voronoi->contains (_self.id, sub.aoi.center) || 
                    closest == _self.id)
                {
                    // slight increase AOI radius to avoid client-side ghost objects
                    sub.aoi.radius += SUBSCRIPTION_AOI_BUFFER;

                    // assign a unique subscription number if one doesn't exist, or if the provided one is not known
                    // otherwise we could re-use a previously assigned subscription ID
                    // TODO: potentially buggy? (for example, if the matcher that originally assigns the ID, leaves & joins again, to assign another subscription the same ID?)
                    // TODO: may need a way to periodically re-check ID with the assigning entry point
                    //if (sub.id == NET_ID_UNASSIGNED || _subscriptions.find (sub.id) == _subscriptions.end ()) 
                    // IMPORTANT NOTE: we assume a provided subscription ID is good, and re-use it
                    //                 if we only re-use if the ID already exists in record (e.g. check existence in _subscriptions)
                    //                 then must make sure the client app also updates the subID, otherwise the movement would be invalid
                    if (sub.id == NET_ID_UNASSIGNED) 
                        sub.id = _net->getUniqueID (ID_GROUP_VON_VAST);
                    else
                        printf ("VASTMatcher [%llu] using existing ID [%llu]\n", in_msg.from, sub.id);

                    // by default we own the client subscriptions we create
                    // we may simply update existing subscription when clients re-subscribe due to matcher failure
                    map<id_t, Subscription>::iterator it = _subscriptions.find (sub.id);
                    if (it == _subscriptions.end ()) 
                        addSubscription (sub, true);
                    else
                    {
                        bool is_owner = true;

                        updateSubscription (sub.id, sub.aoi, 0, &sub.relay, &is_owner);
                    }

                    // notify relay of the client's subscription -> hostID mapping
                    Message msg (SUBSCRIBE_NOTIFY);
                    msg.priority = 1;
                    msg.msggroup = MSG_GROUP_VAST_RELAY;
                    msg.store (sub.id);
                    msg.store (sub.host_id);
                    msg.addTarget (sub.relay.host_id);
                    sendMessage (msg);

                    // send back acknowledgement of subscription to client via the relay
                    // NOTE: acknowledge should be sent via relay in general,
                    //       as the subscribe request could be forwarded many times
                    msg.clear (SUBSCRIBE_R);
                    msg.priority = 1;

                    // store both the assigned subscription ID, and also this matcher's address
                    // (so the client may switch the current matcher)
                    msg.store (sub.id);
                    msg.store (_self.addr);                    
                    
                    msg.addTarget (sub.id);
                    sendClientMessage (msg);

                    /*
                    // if the client connects directly to me, send reply directly
                    if (sub.host_id == in_msg.from)
                        sendClientMessage (msg, in_msg.from);
                    //otherwise send via its relay
                    else
                    {
                        msg.addTarget (sub.id);
                        sendClientMessage (msg);
                    }
                    */

                    // erase closest matcher record, so that the subscribing client will be notified again
                    // this occurs when the client is re-subscribing to a substitute matcher in case of its current matcher's failure
                    if (_closest.find (sub.id) != _closest.end ())
                        _closest.erase (sub.id);

                    // record the subscription request                    
                    LogManager::instance ()->writeLogFile ("VASTMatcher: SUBSCRIBE request from [%llu] success\n", in_msg.from);
                }
                else
                {
                    // forward the message to neighbor closest to the subscribed point
                    in_msg.reset ();
                    in_msg.targets.clear ();
                    in_msg.addTarget (closest);                         

                    sendMessage (in_msg);
                }
            }
            break;

        // move an existing subscription area to a new place
        case MOVE:
        case MOVE_F:
            {                
                // extract subscripton id first
                id_t sub_id;
                in_msg.extract (sub_id);

                timestamp_t send_time = 0;  // time of sending initial MOVE

#ifdef VAST_RECORD_LATENCY
                in_msg.extract (send_time);
#endif

                VONPosition pos;
                in_msg.extract ((char *)&pos, sizeof (VONPosition));
                
                Area new_aoi; 
                new_aoi.center.x = pos.x;
                new_aoi.center.y = pos.y;

                // if radius is also updated
                if (in_msg.msgtype == MOVE_F)
                {
                    in_msg.extract ((char *)&new_aoi.radius, sizeof (length_t));
                    in_msg.extract ((char *)&new_aoi.height, sizeof (length_t));
                }

                updateSubscription (sub_id, new_aoi, send_time);
            }
            break;

        case PUBLISH:
            {
                in_msg.msgtype = (app_msgtype << VAST_MSGTYPE_RESERVED) | MESSAGE;

                // extract publication layer (from end of message)
                layer_t layer;
                Area    area;
                
                in_msg.extract (layer, true);
                in_msg.extract (area, true);

                in_msg.reset ();

                // check through known subscribers
                // TODO: forward publication to neighbors for area publication, or 
                //       for subscribers hosted by neighboring matchers
                in_msg.targets.clear ();

                // the layer of the receiver
                //layer_t target_layer;
        
                // find the peer that can act as initial point for this publication 
                // TODO: find a more efficient / better way

                map<id_t, Subscription>::iterator it = _subscriptions.begin ();               
                for (; it != _subscriptions.end (); it++)
                {                
                    Subscription &sub = it->second;

                    // if the neighbor is 
                    //      1) at the same layer as publisher
                    //      2) interested in this publication
                    // then add as target

                    if (layer == sub.layer && sub.aoi.overlaps (area.center))
                        in_msg.addTarget (sub.id);
                }                

                if (in_msg.targets.size () > 0)
                {
                    vector<id_t> failed_targets;
                    
                    // send the message to relay
                    sendClientMessage (in_msg, 0, &failed_targets);
                    removeFailedSubscribers (failed_targets);
                }
#ifdef DEBUG_DETAIL
                else
                    printf ("VASTMatcher::handleMessage PUBLISH, no peer covers publication at (%d, %d)\n", (int)area.center.x, (int)area.center.y);
#endif
                   
            }
            break;

        // process messages sent by clients to particular targets
        case SEND:
            {
                // extract targets 
                listsize_t n;
                in_msg.extract (n, true);

                // restore targets
                in_msg.targets.clear ();
                id_t target;
                for (size_t i=0; i < n; i++)
                {
                    in_msg.extract (target, true);
                    in_msg.addTarget (target);
                }

                // deliver to the the targets' relays
                in_msg.reset ();
                in_msg.msgtype = (app_msgtype << VAST_MSGTYPE_RESERVED) | MESSAGE;

                vector<id_t> failed_targets;                
                sendClientMessage (in_msg, 0, &failed_targets);
                removeFailedSubscribers (failed_targets);
            }
            break;

        // message specific to the origin matcher
        case ORIGIN_MESSAGE:
            {
                // check if we've gotten origin matcher's address via MATCHER_WORLD_INFO
                if (_origin_id != _origin_addr.host_id)
                {
                    LogManager::instance ()->writeLogFile ("ORIGIN_MESSAGE origin [%llu] address not known, cannot send for [%llu]\n", _origin_id, in_msg.from);
                    break;
                }

                // forward to the origin matcher's client component directly
                in_msg.msgtype = (app_msgtype << VAST_MSGTYPE_RESERVED) | MESSAGE;
                in_msg.msggroup = MSG_GROUP_VAST_CLIENT;

                // prepare target
                in_msg.addTarget (_origin_id);

                // if we don't yet have connection to origin, make sure we have
                if (_net->isConnected (_origin_id) == false)
                    notifyMapping (_origin_addr.host_id, &_origin_addr); 

                // if at least one message is sent, then it's successful
                // NOTE: there's no handling in case of failure
                sendMessage (in_msg);
            }
            break;

        // transfer of subscription
        case SUBSCRIBE_TRANSFER:
            {
                // extract # of subscriptions
                listsize_t n;
                in_msg.extract (n, true);
                Subscription sub;

                int success = 0;
                for (listsize_t i=0; i < n; i++)
                {
                    in_msg.extract (sub);
                    
                    // NOTE by default a transferred subscription is not owned
                    //      it will be owned if a corresponding ownership transfer is sent (often later)                    
                    if (_subscriptions.find (sub.id) == _subscriptions.end ())
                    {
                        if (addSubscription (sub, false))
                            success++;
                    }
                    else
                    {
                        // update only
                        // NOTE that we do not change any ownership status,
                        //      but time sent by remote host is used
                        if (updateSubscription (sub.id, sub.aoi, sub.time, &sub.relay))
                        {
                            success++;

                            // remove all current AOI neighbor records, so we can notify the new Client of more accurate neighbor states
                            //_subscriptions[sub.id].clearNeighbors ();
                        }
                    }
                }
#ifdef DEBUG_DETAIL
                printf ("[%llu] VASTMatcher::handleMessage () SUBSCRIBE_TRANSFER %d sent %d success\n", _self.id, (int)n, success);
#endif
            }
            break;

        case SUBSCRIBE_UPDATE:
            {
                // extract # of subscriptions
                listsize_t n;
                in_msg.extract (n, true);
                id_t sub_id;
                Area aoi;

                vector<id_t> missing_list;

                for (listsize_t i=0; i < n; i++)
                {
                    // TODO: also send time to ensure only the lastest gets used
                    in_msg.extract (sub_id);
                    in_msg.extract (aoi);
                   
                    if (updateSubscription (sub_id, aoi, 0) == false)
                    {
                        // subscription doesn't exist, request the subscription 
                        // this happens if the subscription belongs to a neighboring matcher but AOI overlaps with my region
                        missing_list.push_back (sub_id);                        
                    }
                }

                if (missing_list.size () > 0)
                    // TODO: try not to use requestObjects publicly?
                    _VSOpeer->requestObjects (in_msg.from, missing_list);
            }
            break;

        case NEIGHBOR_REQUEST:
            {
                id_t sub_id;
                id_t neighbor_id;
                in_msg.extract (sub_id);
                in_msg.extract (neighbor_id);

                if (_subscriptions.find (sub_id) != _subscriptions.end ())
                {
                    _subscriptions[sub_id].clearStates (neighbor_id);
                }
            }
            break;

        case STAT:
            {
                if (isGateway () == false)
                {
                    LogManager::instance ()->writeLogFile ("VASTMatcher::handleMessage STAT only gateway can record stat (I'm not...)\n");
                    break;
                }

                layer_t type;
                
                // extract message type
                in_msg.extract (type);

                switch (type)
                {
                // JOIN
                case 1:
                    LogManager::instance ()->writeLogFile ("GW-STAT: [%llu] joins\n", in_msg.from);
                    break;
        
                // LEAVE
                case 2:
                    LogManager::instance ()->writeLogFile ("GW-STAT: [%llu] leaves\n", in_msg.from);
                    break;

                // message type unrecognized
                default:
                    LogManager::instance ()->writeLogFile ("VASTMatcher::handleMessage STAT unrecognized type: %d\n", (int)type);
                    break;
                }
            }
            break;

        /*case SYNC_CLOCK:
            {
                if (isGateway () == false)
                {
                    LogManager::instance ()->writeLogFile ("VASTMatcher::handleMessage () SYNC_CLOCK can only be handled by gateway\n");
                    break;
                }

                // send back the sender and gateway's current timestamp
                timestamp_t sender_time;                
                in_msg.extract (sender_time);
                timestamp_t gateway_time = _net->getTimestamp ();
                
                Message msg (SYNC_CLOCK);
                msg.store (sender_time);
                msg.store (gateway_time);
                msg.msggroup = MSG_GROUP_VAST_CLIENT;

                msg.addTarget (in_msg.from);
        
                sendMessage (msg);            
            }
            break;*/

        // process universal VASTnet message, msgtype = 0
        case DISCONNECT:
            {   
                // NOTE: the disconnecting host may be either a regular client
                //       or a matcher host. In the latter case, we should also
                //       notify the VONpeer component of its departure
                                
                // removing a simple client (subscriber)
                subscriberDisconnected (in_msg.from);       

                // NOTE: it's possible the disconnecting host has both client & matcher components
                // so we still need to do the following processing

                // notify the VONPeer component
                in_msg.msgtype = VSO_DISCONNECT;
                _VSOpeer->handleMessage (in_msg);

                // remove a matcher
                refreshMatcherList ();

                // if I'm gateway, also check for origin matcher disconnection
                if (isGateway () && isOriginMatcher (in_msg.from))
                    originDisconnected (in_msg.from);
            }
            break;

        default:            
            {
                printf ("[%llu] VASTMatcher unrecognized msgtype: %d\n", _self.id, in_msg.msgtype);
            }
            break;
        }
        
        return true;
    }

    // perform routine tasks after all messages have been handled 
    //  (i.e., check for reply from requests sent)
    void 
    VASTMatcher::postHandling ()
    {
        // perform regular tick and check if our VONpeer has joined
        if (_VSOpeer == NULL)
            return;
        
        // we perform matcher tasks only if VSOpeer has joined
        if (_VSOpeer->isJoined ())
        {         
            // update the list of neighboring matchers
            refreshMatcherList ();
        
            // check to call additional matchers for load sharing
            if (_is_static == false)            
                checkOverload ();
        
            // determine the AOI neighbors for each subscriber 
            refreshSubscriptionNeighbors ();
                
            // send neighbor updates to VASTClients        
            notifyClients ();
        
            // TODO: adjust client_limit based on resource (bandwidth) availability
        
            // perform per-second tasks
            timestamp_t now = _net->getTimestamp ();
            if (now >= _next_periodic)
            {
                // NOTE the # of timestamps may not equal to the exact seconds
                _next_periodic = now + _net->getTimestampPerSecond ();
        
                // report loading to neighbors
                //reportLoading ();
        
                // report stat collected to gateway
                //reportStat ();

                // go through each owned subscription and refresh it, 
                // so that the subscription will stay alive (without the client having
                // to send keepalive)
                map<id_t, Subscription>::iterator it = _subscriptions.begin (); 
                for (; it != _subscriptions.end (); it++)
                {
                    Subscription &sub = it->second;
        
                    if (_VSOpeer->isOwner (sub.id))
                    {
                        updateSubscription (sub.id, sub.aoi, sub.time);
                    }
                }

                // auto-send updates for owned subscriptions
                sendKeepAlive ();

                if (isGateway ())
                    removeExpiredMatchers ();
            }
        }

        // allow VSOpeer to perform routine tasks
        _VSOpeer->tick ();
    }

    // record a new subscription at this VASTMatcher
    bool     
    VASTMatcher::addSubscription (Subscription &sub, bool is_owner)
    {      
        map<id_t, Subscription>::iterator it = _subscriptions.find (sub.id);

        // do not add if there's existing subscription
        if (it != _subscriptions.end ()) 
            return false;
        
        // record a new subscription
        sub.dirty = true;
        _subscriptions[sub.id] = sub;
        it = _subscriptions.find (sub.id);

        // update the VSOpeer
        _VSOpeer->insertSharedObject (sub.id, sub.aoi, is_owner, &it->second);
        
        // notify network layer of the subscriberID -> relay hostID mapping
        // also register the relay
        notifyMapping (sub.relay.host_id, &sub.relay);
        notifyMapping (sub.id, &sub.relay);

        return true;
    }

    bool 
    VASTMatcher::removeSubscription (id_t sub_no)
    {
        if (_subscriptions.find (sub_no) == _subscriptions.end ())
            return false;

        _VSOpeer->deleteSharedObject (sub_no);
        _subscriptions.erase (sub_no);

        _closest.erase (sub_no);

        if (_replicas.find (sub_no) != _replicas.end ())
        {
            delete _replicas[sub_no];
            _replicas.erase (sub_no);
        }

        return true;
    }

    // update a subscription content
    // time = 0 means no update & no checking
    bool 
    VASTMatcher::updateSubscription (id_t sub_no, Area &new_aoi, timestamp_t sendtime, Addr *relay, bool *is_owner)
    {
        map<id_t, Subscription>::iterator it = _subscriptions.find (sub_no);
        if (it == _subscriptions.end ())
            return false;
                
        Subscription &sub = it->second;

        // update record only if update occurs later than existing record
        if (sendtime != 0)
        {
            if (sendtime < sub.time)
                return false;
            else 
                sub.time = sendtime;
        }
        
        sub.aoi.center = new_aoi.center;
        if (new_aoi.radius != 0)
            sub.aoi.radius = new_aoi.radius;
        if (new_aoi.height != 0)
            sub.aoi.height = new_aoi.height;

        sub.dirty = true;

        // update states in shared object management
        _VSOpeer->updateSharedObject (sub_no, sub.aoi, is_owner);

        // update relay, if changed
        if (relay != NULL &&
            sub.relay.host_id != relay->host_id)
        {            
            sub.relay = *relay;
            notifyMapping (sub.id, &sub.relay);

            // if I'm owner, need to propagate relay change to all affected 
            if (_VSOpeer->isOwner (sub.id) && 
                _replicas.find (sub.id) != _replicas.end ())
            {
                // clear the record of the hosts already received full replicas
                // so next time when sending the subscription update, 
                // full update (including the relay info) will be sent
                delete _replicas[sub.id];
                _replicas.erase (sub.id);
                
            }
        }

        return true;
    }

    // send a full subscription info to a neighboring matcher
    // returns # of successful transfers
    int 
    VASTMatcher::transferSubscription (id_t target, vector<id_t> &sub_list, bool update_only)
    {
        // error check, if the neighbor matcher indeed exists
        if (_neighbors.find (target) == _neighbors.end ())
            return 0;

        listsize_t num_transfer = 0;
     
        Message msg (SUBSCRIBE_TRANSFER);
        msg.priority = 1;                 
        msg.addTarget (target);

        for (size_t i=0; i < sub_list.size (); i++)
        {
            // extract subscription id
            id_t sub_id = sub_list[i];

            map<id_t, Subscription>::iterator it = _subscriptions.find (sub_id);
            if (it != _subscriptions.end ())
            {
                Subscription &sub = it->second;
                
                // store full subscription or update only
                if (update_only)
                {
                    msg.store (sub_id);
                    msg.store (sub.aoi);
                }
                else
                    msg.store (sub);
                
                num_transfer++;
            }          
        }

        if (num_transfer > 0)
        {
            if (update_only)
                msg.msgtype = SUBSCRIBE_UPDATE;

            // actually send out
            msg.store (num_transfer);
            sendMessage (msg);
        }

        return (int)num_transfer;
    }

    // update the neighbors for each subscriber
    // 'neighbor' is defined as other clients subscribing at the same layer & within AOI
    void
    VASTMatcher::refreshSubscriptionNeighbors ()
    {    
        // nodes within the Voronoi        
        Voronoi *voronoi = _VSOpeer->getVoronoi ();

        // clear visible neighbor states for all subscribers
        map<id_t, Subscription>::iterator it = _subscriptions.begin (); 
        for (; it != _subscriptions.end (); it++)
        {
            it->second.clearStates ();

            // build up a map for subscribers within the Voronoi region of this matcher
            if (voronoi->contains (_self.id, it->second.aoi.center))
                it->second.in_region = true;
        }

        // loop through all known neighbors, and check against all other known subscribers
        // for visibility
        
        it = _subscriptions.begin (); 

        for (; it != _subscriptions.end (); it++)
        {
            Subscription &sub1 = it->second;

            // check each other subscriber till the end of list
            // NOTE that we're checking for mutual visibility at once
            map<id_t, Subscription>::iterator it2 = it;
            it2++;                                    

            for (; it2 != _subscriptions.end (); it2++)
            {
                // check for overlap separately for the two subscribers                
                // NOTE: that if layer 0 is subscribed, then all known subscribers within the Voronoi region of this matcher are reported
                Subscription &sub2 = it2->second;

                if (_VSOpeer->isOwner (sub1.id))
                {
                    // if subscriber 1 can see subscriber 2
                    if ((sub1.layer == 0 && sub2.in_region == true) || 
                        (sub1.layer == sub2.layer && sub1.aoi.overlaps (sub2.aoi.center)))
                        sub1.addNeighbor (&sub2);
                }

                if (_VSOpeer->isOwner (sub2.id))
                {
                    // if subscriber 2 can see subscriber 1
                    if ((sub2.layer == 0 && sub1.in_region == true) || 
                        (sub2.layer == sub1.layer && sub2.aoi.overlaps (sub1.aoi.center)))
                        sub2.addNeighbor (&sub1);
                }
            }

            // add self (NOTE: must add self before clearing neighbors, or self will be cleared)
            sub1.addNeighbor (&sub1);

            // remove neighbors no longer within AOI
            sub1.clearInvisibleNeighbors ();
        }        
    } 

    // check if a disconnecting host contains subscribers
    bool 
    VASTMatcher::subscriberDisconnected (id_t host_id)
    {
        map<id_t, Subscription>::iterator it = _subscriptions.begin ();

        vector<id_t> remove_list;

        // loop through all known subscriptions, and remove the subscription that matches
        for (; it != _subscriptions.end (); it++)
        {
            Subscription &sub = it->second;

            if (sub.host_id == host_id)
                remove_list.push_back (sub.id);
        }

        if (remove_list.size () > 0)
        {
            for (size_t i=0; i < remove_list.size (); i++)
                removeSubscription (remove_list[i]);

            return true;
        }

        return false;
    }

    // notify the gateway that I can be available as an origin matcher
    bool 
    VASTMatcher::notifyCandidacy ()
    {
        // register with gateway as a candidate node, as agreed by policy
        if (isCandidate () == false)
            return false;
        
        Message msg (MATCHER_CANDIDATE);
        msg.priority = 1;
        msg.store (_self.addr);
        msg.addTarget (_gateway.host_id);
        sendMessage (msg);

        return true;   
    }

    // check with VON to refresh current neighboring matchers
    void 
    VASTMatcher::refreshMatcherList ()
    {   
        vector<Node *> &neighbors = _VSOpeer->getNeighbors ();

        _neighbors.clear ();

        // convert neighbors into map form (can be lookup)
        for (size_t i=1; i < neighbors.size (); i++)        
            _neighbors[neighbors[i]->id] = neighbors[i];       
    }    

    // check to call additional matchers for load balancing
    void 
    VASTMatcher::checkOverload () 
    {
        //int n = _subscriptions.size ();
        int n = _VSOpeer->getOwnedObjectSize ();

        // collect stats on # of agents 
        if (n > 0)
            _stat_sub.addRecord (n); 

        // check if no limit is imposed
        if (_overload_limit == 0)
            return;

        // if # of subscriptions exceed limit, notify for overload
        else if (n > _overload_limit)
            _VSOpeer->notifyLoading (((float)n / (float)_overload_limit));
        
        // underload
        // TODO: if UNDERLOAD threshold is not 0,
        // then we need proper mechanism to transfer subscription out before matcher departs
        else if (n == 0)
            _VSOpeer->notifyLoading ((float)(-1));
        
        // normal loading
        else
            _VSOpeer->notifyLoading (0);
    }

    // re-send updates of our owned objects so they won't be deleted
    void 
    VASTMatcher::sendKeepAlive ()
    {
        // if I'm a working matcher, then notify gateway periodically of my alive status
        // NOTE: that we must first know our origin before sending KEEPALIVE
        if (isActive () && _origin_id != 0 && --_matcher_keepalive <= 0)
        {
            // reset countdown for notifications
            _matcher_keepalive = TIMEOUT_MATCHER_KEEPALIVE;

            // obtain bandwidth usage stat            
            size_t sendsize = _net->getSendSize ();
            size_t recvsize = _net->getReceiveSize ();

            // send keepalive to gateway
            // TODO: consider to send it using UDP 
            // (to avoid excessive TCP connections when large number of matchers exist)
            Message msg (MATCHER_ALIVE);
            msg.priority = 1;

            // if world_id or origin matcher's address is unknown, request from gateway
            bool request_reply = false;
            if (_world_id == 0 || _origin_id != _origin_addr.host_id)
                request_reply = true;

            msg.store ((char *)&request_reply, sizeof (bool));
            msg.store (_world_id);
            msg.store (_self.addr);
            msg.store (_origin_id);
            msg.store (sendsize);
            msg.store (recvsize);

            msg.addTarget (_gateway.host_id);
            sendMessage (msg);
        }
    }

    // remove matchers that have not sent in keepalive
    void 
    VASTMatcher::removeExpiredMatchers ()
    {
        // if I'm gateway, remove expired matchers (keepalive not received)
        // to make sure backup matchers are alive        

        // TODO: also check if the backup matchers for a particular world is too many 
        //       should keep it to LIMIT_LIVE_MATCHERS_PER_WORLDs

        vector<id_t> remove_list;            
        map<id_t, MatcherInfo>::iterator it = _matchers.begin ();        

        timestamp_t now = _net->getTimestamp ();
        timestamp_t expire_time = (timestamp_t)((TIMEOUT_MATCHER_KEEPALIVE * 1.5) * _net->getTimestampPerSecond ());

        // record timeout matchers
        // TODO: right now we skip timeout check for any matcher CANDIDATE, but should they also be time-checked?
        for ( ; it != _matchers.end (); it++)
        {                        
            MatcherInfo &info = it->second;
            if (info.state == ACTIVE && now > (info.time + expire_time))
                remove_list.push_back (it->first);
        }        

        // remove timeout'd matchers
        for (size_t i=0; i < remove_list.size (); i++)
        {       
            id_t matcher_id = remove_list[i];
            LogManager::instance ()->writeLogFile ("Matcher [%llu] timeout, remove it. world_id: %u\n", matcher_id, _matchers[matcher_id].world_id);

            if (isOriginMatcher (matcher_id))
                originDisconnected (matcher_id);
            else
                _matchers.erase (matcher_id);            
        }

        //LogManager::instance ()->writeLogFile ("GW-STAT: %u worlds at timestamp (%lu)\n", _alives.size (), now);
    }

    // tell clients updates of their neighbors (changes in other nodes subscribing at same layer)
    void
    VASTMatcher::notifyClients ()
    {
        // check over updates on each Peer's neighbors and notify the 
        // respective Clients of the changes (insert/delete/update)        
        Message msg (NEIGHBOR);

        Node node;              // neighbor info
        vector<id_t> failed;    // clients whose message cannot be sent

        map<id_t, Subscription>::iterator it = _subscriptions.begin (); 
        for (; it != _subscriptions.end (); it++)
        {
            id_t sub_id  = it->first;
            Subscription &sub = it->second;
                       
            map<id_t, NeighborUpdateStatus>& update_status = sub.getUpdateStatus ();
                        
            if (update_status.size () == 0)
                continue;

            // we only notify objects we own, erase updates for other non-own objects
            if (_VSOpeer->isOwner (sub.id) == false)
            {            
                //printf ("VASTMatcher::notifyClients () updates exist for non-owned subscription [%llu]\n", sub.id);
                // TODO: update_status for non-own subscribers occur during refreshSubscriptionNeighbors (), try to avoid it?
                continue;
            }

            // see if we need to notify the client its closest alternative matcher 
            id_t closest = _VSOpeer->getClosestEnclosing (sub.aoi.center);                                    

            if (closest != 0 && _closest[sub.id] != closest)
            {
                _closest[sub.id] = closest;
    
                msg.clear (NOTIFY_CLOSEST);
                msg.priority = 1;
                msg.store (_neighbors[closest]->addr);

                // send a direct message to notify the client (faster)
                sendClientMessage (msg, sub.host_id);
            }
        
            // prepare to notify client of its AOI neighbors
            msg.clear (NEIGHBOR);
            msg.priority = 1;
            
            listsize_t listsize = 0;
            // loop through each neighbor for this peer and record its status change
            // NOTE: size is stored at the end
            map<id_t, NeighborUpdateStatus>::iterator itr = update_status.begin ();
            for (; itr != update_status.end (); itr++)
            {               
                // store each updated neighbor info into the notification messages
                NeighborUpdateStatus status = itr->second;

                // NOTE: we do not update nodes that have not changed
                // TODO: but send periodic keep-alive?
                if (status == UNCHANGED && sub.dirty == false)
                    continue;
                
                listsize++;
                id_t neighbor_id = itr->first;
                
                // obtain neighbor info if it's not a deleted neighbor
                if (status != DELETED)
                {
                    map<id_t, Subscription>::iterator it_neighbor = _subscriptions.find (neighbor_id);
                                    
                    // change neighbor info to DELETED if not exist (something's wrong here)
                    if (it_neighbor == _subscriptions.end ())
                    {
                        printf ("neighbor [%llu] cannot be found when sending neighbor update\n", neighbor_id); 
                        status = DELETED;
                    }
                    else
                    {
                        Subscription &neighbor = it_neighbor->second;
                
                        // convert subscriber info to node info
                        node.id     = neighbor.id;
                        node.addr   = neighbor.relay;
                        node.aoi    = neighbor.aoi;
                        node.time   = neighbor.time;
                    }
                }

                msg.store (neighbor_id);
                msg.store ((char *)&status, sizeof (NeighborUpdateStatus));

                // store actual state update
                switch (status)
                {
                case INSERTED:
                    msg.store (node);
                    break;
                case DELETED:
                    break;
                case UPDATED:
                    //msg.store (*node);
                    // TODO: store only center of AOI (instead of radius too?)
                    //change to udp
                    //msg.reliable=false; 
                    msg.store (node.aoi);
#ifdef VAST_RECORD_LATENCY
                    msg.store (node.time);
#endif
                    break;
                case UNCHANGED:
                    break;
                }
            }

            // we send the update only if there are nodes to update
            if (listsize > 0)
            {
                // store number of entries at the end
                msg.store (listsize);                

#ifdef SEND_NEIGHBORS_VIA_RELAY
                // send via relay
                msg.addTarget (sub_id);
                if (sendClientMessage (msg) == 0)
                {
                    // TODO: relay has failed, should we try to contact client or relay?
                }
#else
                // send directly to client (for faster notify)                
                if (sendClientMessage (msg, sub.host_id) == 0)
                    failed.push_back (sub_id);
#endif
                    
            }

            // at the end we clear the status record for next time-step
            // it's cleared when refreshing neighbor states
            //update_status.clear ();
            
            // clear dirty flag 
            // (IMPORTANT, to prevent UNCHANGED status be continously sent)
            sub.dirty = false;

        } // end for each subscriber      

        removeFailedSubscribers (failed);                

    }

    // handle the failure of a origin matcher
    // NOTE: matcher_id must be a origin matcher
    void
    VASTMatcher::originDisconnected (id_t matcher_id)
    {               
        // determine world_id & remove the matcher record
        if (_matchers.find (matcher_id) == _matchers.end ())
        {
            LogManager::instance ()->writeLogFile ("[%llu] originDisconnected: matcher_id [%llu] not found", _self.id, matcher_id);
            return;
        }            

        world_t world_id = _matchers[matcher_id].world_id;
        _matchers.erase (matcher_id);
        
        // find backup origin matcher for the same world
        // right now we just pick the first available
        // TODO: better ways?
        map<id_t, MatcherInfo>::iterator it = _matchers.begin ();
        for (; it != _matchers.end (); it++)
        {
            MatcherInfo &info = it->second;
            if (info.world_id == world_id && info.state == ACTIVE)
            {
                info.state = ORIGIN;

                LogManager::instance ()->writeLogFile ("originDisconnected: origin [%llu] replaced by [%llu] for world [%u]", _origins[world_id], it->first, world_id);

                // replace mapping from world_id to origin matcher
                _origins[world_id] = it->first;               

                // notify new origin of its promotion
                initMatcher (world_id, info.addr.host_id, info.addr.host_id);

                break;
            }
        }

        // TODO: if no backup matchers are found, promote candidate matcher as new origin matcher
        // but, possible the leaving origin matcher is the last node / client

        // if no backup can be found, simply remove the mapping from world_id to matcher_id
        if (it == _matchers.end ())
        {
            _origins.erase (world_id);
            LogManager::instance ()->writeLogFile ("originDisconnected: no backup origin for world [%u], remove [%llu]", world_id, matcher_id);
        }
    }

    // get a candidate origin matcher to use
    bool 
    VASTMatcher::findCandidate (Addr &new_origin, float level)
    {
        // simply return the first available
        // TODO: better method? (newest, oldest, etc.?)
        map<id_t, MatcherInfo>::iterator it = _matchers.begin ();
        for (; it != _matchers.end (); it++)
        {
            MatcherInfo &info = it->second;
            if (info.state == CANDIDATE)
            {
                new_origin = info.addr;
                info.state = PROMOTING;

                printf ("[%llu] promoting [%llu] as new matcher\n\n", _self.id, new_origin.host_id);

                return true;
            }
        }

        // TODO: do something about it (e.g. create matcher node at gateway)
        printf ("[%llu] no candidate matcher can be found\n", _self.id);

        return false;
    }

    // send a message to clients (optional to include the client's hostID for direct message)
    // returns # of targets successfully sent, optional to return failed targets
    int 
    VASTMatcher::sendClientMessage (Message &msg, id_t client_ID, vector<id_t> *failed_targets)
    {
        // for direct message, we send to a directly connected client (for faster response)
        if (client_ID != NET_ID_UNASSIGNED)
        {
            // perform error check, but note that connections may not be established for a gateway client
            if (msg.targets.size () != 0)
            {
                printf ("VASTMatcher::sendClientMessage targets exist\n");
                return 0;
            }

            msg.addTarget (client_ID);
            msg.msggroup = MSG_GROUP_VAST_CLIENT;    
        }
        else
            msg.msggroup = MSG_GROUP_VAST_RELAY;

        // NOTE: for messages directed to relays, the network layer will do the translation from targets to relay's hostID
        return sendMessage (msg, failed_targets);    
    }


    // deal with unsuccessful send targets
    void 
    VASTMatcher::removeFailedSubscribers (vector<id_t> &list)
    {
        // some targets are invalid, remove the subs
        // TODO: perhaps should check / wait? 
        if (list.size () > 0)
        {
            printf ("VASTMatcher::removeFailedSubscribers () removing failed send targets\n");
           
            // remove failed targets
            for (size_t i=0; i < list.size (); i++)
                removeSubscription (list[i]);
        }
    }

    // initialize a new matcher (also serve to notify existing matcher of new origin)
    bool 
    VASTMatcher::initMatcher (world_t world_id, id_t origin_id, id_t target)
    {
        Message msg (MATCHER_INIT);
        msg.priority = 1;
        msg.store (world_id);
        msg.store (origin_id);
        msg.addTarget (target);

        return (sendMessage (msg) > 0);
    }

    // set the gateway node for this world
    bool
    VASTMatcher::setGateway (const IPaddr &gatewayIP)
    {
        // convert possible "127.0.0.1" to actual IP address
        IPaddr gateway = gatewayIP;
        if (_net->validateIPAddress (gateway) == false)
            return false;
      
        // record gateway first
        _gateway = Addr (net_manager::resolveHostID (&gateway), &gateway);

        return true;
    }

    // check if a node is an origin matcher (gateway-only)
    bool 
    VASTMatcher::isOriginMatcher (id_t id)
    {
        if (_matchers.find (id) != _matchers.end ())
        {
            world_t world_id = _matchers[id].world_id;
            if (_origins.find (world_id) != _origins.end () &&
                _origins[world_id] == id)
                return true;
        }
        return false;
    }

    // store a joining client to the queue of waiting for origin matcher response
    void 
    VASTMatcher::storeRequestingClient (world_t world_id, id_t client_id)
    {
        // as we need to wait for origin matchers' response, 
        // we need to record the requesting client first
        if (_requests.find (world_id) == _requests.end ())
            _requests[world_id] = new vector<id_t>;
    
        // avoid redundent request
        // NOTE: the client_id field indicates hostID 
        // (assuming one subscription per host for now)
        size_t i;
        vector<id_t> &list = *_requests[world_id];
        for (i=0; i < list.size (); i++)
            if (list[i] == client_id)
                break;
        if (i == list.size ())
            list.push_back (client_id);
    }

    // whether the current node can be a spare node for load balancing
    bool 
    VASTMatcher::isCandidate ()
    {
        // currently all nodes with public IP can be candidates
        return (_is_matcher && _net->isPublic ());
    }

    // obtain the ID of the gateway node
    id_t 
    VASTMatcher::getGatewayID ()
    {
        return _gateway.host_id;
    }

    // answer object request from a neighbor node
    // returns # of successful transfers
    int 
    VASTMatcher::copyObject (id_t target, vector<id_t> &obj_list, bool update_only)
    {  
        //return transferSubscription (target, obj_list, update_only);

        
        vector<id_t> full_list;
        vector<id_t> update_list;

        // if update_only is set, determine if it's 1st time copy (copy full)
        if (update_only)
        {
            for (size_t i=0; i < obj_list.size (); i++)
            {
                map<id_t, map<id_t, bool> *>::iterator it = _replicas.find (obj_list[i]);
                
                if (it == _replicas.end ())
                {
                    _replicas[obj_list[i]] = new map<id_t, bool>;
                    it = _replicas.find (obj_list[i]);
                }    
                
                map<id_t, bool> &hostmap = *it->second;
                
                // record not found, insert record & perform full copy
                if (hostmap.find (target) == hostmap.end ())
                {
                    full_list.push_back (obj_list[i]);
                    hostmap[target] = true;
                }
                else
                    update_list.push_back (obj_list[i]);
            }
        }
        else
            full_list = obj_list;

        int num_transfer = 0;
    
        // perform the actual copy of objects (full & update only)
        if (full_list.size () > 0)
            num_transfer += transferSubscription (target, full_list, false);
        if (update_list.size () > 0)
            num_transfer += transferSubscription (target, update_list, true);

        return num_transfer;
        
    }

    // remove an obsolete unowned object
    bool 
    VASTMatcher::removeObject (id_t obj_id)
    {
        return removeSubscription (obj_id);
    }

    // objects whose ownership has transferred to a neighbor node
    bool 
    VASTMatcher::ownershipTransferred (id_t target, vector<id_t> &obj_list)
    {
        if (_neighbors.find (target) == _neighbors.end ())
            return false;

        // notify the clients associated with the subscriptions so they can switch matchers
        Message msg (NOTIFY_MATCHER);
        msg.priority = 1;
        
        msg.store (_neighbors[target]->addr);

        // translate subscriber ID to hostID and sent to clients directly (faster)
        for (size_t i=0; i < obj_list.size (); i++)
        {
            if (_subscriptions.find (obj_list[i]) != _subscriptions.end ())
            {
                sendClientMessage (msg, _subscriptions[obj_list[i]].host_id);
                msg.targets.clear ();
            }
        }

        // NOTE that this will be sent via relays (slower)
        //msg.targets = obj_list;
        //sendClientMessage (msg);

        return true;
    }

    // notify the claiming of an object as mine
    bool 
    VASTMatcher::objectClaimed (id_t sub_id)
    {
        map<id_t, Subscription>::iterator it = _subscriptions.find (sub_id);
        if (it == _subscriptions.end ())
        {
            printf ("VASTMatcher::objectClaimed () subscription [%llu] not found\n", sub_id);
            return false;
        }
        else
        {
            Subscription &sub = it->second;

            // notify the client that I become its matcher
            // NOTE that this message should be sent via relay, as there may not be
            //      direct connection to this client, mapping from sub_id to relay hostID should already exist
            Message msg (NOTIFY_MATCHER);            
            msg.priority = 1;            
            msg.store (_self.addr);
            msg.addTarget (sub.id);

            return (sendClientMessage (msg) > 0);
        }        
    }

    // handle the event of a new VSO node's successful join
    bool 
    VASTMatcher::peerJoined (id_t origin_id)
    {
        // NOTE: world_id is sent as 0, meaning we're simply 
        //       removing the registration of candidate matchers at gateway

        _origin_id = origin_id;

        // send positive reply back to gateway
        sendKeepAlive ();
        
        return true;
    }

    // handle the event of the VSO node's movement
    bool 
    VASTMatcher::peerMoved ()
    {
        return true;
    }

} // end namespace Vast
