

#include "MessageQueue.h"

namespace Vast
{   

    // store message handler for a particular group of messages (specified by group_id)
    // return false if handler already exists
    bool 
    MessageQueue::registerHandler (MessageHandler *handler)
    {
        // Note the msggroup is the NodeID for the logical node
        id_t msggroup = handler->setQueue (this, _net);

        // do not register if handler for this message handler is already registered
        if (_handlers.find (msggroup) != _handlers.end ())
            return false;
       
        // allow custom initialization of the handler (NOTE that msggroup is known at this point)
        handler->initHandler ();

        _handlers[msggroup] = handler;
        
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

    // notify a remote target node's network layer of nodeID -> Address mapping
    // target can be self id or 0 to indicate storing locally
    bool 
    MessageQueue::notifyMapping (id_t nodeID, Addr *addr)
    {        
        // store local copy of the mapping
        _id2host[nodeID] = addr->host_id;
        _net->storeMapping (*addr);

        return true;
    }

    // deliver a message to potentially serveral logical nodes
    // nodes with the same physical address will be sent only one message
    // an optional list of failed targets can be returned for those targets sent unsuccessfully
    // return the # of targets with valid connections (so message can probably be delivered)
    int 
    MessageQueue::sendMessage (Message &msg, vector<id_t> *failed_targets)
    {
        // # of messages sent
        int num_msg = 0;
              
        // categorize messages according to actual physical end-host
        id_t target;
        id_t host_id;

        vector<id_t> &targets = msg.targets;               
        map<id_t, vector<id_t> *> host2targets;     // mapping of the logical nodes stored at each host
        
        // go through all targets and split them according to physical host
        for (size_t i = 0; i < targets.size (); i++)
        {
            target = targets[i];
                             
            // determine target's host_id
            if (_id2host.find (target) != _id2host.end ())
                host_id = _id2host[target];
            else                
            {                    
                // route to default host if mapping is not found
                host_id = _default_host;
            }            

            // verify the link is there
            if (_net->validateConnection (host_id))
            {
                num_msg++;    

                // find the target records, or create one if not exist
                if (host2targets.find (host_id) == host2targets.end ())
                    host2targets[host_id] = new vector<id_t>;            
                
                host2targets[host_id]->push_back (target);
            }
            // if link doesn't exist, record failed targets
            else if (failed_targets != NULL)
                failed_targets->push_back (target);            
        }
        
        // go through each host and send the message to them individually
        for (map<id_t, vector<id_t> *>::iterator it = host2targets.begin (); it != host2targets.end (); it++)
        {
            // replace targets in message then send out message
            msg.targets = *(it->second);

            // TODO: should perform error checking & feedback? 
            _net->sendMessage (it->first, msg, msg.reliable);
                
            // release the memory to store targets 
            delete it->second;
        }

        return num_msg;
    }

    // get a specific address by nodeID
    Addr &
    MessageQueue::getAddress (id_t id)
    {
        static Addr null_address;

        // by default we return self address
        //if (id == _host_id || id == NET_ID_UNASSIGNED)
        if (id == _net->getHostID () || id == NET_ID_UNASSIGNED)
            return _net->getHostAddress ();

        // perform msggroup to hostID translation        
        else if (_id2host.find (id) == _id2host.end ())
        {
            printf ("MessageQueue::getAddress () cannot find hostID for msggroup: %d\n", (int)id);
            return null_address;
        }

        return _net->getAddress (_id2host[id]);
    }

    // perform external and internal message processing, and post handling tasks for each handler
    void
    MessageQueue::tick ()
    {
        // convert incoming messages to VAST or socket messages first
        _net->process ();

        // process incoming (external) messages first and flushing out the messages
        processMessages ();

        // perform post handleMessage tasks for each handler        
        map<id_t, MessageHandler *>::iterator it;       
        for (it = _handlers.begin (); it != _handlers.end (); it++)
            it->second->postHandling ();

        _net->flush ();

        // process once more for internal messages (targeted at self)
        // (e.g., to reflect proper neighbor list for Clients)
        processMessages ();
    }

    // store default route for unaddressable targets
    void 
    MessageQueue::setDefaultHost (id_t default_host)
    {
        _default_host = default_host;
    }


    // process all currently received messages (invoking previously registered handlers)
    // return the number of messsages processed
    int 
    MessageQueue::processMessages ()
    {
        int num_msg = 0;

        Message     *recvmsg;               // pointer to message received
        id_t        fromhost;               // nodeID of the sending host (i.e., a hostID) 
        
        map<id_t, MessageHandler *>::iterator it;       // iterator for message handlers

        // go through each of the message received at the network layer, 
        // invoke the respective handlers 
        // NOTE: if it's a UDP message, fromhost may be NET_ID_UNASSIGNED
        while ((recvmsg = _net->receiveMessage (fromhost)) != NULL)
        {       
            // check for DISCONNECT message 
            if (recvmsg->msgtype == DISCONNECT)
            {                
                vector<id_t> from_list;

                // if no mapping can be found, use the original host ID
                if (from_list.size () == 0)
                    from_list.push_back (fromhost);

                for (size_t i=0; i < from_list.size (); i++)
                {
                    recvmsg->from = from_list[i];

                    // send DISCONNECT to all handlers, note that msgtype may be replaced by
                    // the message handlers when handling the DISCONNECT message (e.g., replace VON_DISCONNECT with DISCONNECT, as by VASTMatcher)
                    for (it = _handlers.begin (); it != _handlers.end (); it++)  
                    {
                        // make sure DISCONNECT is specified every time
                        recvmsg->msgtype = DISCONNECT;
                        it->second->handleMessage (*recvmsg);                        
                    }

                    // NOTE: id2host mapping has to be kept, so that future re-connect may work (client to a matcher, for example)
                    //       without having to re-notify the address mapping again

                    // remove id to host mapping
                    //_id2host.erase (from_list[i]);
                }
            }

            // check if we should process or forward the message 
            else if (recvmsg->from != NET_ID_UNASSIGNED) 
            {             
                
                // record a copy of the fromID to hostID mapping (to send reply messages)
                // NOTE that 'from' cannot be self, otherwise all messages to self could be
                //      routed to a foreign host
                if (fromhost != _net->getHostID () && 
                    _id2host.find (recvmsg->from) == _id2host.end ())
                {

                    // NOTE: 'from' and 'fromhost' can differ if the following condition exists:
                    //       1) 'from' is a logical ID (not host, but a VONpeer, for example)
                    //       2) 'from' is a hostID but the message has been forwarded by 'fromhost' (for example, for QUERY message)
                    // in such case, a forward mapping may be built for a certain hostID
                    // but this may causes issues in some cases 
                    // (e.g. explicit notifyMapping () is required to notify network layer of the correct hostID mapping)

                    // record mapping if this is not a relayed message
                    if (recvmsg->from == fromhost)
                        _id2host[recvmsg->from] = fromhost;
                }
                
                
                //
                // go through each target
                //
                //vector<id_t> forward_targets;       // targets handled by remote hosts
                vector<id_t> local_targets;         // targets handled by local host

                map<id_t, id_t>::iterator it;
                for (size_t i = 0; i < recvmsg->targets.size (); i++)
                {
                    id_t target = recvmsg->targets[i];

                    it = _id2host.find (target);

                    // if we should process this message, either addressed to us, or mapping not found
                    // NOTE: reason why we should process it if mapping not found is that
                    //       before the initial notification for my subscriptionID is processed, this node would not know what it is
                    if (target == _net->getHostID () ||
                        it == _id2host.end () || 
                        it->second == _net->getHostID ())
                        local_targets.push_back (target);
                    else
                    {
                        printf ("MessageQueue::processMessages () [%llu] received target [%llu] not for me\n", _net->getHostID (), target);
                        //forward_targets.push_back (target);
                    }                        
                }

                // if the target is for local host or no mapping can be found 
                // the default action is to let the local host to process it
                if (local_targets.size () > 0)
                {
                    recvmsg->targets = local_targets;

                    // check if local handler for the message group exists
                    id_t msggroup = recvmsg->msggroup;
                                        
                    if (_handlers.find (msggroup) == _handlers.end ())
                    {
                        printf ("MessageQueue::processMessages () cannot find proper handler with msggroup: %d for message from [%d]\n", (int)msggroup, (int)recvmsg->from);
                        continue;
                    }
                    if (_handlers[msggroup]->handleMessage (*recvmsg) == true)
                        num_msg++;                          
                }

                /*
                // if there are still targets left, assume they're for forwarding
                if (forward_targets.size () > 0)
                {
                    recvmsg->targets = forward_targets;
                    sendMessage (*recvmsg);
                }
                */
            }
        }

        return num_msg;
    }

} // end namespace Vast

