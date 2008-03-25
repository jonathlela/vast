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
 *	simulation library for VAST      ver 0.1         2005/04/11 
 *                                   ver 0.2         2006/04/04
 */

#include "vastsim.h"
#include "vastverse.h"
#include "statistics.h"
#include "simnode.h"


using namespace std;        // vector
using namespace VAST;       // vast


// hard limit on simulation steps (in case of overlay partition)
//#define STEPS_STABLIZATION  (10)    // # of steps in 100% consistency before next fail scenario
//#define PARTITION_THRESHOLD (50)    // # of steps in inconsistency to consider as overlay partition
#define STABLIZATION_THRESHOLD (50)     // # of steps to wait for 1st 100% Topology Consistency

VASTsimPara         g_para;
statistics          g_stat;
vector<SimNode *>   g_nodes;           // pointer to all simulation nodes
vastverse          *g_world;
MovementGenerator   g_move_model;
SectionedFile      *g_pos_record = NULL;

int                 g_last_seed;       // storing random seed

int                 g_steps     = 0;
bool                g_joining   = true;



// read parameters from input file
bool ReadPara (VASTsimPara &para)
{
    FILE *fp;
    if ((fp = fopen ("./VASTsim.ini", "rt")) == NULL)
        return false;

    int *p[] = {
        &para.VAST_MODEL,
        &para.NET_MODEL,
        &para.MOVE_MODEL,
        &para.WORLD_WIDTH,
        &para.WORLD_HEIGHT,
        &para.NODE_SIZE,
        &para.TIME_STEPS,
        &para.AOI_RADIUS,
        &para.AOI_BUFFER,
        &para.CONNECT_LIMIT,
        &para.VELOCITY,
        &para.LOSS_RATE,
        &para.FAIL_RATE, 
        &para.UPLOAD_LIMIT,
        &para.DOWNLOAD_LIMIT,
        0
    };

    char buff[255];
    int n = 0;
    while (fgets (buff, 255, fp) != NULL)
    {
        if (buff[0] == '#' || buff[0] == ' ' || buff[0] == '\n')
            continue;

        // read the next valid parameter
        if (sscanf (buff, "%d ", p[n]) != 1)
            return false;
        n++;

        if (p[n] == 0)
            return true;
    }

    return false;
}

int InitVASTsim (VASTsimPara &para)
{
    g_para = para;

    // see if we want to override previous steps
    //bool create_new  = false;
    //bool record_step = true;

    // bogus gateway address
    Addr gateway;

    // create vast factory
    g_world = new vastverse (g_para.VAST_MODEL, g_para.NET_MODEL, g_para.CONNECT_LIMIT, g_para.LOSS_RATE, g_para.FAIL_RATE, g_para.UPLOAD_LIMIT, g_para.DOWNLOAD_LIMIT);

    // create / open position log file
    char filename[80];
    sprintf (filename, "N%04dW%dx%d.pos", para.NODE_SIZE, para.WORLD_WIDTH, para.WORLD_HEIGHT);

    FileClassFactory fcf;
    g_pos_record = fcf.CreateFileClass (0);
    bool replay = true;
    if (g_pos_record->open (filename, SFMode_Read) == false)
    {
        replay = false;
        g_pos_record->open (filename, SFMode_Write);
    }

    // create behavior model
    g_move_model.initModel (g_para.MOVE_MODEL, g_pos_record, replay, 
                            Coord (0,0), Coord (g_para.WORLD_WIDTH, g_para.WORLD_HEIGHT),
                            g_para.NODE_SIZE, g_para.TIME_STEPS, (double)g_para.VELOCITY);    

    // close position log file
    fcf.DestroyFileClass (g_pos_record);

    // initialize random number generator
    srand ((unsigned int)time (NULL));
    g_last_seed = rand ();

    g_stat.init_timer (g_para);
    return 0;
}

int CreateNode ()
{
    // obtain current node index
    int i = g_nodes.size ();        

    g_nodes.push_back (new SimNode (i+1, &g_move_model, g_world, g_para));

    // NOTE: it's important to advance the logical time here, because nodes would 
    //       not be able to receive messages sent during the same time-steps
    
    // make sure all nodes have joined before moving    
    while (1)
    {
        g_world->tick ();

        // each node processes messages received so far
        for (int j=0; j<=i; ++j)
            g_nodes[j]->processmsg ();

        // make sure the new node has joined before moving on
        if (g_nodes[i]->vnode->is_joined () == true)
            break;        
    }

    // store node into stat class for later processing
    g_stat.add_node (g_nodes[i]);
    
    return 0;
}

// return # of inconsistent nodes during this step if stat_collect is enabled
// returns (-1) if simulation has ended  
//
// NOTE: during failure simulation, there's a JOIN-stablize-FAIL-stablize cycle
//       stat is collected only during the FAIL-stablize period 
//
int NextStep ()
{   
    g_steps++;
    
    int i;   

    // each node makes a move
    for (i=0; i<g_para.NODE_SIZE; ++i)
        g_nodes[i]->move ();

    g_world->tick ();

    // each node processes messages received so far
    for (i=0; i<g_para.NODE_SIZE; ++i)
        g_nodes[i]->processmsg ();    

    // each node calculates stats
    for (i=0; i<g_para.NODE_SIZE; ++i)
        g_nodes[i]->record_stat ();

    //
    // stat collection 
    //
    int inconsistent_count = g_stat.record_step ();

    //
    // returns (-1) for terminating the simulation, or # of inconsistent nodes
    //
    if (g_para.FAIL_RATE == 0 && g_steps == g_para.TIME_STEPS)
        return (-1);
    else
        return inconsistent_count;
}

Node *GetNode (int index)
{
    return g_nodes[index]->vnode->getself ();
}

std::vector<Node *>& GetNeighbors (int index)
{   
    return g_nodes[index]->vnode->getnodes ();
}

std::vector<VAST::id_t> & GetEnclosingNeighbors (int index, int level)
{
    return g_nodes[index]->vnode->getvoronoi ()->get_en (g_nodes[index]->get_id (), level);
}

std::vector<line2d> &GetEdges(int index)
{
    return g_nodes[index]->vnode->getvoronoi ()->getedges ();
}

int ShutVASTsim ()
{    
    // TODO: not a clean way to pass stat
    //g_stat.recovery_times = g_recovery_times;
    //g_stat.overlay_partition = g_overlay_partition;

    g_stat.print_stat ();

    // delete nodes
    for (int i=0; i<g_para.NODE_SIZE; ++i)        
        delete g_nodes[i];        

    //printf ("nodes to fail: %d actual # of failed nodes: %d\n", g_failed_size, g_failed_count);
    //printf ("recovery times: %d\n", g_recovery_times);

    return 0;
}





















