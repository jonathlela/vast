




#include "Peer.h"

using namespace Vast;

namespace Vast
{   
     
    id_t
    Peer::subscribe (Area &subscription, layer_t layer)
    {
        // find the nearest manager to connect

        return true;
    }

    // peer moves (along with a previously specified subscription area) to a new location
    Position &
    Peer::move (id_t sub_no, layer_t layer, Position &pos)
    {
        return _self.pos;
    }

    // perform initialization tasks for this handler (optional)
    // NOTE that all internal variables (such as handler_no) have been set at this point
    void 
    Peer::initHandler ()
    {
        // build correct self ID
        if (EXTRACT_HANDLER_NO(_self.id) == 0)
            _self.id = COMBINE_ID(_handler_no, _self.id);
    }

    // returns whether the message was successfully handled
    bool 
    Peer::handleMessage (id_t from, Message &in_msg)
    {
        return true;
    }

} // end namespace Vast
