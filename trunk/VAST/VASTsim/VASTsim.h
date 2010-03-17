/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005 Shun-Yun Hu (syhu@yahoo.com)
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
 *  VASTsim.h -- header for simulation DLL     
 *
 *  ver 0.1 (2005/04/12)
 *   
 */

#ifndef VASTSIM_H
#define VASTSIM_H

#ifdef WIN32
// disable warning about "identifier exceeds'255' characters"
#pragma warning(disable: 4786)
#endif

#define ACE_DISABLED_                        // we disable using ACE in emulation mode
#define RECORD_INCONSISTENT_NODES_           // enable this will greatly reduce run speed

#include "VASTTypes.h"
#include "Voronoi.h"
#include "VASTVerse.h"

#include <stdlib.h>         // atoi, srand
#include <vector>

using namespace Vast;
using namespace std;
    
#define DEFAULT_AOI         195     // the AOI-radius size for all nodes
#define DIM_X               768     // size of the world & its default values
#define DIM_Y               768
#define STEPS_PER_SECOND    10      // default steps per second

typedef struct {
    int     VAST_MODEL;
    int     NET_MODEL;
    int     MOVE_MODEL;
    int     WORLD_WIDTH;    
    int     WORLD_HEIGHT; 
    int     NODE_SIZE;   
    int     RELAY_SIZE;
    int     TIME_STEPS;         // total # of timesteps
    int     STEPS_PERSEC;       // # of steps per second
    int     AOI_RADIUS;
    int     AOI_BUFFER;
    int     CONNECT_LIMIT;      
    int     VELOCITY;           // movement speed of node
    int     LOSS_RATE;
    int     FAIL_RATE;
    int     UPLOAD_LIMIT;       // upload limit 
    int     DOWNLOAD_LIMIT;     // download bandwidth limitation
    int     PEER_LIMIT;         // max # of peers hosted at each relay
    int     RELAY_LIMIT;        // max # of relays each node keeps
    int     OVERLOAD_LIMIT;     // limit to consider as overloaded
} SimPara;

typedef enum  
{
    IDLE,
    WAITING,
    NORMAL,
    FAILING,
    FAILED

} SimNodeState;

// initialize parameters, returns which node index to simulate, -1 for error, 0 for manual control
EXPORT int                  InitPara (VAST_NetModel model, VASTPara_Net &netpara, SimPara &simpara, const char *cmdline = NULL, bool *is_gateway = NULL, Area *aoi = NULL, Addr *gateway = NULL, vector<IPaddr> *entries = NULL);
EXPORT bool                 ReadPara (SimPara &para);
EXPORT int                  InitSim (SimPara &para, VASTPara_Net &netpara);
EXPORT int                  CreateNode (bool wait_till_ready = true);
EXPORT int                  NextStep ();
EXPORT Node *               GetNode (int index);
EXPORT vector<Vast::Node *>*GetNeighbors (int index);
EXPORT vector<Vast::id_t> * GetEnclosingNeighbors (int index, int level = 1);
EXPORT int                  ShutSim ();
EXPORT vector<Vast::line2d>*GetEdges (int index);
EXPORT bool                 GetBoundingBox (int index, point2d& min, point2d& max);


#endif // VASTSIM_H

