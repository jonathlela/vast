

#include "VSOPeer.h"

using namespace Vast;

namespace Vast
{   

    // default constructor
    VSOPeer::VSOPeer (id_t id, VONNetwork *net, VSOPolicy *policy, length_t aoi_buffer)
        :VONPeer (id, net, aoi_buffer), _policy (policy)
    {
        _newpos = _self;

        notifyCandidacy ();
    }

    // default destructor
    VSOPeer::~VSOPeer ()
    {
    }

    bool 
    VSOPeer::handleMessage (Message &in_msg)    
    {
        // if join is not even initiated, do not process any message 
        //if (_state != JOINED)
        //    return false;

        switch ((VSO_Message)in_msg.msgtype)
        {

        // registration of a potential node with the gateway
        case VSO_CANDIDATE:
            {
                Node candidate;

                in_msg.extract (candidate);

                // store potential nodes to join
                _candidates.push_back (candidate);
            }
            break;
                         
        // Overloaded node's request for inserting a new helper
        case VSO_INSERT:                      
            // only the gateway can process it
            if (_self.id != _policy->getGatewayID ())
            {
                printf ("[%llu] VSOPeer::handleMessage () VSO_INSERT received by non-gateway\r\n", _self.id);
            }
            else
            {
                float level;
                Position join_pos;

                in_msg.extract ((char *)&level, sizeof (float));
                in_msg.extract (join_pos);

                // TODO: ignore redundent requests at same position
                
                // promote one of the spare potential nodes
                Node new_node;

                // TODO: if findCandidate () fails, insert virtual
                // return from loop either request successfully served (promotion sent) 
                // or no candidates can be found
                while (findCandidate (level, new_node))
                {
                    // fill in the stressed node's contact info & join location
                    Node requester;
                    
                    requester.id = in_msg.from;
                    requester.time = _net->getTimestamp ();
                    requester.aoi.center = join_pos;

                    Message msg (VSO_PROMOTE);
                    msg.priority = 1;
                    msg.store (requester);
                    msg.store (_net->getHostAddress ());

                    // send promotion message
                    _net->notifyAddressMapping (new_node.id, new_node.addr);
                    msg.addTarget (new_node.id);

                    // record the promoted position if sent success
                    if (_net->sendVONMessage (msg) > 0)   
                    {
                        _promote_requests[new_node.id] = requester;
                        break;
                    }
                }
            }            
            break;
        
        // a candidate node gets promoted as an actual node
        case VSO_PROMOTE:        
            if (_state == ABSENT)
            {
                // TODO: find a way so gateway address need not be sent?
                Position join_pos;
                Addr addr;
                in_msg.extract (join_pos);
                in_msg.extract (addr);

                Node gateway;
                gateway.id = in_msg.from;
                gateway.addr = addr;

                Area aoi (join_pos, 5);

                // join at the specified position, but only if I'm available
                join (aoi, &gateway);
            }
            break;

        // Overloaded node's request for neighbors to move closer
        case VSO_MOVE:            
            {                             
                // received not my enclosing neighbor's help signal
                if (_Voronoi->is_enclosing (in_msg.from) == false)
                {
                    printf ("[%llu] VSO_MOVE received from non-enclosing neighbor [%llu]\r\n", _self.id, in_msg.from);                    
                    break;
                }

                // extract loading
                float level;
                in_msg.extract ((char *)&level, sizeof (float));

                // calculate new position after moving closer to the stressed node
                Position neighbor_pos = _Voronoi->get (in_msg.from);
                Position temp_pos = _newpos.aoi.center;

                // move at a speed proportional to the severity of the overload
                temp_pos += (((neighbor_pos - temp_pos) * VSO_MOVEMENT_FRACTION) * level);

                if (isLegalPosition (temp_pos, false))
                    _newpos.aoi.center = temp_pos;        
            }                
            break;
        
        default:
            return VONPeer::handleMessage (in_msg);
        }

        return true;

    } // end handleMessage ()

    // current node overloaded, call for help
    // note that this will be called continously until the situation improves
    void
    VSOPeer::notifyLoading (float level)
    {                               
        // first adjust the current load level record
        if (level == 0)
        {
            // normal loading
            _load_counter = 0;
            return;
        }
        else if (level < 0)
        {
            // underload       
            _load_counter--;
        }
        else
        {   
            // overload
            _load_counter++;
        }

        // if overload continus, we try to do something
        // TODO: 
        if (_load_counter > VSO_TIMEOUT_OVERLOAD_REQUEST * _net->getTickPerSecond ())
        {    
            // reset # of detected overloads
            if (_overload_count < 0)
                _overload_count = 0;

            _overload_count++;
                       
            // if the overload situation just occurs, try to move boundary first
            if (_overload_count < VSO_INSERTION_TRIGGER) 
            {                        
                // send the level of loading to neighbors
                // NOTE: important to store the VONpeer ID as this is how the responding node will recongnize
                Message msg (VSO_MOVE);
                msg.from = _self.id;   
                msg.priority = 1;            
                msg.store ((char *)&level, sizeof (float));

                msg.targets = _Voronoi->get_en (_self.id);                
                _net->sendVONMessage (msg);
            }

            // if the overload situation persists, request matcher insertion from gateway           
            else
            {           
                //Position pos = findInsertion (_VONpeer->getVoronoi ());
                Position pos;
                _policy->getLoadCenter (pos);

                Message msg (VSO_INSERT);
                msg.priority = 1;
                msg.store ((char *)&level, sizeof (float));
                msg.store (pos);
                msg.addTarget (_policy->getGatewayID ());
                _net->sendVONMessage (msg);    

                _overload_count = 0;
            }

            _load_counter = 0;
        }
        // underload event
        else if (_load_counter < -(VSO_TIMEOUT_OVERLOAD_REQUEST * _net->getTickPerSecond ()))
        {            
            // reset
            if (_overload_count > 0)
                _overload_count = 0;

            _overload_count--;

            if (_overload_count > (-VSO_INSERTION_TRIGGER))
            {
                // TODO: notify neighbors to move further away?
            }

            // if we're not gateway then can depart
            else if (_self.id != _policy->getGatewayID ())
            {                
                // depart as node if loading is below threshold                
                leave ();

                notifyCandidacy ();               
                _overload_count = 0;
            }
                    
            _load_counter = 0;
        }
        // normal loading, reset # of OVERLOAD_M requests
        else if (_load_counter == 0)
            _overload_count = 0;        
    }

    // change the center position in response to overload signals
    void 
    VSOPeer::moveCenter ()
    {
       // move myself towards the center of agents
        Position center;
        if (_policy->getLoadCenter (center))
            _newpos.aoi.center += ((center - _newpos.aoi.center) * VSO_MOVEMENT_FRACTION);

        // if nothing has changed
        if (_newpos.aoi.center == _self.aoi.center)
            return;

        // performe actual movement
        this->move (_newpos.aoi);

        // update self info
        // NOTE: currently only position will change, not AOI
        _self.aoi.center = _newpos.aoi.center;

        // update other info (such as AOI) for the new position
        _newpos = _self;
    }

    
    // find a suitable new node to join given a certain need/stress level
    bool 
    VSOPeer::findCandidate (float level, Node &new_node)
    {
        if (_candidates.size () == 0)
            return false;
        
        // simply return the first
        // TODO: better method?
        new_node = _candidates[0];
        _candidates.erase (_candidates.begin ());

        string str;
        new_node.addr.toString (str);

        printf ("\n[%llu] promoting [%llu] %s as new node\n\n", _self.id, new_node.id, str.c_str ());

        return true;
    }

    // notify the gateway that I can be available to join
    bool 
    VSOPeer::notifyCandidacy ()
    {
        // register with gateway as a candidate node, as agreed by policy
        if (isGateway (_self.id) == true || _policy->isCandidate () == false)
            return false;
        
        Message msg (VSO_CANDIDATE);
        msg.priority = 1;
        msg.store (_self);
        msg.addTarget (_policy->getGatewayID ());
        _net->sendVONMessage (msg);

        return true;       
    }


    bool 
    VSOPeer::isLegalPosition (const Position &pos, bool include_self)
    {        
        /*
        // check for redundency
        for (size_t i=0; i < _legal_pos.size (); i++)
            if (_legal_pos[i] == pos)
                return false;    

        // check if position is out of map
        if (pos.x < 0.0 || pos.y < 0.0 || pos.x >= _para.world_width || pos.y >= _para.world_height)
            return false;

        // loop through all enclosing arbitrators
        for (map<id_t,Node>::iterator it = _arbitrators.begin (); it != _arbitrators.end (); it++)
        {
            if (!include_self && it->first == _VONpeer->getSelf ()->id)
                continue;

            // check if the position to check is somewhat disant to all known arbitrators
            //if (pos.distance (it->second.aoi.center) < (_para.default_aoi / 2))
            if (pos.distance (it->second.aoi.center) < 5)
                return false;
        }
        */
        return true;
    }


} // end namespace Vast
