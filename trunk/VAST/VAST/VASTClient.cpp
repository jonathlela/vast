

#include "VASTClient.h"
#include "MessageQueue.h"

namespace Vast
{   
    // TODO: change to reflect with current
    char VAST_MESSAGE[][20] = 
    {
        "QUERY",
        "QUERY_REPLY",
        "JOIN",
        "JOIN_REPLY",
        "LEAVE",
        "SUBSCRIBE",
        "SUBSCRIBE_R",
        "PUBLISH",        
        "MOVE", 
        "MOVE_F",
        "NEIGHBOR",
        "MESSAGE"
    };

    VASTClient::VASTClient (VASTRelay *relay)
    {
        _relay          = relay;
        _state          = ABSENT;
        _lastmsg        = NULL;
        _sub.id         = NET_ID_UNASSIGNED;
        _matcher_id     = NET_ID_UNASSIGNED;
    }

    VASTClient::~VASTClient ()
    {
        // release memeory
        if (_lastmsg != NULL)
            delete _lastmsg;

        size_t i;
        for (i=0; i < _msglist.size (); i++)
            delete _msglist[i];

        for (i=0; i < _neighbors.size (); i++)
            delete _neighbors[i];
    }

    // join the overlay
    bool
    VASTClient::join (const IPaddr &gatewayIP)
    {
        if (_state != ABSENT)
            return false;
                
        // convert possible "127.0.0.1" to actual IP address
        IPaddr gateway = gatewayIP;
        _net->validateIPAddress (gateway);

        Addr addr (VASTnet::resolveHostID (&gateway), &gateway);
        //((MessageQueue *)_msgqueue)->setGateway (addr);
     
        // gateway is the initial matcher
        _matcher_id = addr.host_id;

        // TODO: define _self's role
        /*
        //_self.aoi.center = pos;
               
        // if I'm the gateway, consider already joined
        if (isGateway (_self.id))
            _state = JOINED;
        else
            _state = JOINING;
        */

        // specifying the gateway to contact is considered joined
        _state = JOINED;

        return true;
    }

    // quit the overlay
    void        
    VASTClient::leave ()
    {
        if (_state != JOINED)
            return;

        // send a LEAVE message to my relay
        Message msg (LEAVE);
        msg.priority = 1;
        msg.msggroup = MSG_GROUP_VAST_MATCHER;
        msg.addTarget (_matcher_id);
        sendMessage (msg);
         
        _state = ABSENT;
    }
    
	// specify a subscription area for point or area publications 
    // returns a unique subscription number that represents subscribed area 
    bool
    VASTClient::subscribe (Area &area, layer_t layer)
    {
        if (_state != JOINED)
            return false;

        // record my subscription, not yet successfully subscribed  
        // NOTE: current we assume we subscribe only one at a time
        _sub.id     = (id_t)(-1);   // indiate we're subscribing
        _sub.aoi    = area;
        _sub.layer  = layer;
        _sub.active = false;
        _sub.relay  = _net->getAddress (_relay->getRelayID ());

        // send out subscription request
        Message msg (SUBSCRIBE);
        msg.priority = 1;        
        msg.msggroup = MSG_GROUP_VAST_MATCHER;
        msg.store (_sub);
        msg.addTarget (_matcher_id);

        sendMessage (msg); 

        return true;
    }

    // send a message to all subscribers within a publication area
    // TODO: a more efficient way to pack publication message?

    bool
    VASTClient::publish (Area &area, layer_t layer, Message &message)
    {
        if (_state != JOINED)
            return false;

        // make a copy 
        // (important as we cannot manipulate external message directly, 
        //  may cause access violation for memory allocated in a different library heap)
        Message msg (message);
        msg.priority = 1;
        msg.msggroup = MSG_GROUP_VAST_MATCHER;

        // modify the msgtype to indicate this is an app-specific message
        msg.msgtype = (msg.msgtype << VAST_MSGTYPE_RESERVED) | PUBLISH;

#ifdef VAST_RECORD_LATENCY
        msg.store (_net->getTimestamp ());      // to calculate PUBLISH latency
#endif
        msg.store (area);
        msg.store (layer);
        
        // send to relay for default processing       
        msg.addTarget (_matcher_id);
        sendMessage (msg);

        return true;
    }
    
    // move a subscription area to a new position
    // returns actual AOI in case the position is already taken
    Area *  
    VASTClient::move (id_t subID, Area &aoi, bool update_only)
    {
        // if no subscription exists or the subID does not match, don't update
        if (_sub.active == false || subID != _sub.id)
            return NULL;
        
        Area &prev_aoi = _sub.aoi;

        if (_state == JOINED)
        {    
            // update center position first            
            prev_aoi.center = aoi.center;

            Message msg (MOVE);
            msg.priority = 3;
            msg.msggroup = MSG_GROUP_VAST_MATCHER;

            // store subscription ID
            msg.store (subID);

#ifdef VAST_RECORD_LATENCY
            msg.store (_net->getTimestamp ());  // to calculate MOVE latency
#endif             

            // store optimized version of position
            VONPosition pos;
            pos.x = aoi.center.x;
            pos.y = aoi.center.y;
            msg.store ((char *)&pos, sizeof (VONPosition));

            // if radius is also updated 
            if (prev_aoi != aoi)
            {
                // we store radius additionally
                msg.msgtype = MOVE_F;
                msg.store ((char *)&aoi.radius, sizeof (length_t));
                msg.store ((char *)&aoi.height, sizeof (length_t));
                prev_aoi = aoi;                
            }

            msg.addTarget (_matcher_id);

            if (update_only == false)
            {
                // MOVE can be delivered unreliably
                //msg.reliable = false;
                sendMessage (msg);
            }
        }
        
        // TODO: possibily perform collison detection for positions
        return &prev_aoi;
    }

    // send a custom message to a particular node
    bool        
    VASTClient::send (Message &message)
    {
        if (_state != JOINED)
            return false;

        Message msg (message);
        msg.msggroup = MSG_GROUP_VAST_MATCHER;
        msg.store (_net->getTimestamp ());  // store sendtime for latency calculation

        // modify the msgtype to indicate this is an app-specific message
        msg.msgtype = (msg.msgtype << VAST_MSGTYPE_RESERVED) | MESSAGE;

        return ((sendMessage (msg) > 0) ? true : false);
    }

    // obtain a list of subscribers within an area
    vector<Node *>& 
    VASTClient::list (Area *area)
    {
        return _neighbors;        
    }

    // obtain a list of physically closest hosts
    vector<Node *>& 
    VASTClient::getPhysicalNeighbors ()
    {
        // obtain some entry points from the network layer
        vector<IPaddr> &entries = _net->getEntries ();

        // clear physical neighbor list
        size_t i;
        for (i=0; i < _physicals.size (); i++)
            delete _physicals[i];
        _physicals.clear ();

        // convert entries to Node format
        for (i=0; i < entries.size (); i++)
        {
            id_t id = entries[i].host;
            Addr addr (id, &entries[i]);
            Area a;

            _physicals.push_back (new Node (id, 0, a, addr));
        }

        return _physicals;
    }

    // obtain a list of logically closest hosts (a subset of nodes by list () that are relay-level)
    vector<Node *>& 
    VASTClient::getLogicalNeighbors ()
    {
        // clear logical neighbor list
        size_t i;
        for (i=0; i < _logicals.size (); i++)
            delete _logicals[i];
        _logicals.clear ();

        // go through each AOI neighbors and keep only those with public IP
        for (i=0; i < _neighbors.size (); i++)
        {
            id_t id = _neighbors[i]->id;
            if (_neighbors[i]->addr.host_id == id)
            {
                _logicals.push_back (new Node (*_neighbors[i]));
            }
        }

        return _logicals;
    }

    // get a message from the network queue
    // true: while there are still messages to be received
    Message *
    VASTClient::receive ()
    {
        // clear last message allocation first
        if (_lastmsg != NULL)
        {
            delete _lastmsg;
            _lastmsg = NULL;
        }
        
        // extract oldest message if available
        if (_msglist.size () != 0)
        {        
            _lastmsg = _msglist[0];
            _msglist.erase (_msglist.begin ());
        }

        return _lastmsg;
    }

    // get current statistics about this node (a NULL-terminated string)
    char *
    VASTClient::getStat (bool clear)
    {
        return NULL;
    }

    // get the current node's information
    Node * 
    VASTClient::getSelf ()
    {        
        return &_self;
    }


    bool 
    VASTClient::isJoined ()
    {         
        return (_state == JOINED);
    }

    // whether the current node is listening for publications
    id_t 
    VASTClient::isSubscribing ()
    {
        if (_sub.active == false)
            return 0;
        else
            return _sub.id; 
    }

    // if myself is a relay
    bool 
    VASTClient::isRelay ()
    {
        return (_relay->getRelayID () == _self.id);
    }

    // whether I have public IP
    bool 
    VASTClient::hasPublicIP ()
    {
        return _net->isPublic ();
    }

    // get message latencies 
    // msgtype == 0 indicates clear up currently collected stat
    StatType * 
    VASTClient::getMessageLatency (msgtype_t msgtype)
    {
        if (msgtype == 0)
        {
            _latency.clear ();
            return NULL;
        }

        // the message type was not found
        if (_latency.find (msgtype) == _latency.end ())
            return NULL;

        // calculate average
        _latency[msgtype].calculateAverage ();

        return &_latency[msgtype];
    }


    //
    //  private methods 
    //

    // perform initialization tasks for this handler (optional)
    // NOTE that all internal variables (such as _net) have been set at this point
    void 
    VASTClient::initHandler ()
    {
        _self.id        = _net->getHostID ();
        _self.addr      = _net->getHostAddress ();

        //_relay_id       = _relay->getRelayID ();
    }

    // handler for various incoming messages
    // returns whether the message was successfully handled
    bool 
    VASTClient::handleMessage (Message &in_msg)
    {
        if (_state == ABSENT)
            return false;

        // store the app-specific message type if exists
        msgtype_t app_msgtype = APP_MSGTYPE(in_msg.msgtype);
        in_msg.msgtype = VAST_MSGTYPE(in_msg.msgtype);

#ifdef DEBUG_DETAIL
        if (in_msg.msgtype < VON_MAX_MSG)
            ; //printf ("[%d] VASTClient::handleMessage from: %d msgtype: %d, to be handled by VONPeer, size: %d\n", _self.id, in_msg.from, in_msg.msgtype, in_msg.size);            
        else
            printf ("[%d] VASTClient::handleMessage from: %d msgtype: %d appmsg: %d (%s) size: %d\n", _self.id, in_msg.from, in_msg.msgtype, app_msgtype, VAST_MESSAGE[in_msg.msgtype-VON_MAX_MSG], in_msg.size);
#endif

        switch (in_msg.msgtype)
        {

        // response from VASTClient regarding whether a subscription is successful
        case SUBSCRIBE_R:
            {
                // check if there are pending requests
                if (_sub.id != (id_t)(-1))
                {
                    printf ("VASTClient::handleMessage () SUBSCRIBE_REPLY received, but no prior pending subscription found\n");
                    break;
                }

                id_t sub_no;
                in_msg.extract (sub_no);
        
                _sub.id = sub_no;
                _sub.active = true;
                                      
                // notify network so incoming messages can be processed by this host
                notifyMapping (sub_no, &_net->getHostAddress ());
            }
            break;

            
        // notification from VASTClient about currently known AOI neighbors
        case NEIGHBOR:
            {
                // loop through each response 
                listsize_t size;
                in_msg.extract (size, true);
                vector<id_t> remove_list;           // list of nodes to be deleted

                id_t                    neighbor_id;
                NeighborUpdateStatus    status;
                Node                    node;
                Area                    aoi;
                //timestamp_t             time;

                for (int i=0; i < (int)size; i++)
                {
                    in_msg.extract (neighbor_id);
                    in_msg.extract ((char *)&status, sizeof (NeighborUpdateStatus));

                    switch (status)
                    {
                    case INSERTED:
                        {
                            in_msg.extract (node);

                            // check for redundency first
                            vector<Node *>::iterator it = _neighbors.begin ();
                            for (; it != _neighbors.end (); it++)
                            {
                                // redundency found, simply update
                                if ((*it)->id == node.id)
                                {                               
                                    (*it)->update (node);                                    
                                    break;
                                }
                            }
                            
                            // if indeed it's a new neighbor
                            if (it == _neighbors.end ())
                                _neighbors.push_back (new Node (node));
                            
                            recordLatency (MOVE, node.time);
                        }
                        break;

                    case DELETED:
                        remove_list.push_back (neighbor_id);
                        break;

                    case UPDATED:
                        {                            
                            in_msg.extract (aoi);
#ifdef VAST_RECORD_LATENCY
                            in_msg.extract (time);
                            recordLatency (MOVE, time);
#endif

                            vector<Node *>::iterator it = _neighbors.begin ();
                            for (; it != _neighbors.end (); it++)
                            {
                                if ((*it)->id == neighbor_id)
                                {                               
                                    //printf ("[%d] Client updates neighbor [%d]'s position (%d, %d) to (%d, %d)\n", _self.id, neighbor_id, (int)(*it)->aoi.center.x, (int)(*it)->aoi.center.y, (int)aoi.center.x, (int)aoi.center.y);
                                    (*it)->aoi = aoi;
#ifdef VAST_RECORD_LATENCY
                                    (*it)->time = time;
#endif
                                    
                                    break;
                                }
                            }
                        }
                        break;
                    }
                    
                }

                // remove neighbors
                for (size_t i=0; i < remove_list.size (); i++)
                {
                    vector<Node *>::iterator it = _neighbors.begin ();
                    for (; it != _neighbors.end (); it++)
                    {
                        if ((*it)->id == remove_list[i])
                        {
                            delete (*it);
                            _neighbors.erase (it);
                            break;
                        }
                    }
                }
            }
            break;
        
        // receiving a published or directly-targeted message
        case MESSAGE:
            {
                // restore app-specific message type
                in_msg.msgtype = app_msgtype;
                
#ifdef VAST_RECORD_LATENCY
                timestamp_t sendtime;
                in_msg.extract (sendtime, true);
                recordLatency (PUBLISH, sendtime);
#endif
                in_msg.reset ();
                storeMessage (in_msg);
            }
            break;

        // process universal VASTnet message, msgtype = 0
        case DISCONNECT:
            {           
                // if we're disconnected from our matcher
                // clear up neighbors due to current subscriptions 
                // as we'll need to re-subscribe with the new matcher
                // TODO: more efficient method?
                if (in_msg.from == _matcher_id)
                {
                    for (size_t i=0; i < _neighbors.size (); i++)
                        delete _neighbors[i];
                    _neighbors.clear ();                   
                }
                // if the relay fails
                else if (in_msg.from == _relay->getRelayID ())
                {
                    // TODO: deal with relay failure
                }
            }
            break;
        
        default:            

            break;
        }
        
        return true;
    }

    // perform routine tasks after all messages have been handled 
    //  (i.e., check for reply from requests sent)
    void 
    VASTClient::postHandling ()
    {

    }


    // store a message to the local queue to be retrieved by receive ()
    void 
    VASTClient::storeMessage (Message &msg)
    {
        _msglist.push_back (new Message (msg));
    }

    // make one latency record
    void 
    VASTClient::recordLatency (msgtype_t msgtype, timestamp_t sendtime)
    {
        StatType &stat = _latency[msgtype];

        timestamp_t duration = _net->getTimestamp () - sendtime;
        
        // do not record zero latency
        if (duration == 0)
            return;

        // record min / max / average
        if (duration < stat.minimum)
            stat.minimum = duration;

        if (duration > stat.maximum)
            stat.maximum = duration;

        stat.total += duration;
        stat.num_records++;
    }
                                    
} // end namespace Vast
