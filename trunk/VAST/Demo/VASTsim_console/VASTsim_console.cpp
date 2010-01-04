
/*
 *	console simulator for VAST      ver 0.1             2005/04/12
 */

                                  
#ifdef WIN32
// disable warning about "identifier exceeds'255' characters"
#pragma warning(disable: 4786)

#include <vld.h>            // visual leak detector (NOTE: must download & install from
                            // http://www.codeproject.com/KB/applications/visualleakdetector.aspx
//#include <conio.h>
#endif

#include <stdlib.h>         // atoi, srand
#include <map>
#include "VASTsim.h"

using namespace std;
using namespace Vast;

#define PRINT_MSG_


int main (int argc, char *argv[])
{    
    SimPara para;                   // simulation parameters
    map<int, vector<Node *> *> nodes;   // neighbors of each node

    // order of preference for parameters: 1. file 2. commandline 3. built-in
    if (ReadPara (para) == false)
    {
        if (argc != 17)
        {
            // use defaults
            para.VAST_MODEL      =  1;        // 1: Direct connection 2: Forwarding
            para.NET_MODEL       =  1;        // 1: emulated 2: emulated with bandwidth limit
            para.MOVE_MODEL      =  1;        // 1: random 2: cluster
            para.WORLD_WIDTH     =  800;
            para.WORLD_HEIGHT    =  600;
            para.NODE_SIZE       =  10;
            para.RELAY_SIZE      =  5;
            para.TIME_STEPS      =  100;
            para.STEPS_PERSEC    =  10;
            para.AOI_RADIUS      =  100;
            para.AOI_BUFFER      =  15;
            para.CONNECT_LIMIT   =  0;
            para.VELOCITY        =  5;
            para.LOSS_RATE       =  0;
            para.FAIL_RATE       =  0;
            para.UPLOAD_LIMIT    =  2000;   // in bytes / step
            para.DOWNLOAD_LIMIT  =  10000;  // in bytes / step
            para.PEER_LIMIT      =  100;
            para.RELAY_LIMIT     =  10;
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
    bool running = true;
    while (running)
    {     
        steps++; 
                 
		 if (NextStep () < 0)
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
        //getch ();
    }

    ShutSim ();
    
    return 0;
}

