



#ifndef _VAST_Manager_H
#define _VAST_Manager_H

#include "config.h"
#include "VASTTypes.h"
#include "voronoi.h"
#include "vor_SF.h"
#include "MessageHandler.h"


#define GATEWAY_HANDLER_NO      2       // assume gateway handler's ID is 2, as it is added to MessageQueue after Vast object

using namespace std;

namespace Vast
{

#define MAX_ADJUST_COUNT            4       // # of time-steps to adjust AOI
#define MAX_DROP_COUNT              4       // # of time-steps to disconnect a non-overlapped neighbor
//#define AOI_ADJUST_SIZE           (5)     // AOI-radius to adjust for dynamic AOI
#define AOI_DETECTION_BUFFER        (10)    // detection buffer around AOI


#define CHECK_EN_ONLY           // a boundary neighbor checks only its ENs as opposed to all AOI neighbors during neighbor discovery                                    
#define CHECK_REQNODE_ONLY_     // check neighbor discovery only for those who have asked


// for neighbor discovery 
//#define NEIGHBOR_OVERLAPPED            (0x01)
//#define NEIGHBOR_ENCLOSED              (0x02)

    typedef enum NeighborStates
    {
        NEIGHBOR_OVERLAPPED = 1,
        NEIGHBOR_ENCLOSED
    };

    typedef enum Manager_Message
    {
        UNKNOWN = 1,
        QUERY,              // find out host to contact for initial neighbor list
        HELLO,              // initial connection request
        EN,                 // query for missing enclosing neighbors
        MOVE,               // position update to normal peers
        MOVE_B,             // position update to boundary peers
        NODE,               // discovery of new nodes                
    };

    // 
    // This class allows a node to join as "Manager" 
    // (i.e., a super-peer that records / matches publications & subscriptions made by "Peers")
    // 
    class Manager : public MessageHandler
    {

    public:

        Manager (Node &self, length_t AOI)
            :MessageHandler (MSGGROUP_ID_VAST), _join_state (ABSENT), _DEFAULT_MANAGER_AOI (AOI)
        {
            // create proper data about self, other info will fill in later (ID, etc.)
            _self = self;

            // clear the handler_no so to assign our own later
            _self.id = EXTRACT_NODE_ID(_self.id);

            _voronoi = new vor_SF ();

        }
        
        ~Manager ()                        
        {
            // delete allocated memory                
            map<id_t, Node>::iterator it = _id2node.begin ();
            for (; it != _id2node.end (); it++)
                delete _neighbor_states[it->first];

            delete _voronoi;

            _join_state = ABSENT;
        }

        // move the current manager position to a new one
        Position &move (Position &p);

        void setAOI (length_t radius)
        {
            _self.aoi = radius;
        }

        // leave the role as manager
        void leave ()
        {
            // break all connections
            for (size_t i=0; i<_neighbors.size (); i++)
                disconnect (_neighbors[i]->id);
        }

        vector<Node *>& getManagers ()
        {
            return _neighbors;
        }

        voronoi *getVoronoi ()
        {
            return _voronoi;
        }

        Node * getSelf ()
        {
            return &_self;
        }

        NodeState joinState ()
        {
            return _join_state;
        }
       
    private:

        // perform initialization tasks for this handler (optional)
        // NOTE that all internal variables (such as handler_no) have been set at this point
        void initHandler ();

        // returns whether the message was successfully handled
        bool handleMessage (id_t from, Message &in_msg);

        // performs tasks after all messages are handled
        void postHandling ();


        //
        //  process functions
        //

        // notify new neighbors HELLO & EN messages
        void contactNewNeighbors ();

        // perform neighbor discovery check
        void checkNeighborDiscovery ();

        // check to disconnect neighbors no longer in view
        // returns number of neighbors removed        
        int removeNonOverlapped ();

        //
        // neighbor management inside Voronoi map
        //

        // insert a new node, will connect to the node if not already connected
        bool insertNode (Node &node, Addr *addr = NULL);
        bool deleteNode (id_t id, bool disconnect = true);
        bool updateNode (Node &node);

        //
        // helper functions
        //

        // send node infos to a particular node
        void sendNodes (id_t target, vector<id_t> &list, bool reliable = false);

        // send a list of IDs to a particular node
        void sendIDs (id_t target, msgtype_t msgtype, vector<id_t> &id_list);

        inline bool isNeighbor (id_t id);
        inline Position &isOverlapped (Position &pos);

        inline bool isAOINeighbor (id_t id, Node &neighbor, length_t buffer = 0)
        {
            return _voronoi->overlaps (id, neighbor.pos, neighbor.aoi + buffer);
        }

        inline bool isRelevantNeighbor (id_t node_id, Node &neighbor, length_t buffer = 0)
        {
            return (_voronoi->is_enclosing (node_id, neighbor.id) || isAOINeighbor (node_id, neighbor, buffer));                    
        }

        inline bool isSelf (id_t id)
        {
            return (_self.id == id);
        }

        // TODO: cleaner way?
        Node                _gateway;                   // info about Gateway node        
        length_t            _DEFAULT_MANAGER_AOI;       // default manager AOI as specified from above
        Addr                _tempAddr;                  // to provide an empty address used for connection via node ID

        NodeState       _join_state;                // state of joining

        voronoi            *_voronoi;
        Node                _self;        
        vector<Node *>      _neighbors;                 // a list of currently connected neighboring managers
         
        map<id_t, Node>     _id2node;                   // mapping from id to basic node info                
        map<id_t, int>      _count_drop;                // counter for disconnecting a remote node                                                                   
        map<id_t, map<id_t, int> *> _neighbor_states;   // neighbors' knowledge of my neighbors (for neighbor discovery notification)        
        map<id_t, Node>     _new_neighbors;             // nodes worth considering to connect        
        map<id_t, int>      _req_nodes;                 // nodes requesting for neighbor discovery check        

        // internal statistics
        Ratio               _NODE_Message;              // stats for NodeMessages received

    };

} // namespace Vast

#endif