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

#include "VASTsim.h"
#include "VASTVerse.h"
#include "SimNode.h"
#include <stdlib.h>         // strtok

#define RECORD_LATENCY      // to record transmission latency for MOVEMENT messages
#include "Statistics.h"

using namespace std;        // vector
using namespace Vast;       // vast

SimPara             g_para;
VASTPara_Net        g_netpara;
statistics          g_stat;
vector<SimNode *>   g_nodes;            // pointer to all simulation nodes
vector<bool>        g_as_relay;
MovementGenerator   g_move_model;
Addr                g_gateway;          // address to gateway node
SectionedFile      *g_pos_record = NULL;

//map<int, VAST *>    g_peermap;          // map from node index to the peer's relay id
//map<int, Vast::id_t> g_peerid;           // map from node index to peer id

int                 g_last_seed;       // storing random seed

int                 g_steps     = 0;
bool                g_joining   = true;


// Initilize parameters, including setting default values or read from INI file
int InitPara (VAST_NetModel model, VASTPara_Net &netpara, SimPara &simpara, const char *cmdline, bool *p_is_gateway, Area *p_aoi, Addr *p_gateway, vector<IPaddr> *p_entries)
{
    netpara.model = model;

    // parameters to be filled
    bool is_gateway;
    Area aoi;
    vector<IPaddr> entries;

    // default default values
    aoi.center.x = (coord_t)(rand () % DIM_X);
    aoi.center.y = (coord_t)(rand () % DIM_Y);
    aoi.radius   = (length_t)DEFAULT_AOI;

    netpara.port  = GATEWAY_DEFAULT_PORT;
    netpara.step_persec = STEPS_PER_SECOND;

/*
    netpara.peer_limit = 100;
    netpara.relay_limit = 10;
    netpara.client_limit = 10;
*/

    simpara.NODE_SIZE = 10;

    // which node to simulate, (0) means manual
    int node_no = 0; 
   
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
                sprintf (GWstr, "%s:%d", p, netpara.port);
                break;
    
            case 2:
                // 3rd parameter: node to simulate                
                node_no = atoi (p);
                break;
    
            // X-coord
            case 3:
                aoi.center.x = (coord_t)atoi (p);
                break;
    
            // Y-coord
            case 4:
                aoi.center.y = (coord_t)atoi (p);
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
/*
#ifndef VAST_ONLY
        para.default_aoi        = simpara.AOI_RADIUS;
        para.world_height       = simpara.WORLD_HEIGHT;
        para.world_width        = simpara.WORLD_WIDTH;        
        para.overload_limit     = simpara.OVERLOAD_LIMIT;
#endif
*/
        netpara.step_persec   = simpara.STEPS_PERSEC;           
        aoi.radius            = simpara.AOI_RADIUS;
    }
    else  
    {
        // INI file not found, cannot perform simulation
        if (node_no != (-1))
        {
            printf ("warning: INI file is not found at working directory, it's required for simulation\n");
            return (-1);
        }
    }

    //bool is_gateway = false;
    is_gateway = false;

    // default gateway set to localhost
    if (GWstr[0] == 0)
    {
        is_gateway = true;
        netpara.is_entry = true;
        sprintf (GWstr, "127.0.0.1:%d", netpara.port);
    }        
        
    // if physical coordinate is not supplied, VAST will need to obtain it itself
    //g_netpara.phys_coord = g_aoi.center;    

    // translate gateway string to Addr object
    string str (GWstr);
    g_gateway = *VASTVerse::translateAddress (str);

    // create VAST node factory (with default physical coordinate)          

    // TODO: we may use more entries during join
    // NOTE: the very first node does not know other existing relays
    if (is_gateway == false)
    {
        entries.push_back (g_gateway.publicIP);
    }

    // return values, if needed
    if (p_is_gateway != NULL)
        *p_is_gateway = is_gateway;

    if (p_aoi != NULL)
        *p_aoi = aoi;

    if (p_gateway != NULL)
        *p_gateway = g_gateway;

    if (p_entries != NULL)
        *p_entries = entries;

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


int InitSim (SimPara &para, VASTPara_Net &netpara)
{
    g_para = para;
    g_netpara = netpara;

    // note there's no need to assign the gateway ID as it'll be found automatically
    g_netpara.model        = (VAST_NetModel)para.NET_MODEL;
    g_netpara.port         = GATEWAY_DEFAULT_PORT;    
    g_netpara.client_limit = para.PEER_LIMIT;
    g_netpara.relay_limit  = para.RELAY_LIMIT;        
    g_netpara.conn_limit   = para.CONNECT_LIMIT;
    g_netpara.recv_quota   = para.DOWNLOAD_LIMIT;
    g_netpara.send_quota   = para.UPLOAD_LIMIT;
    g_netpara.step_persec  = para.STEPS_PERSEC;

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
    srand ((unsigned int)time (NULL));
    g_last_seed = rand ();

    // randomly choose which nodes will be the relays
    int num_relays = 1;
    int i;
    for (i=0; i < para.NODE_SIZE; i++)
        g_as_relay.push_back (false);

    g_as_relay[0] = true;
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

    // starts stat collections
    g_stat.init_timer (g_para);
    return 0;
}

int CreateNode (bool wait_till_ready)
{
    // obtain current node index
    size_t i = g_nodes.size ();        

    SimNode *n = new SimNode (i+1, &g_move_model, g_gateway, g_para, g_netpara, g_as_relay[i]);
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
                g_nodes[j]->processmsg ();
        }
        // make sure the new node has joined before moving on
        while (g_nodes[i]->isJoined () == false);
    }
    else
    {
        // each node processes messages received so far
        for (size_t j=0; j <= i; ++j)
            g_nodes[i]->processmsg ();
        
        g_nodes[i]->isJoined ();
    }

    // store node into stat class for later processing
    g_stat.add_node (n);
    
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

    int n = g_nodes.size ();

    /*
    // TODO: may not need to do this every time
    // build up node # -> peer mapping (or rather, the VAST interface of its relay)
    for (i=0; i < n; ++i)
    {
        Vast::id_t peer_id = g_nodes[i]->getPeerID ();
        g_peerid[i] = peer_id;

        // find which relay has it
        int j;
        for (j=0; j < n; j++)
            if (g_nodes[j]->isJoined () && g_nodes[j]->vnode->getVoronoi (peer_id) != NULL)
            {
                g_peermap[i] = g_nodes[j]->vnode;
                break;
            }
    }
    */
    
    // each node makes a move or checks for joining
    for (i=0; i < n; ++i)
    {
        if (g_nodes[i]->isJoined ())
            g_nodes[i]->move ();
    }    

    // fail a node if time has come    
	if (g_para.FAIL_RATE > 0 && g_steps >= g_para.FAIL_RATE && g_steps % g_para.FAIL_RATE == 0)
    {
        FailMethod method = RELAY_ONLY;
        //FailMethod method = CLIENT_ONLY;

        // random fail
        switch (method)
        {
        case RANDOM:
            {
                int index = rand () % n;
                
                // do not fail gateway
                if (index == 0)
                    index = 1;

                Vast::id_t peer_id = g_nodes[index]->getPeerID ();
                printf ("failing [%ld]..\n", peer_id);
                g_nodes[index]->fail ();
            }
            break;

        case RELAY_ONLY:
            {
                // fail each active relay (except gateway) until all are failed
                for (int i=1; i<n; i++)
                {
                    if (g_as_relay[i] == true)
                    {
                        g_nodes[i]->fail ();
                        g_as_relay[i] = false;
                        break;
                    }
                }
            }
            break;

        case CLIENT_ONLY:
            {
                // fail each active non-relay 
                for (int i=1; i<n; i++)
                {
                    if (g_as_relay[i] == false && g_nodes[i]->isJoined () == true)
                    {
                        g_nodes[i]->fail ();
                        break;
                    }
                }
            }
            break;
        }      
    }
    
    // each node processes messages received so far
    for (i=0; i < n; ++i)
        g_nodes[i]->processmsg ();    

    // each node calculates stats
    for (i=0; i < n; ++i)
        g_nodes[i]->record_stat ();

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
    /*
    // NOTE: this has the effect of not displaying any node that's not actively joined
    if (g_peermap.find (index) == g_peermap.end () || g_peermap[index]->isJoined () == false)
        return NULL;

    return g_peermap[index]->getPeer (g_peerid[index]);
    */
    if ((unsigned)index >= g_nodes.size () || g_nodes[index]->vnode == NULL)
        return NULL;

    return g_nodes[index]->vnode->getSelf ();
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

    return &v->get_en (g_nodes[index]->get_id (), level);
    */
}

std::vector<line2d> *GetEdges(int index)
{
    return NULL;

    //if (g_peermap.find (index) == g_peermap.end ())
    //    return NULL;

    /*
    Voronoi *v = g_peermap[index]->getVoronoi (g_peerid[index]);

    if (v == NULL)
        return NULL;

    return &v->getedges ();
    */
}

bool GetBoundingBox (int index, point2d& min, point2d& max)
{
    return false;
}

int ShutSim ()
{    

    g_stat.print_stat ();

   // g_peermap.clear ();
   // g_peerid.clear ();
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





















