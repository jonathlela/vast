

#include "MessageHandler.h"
#include "MessageQueue.h"               // to allow casting of MessageQueue pointer so that new handlers can be registered 

namespace Vast
{   

    /*
    void MessageHandler::registerID (id_t my_id)
    {
        _id = my_id;
    }
    */
        
    // add additional handlers to the same message queue
    bool MessageHandler::addHandler (MessageHandler *handler)
    {
        return ((MessageQueue *)_msgqueue)->registerHandler (handler);
    }

    // remove handlers added from current handler
    bool MessageHandler::removeHandler (MessageHandler *handler)
    {
        return ((MessageQueue *)_msgqueue)->unregisterHandler (handler);
    }

    // store network layer so that the logic in processmsg may send message to network    
    id_t
    MessageHandler::setQueue (void *msgqueue, Vast::VASTnet *net)
    {                
        _msgqueue = msgqueue;
        _net = net; 
        return _id;
    }

    /*
    Addr &
    MessageHandler::getNetworkAddress (id_t id)
    {
        return ((MessageQueue *)_msgqueue)->getNetworkAddress (id);
    }
    */

    int 
    MessageHandler::sendMessage (Message &msg, bool is_reliable)
    {
        return ((MessageQueue *)_msgqueue)->sendMessage (msg, is_reliable);
    }

} // end namespace Vast