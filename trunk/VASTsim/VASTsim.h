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
#define RECORD_INCONSISTENT_NODES   0

#include "VASTTypes.h"
#include "voronoi.h"


#include <stdlib.h>         // atoi, srand
#include <vector>

using namespace Vast;
using namespace std;

//namespace Vast {

typedef struct {
    int     VAST_MODEL;
    int     NET_MODEL;
    int     MOVE_MODEL;
    int     UPLOAD_LIMIT;
    int     DOWNLOAD_LIMIT;
    int     WORLD_WIDTH;    
    int     WORLD_HEIGHT;   
    int     NODE_SIZE;      
    int     TIME_STEPS;     
    int     AOI_RADIUS;
    int     AOI_BUFFER;
    int     CONNECT_LIMIT;  
    int     VELOCITY;       
    int     LOSS_RATE;
    int     FAIL_RATE;
} VASTsimPara;

typedef enum  
{
    NORMAL,
    FAILING,
    FAILED
} NodeState;


EXPORT bool                 ReadPara (VASTsimPara &para);
EXPORT int                  InitVASTsim (VASTsimPara &para);
EXPORT int                  CreateNode ();
EXPORT int                  NextStep ();
EXPORT Node *               GetNode (int index);
EXPORT vector<Vast::Node *>&GetNeighbors (int index);
EXPORT vector<Vast::id_t> & GetEnclosingNeighbors (int index, int level = 1);
EXPORT int                  ShutVASTsim ();
EXPORT vector<Vast::line2d>&GetEdges(int index);

//} // namespace Vast

#endif // VASTSIM_H

