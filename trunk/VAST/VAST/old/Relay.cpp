

#include "Relay.h"

using namespace Vast;

namespace Vast
{   

    // constructor for physical topology class, may supply an artifical physical coordinate
    Relay::Relay ()
            :MessageHandler (MSG_GROUP_RELAY)
    {        
    }

    Relay::~Relay ()
	{
	}

    void 
    Relay::initHandler ()
    {		
    }

    // returns whether the message was successfully handled
    bool 
    Relay::handleMessage (Message &in_msg)
    {
        switch (in_msg.msgtype)
        {

        case DISCONNECT:
            break;

        default:
            return false;
        }

        return true;
    }

    // performs some tasks the need to be done after all messages are handled
    // such as neighbor discovery checks
    void 
    Relay::postHandling ()
    {

    }

} // end namespace Vast
