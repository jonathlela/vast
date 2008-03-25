
/*
 *	Gateway (first node in a given Voronoi-based Overlay Network)
 *  
 *  version history: 2006/05/27     start
 *
 */

#include "vastverse.h"
#include "ace/ACE.h"
#include "ace/OS.h"

using namespace std;
using namespace VAST;

#ifdef ACE_DISABLED
#error "ACE needs to be enabled to build demo_gateway, please modify /include/config.h"
#endif


#define INTERVAL  100         // interval for the gateway to process incoming message
                              // (in millisecond)

int main (int argc, char *argv[])
{  
    printf ("VAST Gateway\n");
    if (argc != 3)
    {
        printf ("Usage: %s [self_IP] [AOI-radius]\n", argv[0]);
        exit (0);
    }
    
    int   port = 3737;
    aoi_t aoi  = (aoi_t)atoi (argv[2]);
    IPaddr ip (argv[1], port);
    Addr gatewayAddr;
	
    gatewayAddr.publicIP = ip;

    // creating the first node, with origin as the join location
    vastverse       world (VAST_MODEL_DIRECT, VAST_NET_ACE, 0, 0, 0);
    vast *          gateway = NULL;
    Position        pos;
    
    pos.x = 0;
    pos.y = 0;

    gateway     = world.create_node (gatewayAddr.publicIP.port, (aoi_t)((float)aoi * 0.10));
    vastid *vid = world.create_id (gateway, true, gatewayAddr);
    gateway->join (NET_ID_GATEWAY, aoi, pos, gatewayAddr);
    
    // go into infinite loop to process message
    bool done = false;
    ACE_Time_Value tv (0, INTERVAL*1000);
    while (!done)
    {        
        //printf ("%d\n", ++i);
        gateway->tick ();
        ACE_OS::sleep (tv);
    }    

    world.destroy_id (vid);
    world.destroy_node (gateway);

    return 0;
}
