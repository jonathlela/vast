
///////////////////////////////////////
// main.cpp - Main program for testing vastate under development stage

#include "precompile.h"
//#include "vastate.h"
//#include "shared.h"
#include "vastory.h"

using namespace std;
using namespace VAST;
using namespace VASTATE;

int main ()
{
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

    // do something
    // overlay creator,  
    // arbitrator_logic, storage_logic, system_parameter
    // peer_logic, peer_info, capacity
    // id, arbitrator_info, gateway


    factory.destroy (manager);

    getchar ();
    return 0;
}