
/*
 *	console simulator for VAST      ver 0.1             2005/04/12
 */

#ifdef WIN32
// disable warning about "identifier exceeds'255' characters"
#pragma warning(disable: 4786)
#endif

#include <stdlib.h>         // atoi, srand
#include <map>
#include "vastsim.h"

using namespace std;
using namespace Vast;

int main (int argc, char *argv[])
{    
    VASTsimPara para;                   // simulation parameters
    //map<int, vector<Node *> *> nodes;   // neighbors of each node

    // order of preference for parameters: 1. file 2. commandline 3. built-in
    if (ReadPara (para) == false)
    {
        if (argc != 12)
        {
            // use defaults
            para.VAST_MODEL      =  1;        // 1: Direct connection 2: Forwarding
            para.NET_MODEL       =  1;        // 1: emulated 2: emulated with bandwidth limit
            para.MOVE_MODEL      =  1;        // 1: random 2: cluster
            para.WORLD_WIDTH     =  800;
            para.WORLD_HEIGHT    =  600;
            para.NODE_SIZE       =  50;
            para.TIME_STEPS      =  1000;
            para.AOI_RADIUS      =  100;
            para.AOI_BUFFER      =  15;
            para.CONNECT_LIMIT   =  0;
            para.VELOCITY        =  5;
            para.LOSS_RATE       =  0;
            para.FAIL_RATE       =  0;        
        }
        else
        {
            para.VAST_MODEL          = atoi (argv[1]);
            para.NET_MODEL           = atoi (argv[2]);
            para.MOVE_MODEL          = atoi (argv[3]);
            para.WORLD_WIDTH         = atoi (argv[4]);
            para.WORLD_HEIGHT        = atoi (argv[5]);        
            para.NODE_SIZE           = atoi (argv[6]);
            para.TIME_STEPS          = atoi (argv[7]);
            para.AOI_RADIUS          = atoi (argv[8]);
            para.AOI_BUFFER          = atoi (argv[9]);
            para.CONNECT_LIMIT       = atoi (argv[10]);
            para.VELOCITY            = atoi (argv[11]);
            para.LOSS_RATE           = atoi (argv[12]);        
            para.FAIL_RATE           = atoi (argv[13]);
        }        
    }
    
    InitVASTsim (para);

    int i;    
    
    // create nodes
    printf ("Creating nodes ...\n");
    for (i=0; i<para.NODE_SIZE; ++i)
    {
        CreateNode ();
        printf ("\r%3d%%", (int) ((double) ((i+1) * 100) / (double) para.NODE_SIZE));
    }
    printf ("\n");
    
    // start simulation moves    
    int steps = 0;
    while (true)
    {     
        steps++; 
        
        if (NextStep () < 0)
            break;
  
        // obtain positions of my current neighbors
        /*
        for (int j=0; j<para.NODE_SIZE; ++j)
            nodes[j] = &GetNeighbors (j);              
        */
        
        // do various stat calculations/collection here
        printf ("step %d\n", steps);     

    }

    ShutVASTsim ();
    
    return 0;
}











