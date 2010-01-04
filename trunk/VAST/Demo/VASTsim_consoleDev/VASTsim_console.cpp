
/*
*	console simulator for VAST      ver 0.1             2005/04/12
*/

#ifdef WIN32
// disable warning about "identifier exceeds'255' characters"
#pragma warning(disable: 4786)
#endif

#include <stdlib.h>         // atoi, srand
#include <map>
#include "VASTsim.h"

//
using namespace std;
using namespace Vast;

#define PRINT_MSG_


int main (int argc, char *argv[])
{    

	SimPara para;                   // simulation parameters
	map<int, vector<Node *> *> nodes;   // neighbors of each node

	string conf_filename = "VASTsim.ini";
	if (argc > 5)
		conf_filename = argv[5];
	
	// order of preference for parameters: 1. file 2. commandline 3. built-in
	if (ReadPara (para) == false)
	{
		if (argc != 23)
		{
			// use defaults
			para.VAST_MODEL      =  1;        // 1: Direct connection 2: Forwarding
			para.NET_MODEL       =  1;        // 1: emulated 2: emulated with bandwidth limit
			para.MOVE_MODEL      =  1;        // 1: random 2: cluster
			para.WORLD_WIDTH     =  256;
			para.WORLD_HEIGHT    =  256;
			para.NODE_SIZE       =  20;
			para.RELAY_SIZE      =  2;
			para.TIME_STEPS      =  50;    
			para.STEPS_PERSEC    =  10;       // net_work_layer step per sec
			para.AOI_RADIUS      =  65;
			para.AOI_BUFFER      =  15;
			para.CONNECT_LIMIT   =  0;
			para.VELOCITY        =  5;
			para.LOSS_RATE       =  0;
			para.FAIL_RATE       =  10;
			para.UPLOAD_LIMIT    =  0;    // in bytes / sec
			para.DOWNLOAD_LIMIT  =  0;    // in bytes / sec
			para.WITH_LATENCY    =  1;
			para.ARRIVAL_RATIO   =  0;
		    para.DEPATURE_MODEL  =  2;		// 0: RANDOM, 1: RELAY_ONLY, 2:CLIENT_ONLY
			para.MAX_JOIN_NODES  =  3;
			para.MAX_LEAVE_NODES =  3;
			para.STABLE_STEPS   =   25;
			para.PEER_LIMIT      =  100;
			para.RELAY_LIMIT     =  100;

		}
		else
		{
			int i=1;
			para.VAST_MODEL          = atoi (argv[i++]);
			para.NET_MODEL           = atoi (argv[i++]);
			para.MOVE_MODEL          = atoi (argv[i++]);
			para.WORLD_WIDTH         = atoi (argv[i++]);
			para.WORLD_HEIGHT        = atoi (argv[i++]);        
			para.NODE_SIZE           = atoi (argv[i++]);
			para.RELAY_SIZE          = atoi (argv[i++]);
			para.TIME_STEPS          = atoi (argv[i++]);
			para.STEPS_PERSEC        = atoi (argv[i++]);
			para.AOI_RADIUS          = atoi (argv[i++]);
			para.AOI_BUFFER          = atoi (argv[i++]);
			para.CONNECT_LIMIT       = atoi (argv[i++]);
			para.VELOCITY            = atoi (argv[i++]);
			para.LOSS_RATE           = atoi (argv[i++]);        
			para.FAIL_RATE           = atoi (argv[i++]);
			para.UPLOAD_LIMIT        = atoi (argv[i++]);
			para.DOWNLOAD_LIMIT      = atoi (argv[i++]);
			para.WITH_LATENCY        = atoi (argv[i++]);
			para.ARRIVAL_RATIO       = atoi (argv[i++]);
			para.DEPATURE_MODEL      = atoi (argv[i++]);
			para.MAX_JOIN_NODES      = atoi (argv[i++]);
			para.MAX_LEAVE_NODES     = atoi (argv[i++]);
			para.MAX_LEAVE_NODES     = atoi (argv[i++]);
			para.STABLE_STEPS        = atoi (argv[i++]);
			para.PEER_LIMIT          = atoi (argv[i++]);
			para.RELAY_LIMIT         = atoi (argv[i++]);
		}        
	}

	InitSim (para);

	int i;    
	
	// create nodes
	printf ("Creating nodes ...\n");
	for (i=0; i<para.NODE_SIZE; ++i)
	{
		CreateNode ();
		printf ("\r%3d%%\n", (int) ((double) ((i+1) * 100) / (double) para.NODE_SIZE));
	}    


	// start simulation moves    
	int steps = 0;
	while (true)
	{     
		steps++; 

		if ((NextStep () < 0)/* && (steps % delay_rate == 1)*/)
			break;

		// obtain positions of my current neighbors

		for (int j=0; j < para.NODE_SIZE; ++j)
		{
#ifdef PRINT_MSG
			printf ("[%d] has neighbors: ", j+1);
#endif     
			vector<Node *> *neighbors = GetNeighbors (j);            
			nodes[j] = neighbors;

#ifdef PRINT_MSG
			if (neighbors != NULL)
			{
				for (unsigned int i=0; i < neighbors->size (); i++)
					printf ("[%d] (%d, %d) ", neighbors->at (i)->id, (int)neighbors->at (i)->aoi.center.x, (int)neighbors->at (i)->aoi.center.y);
			}

			printf ("\n");
#endif
		}       
		// do various stat calculations/collection here
		printf ("step %d\n", steps);
	}

	ShutSim ();
	
	//_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	return 0;
}











