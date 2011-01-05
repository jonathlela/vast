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
 *	simulation library for VAST      ver 0.1         2005/04/11 
 *                                   ver 0.2         2006/04/04
 */

#include "VASTsim.h"
#include "VASTVerse.h"
#include "SimNode.h"
#include <stdlib.h>         // strtok

#define RECORD_LATENCY      // to record transmission latency for MOVEMENT messages
#include "Statistics.h"

using namespace std;        // vector
using namespace Vast;       // vast

SimPara             g_para;
VASTPara_Net        g_vastnetpara (VAST_NET_EMULATED);
statistics          g_stat;
vector<SimNode *>   g_nodes;            // pointer to all simulation nodes
vector<bool>        g_as_relay;
vector<bool>        g_as_matcher;
MovementGenerator   g_move_model;
char                g_GWstr[80];          // address to gateway node
SectionedFile      *g_pos_record = NULL;

//map<int, VAST *>    g_peermap;          // map from node index to the peer's relay id
//map<int, Vast::id_t> g_peerid;           // map from node index to peer id

int                 g_last_seed;       // storing random seed

int                 g_steps     = 0;
bool                g_joining   = true;


// Initilize parameters, including setting default values or read from INI file
int InitPara (VAST_NetModel model, VASTPara_Net &netpara, SimPara &simpara, const char *cmdline, bool *p_is_gateway, world_t *p_world_id, Area *p_aoi, char *p_GWstr, int *p_interval)
{
    netpara.model = model;

    // parameters to be filled
    bool is_gateway;
    world_t world_id = VAST_DEFAULT_WORLD_ID;
    Area aoi;


    // set default values
    aoi.center.x = (coord_t)(rand () % DIM_X);
    aoi.center.y = (coord_t)(rand () % DIM_Y);
    aoi.radius   = (length_t)DEFAULT_AOI;

    netpara.port  = GATEWAY_DEFAULT_PORT;

    netpara.relay_limit     = 0;
    netpara.client_limit    = 0;
    netpara.overload_limit  = 0;

    // by default the node can be relay & matcher
    netpara.is_relay = true;
    netpara.is_matcher = true;

    // dynamic load balancing is enabled by default
    netpara.is_static = false;

    simpara.NODE_SIZE = 10;
    simpara.STEPS_PERSEC = STEPS_PER_SECOND;

    // which node to simulate, (0) means manual
    int node_no = 0; 

    // interval in seconds to pause before joining
    int interval = 0;
   
    char GWstr[80];
    GWstr[0] = 0;

    // process command line parameters, if available
    char *p;
    int para_count = 0;
    
    if (cmdline != NULL)
    {
        char command[255];
        strcpy (command, cmdline);
    
        p = strtok (command, " ");
    
        while (p != NULL)
        {
            switch (para_count)
            {
            // port
            case 0:
                netpara.port = (unsigned short)atoi (p);
                break;
    
            // Gateway IP
            case 1:
                // (zero indicates gateway)
                if (p[0] != '0')
                    sprintf (GWstr, "%s:%d", p, netpara.port);
                break;
    
            // 3rd parameter: node to simulate   
            case 2:                             
                node_no = atoi (p);
                break;

            // world id
            case 3:
                world_id = (world_t)atoi (p);
                break;

            // interval for pausing in joining
            case 4:
                interval = atoi (p);
                break; 

            // X-coord
            case 5:
                aoi.center.x = (coord_t)atoi (p);
                break;
    
            // Y-coord
            case 6:
                aoi.center.y = (coord_t)atoi (p);
                break;

            // AOI-radius
            case 7:
                aoi.radius = (length_t)atoi (p);
                break;

            // is relay
            case 8:
                netpara.is_relay = (atoi (p) == 1);
                break;

            // is matcher
            case 9:
                netpara.is_matcher = (atoi (p) == 1);
                break;
            }
    
            p = strtok (NULL, " ");
            para_count++;
        }
    }
	    
    // see if simulation behavior file exists for simulated behavior            
    if (ReadPara (simpara) == true)
    {
        // override defaults         
        netpara.overload_limit = simpara.OVERLOAD_LIMIT;

        // command line radius takes precedence than INI radius
        if (aoi.radius == (length_t)DEFAULT_AOI)
            aoi.radius = (length_t)simpara.AOI_RADIUS;        
    }
    else  
    {
        // INI file not found, cannot perform simulation
        if (node_no != 0)
        {
            printf ("warning: INI file is not found at working directory, it's required for simulation\n");
            return (-1);
        }
    }
    
    is_gateway = false;

    // default gateway set to localhost
    if (GWstr[0] == 0)
    {
        is_gateway = true;
        netpara.is_entry = true;
        sprintf (GWstr, "127.0.0.1:%d", netpara.port);
    }
        
    // if physical coordinate is not supplied, VAST will need to obtain it itself
    //g_vastnetpara.phys_coord = g_aoi.center;    

    // translate gateway string to Addr object
    strcpy (g_GWstr, GWstr);

    // create VAST node factory (with default physical coordinate)          

    // return values, if needed
    if (p_is_gateway != NULL)
        *p_is_gateway = is_gateway;

    if (p_aoi != NULL)
        *p_aoi = aoi;

    if (p_world_id != NULL)
        *p_world_id = world_id;

    if (p_GWstr != NULL)
        strcpy (p_GWstr, g_GWstr);

    if (p_interval != NULL)
        *p_interval = interval;

    return node_no;
}

// read parameters from input file
bool ReadPara (SimPara &para)
{
    FILE *fp;
    if ((fp = fopen ("VASTsim.ini", "rt")) == NULL)
        return false;

    int *p[] = {
        &para.VAST_MODEL,
        &para.NET_MODEL,
        &para.MOVE_MODEL,
        &para.WORLD_WIDTH,
        &para.WORLD_HEIGHT,
        &para.NODE_SIZE,
        &para.RELAY_SIZE,
        &para.MATCHER_SIZE,
        &para.TIME_STEPS,
        &para.STEPS_PERSEC,
        &para.AOI_RADIUS,
        &para.AOI_BUFFER,
        &para.CONNECT_LIMIT,
        &para.VELOCITY,
        &para.STABLE_SIZE,
        &para.JOIN_RATE,
        &para.LOSS_RATE,
        &para.FAIL_RATE, 
        &para.UPLOAD_LIMIT,
        &para.DOWNLOAD_LIMIT,
        &para.PEER_LIMIT,
        &para.RELAY_LIMIT,
        &para.OVERLOAD_LIMIT,
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


int InitSim (SimPara &para, VASTPara_Net &netpara)
{
    g_para = para;
    g_vastnetpara = netpara;

    // note there's no need to assign the gateway ID as it'll be found automatically
    g_vastnetpara.model        = (VAST_NetModel)para.NET_MODEL;
    g_vastnetpara.port         = GATEWAY_DEFAULT_PORT;    
    g_vastnetpara.client_limit = para.PEER_LIMIT;
    g_vastnetpara.relay_limit  = para.RELAY_LIMIT;        
    g_vastnetpara.conn_limit   = para.CONNECT_LIMIT;
    g_vastnetpara.recv_quota   = para.DOWNLOAD_LIMIT;
    g_vastnetpara.send_quota   = para.UPLOAD_LIMIT;

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
    g_move_model.initModel (g_para.MOVE_MODEL, g_pos_record, replay, 
                            Position (0,0), Position ((coord_t)g_para.WORLD_WIDTH, (coord_t)g_para.WORLD_HEIGHT),
                            g_para.NODE_SIZE, g_para.TIME_STEPS, (double)g_para.VELOCITY);    

    // close position log file
    fcf.DestroyFileClass (g_pos_record);

    // initialize random number generator
    //srand ((unsigned int)time (NULL));
    srand (37);
    g_last_seed = rand ();

    // randomly choose which nodes will be the relays
    int num_relays = 1;
    int i;
    for (i=0; i < para.NODE_SIZE; i++)
    {
        g_as_relay.push_back (false);
        g_as_matcher.push_back (false);
    }

    // determine which nodes will be relays & matchers
    g_as_relay[0] = true;
    g_as_matcher[0] = true;

    i = 1;
    while (num_relays < para.RELAY_SIZE)
    {
        //if (rand () % 100 <= ((float)para.RELAY_SIZE / (float)para.NODE_SIZE * 100))
        //{    
            g_as_relay[i] = true;
            num_relays++;
        //}
        if (++i == para.NODE_SIZE)
            i = 0;
    }

    i = 1;
    while (i < para.MATCHER_SIZE)
    {
        g_as_matcher[i++] = true;       
    }

    // starts stat collections
    g_stat.init_timer (g_para);
    return 0;
}

bool CreateNode (bool wait_till_ready)
{
    // obtain current node index
    size_t i = g_nodes.size ();
    
    // get # of currently active nodes
    // TODO: more efficient way? (redundent with NextStep)
    int num_active = 0;    
    for (size_t j=0; j < i; j++)
    {
        if (g_nodes[j]->isJoined ())
            num_active++;
    } 

    // do not create beyond stable size
    if (g_para.STABLE_SIZE != 0 && (num_active > g_para.STABLE_SIZE))
        return false;

    g_vastnetpara.is_relay      = g_as_relay[i];
    g_vastnetpara.is_matcher    = g_as_matcher[i];
    g_vastnetpara.is_static     = STATIC_PARTITIONING;

    SimNode *n = new SimNode (i+1, &g_move_model, g_GWstr, g_para, g_vastnetpara);
    g_nodes.push_back (n);

    // NOTE: it's important to advance the logical time here, because nodes would 
    //       not be able to receive messages sent during the same time-steps
    
    if (wait_till_ready)
    {
        // make sure all nodes have joined before moving    
        do 
        {
            // each node processes messages received so far
            for (size_t j=0; j <= i; ++j)
                g_nodes[j]->processMessage ();
        }
        // make sure the new node has joined before moving on
        while (g_nodes[i]->isJoined () == false);
    }
    else
    {
        // each node processes messages received so far
        for (size_t j=0; j <= i; ++j)
            g_nodes[i]->processMessage ();        
    }

    // store node into stat class for later processing
    g_stat.add_node (n);
    
    return true;
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

    // total # of nodes created (both active & failed)
    int n = g_nodes.size ();
    
    // # of currently active nodes
    int num_active = 0;

    // each node makes a move or checks for joining
    for (i=0; i < n; ++i)
    {
        if (g_nodes[i]->isJoined ())
        {
            num_active++;
            g_nodes[i]->move ();
        }
    }    

    // fail a node if time has come, but only within a certain time interval   
    if (g_para.FAIL_RATE > 0   && 
        num_active > g_para.STABLE_SIZE && 
        g_steps > (g_para.TIME_STEPS / 3) && g_steps <= (2 * g_para.TIME_STEPS / 3) &&
        //num_active < (g_para.STABLE_SIZE * STABLE_SIZE_MULTIPLIER) &&
        g_steps % g_para.FAIL_RATE == 0)
    {
        //FailMethod method = RELAY_ONLY;
        //FailMethod method = MATCHER_ONLY;
        //FailMethod method = CLIENT_ONLY;
        FailMethod method = RANDOM;
        
        // index of the node to fail
        int i = n;

        switch (method)
        {
        // random fail
        case RANDOM:
            {
                if (num_active > 1)
                {
                    // NOTE: we assume there's definitely some node to fail, otherwise
                    //       will enter infinite loop
                    bool failed = false;
                    int tries = 0;
                    while (!failed)
                    {
                        // do not fail gateway
                        i = (rand () % (n-1))+1;
                                        
                        if (g_nodes[i]->isJoined () || tries > n)
                            break;
                
                        tries++;
                    }
                }
            }
            break;

        case RELAY_ONLY:
            {
                // fail each active relay (except gateway) until all are failed
                for (i=1; i<n; i++)
                {
                    if (g_as_relay[i] == true)
                    {
                        g_as_relay[i] = false;
                        break;
                    }
                }
            }
            break;

        case MATCHER_ONLY:
            {
                // fail each active relay (except gateway) until all are failed
                for (i=1; i<n; i++)
                {
                    if (g_as_matcher[i] == true)
                    {
                        g_as_matcher[i] = false;
                        break;
                    }
                }
            }
            break;

        case CLIENT_ONLY:
            {
                // fail each active non-relay 
                for (i=1; i<n; i++)
                {
                    if ((g_as_relay[i] == false && g_as_matcher[i] == false) && 
                        g_nodes[i]->isJoined () == true)
                    {
                        break;
                    }
                }
            }
            break;
        }      

        if (i > 0 && i < n)
        {
            printf ("failing [%llu]..\n", g_nodes[i]->getPeerID ());
            g_nodes[i]->fail ();
        }
    }
    
    // each node processes messages received so far
    for (i=0; i < n; ++i)
        g_nodes[i]->processMessage ();    

    // each node calculates stats
    for (i=0; i < n; ++i)
        g_nodes[i]->recordStat ();

    //
    // stat collection 
    //
    int inconsistent_count = g_stat.record_step ();    

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
    // check for invalid index, VAST node not yet joined, or failed node
    if ((unsigned)index >= g_nodes.size () || 
        g_nodes[index]->vnode == NULL ||
        g_nodes[index]->isFailed ())
        return NULL;

    return g_nodes[index]->getSelf ();
}

std::vector<Node *>* GetNeighbors (int index)
{   
    //if (g_peermap.find (index) == g_peermap.end ())
    //    return NULL;

    // neighbors as stored on the relay's VONPeers
    //return g_peermap[index]->getPeerNeighbors (g_peerid[index]);

    // check if index is invalid, or the node referred has not yet joined successfully
    if ((unsigned)index >= g_nodes.size () || g_nodes[index]->vnode == NULL)
        return NULL;

    return &g_nodes[index]->vnode->list ();
}

std::vector<Vast::id_t> *GetEnclosingNeighbors (int index, int level)
{
    return NULL;

    /*
    if (g_peermap.find (index) == g_peermap.end ())
        return NULL;

    Voronoi *v = g_peermap[index]->getVoronoi (g_peerid[index]);   

    if (v == NULL)
        return NULL;

    return &v->get_en (g_nodes[index]->getID (), level);
    */
}

std::vector<line2d> *GetEdges (int index)
{
    //return NULL;

    if ((unsigned)index >= g_nodes.size () || g_nodes[index]->vnode == NULL)
        return NULL;

    Voronoi *v = g_nodes[index]->getVoronoi ();

    if (v == NULL)
        return NULL;

    return &v->getedges ();
}

bool GetBoundingBox (int index, point2d& min, point2d& max)
{
    return false;
}

int ShutSim ()
{    

    g_stat.print_stat ();

    g_as_relay.clear ();

    int n = g_nodes.size ();

    // delete nodes
    for (int i=0; i<n; ++i) 
    {
        delete (SimNode *)g_nodes[i];        
        g_nodes[i] = NULL;
    }
    g_nodes.clear ();

    return 0;
}





















