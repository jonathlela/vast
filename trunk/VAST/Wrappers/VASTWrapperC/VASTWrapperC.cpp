/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2011 Shun-Yun Hu (syhu@ieee.org)
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
//#include "ace/OS.h"

//#include "Config.h"             // define VASTC_EXPORT
#include "VASTwrapperC.h"


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
//vector<IPaddr> g_entries;       // IP of entry points
NodeState   g_state = ABSENT;   // the join state of this node

VASTPara_Net g_netpara (VAST_NET_ACE);  // network parameters

// VAST-specific variables
VASTVerse * g_world = NULL;
VAST *      g_self  = NULL;
Vast::id_t  g_sub_no = 0;       // subscription # for my client (peer)  
layer_t     g_layer;

bool        g_init = false;     // whether VASTInit is called

#define VAST_EVENT_LAYER    1                   // layer ID for sending events 
#define VAST_UPDATE_LAYER   2                   // layer ID for sending updates

//
// basic init of VAST
//

VASTC_EXPORT void checkVASTJoin ()
{    
    if (g_world == NULL)
        return;

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
VASTC_EXPORT bool VAST_CALL InitVAST (bool is_gateway, const char *gateway)
{
    if (g_init == true)
        return true;
    
    // store default gateway address
    string GWstr (gateway);

    printf ("InitVAST called, gateway: %s, is_gateway: %s\n", gateway, (is_gateway ? "true" : "false"));

    g_gateway = *VASTVerse::translateAddress (GWstr);
    
    // NOTE: the very first node does not know other existing relays
    /*
    g_entries.clear ();
    if (is_gateway == false)
    {
        g_entries.push_back (g_gateway.publicIP);
    }
    */

    // set default network parameters
    g_netpara.relay_limit    = 0;
    g_netpara.client_limit   = 0;
    g_netpara.overload_limit = 0;
    g_netpara.conn_limit     = 0;        

    g_netpara.port           = g_gateway.publicIP.port;
    g_netpara.recv_quota     = 0;
    g_netpara.recv_quota     = 0;

    //g_netpara.is_entry       = true;
    g_netpara.is_relay       = true;   
    g_netpara.is_matcher     = true;
    g_netpara.is_static      = false;

    // TODO: read configuration from INI file


    // create VAST node factory    
    g_world = new VASTVerse (is_gateway, GWstr, &g_netpara, NULL);

    return true;
}

// close down the VAST library
VASTC_EXPORT bool VAST_CALL ShutVAST ()
{
    // make sure VAST node is left already
    VASTLeave ();
            
    if (g_world)
    {
        delete g_world;     
        g_world = NULL;
    }

    g_init = false;
    //g_entries.clear ();

    return true;
}


//
// unique layer
//

// obtain a unique & unused layer (preferred layer # as input)
VASTC_EXPORT void VAST_CALL VASTReserveLayer (uint32_t layer)
{
}

// get the currently reserved layer, 0 for not yet reserved
VASTC_EXPORT uint32_t VAST_CALL VASTGetLayer ()
{
    return 0;
}

// release back the layer
VASTC_EXPORT bool VAST_CALL VASTReleaseLayer ()
{
    return true;
}

//
// main join / move / publish functions
//

// join at location on a partcular layer, create a VAST node
VASTC_EXPORT bool VAST_CALL VASTJoin (world_t world_id, float x, float y, uint16_t radius)
{
    if (g_self != NULL)
        return false;

    // store AOI
    g_aoi.center.x = x;
    g_aoi.center.y = y;
    g_aoi.radius   = radius;

    // make backup of AOI
    g_prev_aoi = g_aoi;

    g_layer = VAST_EVENT_LAYER;
   
    g_world->createVASTNode (world_id, g_aoi, g_layer);

    return true;
}

// leave the overlay, destroy VAST node
VASTC_EXPORT bool VAST_CALL VASTLeave ()
{
    if (g_self == NULL)
        return false;

    // leave overlay if still exists
    if (g_self->isJoined ())
    {
        g_self->leave ();
        g_world->tick (0);
    }

    g_world->destroyVASTNode (g_self);
    g_self = NULL;

    g_state = ABSENT;

    return true;    
}

// move to a new position
VASTC_EXPORT bool VAST_CALL VASTMove (float x, float y)
{
    if (isVASTJoined () == false)
        return false;

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
VASTC_EXPORT int VAST_CALL VASTTick (int time_budget)
{
    // tick can only happen if VAST factory exists
    if (g_world == NULL)
        return 0;

    // check if VAST is ready to create VASTNodes (gateway connected)
    if (g_world->isInitialized ())
        g_init = true;

    // record last time performing per-second task
    static size_t tick_count = 0;
 
    tick_count++;

    // check if we've joined
    if (g_state != JOINED)
        checkVASTJoin ();
          
    // execute tick while obtaining time left
    bool per_sec;
    int sleep_time = g_world->tick (time_budget, &per_sec);
    
    // do per-second things / checks
    // NOTE: we assume this takes little time and does not currently count in as time spent in cycle       
    if (per_sec)
    {
        //printf ("tick_count: %u, sleep: %lu ms\n", tick_count, sleep_time);
        
        // just do some per second stat collection stuff
        g_world->tickLogicalClock ();
    } 

    // return remaining time for this tick
    return sleep_time; 
}

// publish a message to current layer at current location, with optional radius
VASTC_EXPORT bool VAST_CALL VASTPublish (const char *msg, size_t size, uint16_t radius)
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
VASTC_EXPORT const char * VAST_CALL VASTReceive (uint64_t *ret_from, size_t *ret_size)
{
    if (isVASTJoined () == false)
        return NULL;

    static VAST_C_Msg recvmsg;
    static VASTBuffer recv_buf;

    recv_buf.clear ();
       
    Message *msg = NULL;
    if ((msg = g_self->receive ()) != NULL)
    {
        //reserve buffer, also reserve a null at end
        recv_buf.reserve (msg->size + 1);        
        recv_buf.size = msg->extract (recv_buf.data, 0);

        // put 0 at end
        recv_buf.data[recv_buf.size] = 0;
    }
    else
        recv_buf.size = 0;

    if (recv_buf.size > 0)
    {
        recvmsg.from = msg->from;
        recvmsg.msg  = recv_buf.data;
        recvmsg.size = recv_buf.size;

        *ret_from = recvmsg.from;
        *ret_size = recvmsg.size;        

        //printf ("VASTReceive () returns string of size: %d from %llu\n", size, msg->from);

        return recvmsg.msg;        
    }
    else
    {
        *ret_size = 0;
        *ret_from = 0;

        return NULL;
    }
}

//
// socket messaging
//


// open a new TCP socket
VASTC_EXPORT uint64_t VAST_CALL VASTOpenSocket (const char *ip_port)
{
    // convert IP_port
    printf ("opening socket: %s\n", ip_port);
    
    string host (ip_port);    
    Addr addr = *VASTVerse::translateAddress (host);
    
    if (g_world)
        return g_world->openSocket (addr.publicIP);
    else
        return NET_ID_UNASSIGNED;
}

// close a TCP socket
VASTC_EXPORT bool VAST_CALL VASTCloseSocket (uint64_t socket)
{
    if (g_world)
        return g_world->closeSocket (socket);
    else
        return false;
}

// send a message to a socket
VASTC_EXPORT bool VAST_CALL VASTSendSocket (uint64_t socket, const char *msg, size_t size)
{
    if (g_world)
        return g_world->sendSocket (socket, msg, size);
    else
        return false;
}

// receive a message from socket, if any
// returns the message in byte array, and the socket_id, message size, NULL for no messages
// NOTE: the returned data is valid until the next call to receiveSocket
VASTC_EXPORT const char * VAST_CALL VASTReceiveSocket (uint64_t *ret_from, size_t *ret_size)
{
    if (g_world == NULL)
        return NULL;

    static VAST_C_Msg recvmsg;
    static VASTBuffer recv_buf;

    recv_buf.clear ();
       
    id_t    recv_socket;
    size_t  recv_size;

    char *msg = NULL;
    if ((msg = g_world->receiveSocket (recv_socket, recv_size)) != NULL)
    {
        //reserve buffer, also reserve a null at end
        recv_buf.reserve (recv_size + 1);
        memcpy (recv_buf.data, msg, recv_size);
        recv_buf.size = recv_size;
                
        // put 0 at end
        recv_buf.data[recv_buf.size] = 0;
    }
    else
        recv_buf.size = 0;

    if (recv_buf.size > 0)
    {
        recvmsg.from = recv_socket;
        recvmsg.msg  = recv_buf.data;
        recvmsg.size = recv_buf.size;

        *ret_from = recvmsg.from;
        *ret_size = recvmsg.size;        

        //printf ("VASTReceive () returns string of size: %d from %llu\n", size, msg->from);

        return recvmsg.msg;        
    }
    else
    {
        *ret_size = 0;
        *ret_from = 0;

        return NULL;
    }
}


//
// helpers
//

// is initialized done (ready to join)
VASTC_EXPORT bool VAST_CALL isVASTInit ()
{
    return g_init;
}

// whether the join is successful
VASTC_EXPORT bool VAST_CALL isVASTJoined ()
{
    return (g_state == JOINED);
}


// obtain an ID of self
VASTC_EXPORT uint64_t VAST_CALL VASTGetSelfID ()
{
    if (isVASTJoined () == true)
        return g_self->getSelf ()->id;
    else
        return 0;        
}
