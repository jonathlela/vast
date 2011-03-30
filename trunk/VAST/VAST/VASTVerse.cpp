/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2011 Shun-Yun Hu (syhu@ieee.org)
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

#include "VASTVerse.h"

#include <vector>
#include <map>
#include <time.h>           // time

#include "MessageQueue.h"
#include "VASTRelay.h"
#include "VASTClient.h"
#include "VASTMatcher.h"
#include "VoronoiSF.h"

// for starting separate thread for running VAST node
#include "VASTThread.h"


namespace Vast
{  
	
    class VASTPointer
    {
    public:
        VASTPointer ()
        {
            net         = NULL;
            msgqueue    = NULL;
            client      = NULL;
            relay       = NULL;
            matcher     = NULL;
            callback    = NULL;
            thread      = NULL;
        }

        VASTnet *       net;            // network interface
        MessageQueue *  msgqueue;       // message queue for managing mesage handlers
        VAST *          client;         // clients that enter an overlay
        VASTRelay *     relay;          // physical coordinate locator & joiner
        VASTMatcher *   matcher;        // a relay node to the network
        VASTCallback *  callback;       // callback for processing incoming app messages
        VASTThread *    thread;         // thread for running a VASTNode
    };

    //
    //  Internal helper functions
    //
   
    VASTnet *
    createNet (unsigned short port, VASTPara_Net &para, vector<IPaddr> &entries, int step_persec)
    {
        if (step_persec == 0)
        {
            printf ("VASTnet::createNet () steps per second not specified, set to default: 10\n");
            step_persec = 10;
        }

        VASTnet *net = new VASTnet (para.model, port, step_persec);

        // store initial entry points
        net->addEntries (entries);
        
		net->setBandwidthLimit (BW_UPLOAD,   para.send_quota / step_persec);
		net->setBandwidthLimit (BW_DOWNLOAD, para.recv_quota / step_persec);

        return net;
    }

    bool 
    destroyNet (VASTnet *net)
    {
        if (net == NULL)
            return false;

        delete net;
        return true;
    }
    
    VASTVerse::
    VASTVerse (bool is_gateway, const string &GWstr, VASTPara_Net *netpara, VASTPara_Sim *simpara, VASTCallback *callback, int tick_persec)
        :_netpara (*netpara)
    {
        _state = ABSENT;
        _lastsend = _lastrecv = 0;
        _next_periodic = 0;

        _timeout = 0;

        // record gateway info here, actual connection will be made in tick ()
        Addr *addr = VASTVerse::translateAddress (GWstr);
        
        // use localhost as default
        if (addr == NULL)
            addr = VASTVerse::translateAddress (string ("127.0.0.1:1037"));

        _gateway = addr->publicIP;
       
        if (is_gateway == false)
            _entries.push_back (_gateway);

        _GWconnected = false;

        // record simulation parameters, if any
        if (simpara != NULL)
            _simpara = *simpara;
        else
            // use 0 as defaults for all values
            memset (&_simpara, 0, sizeof (VASTPara_Sim)); 

        // zero all pointers
        _pointers = new VASTPointer ();
        memset (_pointers, 0, sizeof (VASTPointer));

        // store callback, if any
        VASTPointer *handlers   = (VASTPointer *)_pointers;
        handlers->callback      = callback;
        
        // start thread if both callback & tick_persec is specified
        if (callback && tick_persec > 0)
        {
            printf ("starting thread..\n");
            handlers->thread = new VASTThread (tick_persec);
            handlers->thread->open (this);
            printf ("thread started...\n");
        }        
    }

    VASTVerse::~VASTVerse ()
    {
        VASTPointer *handlers = (VASTPointer *)_pointers;

        // stop the running thread
        if (handlers->thread != NULL)
        {
            handlers->thread->close (0);
            handlers->thread = NULL;
        }

        // delete those vast_nodes not yet deleted
        if (handlers->client != NULL)
        {
            destroyVASTNode (handlers->client);
            handlers->client = NULL;
        }

        // notify disconnection from gateway
        if (_GWconnected == true)
        {
            // notify callback
            if (handlers->callback)
            {                    
                handlers->callback->gatewayDisconnected ();
            }
            _GWconnected = false;
        }

        if (handlers->matcher != NULL)
        {
            delete handlers->matcher;
            handlers->matcher = NULL;
        }

        if (handlers->relay != NULL)
        {
            delete handlers->relay;
            handlers->relay = NULL;
        }

        if (handlers->msgqueue != NULL)
        {
            //destroyQueue (handlers->msgqueue);
            delete handlers->msgqueue;
            handlers->msgqueue = NULL;
        }

        if (handlers->net != NULL)
        {
            destroyNet (handlers->net);
            handlers->net = NULL;
        }        

        delete (VASTPointer *)_pointers;
        _pointers = NULL;

        // destory TimeMonitor
        TimeMonitor::terminateInstance ();

    }

    // create & destroy a VASTNode
    // here we only records the info for the new VASTNode, 
    // actual creation will occur during tick ()
    bool 
    VASTVerse::createVASTNode (world_t world_id, Area &area, layer_t layer)
    {
        // right now can only create one
        if (_vastinfo.size () > 0)
            return false;

        Subscription info;
        size_t id = _vastinfo.size () + 1;  // NOTE: id is mainly useful for simulation lookup

        // store info about the VASTNode to be created
        // NOTE we store gateway's info in relay        
        info.aoi = area;
        info.layer = layer;
        info.id = id;
        info.world_id = world_id;

        _vastinfo.push_back (info);

        // store gateway as an entry point if I'm client
        // (will be used by network layer to decide whether to join as gateway or client)
        //if (_is_gateway == false)
        //    _entries.push_back (gateway);        

        printf ("VASTVerse::createVASTNode world_id: %u layer: %u\n", world_id, layer);

        return true;
    }

    bool 
    VASTVerse::destroyVASTNode (VAST *node)
    {
        if (node == NULL)
            return false;

        // perform leave if client exists
        node->leave ();

        if (_vastinfo.size () == 0)
            return false;

        _vastinfo.clear ();

        // send unsent messages & notify callback of node leave
        this->tick ();
      
        return destroyClient (node);
    }

    // obtain a reference to the created VASTNode
    VAST *
    VASTVerse::getVASTNode ()
    {
        VASTPointer *handlers = (VASTPointer *)_pointers;

        if (_state == JOINED)
            return handlers->client;
        else
            return NULL;
    }

    // whether this VASTVerse is init to create VASTNode instances
    // NOTE: everytime isInitialized () is called, it will always check the readiness of
    //       network, relay, and matcher components
    bool 
    VASTVerse::isInitialized ()
    {
        VASTPointer *handlers = (VASTPointer *)_pointers;

        Position *physcoord = NULL;     // physical coordinate obtained

        //
        // initialize various necessary functions of a VAST factory
        //

        // create the basic network layer & message queue
        if (handlers->net == NULL)
        {
            printf ("VASTVerse::isInitialized () creating VASTnet...\n");

            // create network layer
            handlers->net = createNet (_netpara.port, _netpara, _entries, _simpara.step_persec);
            if (handlers->net == NULL)
                return false;

            printf ("VASTVerse::isInitialized () creating MessageQueue...\n");
            handlers->msgqueue = new MessageQueue (handlers->net);
        }

        // wait for the network layer to join properly
        else if (handlers->net->isJoined () == false)
            return false;

        // after we get unique ID, obtain physical coordinate
        else if (handlers->relay == NULL)
        {
            // convert possible "127.0.0.1" gateway address to actual IP address
            if (handlers->net->validateIPAddress (_gateway) == false)
                return false;

            // create the Relay node and store potential overlay entries 
            printf ("VASTVerse::isInitialized () creating VASTRelay...\n");

            id_t id = handlers->net->getHostID ();

            // check if unique hostID is obtained
            // NOTE we will try to determine physical coordinate only after unique ID is gotten
            if (id != NET_ID_UNASSIGNED)
            {
                printf ("unique ID obtained [%llu]\n", id);
                printf ("if this hangs, check if physical coordinate is obtained correctly\n");

                bool hasPhysCoord = !(_netpara.phys_coord.x == 0 && _netpara.phys_coord.y == 0);
                handlers->relay = new VASTRelay (_netpara.is_relay, _netpara.client_limit, _netpara.relay_limit, hasPhysCoord ? &_netpara.phys_coord : NULL);

                handlers->msgqueue->registerHandler (handlers->relay);
            }
        }
        // NOTE: if physical coordinate is not supplied, the login process may pause here indefinitely
        else if (handlers->relay->isJoined () == false)
            return false;

        // create matcher instance (though it may not be used)
        else if (handlers->matcher == NULL)
        {             
            // relay has just been properly created, get physical coordinates
            physcoord = handlers->relay->getPhysicalCoordinate ();
            printf ("[%llu] physical coord: (%.3f, %.3f)\n", handlers->net->getHostID (), physcoord->x, physcoord->y);

            // create (idle) 'matcher' instance
            handlers->matcher = new VASTMatcher (_netpara.is_matcher, _netpara.overload_limit, _netpara.is_static, (_netpara.matcher_coord.isEmpty () ? NULL : &_netpara.matcher_coord));
            handlers->msgqueue->registerHandler (handlers->matcher);            
            return true;
        }
        else
            // everything is well & done
            return true;

        // by default init is not yet done
        return false;
    }

    /*
    // to add entry points for this VAST node (should be called before createVASTNode)
    // format is "IP:port" in string, returns the number of successfully added entries
    int 
    VASTVerse::addEntries (std::vector<std::string> entries)
    {
        int entries_added = 0;
        Addr *addr;

        for (size_t i=0; i < entries.size (); i++)
        {
            if ((addr = VASTVerse::translateAddress (entries[i])) != NULL)
            {
                _entries.push_back (addr->publicIP);
                entries_added++;
            }
        }

        return entries_added;
    }
    */

    VAST *
    VASTVerse::createClient (const IPaddr &gateway, world_t world_id)
    {
        // no need to check every time?
        if (isInitialized () == false)
            return NULL;

        VASTPointer *handlers = (VASTPointer *)_pointers;

        // avoid double-creation
        if (handlers->client != NULL)
            return handlers->client;

        // make sure matcher has joined first
        if (handlers->matcher->isJoined () == false)
        {
            handlers->matcher->join (gateway);
            return NULL;
        }

        VAST *vnode = NULL;

        if ((vnode = new VASTClient (handlers->relay)) != NULL)
        {
            handlers->msgqueue->registerHandler (vnode);

            // perform join for the client 
            vnode->join (gateway, world_id);
            handlers->client = vnode;
        }

        return vnode;
    }

    bool     
    VASTVerse::destroyClient (VAST *node)
    {
        VASTPointer *handlers = (VASTPointer *)_pointers;

        if (node == NULL || handlers->msgqueue == NULL)
            return false;        

        // unregister from message queue
        handlers->msgqueue->unregisterHandler ((VASTClient *)node);            

        // release memory
        delete (VASTClient *)node;        
        handlers->client = NULL;

        return true;
    }

    /*
    Voronoi *
    VASTVerse::createVoronoi ()
    {
        return new VoronoiSF ();
    }

    bool
    VASTVerse::destroyVoronoi (Voronoi *v)
    {
        if (v == NULL)
            return false;

        delete (VoronoiSF *)v;
        return true;
    }
    */

    // advance one time-step 
	int
    VASTVerse::tick (int time_budget, bool *per_sec)
    {        
        // if no time is available, at least give a little time to run at least once
        if (time_budget < 0)
            time_budget = 1;

        // set budget (in microseconds)
        TimeMonitor::instance ()->setBudget (time_budget);

        VASTPointer *handlers = (VASTPointer *)_pointers;

        // # of ticks a joining stage is considered timeout
        timestamp_t timeout_period = (handlers->net != NULL ? (VASTVERSE_RETRY_PERIOD * handlers->net->getTimestampPerSecond ()) : 0);

        //
        // perform init & joining tasks
        //

        // if we've joined to gateway
        if (isInitialized ())
        {
            // connect to gateway for the first time
            if (_GWconnected == false)
            {
                // notify callback
                if (handlers->callback)
                {                    
                    handlers->callback->gatewayConnected (handlers->net->getHostID ());
                }
                _GWconnected = true;
            }

            // if a createVASTNode has been called
            if (_state != JOINED && _vastinfo.size () > 0)
            {
                Subscription &info = _vastinfo[0];
        
                switch (_state)
                {
                // try to create VASTClient and let it start joining
                case ABSENT:
                    {
                        printf ("state = ABSENT\n");
                        if ((createClient (_gateway, info.world_id)) != NULL)
                        {                            
                            printf ("VASTVerse::tick () VASTclient created\n");
                            _state = JOINING;
                        }
                    }
                    break;
        
                // try to perform subscription after the client has joined
                case JOINING:
                    {
                        printf ("state = JOINING\n");
                        if (handlers->client->isJoined ())
                        {
                            printf ("VASTVerse::tick () subscribing ... \n");
                            handlers->client->subscribe (info.aoi, info.layer);
                            _state = JOINING_2;
                        }
                        // check if this stage is timeout, revert to previous state
                        else
                        {
                            _timeout++;
                            if (_timeout > timeout_period)
                            {
                                LogManager::instance ()->writeLogFile ("VASTVerse::tick () VASTClient join timeout after %d seconds, revert to ABSENT state", VASTVERSE_RETRY_PERIOD);
                                _timeout = 0;
                                destroyClient (handlers->client);
                                _state = ABSENT;
                            }
                        }
                    }
                    break;
        
                // wait until subscription is successful
                case JOINING_2:
                    {
                        printf ("state = JOINING_2\n");
                        if (handlers->client->getSubscriptionID () != NET_ID_UNASSIGNED)
                        {
                            printf ("VASTVerse::getVASTNode () ID obtained\n");
                            printf ("state = JOINED\n");
                            _state = JOINED;
        
                            // call callback to notify for join
                            if (handlers->callback)
                            {
                                handlers->callback->nodeJoined (handlers->client);
                            }
                        }
                        // check if this stage is timeout, revert to previous state
                        else
                        {
                            _timeout++;
        
                            if (_timeout > timeout_period)
                            {
                                LogManager::instance ()->writeLogFile ("VASTVerse::tick () wait for subscription ID timeout after %d seconds, revert to JOINING state", VASTVERSE_RETRY_PERIOD);
                                _timeout = 0;
                                _state = JOINING;
                            }
                        }
                    }
                    break;
                }
            } // end creating VASTNode

            // we've left the network
            else if (_state == JOINED && _vastinfo.size () == 0)
            {
                _state = ABSENT;
                printf ("Node left\nstate = ABSENT\n");
        
                // call callback to notify for leave
                if (handlers->callback)
                {
                    // notify callback
                    handlers->callback->nodeLeft ();
                }
            }            
        }                       

        //
        // perform message queue ticking (process incoming messages)
        //

        // perform routine procedures for each logical time-step
        if (handlers->msgqueue != NULL)
            handlers->msgqueue->tick ();

        // call callback to perform per-tick task, if any
        if (handlers->callback)
        {
            handlers->callback->performPerTickTasks ();
        }

        //
        // perform other tasks
        //

        // whether to perform per-second tasks
        bool do_per_sec = false;

        if (handlers->net != NULL)
        {
            VASTnet *net = handlers->net;

            // check if we need to perform per-second task (get time in millisecond)
            timestamp_t now = net->getTimestamp ();            
        
            if (now >= _next_periodic)
            {
                do_per_sec = true;

                // next time to enter this is one second later
                _next_periodic = (now / net->getTimestampPerSecond () + 1) * net->getTimestampPerSecond ();

                // record network stat for this node
                size_t size = net->getSendSize (0);            
                _sendstat.addRecord (size - _lastsend);
                _sendstat_interval.addRecord (size - _lastsend);
                _lastsend = size;
            
                size = net->getReceiveSize (0);
                _recvstat.addRecord (size - _lastrecv);
                _recvstat_interval.addRecord (size - _lastrecv);
                _lastrecv = size;

                // call callback to perform per-second task, if any
                if (handlers->callback)
                {
                    handlers->callback->performPerSecondTasks (now);
                }
            }

            //
            // process incoming messages by calling app-specific message handlers, if any
            //
            
            // process incoming socket messages 
            // NOTE: the we don't have to join in order to process
            if (handlers->callback)
            {
                char *socket_msg = NULL;
                id_t socket_id;
                size_t size;
                
                while ((socket_msg = handlers->net->receiveSocket (socket_id, size)) != NULL)
                {
                    // if the callback cannot handle or does not wish to process more, stop
                    if (handlers->callback->processSocketMessage (socket_id, socket_msg, size) == false)
                        break;
                }
            }

            // process incoming VAST messages, only after JOINED 
            if (_state == JOINED && handlers->client && handlers->callback)
            {
                // process input VAST messages, if any
                Message *msg;
            
                while ((msg = handlers->client->receive ()) != NULL)
                {
                    // if the callback cannot handle or does not wish to process more, stop
                    if (handlers->callback->processMessage (*msg) == false)
                        break;
                }
            }
        }

        // return whether per_second tasks were performed
        if (per_sec != NULL)
            *per_sec = do_per_sec;

        int time_left = TimeMonitor::instance ()->available ();

        // record execution interval

        return time_left;
    }

    // move logical clock forward (perform periodic stuff here)
    void  
    VASTVerse::tickLogicalClock ()
    {
        // increase tick globally when the last node has processed its messages for this round
        // this will prevent its message be delayed one round in processing by other nodes
        // TODO: find a better way? 
        VASTPointer *handlers = (VASTPointer *)_pointers;
        if (handlers->net)
            handlers->net->tickLogicalClock ();
    }

    // stop operations on this node
    void     
    VASTVerse::pauseNetwork ()
    {        
        // TODO: no functions now
        VASTPointer *handlers = (VASTPointer *)_pointers;
        if (handlers->net != NULL)
        {
            //handlers->net->flush ();
            handlers->net->stop ();			
        }        
    }

    // resume operations on this node
    void     
    VASTVerse::resumeNetwork ()
    {        
        // TODO:
        VASTPointer *handlers = (VASTPointer *)_pointers;
        if (handlers->net != NULL)
            handlers->net->start ();        
    }

    // open a new TCP socket
    id_t 
    VASTVerse::openSocket (IPaddr &ip_port, bool is_secure)
    {
        VASTPointer *handlers = (VASTPointer *)_pointers;
        if (handlers->net == NULL)
            return NET_ID_UNASSIGNED;

        return handlers->net->openSocket (ip_port, is_secure);            
    }

    // close a TCP socket
    bool 
    VASTVerse::closeSocket (id_t socket)
    {
        VASTPointer *handlers = (VASTPointer *)_pointers;
        if (handlers->net == NULL)
            return false;

        return handlers->net->closeSocket (socket);
    }

    // send a message to a socket
    bool 
    VASTVerse::sendSocket (id_t socket, const char *msg, size_t size)
    {
        VASTPointer *handlers = (VASTPointer *)_pointers;
        if (handlers->net == NULL)
            return false;

        return handlers->net->sendSocket (socket, msg, size);                    
    }

    // receive a message from socket, if any
    // returns the message in byte array, and the socket_id, message size, NULL for no messages
    // NOTE: the returned data is valid until the next call to receiveSocket
    char *
    VASTVerse::receiveSocket (id_t &socket, size_t &size)
    {
        VASTPointer *handlers = (VASTPointer *)_pointers;
        if (handlers->net == NULL)
            return NULL;

        return handlers->net->receiveSocket (socket, size);
    }

    // get current timestamp from host machine
    timestamp_t 
    VASTVerse::getTimestamp ()
    {
        VASTPointer *handlers = (VASTPointer *)_pointers;
        if (handlers->net == NULL)
            return 0;
            
        return handlers->net->getTimestamp ();
    }

    // obtain access to Voronoi class (usually for drawing purpose)
    // returns NULL if matcher does not exist on this node
    Voronoi *
    VASTVerse::getMatcherVoronoi ()
    {
        if (isInitialized () == false)
            return NULL;

        VASTPointer *handlers = (VASTPointer *)_pointers;

        // if there's a joined matcher on this node, then we should have a Voronoi map
        if (handlers->matcher->isJoined ())
            return handlers->matcher->getVoronoi ();

        return NULL;
    }

    // obtain the matcher's adjustable AOI radius, returns 0 if no matcher exists
    Area *
    VASTVerse::getMatcherAOI ()
    {
        if (isInitialized () == false)
            return NULL;

        VASTPointer *handlers = (VASTPointer *)_pointers;

        // if there's a joined matcher on this node, then we should have a Voronoi map
        if (handlers->matcher->isJoined ())
            return handlers->matcher->getMatcherAOI ();

        return NULL;
    }

    // whether I am a matcher node
    bool 
    VASTVerse::isMatcher ()
    {
        if (isInitialized () == false)
            return false;

        VASTPointer *handlers = (VASTPointer *)_pointers;
                
        return handlers->matcher->isActive ();
    }

    // whether I am a gateway node
    bool 
    VASTVerse::isGateway ()
    {
        if (isInitialized () == false)
            return false;

        VASTPointer *handlers = (VASTPointer *)_pointers;
                
        return handlers->matcher->isGateway ();
    }


    // obtain the number of active connections at this node
    int 
    VASTVerse::getConnectionSize ()
    {
        VASTPointer *handlers = (VASTPointer *)_pointers;
        if (handlers->net != NULL)
            return (int)handlers->net->getConnections ().size ();

        return 0;
    }

    // obtain the tranmission size by message type, default is to return all types
    StatType &
    VASTVerse::getSendStat (bool interval_only)
    {
        if (interval_only)
            return _sendstat_interval;
        else
            return _sendstat;
    }
    
    StatType &
    VASTVerse::getReceiveStat (bool interval_only)
    {
        if (interval_only)
            return _recvstat_interval;
        else
            return _recvstat;
    }

    // reset stat collection for a particular interval, however, accumulated stat will not be cleared
    void
    VASTVerse::clearStat ()
    {
        _sendstat_interval.reset ();
        _recvstat_interval.reset ();
    }

    // record nodeID on the same host
    void 
    VASTVerse::recordLocalTarget (id_t target)
    {
        VASTPointer *handlers = (VASTPointer *)_pointers;
        if (handlers->net != NULL)
            return handlers->net->recordLocalTarget (target);
    }

    // obtain gateway's IP & port
    IPaddr &
    VASTVerse::getGateway ()
    {
        return _gateway;
    }

    // translate a string-based address into Addr object
    Addr *
    VASTVerse::translateAddress (const string &str)
    {
        static Addr addr;        

        // convert any hostname to IP        
        string address = str;

        /*
        // convert a hostname to numeric IP string
        if (isalpha (str[0]))
        {            
            size_t port_pos = address.find (":");
            if (port_pos == (unsigned)(-1))
            {
                printf ("VASTVerse::translateAddress () address format incorrect, should be [numeric IP or hostname:port]\n");
                return NULL;
            }

            const char *IP = _net->getIPFromHost (address.substr (0, port_pos).c_str ());
            const char *port = address.substr (port_pos+1).c_str ();
                        
            // printout for debug purpose
            char converted[255];
            sprintf (converted, "%s:%s\0", IP, port);            
            printf ("VASTVerse::translateAddress () convert hostname into: %s\n", converted);

            // store numeric
            address = string (converted);            
        }
        */

        // determine IP from string
        if (IPaddr::parseIP (addr.publicIP, address) != 0)
        {
            printf ("VASTVerse::translateAddress () cannot resolve address: %s\n", str.c_str ());
            return NULL;
        }

        // determine hostID based on IP
        addr.host_id = net_manager::resolveHostID (&addr.publicIP);

        return &addr;
    }

} // end namespace Vast

