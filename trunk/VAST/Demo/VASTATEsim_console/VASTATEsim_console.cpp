
/*
 *	console simulator for VAST      ver 0.1             2005/04/12
 */


#ifdef WIN32

//#include <vld.h>            // visual leak detector (NOTE: must download & install from
                            // http://www.codeproject.com/KB/applications/visualleakdetector.aspx

// disable warning about "identifier exceeds'255' characters"
#pragma warning(disable: 4786)

#endif

#include <stdlib.h>         // atoi, srand
#include <map>
#include "VASTATEsim.h"

using namespace std;
using namespace Vast;

int main (int argc, char *argv[])
{    
    SimPara para;                   // simulation parameters
    map<int, vector<Node *> *> nodes;   // neighbors of each node

    // order of preference for parameters: 1. file 2. commandline 3. built-in
    if (ReadPara (para, (argc == 2 ? argv[1] : NULL)) == false)
    {
            // use defaults
            para.VAST_MODEL      =  1;        // 1: Direct connection 2: Forwarding
            para.NET_MODEL       =  1;        // 1: emulated 2: emulated with bandwidth limit
            para.MOVE_MODEL      =  2;        // 1: random 2: cluster
            para.WORLD_WIDTH     =  800;
            para.WORLD_HEIGHT    =  600;
            para.NODE_SIZE       =  10;
            para.RELAY_SIZE      =  1;
            para.TIME_STEPS      =  100;
            para.STEPS_PERSEC    =  10;
            para.AOI_RADIUS      =  200;
            para.AOI_BUFFER      =  15;
            para.CONNECT_LIMIT   =  0;
            para.VELOCITY        =  5;
            para.LOSS_RATE       =  0;
            para.FAIL_RATE       =  0;
            para.UPLOAD_LIMIT    =  2000;   // in bytes / step
            para.DOWNLOAD_LIMIT  =  10000;  // in bytes / step
            para.PEER_LIMIT      =  1000;
            para.RELAY_LIMIT     =  10;
    }
    
    InitSim (para);

    int i;    
    
    // create nodes
    printf ("Creating nodes ...\n");
    for (i=0; i<para.NODE_SIZE; ++i)
    {
        CreateNode (true);
        printf ("\r%3d%%\n", (int) ((double) ((i+1) * 100) / (double) para.NODE_SIZE));
    }    
    
    // start simulation moves    
    int steps = 0;
    while (true)
    {     
        steps++; 
        
        if (NextStep () < 0)
            break;
  
        // obtain positions of my current neighbors
     
        /*
        // print a list of neighbors
        for (int j=0; j < para.NODE_SIZE; ++j)
        {

            printf ("[%d] has neighbors: ", j+1);

            vector<Node *> *neighbors = GetNeighbors (j);            
            nodes[j] = neighbors;
  
            if (neighbors != NULL)
            {
                for (size_t i=0; i < neighbors->size (); i++)                    
                    printf ("[%ld] (%d, %d) ", neighbors->at (i)->id, (int)neighbors->at (i)->aoi.center.x, (int)neighbors->at (i)->aoi.center.y);
            }

            printf ("\n");
        }
        */

        // do various stat calculations/collection here
        printf ("step %d\n", steps);
    }

    ShutSim ();
    
    return 0;
}











