

#include "MessageQueue.h"

namespace Vast
{   

    // process all currently received messages (invoking previously registered handlers)
    // return the number of messsages processed
    int 
    MessageQueue::processMessages ()
    {
        int num_msg = 0;

        // TODO: both fromhost and senttime are not used
        id_t        fromhost;           // nodeID of the sending host
        Message     *recvmsg;           // pointer to message received
        timestamp_t senttime;           // logical time the message was sent 
                                        
        map<id_t, MessageHandler *>::iterator it;

        // go through each of the message received at the network layer, and invoke
        // the respective handlers 
        while ((recvmsg = _net->receiveMessage (fromhost, senttime)) != NULL)
        {                    
            // check for special DISCONNECT message (NULL message content & size)
            if (recvmsg->data == NULL && recvmsg->size == 0)
            {
                // send the message to all handlers
                for (it = _handlers.begin (); it != _handlers.end (); it++)                    
                    it->second->handleMessage (*recvmsg);
                continue;
            }

            // loop through each target node and let them handle the message
            for (size_t i = recvmsg->targets.size ()-1; i >= 0; i++)
            {
                id_t target = recvmsg->targets[i];

                // skip if handler for this message is not supported / registered
                if (_handlers.find (target) == _handlers.end ())
                    continue;
                
                if (_handlers[target]->handleMessage (*recvmsg) == true)
                    num_msg++;
            }
        }

        // perform some post handleMessage tasks for each handler        
        for (it = _handlers.begin (); it != _handlers.end (); it++)
            it->second->postHandling ();

        return num_msg;
    }

    // store message handler for a particular group of messages (specified by group_id)
    // return false if handler already exists
    bool 
    MessageQueue::registerHandler (MessageHandler *handler)
    {
        // Note the handlerID is the NodeID for the logical node
        id_t handler_id = handler->setQueue (this, _net);

        // do not register if handler for this message handler is already registered
        if (handler_id == 0 || _handlers.find (handler_id) != _handlers.end ())
            return false;
       
        // allow custom initialization of the handler (NOTE that handler_no is known at this point)
        handler->initHandler ();

        _handlers[handler_id] = handler;

        return true;
    }

    // remove an existing handler
    // TODO: may need to reset / recycle the handler_counter, if expected handler registration is frequent
    bool 
    MessageQueue::unregisterHandler (MessageHandler *handler)
    {
        map<id_t, MessageHandler *>::iterator it;
        for (it = _handlers.begin (); it != _handlers.end (); it++)
            if (it->second == handler)
            {
                _handlers.erase (it);                            
                return true;
            }
        return false;
    }

    /*
    Addr &
    MessageQueue::getNetworkAddress (id_t id)
    {
        // perform NetworkID to handlerID translation
        static Addr address;
        address = _net->getAddress (id);
        return address;
    }
    */

    void 
    MessageQueue::registerID (id_t id)
    {
        _net->registerID (EXTRACT_HOST_ID (id));
    }

    // deliver a message to potentially serveral logical nodes
    // nodes with the same physical address will be sent only one message
    int 
    MessageQueue::sendMessage (Message &msg, bool is_reliable)    
    {
        int num_msg = 0;

        id_t target;        
        vector<id_t> &targets = msg.targets;               
        map<id_t, vector<id_t> *> host2targets;     // mapping of the logical nodes stored at each host
        
        // go through all targets and split them according to physical host
        for (size_t i = targets.size ()-1; i >= 0; i--)
        {
            target = targets[i];
            id_t host_id = _id2host[target];

            // find the target records, or create one if not exist
            if (host2targets.find (host_id) == host2targets.end ())
                host2targets[host_id] = new vector<id_t>;
            host2targets[host_id]->push_back (target);
        }
        
        // go through each host and send the message to them individually
        for (map<id_t, vector<id_t> *>::iterator it = host2targets.begin (); it != host2targets.end (); it++)
        {
            // replace targets in message then send out message
            msg.targets = *(it->second);

            if (_net->sendMessage (it->first, msg, is_reliable) > 0)                        
                num_msg++;

            delete it->second;
        }

        return num_msg;
    }

} // end namespace Vast

