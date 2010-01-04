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
 *  VASTATEsim.h -- header for simulation DLL     
 *
 *  ver 0.1 (2005/04/12)
 *   
 */

#ifndef VASTATESIM_H
#define VASTATESIM_H

#ifdef WIN32
// disable warning about "identifier exceeds'255' characters"
#pragma warning(disable: 4786)
#endif

#define ACE_DISABLED_        // we disable using ACE in emulation mode
#define RECORD_INCONSISTENT_NODES

#include "VASTATETypes.h"
#include "Voronoi.h"

#include "SimAgent.h"
#include "SimArbitrator.h"

#include <stdlib.h>         // atoi, srand
#include <vector>

using namespace Vast;
using namespace std;

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
    int     VELOCITY;       
    int     LOSS_RATE;
    int     FAIL_RATE;
    int     UPLOAD_LIMIT;
    int     DOWNLOAD_LIMIT;
    int     PEER_LIMIT;
    int     RELAY_LIMIT;
} SimPara;

typedef enum  
{
    IDLE,
    INIT,
    WAITING,
    NORMAL,
    FAILING,
    FAILED

} SimPeerState;

// actions a SimPeer may perform
typedef enum 
{
    UNKNOWN = 0,
    MOVE,
    TALK,
    CREATE_OBJ,
    CREATE_FOOD,
    ATTACK,
    EAT
} SimPeerAction;

// objecttypes
typedef enum 
{
    AVATAR = 0,
    FOOD,
    WEAPON,
    CLOTHES
} SimObjectType;


EXPORT bool                 ReadPara (SimPara &para, char *inifile = NULL);
EXPORT int                  InitSim (SimPara &para);
EXPORT int                  CreateNode (bool wait_till_ready = false);
EXPORT int                  NextStep ();
EXPORT Node *               GetNode (int index);
EXPORT vector<Vast::Node *>*GetNeighbors (int index);
EXPORT vector<Vast::id_t> * GetEnclosingNeighbors (int index, int level = 1);
EXPORT int                  ShutSim ();
EXPORT vector<Vast::line2d>*GetEdges (int index);
EXPORT bool                 GetBoundingBox (int index, point2d& min, point2d& max);

// NOTE: simagent and simarbitrator must be created and deleted inside the DLL
EXPORT SimAgent *           CreateSimAgent ();
EXPORT void                 DestroySimAgent (SimAgent *a);
EXPORT SimArbitrator *      CreateSimArbitrator ();
EXPORT void                 DestroySimArbitrator (SimArbitrator *a);

#endif // VASTATESIM_H

