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

#include "VASTATEsim.h"
#include "VASTVerse.h"
#include "Statistics.h"
#include "SimPeer.h"

using namespace std;        // vector
using namespace Vast;       // vast

SimPara             g_para;
statistics         *g_stat;
vector<SimPeer *>  *g_nodes;            // pointer to all simulation nodes
vector<bool>       *g_as_relay;
MovementGenerator  *g_move_model;
SectionedFile      *g_pos_record = NULL;

int                 g_last_seed;       // storing random seed

int                 g_steps     = 0;
bool                g_joining   = true;


// read parameters from input file
bool ReadPara (SimPara &para, char *inifile)
{
    FILE *fp;
    string filename = (inifile == NULL ? "VASTATEsim.ini" : inifile);
 
    if ((fp = fopen (filename.c_str (), "rt")) == NULL)
        return false;

    int *p[] = {
        &para.VAST_MODEL,
        &para.NET_MODEL,
        &para.MOVE_MODEL,
        &para.WORLD_WIDTH,
        &para.WORLD_HEIGHT,
        &para.NODE_SIZE,
        &para.RELAY_SIZE,
        &para.TIME_STEPS,
        &para.STEPS_PERSEC,
        &para.AOI_RADIUS,
        &para.AOI_BUFFER,
        &para.CONNECT_LIMIT,
        &para.VELOCITY,
        &para.LOSS_RATE,
        &para.FAIL_RATE, 
        &para.UPLOAD_LIMIT,
        &para.DOWNLOAD_LIMIT,
        &para.PEER_LIMIT,
        &para.RELAY_LIMIT,
        0
    };

    char buff[255];
    int n = 0;
    while (fgets (buff, 255, fp) != NULL)
    {
        // skip any comments or empty lines
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

int InitSim (SimPara &para)
{
    g_para = para;

    // create / open position log file
    char filename[80];
    sprintf (filename, VAST_POSFILE_FORMAT, para.NODE_SIZE, para.WORLD_WIDTH, para.WORLD_HEIGHT, para.TIME_STEPS);

    FileClassFactory fcf;
    g_pos_record = fcf.CreateFileClass (0);
    bool replay = true;
    if (g_pos_record->open (filename, SFMode_Read) == false)
    {
        replay = false;
        g_pos_record->open (filename, SFMode_Write);
    }

    // create behavior model
    g_move_model = MovementGenerator::createInstance ();
    g_move_model->initModel (g_para.MOVE_MODEL, g_pos_record, replay, 
                            Position (0,0), Position ((coord_t)g_para.WORLD_WIDTH, (coord_t)g_para.WORLD_HEIGHT),
                            g_para.NODE_SIZE, g_para.TIME_STEPS, (double)g_para.VELOCITY);    

    // close position log file
    fcf.DestroyFileClass (g_pos_record);
    g_pos_record = NULL;

    // initialize random number generator
    srand ((unsigned int)time (NULL));
    g_last_seed = rand ();

    // randomly choose which nodes will be the relays
    g_as_relay = new vector<bool>;
    int num_relays = 1;
    int i;
    for (i=0; i < para.NODE_SIZE; i++)
        g_as_relay->push_back (false);

    g_as_relay->at (0) = true;
    i = 1;
    while (num_relays < para.RELAY_SIZE)
    {
        g_as_relay->at (i++) = true;
        num_relays++;
    }

    // starts stat collections
    g_stat = new statistics;
    g_nodes = new vector<SimPeer *>;
    g_stat->init_timer (g_para);
    return 0;
}

int CreateNode (bool wait_till_ready)
{
    // obtain current node index
    size_t i = g_nodes->size ();        

    g_nodes->push_back (new SimPeer (i+1, g_move_model, g_para, g_as_relay->at (i)));
    
    // NOTE: it's important to advance the logical time here, because nodes would 
    //       not be able to receive messages sent during the same time-steps
    
    if (wait_till_ready)
    {
        // make sure all nodes have joined before moving    
        do
        {    
            // each node processes messages received so far
            for (size_t j=0; j <= i; ++j)
                g_nodes->at (j)->processmsg ();   
        }
        while (!g_nodes->at (i)->isJoined ());
    }
    else
    {
        // each node processes messages received so far
        for (size_t j=0; j <= i; ++j)
            g_nodes->at (j)->processmsg ();
        
        g_nodes->at (i)->isJoined ();
    }

    // store node into stat class for later processing
    g_stat->add_node (g_nodes->at (i));
    
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

    int n = g_nodes->size ();

    // each node makes a move or checks for joining
    for (i=0; i < n; ++i)
    {
        if (g_nodes->at (i)->isJoined ())
            g_nodes->at (i)->move ();
    }

    // fail a node if time has come    
	if (g_para.FAIL_RATE > 0 && 
        g_steps >= 500 &&         
        g_steps % g_para.FAIL_RATE == 0)
    {
        FailMethod method = RELAY_ONLY;
        //FailMethod method = CLIENT_ONLY;

        int index = -1;

        // random fail
        switch (method)
        {
        case RANDOM:
            {
                int count = n, i = rand () % n;
                while (count-- > 0)
                {                    
                    if (i != 0 && g_nodes->at (index)->isJoined () == true)
                        break;

                    i = rand () % n;
                }
                index = i;
            }
            break;

        case RELAY_ONLY:
            {
                int i;
                // fail each active relay (except gateway) until all are failed
                for (i=1; i<n; i++)
                {
                    if (g_as_relay->at (i) == true && g_nodes->at (i)->isJoined () == true)
                        break;
                }

                if (i != n)
                    index = i;
            }
            break;

        case CLIENT_ONLY:
            {
                // fail each active non-relay 
                for (index=1; index<n; index++)
                {
                    if (g_as_relay->at (index) == false && g_nodes->at (index)->isJoined () == true)                      
                        break;
                }
            }
            break;
        }

        if (index != -1 && index != n && g_nodes->at (index)->isJoined ())
        {
            printf ("failing [%ld]..\n", g_nodes->at (index)->get_id ());
            g_nodes->at (index)->fail ();
        }
    }
    
    // each node processes messages received so far
    for (i=0; i < n; ++i)
        g_nodes->at (i)->processmsg ();    

    int inconsistent_count = 0;

#ifdef SIM_RECORD_STAT
    // each node calculates stats
    for (i=0; i < n; ++i)
        g_nodes->at (i)->record_stat ();

    //
    // stat collection 
    //
    inconsistent_count = g_stat->record_step ();    
#endif

    //
    // returns (-1) for terminating the simulation, or # of inconsistent nodes
    //
    if (g_steps >= g_para.TIME_STEPS)
        return (-1);
    else
        return inconsistent_count;
}

Node *GetNode (int index)
{
    if (g_nodes->at (index) == NULL)
        return NULL;

    return g_nodes->at (index)->getSelf ();
}

std::vector<Node *>* GetNeighbors (int index)
{   
    return &g_nodes->at (index)->getNeighbors ();
}

std::vector<Vast::id_t> *GetEnclosingNeighbors (int index, int level)
{
    return NULL;
}

std::vector<line2d> *GetEdges(int index)
{
    return g_nodes->at (index)->getArbitratorBoundaries ();
}

bool GetBoundingBox (int index, point2d& min, point2d& max)
{
    return g_nodes->at (index)->getBoundingBox (min, max);
}

int ShutSim ()
{    
    g_stat->print_stat ();

    int n = g_nodes->size ();
    // delete nodes
    for (int i=0; i<n; ++i)     
        delete g_nodes->at (i);        
    
    g_nodes->clear ();

    // de-allocate all globals
    delete g_nodes;
    delete g_stat;       
    delete g_as_relay;

    // deallocate the global movement model instance
    MovementGenerator::destroyInstance ();
    g_move_model = NULL;

    return 0;
}

SimAgent * CreateSimAgent ()
{
    return new SimAgent ();
}

void DestroySimAgent (SimAgent *a)
{
    delete a;
}

SimArbitrator * CreateSimArbitrator ()
{
    return new SimArbitrator ();
}

void DestroySimArbitrator (SimArbitrator *a)
{
    delete a;
}



















