

#include "VAST.h"

namespace Vast
{   

    // join the overlay 		
    bool
    VAST::join (id_t id, Area &world_size, length_t default_manager_aoi)
    {
        // avoid redundent join
        if (_join_state != ABSENT)
            return false;

        // create unique ID here... after the handler_no should be assigned
        _self.id = COMBINE_ID(_handler_no, id);
        
        _worldSize = world_size;

        // connect to gateway and obtain initial information (?)

        // create initial Peer and Manager
        _managers.push_back (new Manager (_self, default_manager_aoi));
        _peers.push_back (new Peer (_self));

        // chain the new manager and peer to the same MessageQueue
        addHandler (_managers[0]);
        addHandler (_peers[0]);

        _join_state = JOINING;

        return true;
    }

    // quit the overlay
    void        
    VAST::leave ()
    {
        // destroy all managers and peers
        size_t i;
        for (i=0; i<_managers.size (); i++)
        {
            removeHandler (_managers[i]);
            delete _managers[i];
        }
        for (i=0; i<_peers.size (); i++)
        {
            removeHandler (_peers[i]);
            delete _peers[i];
        }

        _join_state = ABSENT;
    }
    
	// put myself as one of the overlay managers at an (optionally) specific position
    // also optionally specify whether I will allow the overlay to adjust the manager's position automatically
	Position &		
    VAST::manage (Position *p, bool autoAdjust)
    {
        _autoAdjustManager = autoAdjust;
        Position pos;

        // check if the placement of superpeer is done automatically
        if (p == NULL)
        {
            _autoAdjustManager = true;
            
            // produce a new position for the Manager
            pos = locateManagerPosition ();            
        }
        else 
            pos = *p;

        // carry out the move by Manager              
        return _managers[0]->move (pos);;
    }

	// specify some subscription areas for point or area publications 
    // returns a unique subscription number that represents the "collection" of overlapping subscribed areas
    id_t
    VAST::subscribe (vector<Area> &list, layer_t layer)
    {
        // modify manager AOI-radius accordingly
        _managers[0]->setAOI (list[0].radius);

        // right now assume only one area is given, also only one peer exists
        return _peers[0]->subscribe (list[0], layer);
    }

    // send a message to all subscribers within a publication area
    bool
    VAST::publish (Area &area, layer_t layer, Message &msg)
    {
        return true;
    }
    
    // register for subscription notifications in a given area
    bool
    VAST::registerSubscriptionNotifications (Area &area, layer_t layer)
    {
        return true;
    }

    // move to a new position, returns actual position
    Position &  
    VAST::move (id_t sub_no, layer_t layer, Position &pos)
    {
        return _peers[0]->move (sub_no, layer, pos);
    }

    // send a custom message to a particular node
    bool        
    VAST::send (Node &target, Message &msg)
    {
        return true;
    }

    // get a message from the network queue
    // true: while there are still messages to be received
    bool 
    VAST::receive (Message *msg)
    {
        return true;
    }

    // obtain a list of subscription notifications since last call
    vector<Subscriber *> &
    VAST::getSubscribers ()        
    {
        static vector<Subscriber *> temp;
        return temp;
    }

    // obtain a list of currently connected overlay managers
    vector<Node *> &    
    VAST::getManagers ()
    {        
        return _managers[0]->getManagers ();
    }

    bool 
    VAST::isJoined ()
    {         
        return (_managers[0]->joinState () == JOINED || _peers[0]->joinState () == JOINED);
    }

    // get the current node's information
    Node * 
    VAST::getSelf ()
    {
        //return &_self;
        return _managers[0]->getSelf ();
    }

    // get current statistics about this node (a NULL-terminated string)
    char *
    VAST::getStat (bool clear)
    {
        return NULL;
    }

    voronoi *
    VAST::getVoronoi ()
    {
        return _managers[0]->getVoronoi ();
    }


    //
    //  private methods 
    //

    // handler for various incoming messages
    // returns whether the message was successfully handled
    bool 
    VAST::handleMessage (id_t from, Message &in_msg)
    {
        // extract the actual target and let call the appropriate handler

        return true;
    }

    // find a location to place the manager, right now just randomly assign
    Position 
    VAST::locateManagerPosition ()
    {
        return Position (rand () % _worldSize.width, rand () % _worldSize.height);
    }


} // end namespace Vast