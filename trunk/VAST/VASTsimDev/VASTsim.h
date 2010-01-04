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

#define ACE_DISABLED        // we disable using ACE in emulation mode
#define RECORD_INCONSISTENT_NODES_   

#include "VASTTypes.h"
#include "Voronoi.h"


#include <stdlib.h>         // atoi, srand
#include <vector>
#include <time.h>

using namespace Vast;
using namespace std;

typedef struct
{
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
	int		WITH_LATENCY;
	int		ARRIVAL_RATIO;
	int		DEPATURE_MODEL;
	int     MAX_JOIN_NODES;
	int		MAX_LEAVE_NODES;
	int     STABLE_STEPS;
	int     PEER_LIMIT;         // max # of peers hosted at each relay
	int     RELAY_LIMIT;        // max # of relays each node keeps int		STABLE_STEPS;

} SimPara;

typedef enum  
{
	IDLE,
	WAITING,
	NORMAL,
	FAILING,
	FAILED
	
} SimNodeState;

EXPORT bool                 ReadPara (SimPara &para);
EXPORT int                  InitSim (SimPara &para);
EXPORT int                  NextStep ();
EXPORT int                  CreateNode ();
EXPORT Node *               GetNode (int index);
EXPORT vector<Vast::Node *>*GetNeighbors (int index);
EXPORT vector<Vast::id_t> * GetEnclosingNeighbors (int index, int level = 1);
EXPORT int                  ShutSim ();
EXPORT vector<Vast::line2d>*GetEdges(int index);

EXPORT int                  CreateNode (id_t beh_id); // create node with behavior id 
//EXPORT int					GetNumNodes();

#endif // VASTSIM_H