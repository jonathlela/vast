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
#include "Statistics.h"
#include "SimNode.h"

#include <algorithm>

using namespace std;        // vector
using namespace Vast;       // vast

SimPara                  g_para;
statistics               g_stat;
vector<SimNode *>        g_nodes;              // pointer to all simulation nodes
vector<bool>             g_as_relay;
MovementGenerator        g_move_model;
SectionedFile           *g_pos_record = NULL;


map<int, VAST *>         g_peermap;            // map from node index to the peer's relay id
map<int, id_t>           g_peerid;             // map from node index to peer id

int                      g_last_seed;          // storing random seed
int                      g_steps     = 0;
bool                     g_joining   = true;
const static int         APP_STEP_PERSEC = 10;
bool					 g_is_init   = false;
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
		&para.WITH_LATENCY,
		&para.ARRIVAL_RATIO,
		&para.DEPATURE_MODEL,
		&para.MAX_JOIN_NODES,
		&para.MAX_LEAVE_NODES,
		&para.STABLE_STEPS ,
		&para.PEER_LIMIT,
		&para.RELAY_LIMIT,
		0
	};

	char buff[512];
	int n = 0;
	while (fgets (buff, 512, fp) != NULL)
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
	//replay = false;
	size_t max_node_size = 200;
/*
	if (g_para.RELAY_SIZE >= g_para.NODE_SIZE)
	{
		max_node_size = 500;
	}
	else
		max_node_size = g_para.NODE_SIZE + g_para.STABLE_STEPS * ((g_para.MAX_JOIN_NODES / g_para.ARRIVAL_RATIO)) + 100;
*/

	g_move_model.initModel (g_para.MOVE_MODEL, g_pos_record, replay, 
		Position (0,0), Position ((coord_t)g_para.WORLD_WIDTH, (coord_t)g_para.WORLD_HEIGHT),
		max_node_size, g_para.TIME_STEPS / (g_para.STEPS_PERSEC/APP_STEP_PERSEC), (double)g_para.VELOCITY);    

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

size_t getActiveSize ()
{
	vector<SimNode*>::iterator it = g_nodes.begin();
	vector<SimNode*>::iterator end_it = g_nodes.end ();
	size_t num_active = 0;
	for (;it != end_it; it++)
	{
		if((*it)->state != FAILED)
		{
			num_active++;
		}
	}
	return num_active;
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

	size_t i;   
		
	//const static int delay_rate = (g_para.STEPS_PERSEC/APP_STEP_PERSEC);

	// TODO: may not need to do this every time
	// build up node # -> peer mapping (or rather, the VAST interface of its relay)
	for (i=0; i < g_nodes.size(); ++i)
	{
		if (!g_nodes[i]->isJoined())
		{
			continue;
		}
		id_t peer_id = g_nodes[i]->getPeerID ();
		g_peerid[i] = peer_id;

		// find which relay has it
		size_t j;
		for (j=0; j < g_nodes.size(); j++)
		{
			if (g_nodes[j]->isJoined() && g_nodes[j]->vnode->getVoronoi (peer_id) != NULL)
			{
				g_peermap[i] = g_nodes[j]->vnode;
				break;
			}            
		}
	}

	int app_step = g_steps /*/ delay_rate*/;
	//if (/*g_steps % delay_rate == 0*/)
			
		// each node makes a move
		for (i=0; i < g_nodes.size(); ++i)
		{		
			g_nodes[i]->move ();						
		}

		size_t cur_num_nodes = getActiveSize();
		// fail a node if time has come
		if (g_para.FAIL_RATE > 0 
//			&& app_step >= g_para.FAIL_RATE 
//			&& app_step % g_para.FAIL_RATE == 0
			&& g_para.STABLE_STEPS == g_steps)
		{
			//FailMethod method = g_para.DEPATURE_MODEL;
			//FailMethod method = CLIENT_ONLY;

			// random fail
			switch (g_para.DEPATURE_MODEL)
			{
				case RANDOM:
				{
/*
					int failed_nodes = 0;
					for (int i = 0; i < g_nodes.size(); i++)
					{
						int index = rand () % g_nodes.size();
						if (index != 0 && g_nodes[index]->isJoined () == true )
						{
							g_nodes[index]->fail ();								
							failed_nodes++;
							cur_num_nodes--;
							if (failed_nodes == g_para.MAX_LEAVE_NODES)
							{
								break;
							}						
						}					
					}		
*/
					vector<id_t> fail_list;

					while(fail_list.size() < g_para.MAX_LEAVE_NODES)
					{
						int fail_index = rand()% g_nodes.size();	
						if (fail_index != 0
							&&g_nodes[fail_index]->isJoined () == true
							&& count(fail_list.begin(), fail_list.end(), fail_index) ==0
							)
						{
							fail_list.push_back(fail_index);
						}														
					}					


					for(int i = 0; i < fail_list.size(); i++)
					{
						g_nodes[fail_list[i]]->fail ();
					}

					
				}
					break;

				case RELAY_ONLY:
					{
						// fail each active relay (except gateway) until all are failed
/*
						int failed_nodes = 0;
						for (int i=1; i<g_nodes.size(); i++)
						{
							if (g_as_relay[i] == true
								&& g_nodes[i]->isJoined () == true )
							{
								g_nodes[i]->fail ();
								g_as_relay[i] = false;
								failed_nodes++;
								cur_num_nodes--;
								if (failed_nodes == g_para.MAX_LEAVE_NODES)
								{
									break;
								}
							}
						}
*/
						vector<id_t> fail_list;

						while(fail_list.size() < g_para.MAX_LEAVE_NODES)
						{
							int fail_index = rand()% g_nodes.size();	
							if (fail_index != 0
								&& g_as_relay[fail_index] == true
								&& g_nodes[fail_index]->isJoined () == true
								&& count(fail_list.begin(), fail_list.end(), fail_index) ==0
								)
							{
								fail_list.push_back(fail_index);
							}														
						}					


						for(int i = 0; i < fail_list.size(); i++)
						{
							g_nodes[fail_list[i]]->fail ();
						}

					}
					break;

				case CLIENT_ONLY:
					{
/*
						// fail each active non-relay 
						int failed_nodes = 0;
						for (int i=1; i<g_nodes.size(); i++)
						{							
							if (g_as_relay[i] == false && g_nodes[i]->isJoined () == true )
							{
								g_nodes[i]->fail ();
								failed_nodes++;
								cur_num_nodes--;
								if (failed_nodes == g_para.MAX_LEAVE_NODES)
								{
									break;
								}												
							}
						}
*/

						vector<id_t> fail_list;						
						while(fail_list.size() < g_para.MAX_LEAVE_NODES)
						{
							int fail_index = rand()% g_nodes.size();	
							if (g_as_relay[fail_index] == false 
								&& g_nodes[fail_index]->isJoined () == true
								&& count(fail_list.begin(), fail_list.end(), fail_index) ==0
								)
							{
								fail_list.push_back(fail_index);
							}														
						}					


						for(int i = 0; i < fail_list.size(); i++)
						{
							g_nodes[fail_list[i]]->fail ();
						}

						//printf("There are no nodes to fail/n!!");
					}
					break;
				}      
		}
	
		if (g_para.ARRIVAL_RATIO > 0 
			&& app_step >= g_para.ARRIVAL_RATIO 
			&& app_step % g_para.ARRIVAL_RATIO == 0
			&& g_para.STABLE_STEPS > g_steps
			)
		{

			//int num_active = min(d_num, g_para.MAX_JOIN_NODES);
			// how many nodes should be joined at this round
			int id = g_nodes.size();
			for(int i =0; i < g_para.MAX_JOIN_NODES; i++, id++, cur_num_nodes++)
			{
				CreateNode(id);
			}
		}

	// each node processes messages received so far
	for (i=0; i < g_nodes.size(); ++i)
	{
		g_nodes[i]->processmsg ();    
		if (g_nodes[i]->state == WAITING && !g_stat.is_rec(g_nodes[i]->getBehId()))
		{
			g_stat.add_node (g_nodes[i]);
		}		
	}

	// each node calculates stats
	for (i=0; i < g_nodes.size(); ++i)
	{	
		if (g_nodes[i]->state != IDLE)
		{
			g_nodes[i]->record_stat ();		
		}		
	}

	//
	// stat collection 
	//
	int inconsistent_count = g_stat.record_step ();    
	//int inconsistent_count = 0;
	//
	// returns (-1) for terminating the simulation, or # of inconsistent nodes
	//

	// nodes joining control	
	//int prob_join = rand()%100;

	printf(" Current number of nodes %d\n", cur_num_nodes);
	if (g_steps == g_para.TIME_STEPS)
		return (-1);
	else
		return inconsistent_count;
}

int CreateNode ()
{
	// obtain current node index
	size_t i = g_nodes.size ();

	g_nodes.push_back (new SimNode (i+1, &g_move_model, g_para, g_as_relay[i], i));

	// NOTE: it's important to advance the logical time here, because nodes would 
	//       not be able to receive messages sent during the same time-steps

	// make sure all nodes have joined before moving    
	while (1)
	{
		// each node processes messages received so far
		for (size_t j=0; j <= i; ++j)
			g_nodes[j]->processmsg ();

		// make sure the new node has joined before moving on
		if (g_nodes[i]->isJoined ())
			break;        
	}

	// store node into stat class for later processing
	g_stat.add_node (g_nodes[i]);

	return 0;
}

int CreateNode (id_t i)
{
	// obtain current node index
	int num_relays = std::count(g_as_relay.begin(), g_as_relay.end(), true);
	g_as_relay.push_back(false);
	
	if (num_relays < g_para.RELAY_SIZE)
	{
	//	if (rand () % 100 <= ((float)para.RELAY_SIZE / (float)para.NODE_SIZE) * 100)
		{
			g_as_relay[i]=true;
		}		
	}

	g_nodes.push_back (new SimNode (i+1, &g_move_model, g_para, g_as_relay[i], i));
/*
	// NOTE: it's important to advance the logical time here, because nodes would 
	//       not be able to receive messages sent during the same time-steps
	if (!g_is_init)
	{
		// make sure all nodes have joined before moving    
		while (1)
		{
			// each node processes messages received so far
			for (size_t j=0; j <= i; ++j)
			{				
				g_nodes[j]->processmsg ();
			}
			// make sure the new node has joined before moving on
			if (g_nodes[i]->isJoined ())
				break;        
		}
	}
*/


	
	// store node into stat class for later processing
		
	return 0;
}

Node *GetNode (int index)
{
	if (g_peermap.find (index) == g_peermap.end ())
		return NULL;

	return g_peermap[index]->getPeer (g_peerid[index]);
}

std::vector<Node *>* GetNeighbors (int index)
{   
	if (g_peermap.find (index) == g_peermap.end ())
		return NULL;

	// neighbors as stored on the relay's VONPeers
	//return g_peermap[index]->getPeerNeighbors (g_peerid[index]);

	// neighbors as known at each Clients
	return &g_nodes[index]->vnode->list ();
}

std::vector<Vast::id_t> *GetEnclosingNeighbors (int index, int level)
{
	if (g_peermap.find (index) == g_peermap.end ())
		return NULL;

	return &g_peermap[index]->getVoronoi (g_peerid[index])->get_en (g_nodes[index]->get_id (), level);
}

std::vector<line2d> *GetEdges(int index)
{
	if (g_peermap.find (index) == g_peermap.end ())
		return NULL;

	return &g_peermap[index]->getVoronoi (g_peerid[index])->getedges ();
}

int ShutSim ()
{    

	g_stat.print_stat ();
	g_peermap.clear ();
	g_peerid.clear ();
	g_as_relay.clear ();

	// delete nodes
	for (int i=0; i<g_para.NODE_SIZE; ++i) 
	{
		delete (SimNode *)g_nodes[i];        
		g_nodes[i] = NULL;
	}
	g_nodes.clear ();

	return 0;
}

