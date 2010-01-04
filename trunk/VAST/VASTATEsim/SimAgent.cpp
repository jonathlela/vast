

#include "SimAgent.h"
#include "Agent.h"

#include "VASTATEsim.h" // SimObjectType

using namespace Vast;

SimAgent::~SimAgent ()
{
    
    // remove allocated neighbors
    for (size_t i=0; i < _neighbors.size (); i++)
        delete _neighbors[i];

    _neighbors.clear ();
            
    // remove all discovered objects
    for (map<obj_id_t, Object *>::iterator it = _objects.begin (); it != _objects.end (); it++)
        delete it->second;

    _objects.clear ();    

    // clear chat messages
    for (size_t i=0; i < _chatmsg.size (); i++)
        delete _chatmsg[i];

    _chatmsg.clear ();

}

// callback - learn about the addition/removal of objects
void 
SimAgent::onCreate (Object &obj, bool is_self)
{
    obj_id_t id = obj.getID ();
   
    if (_objects.find (id) != _objects.end ())
    {
        printf ("SimAgent::onCreate () redudent object creation [%ld]\n", id);
        //return;
    }

    _objects[id] = new Object (obj);    

    // NOTE: _self may not have been initialized yet 
    //if (_self != NULL)
    printf ("[%ld] onCreate() object %s at (%d, %d)\n", _self->id, obj.toString (), (int)obj.getPosition ().x, (int)obj.getPosition ().y);       

    // record neighbors for avatar objects
    if (obj.agent != 0)
    {
        Area a (obj.getPosition (), 0);
        Vast::id_t agent_id = obj.agent;
        Addr addr;
        Node *node = new Node (agent_id, (timestamp_t)0, a, addr);
        _neighbors.push_back (node);

        // debug, see if the chat attribute already exists
        string str;
        if (obj.get (0, str))
            printf ("[%ld] onCreate learns of chat attributes\n", _self->id);
    
        int count = 999;
        obj.get (1, count);
    
        printf ("agent chatmsg count: %d\n", count);
    }
    
    // debug: check if attributes have been received for different objects
    switch ((SimObjectType)obj.type)
    {
    case FOOD:
        {
            int foodtype = 99;
            float quantity = 99.99f;

            obj.get (0, foodtype);
            obj.get (1, quantity);

            printf ("food type: %d quantity: %f\n", foodtype, quantity);
        }
        break;
    default:
        break;
    }
}

void 
SimAgent::onDestroy (const obj_id_t &obj_id)
{
    if (_objects.find (obj_id) == _objects.end ())
        return;

    Object *obj = _objects[obj_id];

    //printf ("[%d] onDestroy() object %s destroyed\n", _self->id, obj->toString ());

    for (unsigned int i=0; i < _neighbors.size (); i++)
    {
        if (_neighbors[i]->id == obj->agent)
        {
            delete _neighbors[i];
            _neighbors.erase (_neighbors.begin () + i);
            break;
        }
    }

    delete obj;
    _objects.erase (obj_id);
}

// callback - learn about state changes of known AOI objects
void 
SimAgent::onUpdate (const obj_id_t &obj_id, int index, const void *value, size_t length, version_t version)
{    
    if (_objects.find (obj_id) == _objects.end ())
        return;
    
#ifdef DEBUG_DETAIL
    printf ("[%ld] onUpdate() object %s\n", _self->id, _objects[obj_id]->toString ());
#endif

    Object *obj = _objects[obj_id];

    // update the chat message
    if (obj->agent != 0)
    {
        if (index == 0)
        {
            _chatmsg.push_back (new std::string ((char *)value, length));
        
            _objects[obj_id]->set (0, string ((char *)value, length));
            _objects[obj_id]->version = version;
        }
        // chat msg count
        else if (index == 1)
        {
            _objects[obj_id]->set (1, *(int *)value);
    
            int count = 999;
            _objects[obj_id]->get (1, count);
    
            printf ("count: %d\n", count);
        }
    }
}

void 
SimAgent::onMove (const obj_id_t &obj_id, const Position &newpos, version_t version)
{
#ifdef DEBUG_DETAIL
    printf ("[%ld] onMove() object %s moves to (%d, %d) \n", _self->id, _objects[obj_id]->toString (), (int)newpos.x, (int)newpos.y);
#endif

    Object *obj = _objects[obj_id]; 

    Vast::id_t agent = obj->agent;

    // update the new position for the neighbor
    for (unsigned int i=0; i < _neighbors.size (); i++)
    {
        if (_neighbors[i]->id == agent)
        {
            _neighbors[i]->aoi.center = newpos;
            break;
        }
    }

    obj->setPosition (newpos);
}

// callback - learn about the result of login
void 
SimAgent::onAdmit (const char *status, size_t size)
{
    // try to enter the virtual world if authenticated
    if (strcmp (status, "you've passed!") == 0)
    {
    }
}

// callback - an app-specific message from gateway arbitrator (server) to agent
void 
SimAgent::onMessage (Message &in_msg)
{
    printf ("Agent onMessage () called\n");
}

size_t 
SimAgent::getChat (char *buf)
{
    if (_chatmsg.size () == 0)
    {
        buf[0] = 0;
        return 0;
    }

    size_t size = _chatmsg[0]->size ();
    memcpy (buf, _chatmsg[0]->c_str (), size); 
    buf[size] = 0;

    delete _chatmsg[0];
    _chatmsg.erase (_chatmsg.begin ());
    
    return size;
}


