/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2010 Shun-Yun Hu (syhu@yahoo.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*
 *  VASTWrapperC -- header for a C-interface wrapper of the VAST C++ class
 *
 *  History:
 *      2010/06/17      first version
 *   
 */


#include "ace/ACE.h"
#include "ace/OS.h"

//#include "Config.h"             // define EXPORT
#include "VASTWrapperC.h"


#ifdef ACE_DISABLED
#error "ACE needs to be enabled to build demo_console, please modify /common/Config.h"
#endif

// VAST-related includes
#include "VASTVerse.h"
#include "VASTUtil.h"

using namespace Vast;

// global variables 
Area        g_aoi;              // my AOI (with center as current position)
Area        g_prev_aoi;         // previous AOI (to detect if AOI has changed)
Addr        g_gateway;          // address for gateway
vector<IPaddr> g_entries;       // IP of entry points
NodeState   g_state = ABSENT;   // the join state of this node

VASTPara_Net g_netpara;         // network parameters

// VAST-specific variables
VASTVerse * g_world = NULL;
VAST *      g_self  = NULL;
Vast::id_t  g_sub_no = 0;        // subscription # for my client (peer)  
layer_t     g_layer;

bool        g_init = false;     // whether VASTInit is called

#define VAST_EVENT_LAYER    1                   // layer ID for sending events 
#define VAST_UPDATE_LAYER   2                   // layer ID for sending updates

//
// basic init of VAST
//

void checkVASTJoin ()
{    
    // create the VAST node
    switch (g_state)
    {
    case ABSENT:
        if ((g_self = g_world->getVASTNode ()) != NULL)
        {                
            g_sub_no = g_self->getSubscriptionID (); 
            g_state = JOINED;
        }
        break;

    default:
        break;
    }
}


// initialize the VAST library
int InitVAST (bool is_gateway, uint16_t port)
{
    ACE::init ();

    // store default gateway address
    string str ("127.0.0.1:1037");
    g_gateway = *VASTVerse::translateAddress (str);
    
    //g_gateway.fromString (str);
    //g_gateway.host_id = ((Vast::id_t)g_gateway.publicIP.host << 32) | ((Vast::id_t)g_gateway.publicIP.port << 16) | NET_ID_RELAY;

    // NOTE: the very first node does not know other existing relays
    if (is_gateway == false)
    {
        g_entries.push_back (g_gateway.publicIP);
    }

    // set default network parameters
    g_netpara.relay_limit    = 0;
    g_netpara.client_limit   = 0;
    g_netpara.overload_limit = 0;
    g_netpara.conn_limit     = 0;        

    g_netpara.port           = port;
    g_netpara.step_persec    = 10;
    g_netpara.recv_quota     = 0;
    g_netpara.recv_quota     = 0;

    //g_netpara.is_entry       = true;
    g_netpara.is_relay       = true;   
    g_netpara.is_matcher     = true;
    g_netpara.is_static      = false;
    g_netpara.model          = VAST_NET_ACE;

    // read configuration from INI file

    g_init = true;    

    return 0;
}

// close down the VAST library
int ShutVAST ()
{
    if (g_self)
    {
        g_world->destroyVASTNode (g_self);
        g_self = NULL;
    }
            
    if (g_world)
    {
        delete g_world;     
        g_world = NULL;
    }

    g_init = false;

    return 0;
}


//
// unique layer
//

// obtain a unique & unused layer (preferred layer # as input)
void VASTReserveLayer (uint32_t layer)
{
}

// get the currently reserved layer, 0 for not yet reserved
uint32_t VASTGetLayer ()
{
    return 0;
}

// release back the layer
bool VASTReleaseLayer ()
{
    return true;
}

//
// main join / move / publish functions
//

// join at location on a partcular layer
bool VASTJoin (float x, float y, uint16_t radius)
{
    // store AOI
    g_aoi.center.x = x;
    g_aoi.center.y = y;
    g_aoi.radius   = radius;

    // make backup of AOI
    g_prev_aoi = g_aoi;

    g_layer = VAST_EVENT_LAYER;
   
    // create VAST node factory    
    g_world = new VASTVerse (g_entries, &g_netpara, NULL);
    g_world->createVASTNode (g_gateway.publicIP, g_aoi, g_layer);

    return true;
}

// leave the overlay
bool VASTLeave ()
{
    if (g_self->isJoined ())
    {
        g_self->leave ();
        return true;
    }
    else
        return false;
}

// move to a new position
bool VASTMove (float x, float y)
{
    // store new AOI
    g_aoi.center.x = x;
    g_aoi.center.y = y;

    // move only if position changes
    if (!(g_prev_aoi == g_aoi))
    {
        g_prev_aoi = g_aoi;               
        g_self->move (g_sub_no, g_aoi);
    }

    return true;
}

// do routine processing & logical clock progression
size_t VASTTick (size_t time_budget)
{
    // record last time performing per-second task
    static ACE_Time_Value last_persec = ACE_OS::gettimeofday ();
    static size_t tick_count = 0;
        
    tick_count++;

    // check if we've joined
    if (g_state != JOINED)
        checkVASTJoin ();

    ACE_Time_Value now = ACE_OS::gettimeofday();

    // elapsed time in millisecond
    timestamp_t elapsed = (timestamp_t)(now.sec () - last_persec.sec ()) * 1000 + (now.usec () - last_persec.usec ()) / 1000;
          
    // execute tick while obtaining time left
    size_t sleep_time = g_world->tick (time_budget) * 1000;
    
    // do per-second things / checks
    // NOTE: we assume this takes little time and does not currently count in as time spent in cycle       
    if (elapsed > 1000)
    {
        time_t curr_sec = last_persec.sec ();
        printf ("%ld s, tick %lu, tick_persec %lu, sleep: %lu ms\n", curr_sec, tick_count, g_netpara.step_persec, sleep_time);
        //count_per_sec = 0;		
        
        // just do some per second stat collection stuff
        g_world->tickLogicalClock ();

        // store new last sec
        last_persec = now;
    } 

    // return remaining time for this tick
    return sleep_time; 

    /*
    if (sleep_time > 0)
    {
        // NOTE the 2nd parameter is specified in microseconds (us) not milliseconds
        ACE_Time_Value duration (0, sleep_time);            
        ACE_OS::sleep (duration); 
    } 
    */

}

// publish a message to current layer at current location, with optional radius
bool VASTPublish (const char *msg, size_t size, uint16_t radius)
{
    if (isVASTJoined () == false)
        return false;

    Area area = g_aoi;
    area.radius = radius;

    Message vastmsg (123);
    vastmsg.store (msg, size, true);
        
    return g_self->publish (area, g_layer, vastmsg);
}

// receive any message received
VAST_C_Msg * VASTReceive ()
{
    if (isVASTJoined () == false)
        return false;

    static char recv_buf[VAST_BUFSIZ];
    static VAST_C_Msg recvmsg;

    // check and store any incoming string messages    
    size_t size = 0;
    
    Message *msg = NULL;
    if ((msg = g_self->receive ()) != NULL)
    {
        size = msg->extract (recv_buf, 0);
        recv_buf[size]=0;
    }
    else
        size = 0;

    if (size > 0)
    {
        recvmsg.from = msg->from;
        recvmsg.msg = recv_buf;
        recvmsg.size = size;

        return &recvmsg;
    }
    else
        return NULL;
}

//
// helpers
//

// is initialized done (ready to join)
bool isVASTInit ()
{
    return g_init;
}

// whether the join is successful
bool isVASTJoined ()
{
    return (g_state == JOINED);
}


// obtain an ID of self
uint64_t GetSelfID ()
{
    if (isVASTJoined () == true)
        return g_self->getSelf ()->id;
    else
        return 0;        
}
