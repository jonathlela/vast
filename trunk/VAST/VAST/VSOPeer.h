/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2009 Shun-Yun Hu  (syhu@yahoo.com)
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

/*
 *  VSOPeer.h -- a logical node that performs VON-based Self-organizing Overlay (VSO) functions 
 *               main functions include: notifyLoading and self adjustment of area
 *          
 *               this is a sub-class of VONPeer
 *
 *               NOTE that we assume the following:
 *                  - each VON node is uniquely identified by an external ID
 *                  - each VON node can be associated with a physically reachable address specified in Addr
 *
 *  history 2010/03/18      adopted from ArbitratorImpl.h in VASTATE
 *
 */



#ifndef _VAST_VSOPeer_H
#define _VAST_VSOPeer_H

#include "Config.h"
#include "VASTTypes.h"
#include "VONPeer.h"
#include "VSOPolicy.h"

// load balancing settings
#define VSO_MOVEMENT_FRACTION           (0.1f)  // fraction of remaining distance to move for nodes
#define VSO_TIMEOUT_OVERLOAD_REQUEST    (3)     // seconds to re-send a overload help request
#define VSO_INSERTION_TRIGGER           (5)     // # of matcher movement requests before an insertion should be requested

#define VSO_AOI_OVERLAP_BUFFER          (5)    // buffer size determining whether an AOI overlaps with a region

// ownership transfer setting
//#define COUNTDOWN_REMOVE_AVATAR 20      // # of steps to delete disconnected avatar object
//#define COUNTDOWN_PROMOTE       20      // # of steps can insert a new Arbitrator in the same area after an Arbitrator inserted
#define COUNTDOWN_TRANSFER          (0.3) // # of seconds before transfering ownership to a neighbor
//#define COUNTDOWN_TAKEOVER      5       // # of steps to wait before ownership takeover
//#define COUNTDOWN_REMOVE_OBJ    5       // # of steps to wait before removing a non-AOI object at an agent

#define TIMEOUT_EXPIRING_OBJECT     (0.5)    // seconds to delete an un-ownered object (assuming 25 steps / sec * 10 sec)
//#define TIMEOUT_JOINING             (25*3)  // time-steps to re-send a join request
//#define TIMEOUT_OVERLOAD_REQUEST    (25*1)  // time-steps to re-send a overload help request


using namespace std;

namespace Vast
{

    // WARNING: VON messages currently should not exceed VON_MAX_MSG defined in Config.h
    //          otherwise there may be ID collisons with other handlers that use VONpeer
    //          internally (e.g., VASTClient in VAST or Arbitrator in VASTATE)
    typedef enum
    {
        VSO_DISCONNECT = 0,     // VSO's disconnect
        VSO_CANDIDATE = 11,     // notify gateway of a candidate node    
        VSO_PROMOTE,            // promotes a candidate to functional
        VSO_INSERT,             // overload request for insertion
        VSO_MOVE,               // overload request for movement
        VSO_JOINED,             // a new node has joined successfully
        VSO_TRANSFER,           // transfer ownership of an object
        VSO_TRANSFER_ACK,       // acknowledgment of ownership transfer
        VSO_REQUEST,            // request for object
        
    } VSO_Message;

    // object ownership info maintained by a VSOPeer
    class VSOSharedObject
    {
    public:

        VSOSharedObject ()
        {
            obj         = NULL;
            is_owner    = false;
            in_transit  = 0;
            last_update = 0;
            closest     = 0;
        };
    
        void *      obj;            // pointer to the object itself
        Area        aoi;            // area of interest of the object (will determine how far the object should spread)
        bool        is_owner;       // whether it is owned by the local host
        timestamp_t in_transit;     // countdown of object in ownership transfer to others 
        timestamp_t last_update;    // time of object's last update (used for Object expiring)
        id_t        closest;        // ID for closest managing node for this object
    };

    // message for ownership transfer
    class VSOOwnerTransfer : public Serializable 
    {
    public:
        VSOOwnerTransfer (id_t id, id_t new_o, id_t old_o)
        {
            obj_id = id;
            new_owner = new_o;
            old_owner = old_o;
        };

        VSOOwnerTransfer ()
        {
            obj_id = 0;
            new_owner = 0;
            old_owner = 0;
        };

        // size of this class
        inline size_t sizeOf () const
        {
            return sizeof (id_t)*3;
        }
    
        size_t serialize (char *p) const
        {
            if (p != NULL)
            {
                memcpy (p, &obj_id, sizeof (id_t));     p += sizeof (id_t);
                memcpy (p, &new_owner, sizeof (id_t));  p += sizeof (id_t);
                memcpy (p, &old_owner, sizeof (id_t)); 
            }
            return sizeOf ();
        }
    
        size_t deserialize (const char *p, size_t size)
        {
            // perform size check        
            if (p != NULL && size >= sizeOf ())
            {
                memcpy (&obj_id, p, sizeof (id_t));   p += sizeof (id_t);
                memcpy (&new_owner, p, sizeof (id_t));   p += sizeof (id_t);
                memcpy (&old_owner, p, sizeof (id_t));
    
                return sizeOf ();
            }
            return 0;
        }

        id_t    obj_id;     // object to be transferred
        id_t    new_owner;  // nodeID for new owner
        id_t    old_owner;  // nodeID for previous owner
    };

    // 
    // This class joins a node as "VONPeer", which allows the user client
    // to execute VON commands: move, getNeighbors
    // 
    class EXPORT VSOPeer : public VONPeer
    {

    public:

        VSOPeer (id_t id, VONNetwork *net, VSOPolicy *policy, length_t aoi_buffer = AOI_DETECTION_BUFFER);        
        ~VSOPeer ();                        

        // perform joining the overlay
        void join (Area &aoi, Node *gateway);

        // returns whether the message was successfully handled
        bool handleMessage (Message &in_msg);

        // current node overloaded, call for help
        // note that this will be called continously until the situation improves
        void notifyLoading (float level);

        // notify the gateway that I can be available to join
        bool notifyCandidacy ();

        // check if a particular point is within our region
        bool inRegion (Position &pos);

        // process incoming messages & other regular maintain stuff
        void tick ();

        //
        // shared object ownership management
        //

        // add a particular shared object into object pool for ownership management
        bool insertSharedObject (id_t obj_id, Area &aoi, bool is_owner = true, void *obj = NULL);

        // change position for a particular shared object
        bool updateSharedObject (id_t obj_id, Area &aoi);

        // remove a shared object
        bool deleteSharedObject (id_t obj_id);

        // obtain reference to a shared object, returns NULL for invalid object
        VSOSharedObject *getSharedObject (id_t obj_id);

        // get the number of objects I own
        int getOwnedObjectSize ();

        // check if I'm the owner of an object
        bool isOwner (id_t obj_id);

        // request particular object from a neighbor node
        // TODO: try to hide this from public?
        void requestObjects (id_t target, vector<id_t> &obj_list);

        // find the closest enclosing neighbor to a given position (other than myself)
        // returns 0 for no enclosing neighbors
        id_t getClosestEnclosing (Position &pos);

    private:

        // change the center position in response to overload signals
        void movePeerPosition ();

        // check if neighbors need to be notified of object updates
        // returns the # of updates sent
        int checkUpdateToNeighbors ();

        // remove obsolete objects (those unowned objects no longer being updated)
        void removeObsoleteObjects ();        

        //
        // ownership transfer methods
        //

        // check to see if subscriptions have migrated 
        // returns the # of transfers
        int checkOwnershipTransfer ();

        // process ownership transfer notification
        void processTransfer (Message &in_msg);

        // accept and ownership transfer
        void acceptTransfer (VSOOwnerTransfer &transfer);

        // make an object my own
        void claimOwnership (id_t obj_id, VSOSharedObject &so);

        //
        // helper methods
        //

        // get a new node that can be inserted
        bool findCandidate (float level, Node &new_node);

        // get the center of all current agents I maintain
        bool getLoadCenter (Position &center);

        // check whether a new node position is legal
        bool isLegalPosition (const Position &pos, bool include_self);

        // get a list of neighbors whose regions are covered by the specified AOI
        // optionally to include the closest neighbor
        bool getOverlappedNeighbors (Area &aoi, vector<id_t> &neighbors, id_t closest = 0);

        // helper function to identify if a node's gateway
        inline bool isGateway (id_t id)
        {
            return (_policy->getGatewayID () == id);
        }
        
        
        VSOPolicy *         _policy;            // various load balancing policies
        
        NodeState           _vso_state;         // state of the VSOpeer

        Node                _newpos;            // new AOI & position to be updated due to load balancing
                
        // server data (Gateway node)       
        vector<Node>        _candidates;          // list of potential nodes      
        map<id_t, Node>     _promote_requests;    // requesting nodes' timestamp of promotion and position, index is the promoted node
        
        // ownership related
        map<id_t, VSOSharedObject>  _objects;               // list of objects I manage
        map<id_t, int>              _transfer_countdown;    // countdown to transfer ownership
        map<id_t, int>              _reclaim_countdown;     // countdown to reclaim ownership to unowned objects
        
        map<id_t, VSOOwnerTransfer> _transfer;              // pending ownership transfers
        map<id_t, id_t>             _obj_requested;         // objects request & the target being asked

        // counters for current node
        int                 _load_counter;      // counter for # of timesteps overloaded (positive) or underloaded (negative)
        int                 _overload_count;    // counter for # of times a OVERLOAD_M request is sent
    };

} // namespace Vast

#endif
