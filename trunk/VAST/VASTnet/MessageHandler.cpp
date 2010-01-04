

#include "MessageHandler.h"
#include "MessageQueue.h"               // to allow casting of MessageQueue pointer so that new handlers can be registered 

namespace Vast
{   
        
    // add additional handlers to the same message queue
    bool 
    MessageHandler::addHandler (MessageHandler *handler)
    {
        return ((MessageQueue *)_msgqueue)->registerHandler (handler);
    }

    // remove handlers added from current handler
    bool 
    MessageHandler::removeHandler (MessageHandler *handler)
    {
        return ((MessageQueue *)_msgqueue)->unregisterHandler (handler);
    }

    int 
    MessageHandler::sendMessage (Message &msg, vector<id_t> *failed_targets)
    {
        // assign default message group of the current handler
        // NOTE: inter-message group communication is possible by specifying / overriding the 0 value
        if (msg.msggroup == 0)
            msg.msggroup = _msggroup;

        return ((MessageQueue *)_msgqueue)->sendMessage (msg, failed_targets);
    }

    // notify the network layer of nodeID -> Address mapping        
    bool 
    MessageHandler::notifyMapping (id_t node_id, Addr *addr)
    {
        return ((MessageQueue *)_msgqueue)->notifyMapping (node_id, addr);
    }

    // get a specific address by msggroup
    Addr &
    MessageHandler::getAddress (id_t id)
    {
        return ((MessageQueue *)_msgqueue)->getAddress (id);
    }

    // obtain a unique ID generated on this host, based on an optional user-specified group ID
    id_t 
    MessageHandler::getUniqueID (int group_id, bool is_gateway)
    {
        return ((MessageQueue *)_msgqueue)->getUniqueID (group_id, is_gateway);
    }

    // store network layer so that the logic in processmsg may send message to network    
    id_t
    MessageHandler::setQueue (void *msgqueue, Vast::VASTnet *net)
    {                
        _msgqueue = msgqueue;
        _net = net;        
        return _msggroup;
    }

    // test if an ID is from gateway
    bool 
    MessageHandler::isGateway (id_t id)
    {
        return (EXTRACT_HOST_ID(id) == NET_ID_GATEWAY);
    }

} // end namespace Vast

