

/*
 *  SimArbitrator.h -- example arbitrator class implementing ArbitratorLogic
 *                     this is app-specific behavior for an arbitrator
 *
 *  history:    2009/06/23  init
 */


#ifndef VASTATESIM_SIMARBITRATOR_H
#define VASTATESIM_SIMARBITRATOR_H

#include "ArbitratorLogic.h"

using namespace Vast;

class EXPORT SimArbitrator : public ArbitratorLogic
{
public:
    SimArbitrator ()
    {
    }

    ~SimArbitrator ()
    {
        // erase record of objects maintained by this arbitrator
        _agents.clear ();
        _objects.clear ();
    }

    // callback - for handling a login request with authentication message
    bool onLogin (Vast::id_t from_id, const char *auth, size_t size);

    // callback - for handling a logout request with authentication message
    bool onLogout (Vast::id_t from_id);

    // callback - an app-specific message from agent to gateway server
    void onMessage (Message &in_msg);

    // callback -  for receiving a remote Event (Arbitrator only)
    bool onEvent (Vast::id_t from_id, Event &e);
    bool onMoveEvent (Vast::id_t from_id, const obj_id_t &obj_id, const Position &newpos);
    
    // callback - by remote Arbitrator to create or initialize objects        
    void onCreate       (Object *obj, bool initializable);
    void onDestroy      (const obj_id_t &obj_id);
    
    // callback - by remote Arbitrator to notify their Object states have changed
    void onUpdate (const obj_id_t &obj_id, int index, const void *value, size_t length, version_t version);
    void onMove (const obj_id_t &obj_id, const Position &newpos, version_t version);
    
    // callback - to learn about the creation of avatar objects
    void onAgentEntering (Node &agent);
    void onAgentLeaving (Node &agent);

    // callback - to process any non-triggered Arbitrator logic (perform actions not provoked by events)
    void tick (); 

private:
    map<Vast::id_t, Object *>   _agents;        // mapping between agentID & object
    map<obj_id_t, Object *>     _objects;       // mapping between objectID & object

};


#endif
