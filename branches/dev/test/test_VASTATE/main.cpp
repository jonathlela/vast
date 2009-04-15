
///////////////////////////////////////
// main.cpp - Main program for testing vastate under development stage

#include "precompile.h"
//#include "vastate.h"
//#include "shared.h"
#include "vastory.h"

using namespace std;
using namespace VAST;

int main ()
{
    /*
    vastverse vs(VAST_MODEL_DIRECT, VAST_NET_EMULATED, 20);

    Addr gateway;
    gateway.id = NET_ID_GATEWAY;

    // system parameters
    system_parameter_t sp;
    sp.width = 800;
    sp.height = 600;
    sp.aoi = 100;

    // create vastate
    vastory factory;
    vastate *manager = factory.create (&vs, gateway, sp);
    manager->start (true);

    // get id
    for (int i = 0; i < 10; i ++)
        manager->process_message ();

    VAST::gateway *g = manager->create_gateway ();
    g->start (gateway);

    for (int i = 0; i < 10; i ++)
        manager->process_message ();

    vastverse *vss[10];
    vastate *peers[10];
    for (int i = 0; i < 10; i ++)
    {
        vss[i] = NULL;
        peers[i] = NULL;
    }

    for (int i = 0; i < 1; i ++)
    {
        vss[i] = new vastverse (VAST_MODEL_DIRECT, VAST_NET_EMULATED, 20);
        peers[i] = factory.create (vss[i], gateway, sp);

        vastate* p = peers[i];
        p->create_arbitrator (
    }

    // do something
    // overlay creator,  
    // arbitrator_logic, storage_logic, system_parameter
    // peer_logic, peer_info, capacity
    // id, arbitrator_info, gateway

    factory.destroy (manager);
    for (int i = 0; i < 10; i ++)
        if (vss[i] != NULL)
            delete vss[i];

    getchar ();
    return 0;
    */
}