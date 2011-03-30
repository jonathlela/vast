
/*
 *	console simulator for VAST      ver 0.1             2005/04/12
 */

                                  
#ifdef WIN32
// disable warning about "identifier exceeds'255' characters"
#pragma warning(disable: 4786)

//#include <vld.h>            // visual leak detector (NOTE: must download & install from
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
    SimPara simpara;                       // simulation parameters
    VASTPara_Net netpara (VAST_NET_EMULATED);               // network parameter
    map<int, vector<Node *> *> nodes;   // neighbors of each node
   
    ReadPara (simpara);

    // read parameters and initialize simulations
    InitPara (VAST_NET_EMULATED, netpara, simpara);
    
    InitSim (simpara, netpara);

    // # of nodes currently created
    int nodes_created = 0;
    int create_countdown = 0;
        
    // start simulation moves    
    int steps = 0;
    bool running = true;
    while (running)
    {     
        steps++; 

        // create nodes
        if (nodes_created < simpara.NODE_SIZE)
        {
            if (create_countdown == 0)
            {                
                CreateNode (nodes_created == 0);
                nodes_created++;
                printf ("creating node %d \r%3d%%\n", nodes_created, (int) ((double) (nodes_created * 100) / (double) simpara.NODE_SIZE));
                create_countdown = simpara.JOIN_RATE;
            }            
            else
                create_countdown--;
        }    
                 
		if (NextStep () < 0)
            break;
  
        // obtain positions of my current neighbors        
        for (int j=0; j < simpara.NODE_SIZE; ++j)
        {
#ifdef PRINT_MSG
            printf ("[%d] has neighbors: ", j+1);
#endif     
            vector<Node *> *neighbors = GetNeighbors (j);            
            nodes[j] = neighbors;

#ifdef PRINT_MSG
            if (neighbors != NULL)
            {
                for (size_t i=0; i < neighbors->size (); i++)
                    printf ("[%llu] (%d, %d) ", neighbors->at (i)->id, (int)neighbors->at (i)->aoi.center.x, (int)neighbors->at (i)->aoi.center.y);
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

