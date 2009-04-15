


#ifndef _VAST_Peer_H
#define _VAST_Peer_H

#include "config.h"
#include "MessageHandler.h"

using namespace std;

namespace Vast
{
    // 
    // This class allows a user to join as "Peer" (i.e., a regular user that sends / receives messagesa)
    // 
    class Peer : public MessageHandler
    {

    public:

        Peer (Node &self)                        
            :MessageHandler (MSGGROUP_ID_VAST), _join_state (ABSENT)
        {            
            _self = self;

            // clear the handler_no so to assign our own later
            _self.id = EXTRACT_NODE_ID(_self.id);

        }

        
        ~Peer ()                        
        {
        }

        // subscribe to a particular area at a given layer
        // returns the unique subscription ID
        id_t subscribe (Area &subscription, layer_t layer);

        // peer moves (along with a previously specified subscription area) to a new location
        Position &move (id_t sub_no, layer_t layer, Position &pos);

        NodeState joinState ()
        {
            return _join_state;
        }

    private:
        NodeState        _join_state;           // state of joining

        id_t _id;       

        // perform initialization tasks for this handler (optional)
        // NOTE that all internal variables (such as handler_no) have been set at this point
        void initHandler ();

        // returns whether the message was successfully handled
        bool handleMessage (id_t from, Message &in_msg);

        Node                 _self;

    };

} // namespace Vast
#endif