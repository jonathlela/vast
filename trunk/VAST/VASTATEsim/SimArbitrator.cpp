
#include "SimArbitrator.h"
#include "Arbitrator.h"
#include "VASTATEsim.h" // SimObjectType

#include "SimPeer.h"    // createRandomObjects


// callback - for handling a login request with authentication message
bool 
SimArbitrator::onLogin (Vast::id_t from_id, const char *auth, size_t size)
{
    char msg[]="you've passed!\0";

    ((Arbitrator *)_arbitrator)->admit (from_id, msg, strlen (msg));

    return true;
}

// callback - for handling a logout request with authentication message
bool 
SimArbitrator::onLogout (Vast::id_t from_id)
{
    printf ("onLogout from %ld\n", from_id);

    return true;
}

// callback - an app-specific message from agent to gateway server
void 
SimArbitrator::onMessage (Message &in_msg)
{
    printf ("SimArbitrator::onMessaged () called\n");
}

// callback -  for receiving a remote Event (Arbitrator only)
bool 
SimArbitrator::onEvent (Vast::id_t from_id, Event &e)
{
    // extract the action
    SimPeerAction action = (SimPeerAction)e.type;

    switch (action)
    {

    case MOVE:
        {
            // extract position
            Position newpos;
            e.get (0, newpos);

            // get agent's avatar's object
            if (_agents.find (from_id) != _agents.end ())
            {
                // perform move for the said object
                Arbitrator *arb = (Arbitrator *)_arbitrator;

                Object *obj = _agents[from_id];

                // to mimic delay caused by OpenSim server response
                //_sleep (1000);

                // move the object                
                arb->moveObject (obj->getID (), newpos);
            }
        }
        break;

    case TALK:
        {
            // extract chat message
            std::string chatmsg;

            e.get (0, chatmsg);

            // get agent's avatar's object
            if (_agents.find (from_id) != _agents.end ())
            {
                // locate arbitrator and agent
                Arbitrator *arb = (Arbitrator *)_arbitrator;
                Object *obj = _agents[from_id];

                // store the talk message as attribute update                                
                arb->updateObject (obj->getID (), 0, VASTATE_ATTRIBUTE_TYPE_STRING, (void *)chatmsg.c_str ());

                // also update the chat count
                int count;
                obj->get (1, count);
                count++;
                arb->updateObject (obj->getID (), 1, VASTATE_ATTRIBUTE_TYPE_INT, &count);
            }
        }
        break;

    case CREATE_OBJ:
        {
            Arbitrator *arb = (Arbitrator *)_arbitrator;

            Area area;
            area.radius = 800;
            area.height = 600;
            SimPeer::createRandomObjects (arb, 100, 1000, area);            

        }
        break;

    // create some food at specified location
    case CREATE_FOOD:
        {
            Arbitrator *arb = (Arbitrator *)_arbitrator;

            Position pos;
            e.get (0, pos);

            SimObjectType objtype = FOOD;
            Object *obj = arb->createObject ((byte_t)objtype, pos);

            int foodtype = 3;
            float quantity = 5.5f;            

            obj->add (foodtype);
            obj->add (quantity);
        }
        break;

    case ATTACK:
        break;

    case EAT:
        break;
        
    default:
        printf ("SimArbitrator::onEvent unrecognized event type: %d\n", action);
        break;
    }


    // 

    return true;
}

bool 
SimArbitrator::onMoveEvent (Vast::id_t from_id, const obj_id_t &obj_id, const Position &newpos)
{
    // perform move for the said object
    Arbitrator *arb = (Arbitrator *)_arbitrator;
#ifdef DEBUG_DETAIL
    printf ("SimArbitrator::onMoveEvent newpos for [%d] is (%d, %d)\n", from_id, (int)newpos.x, (int)newpos.y);
#endif

    if (_objects.find (obj_id) == _objects.end ())
        return false;

    Object *obj = _objects[obj_id];
    Position pos (newpos.x, newpos.y);

    arb->moveObject (obj->getID (), pos);

    return true;
}

// callback - by remote Arbitrator to create or initialize objects        
void 
SimArbitrator::onCreate (Object *obj, bool initializable)
{
#ifdef DEBUG_DETAIL
    printf ("SimArbitrator::onCreate obj %s agent: %d\n", obj->toString (), obj->agent);
#endif

    // store to object store
    obj_id_t obj_id = obj->getID ();

    //_objects[obj_id] = new Object (*obj);
    _objects[obj_id] = obj;

    // if this is an avatar object created by myself, then initialize it
    if (initializable == true && obj->agent != 0)
    {
        _agents[obj->agent] = _objects[obj_id];

        // create a "Talk Message" attribute
        std::string chatmsg ("my first chat message");
        obj->add (chatmsg);

        // test object get
        string str;
        obj->get (0, str);

        printf ("I've created the string '%s'\n", str.c_str ());

        // create "Chat Count" attribute
        int count = 0;
        obj->add (count);        
    }
}

void 
SimArbitrator::onDestroy (const obj_id_t &obj_id)
{
    if (_objects.find (obj_id) == _objects.end ())
        return;

    Object *obj = _objects[obj_id];    

    if (obj->agent != 0)
        _agents.erase (obj->agent);

    //delete obj;
    _objects.erase (obj_id);
}

// callback - by remote Arbitrator to notify their Object states have changed
void 
SimArbitrator::onUpdate (const obj_id_t &obj_id, int index, const void *value, size_t length, version_t version)
{
    //printf ("SimArbitrator:: onUpdate for obj [%s] ver: %d\n", _objects[obj_id], version);
}

void 
SimArbitrator::onMove (const obj_id_t &obj_id, const Position &newpos, version_t version)
{
#ifdef DEBUG_DETAIL
    printf ("SimArbitrator::onMove [%s] moves to (%d, %d) ver: %d\n", _objects[obj_id]->toString (), (int)newpos.x, (int)newpos.y, version);
#endif
}
    
// callback - to learn about the creation of avatar objects
void 
SimArbitrator::onAgentEntering (Node &agent)
{
    printf ("SimArbitrator::onAgentEntering [%ld] enters this region\n", agent.id);
}
    
void 
SimArbitrator::onAgentLeaving (Node &agent)
{

    printf ("SimArbitrator::onMove [%ld] leaves this region\n", agent.id);

}

// callback - to process any non-triggered Arbitrator logic (perform actions not provoked by events)
void 
SimArbitrator::tick ()
{
}


