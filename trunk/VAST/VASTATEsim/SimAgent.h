

/*
 *  SimAgent.h -- example agent class implementing AgentLogic
 *                this is app-specific behavior for an agent
 *
 *  history:    2009/06/23  init
 */


#ifndef VASTATESIM_SIMAGENT_H
#define VASTATESIM_SIMAGENT_H

#include "AgentLogic.h"
#include <string>

using namespace Vast;

class EXPORT SimAgent : public AgentLogic
{
public:
    SimAgent ()
        //: _self (NULL)
    {
    }

    ~SimAgent ();

    // callback - learn about the addition/removal of objects
    void onCreate (Object &obj, bool is_self = false);
    void onDestroy (const obj_id_t &obj_id);
    
    // callback - learn about state changes of known AOI objects
    void onUpdate (const obj_id_t &obj_id, int index, const void *value, size_t length, version_t version);
    void onMove (const obj_id_t &obj_id, const Position &newpos, version_t version);

    // callback - learn about the result of login
    void onAdmit (const char *status, size_t size);

    // callback - an app-specific message from gateway arbitrator (server) to agent
    void onMessage (Message &in_msg);

    vector<Node *> &getNeighbors ()
    {
        return _neighbors;
    }

    // get the first message
    size_t getChat (char *buf);

private:
    // self node
    //Vast::Node             *_self;
    vector<Node *>          _neighbors;    
    vector<std::string *>   _chatmsg;
    
    map<obj_id_t, Object *> _objects;

};


#endif

