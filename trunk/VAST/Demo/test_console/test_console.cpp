
/*
 *  test_console    a VAST experimental node
 *  
 *  version:    2010/06/15  init - adopted from demo_console
 *
 */

#ifdef WIN32
// disable warning about "unsafe functions"
#pragma warning(disable: 4996)
#endif

#include "ace/ACE.h"
#include "ace/OS.h"

#include <stdio.h>

// for getting keyboard inputs
#ifdef WIN32
#include <windows.h>
#include <conio.h>
#endif

#include "Movement.h"

// use VAST for functions
#include "VASTVerse.h"
#include "VASTUtil.h"
#include "VASTsim.h"

#define VAST_EVENT_LAYER    1                   // layer ID for sending events 
#define VAST_UPDATE_LAYER   2                   // layer ID for sending updates

using namespace Vast;
using namespace std;

#ifdef ACE_DISABLED
#error "ACE needs to be enabled to build demo_console, please modify /common/Config.h"
#endif

// target game cycles per second
const int FRAMES_PER_SECOND      = 40;


// global
Area        g_aoi;              // my AOI (with center as current position)
Area        g_prev_aoi;         // previous AOI (to detect if AOI has changed)
Addr        g_gateway;          // address for gateway
bool        g_finished = false; // whether we're done for this program execution
NodeState   g_state = ABSENT;   // the join state of this node
char        g_lastcommand = 0;  // last keyboard character typed 
size_t      g_count = 0;        // # of ticks so far (# of times the main loop has run)
size_t      time_offset = 2;     
int         g_node_no = (-1);   // which node to simulate (-1 indicates none, manual control)

VASTPara_Net g_netpara;         // network parameters

MovementGenerator g_movement;
FILE       *g_position_log = NULL;     // logfile for node positions
FILE	   *g_neighbor_log = NULL;	   // logfile for node neighbors

// VAST-specific variables
VASTVerse *     g_world = NULL;
VAST *          g_self  = NULL;
Vast::id_t      g_sub_no = 0;        // subscription # for my client (peer)  

#ifdef WIN32
void getInput ()
{
    // get input
    while (kbhit ())
    {
        char c = (char)getch ();
        //printf ("%d ", c);
        switch (c)
        {
        // quit
        case 'q':
            g_finished = true;
            break;

        // leave overlay
        case 'l':
            break;

        // join at current position
        case 'j':
            break;
        
        // movements
        case -32:
            switch (getch ())
            {
            // LEFT
            case 75:
                g_aoi.center.x -= 5;
                break;
            // RIGHT
            case 77:
                g_aoi.center.x += 5;
                break;                            
            // UP
            case 72:
                g_aoi.center.y -= 5;
                break;
            // DOWN
            case 80:
                g_aoi.center.y += 5;
                break;
            }
            break;

        default:
            g_lastcommand = c;
            break;
        }
    }
}
#endif

void checkJoin ()
{    
    // create the VAST node
    switch (g_state)
    {
    case ABSENT:
        if ((g_self = g_world->getVASTNode ()) != NULL)
        {                
            g_sub_no = g_self->getSubscriptionID (); 
            g_state = JOINED;
        }
        break;

    default:
        break;
    }

    if (g_state == JOINED && g_position_log != NULL)
    {
        Node *self;
        Vast::id_t nodeID;

        self = g_self->getSelf ();
        nodeID = g_self->getSubscriptionID ();

		// record join time
		time_t rawtime;          
		time (&rawtime);

		tm *timeinfo = gmtime (&rawtime);
        
		ACE_Time_Value startTime = ACE_OS::gettimeofday();
		fprintf (g_position_log, "# Node joined, Position Log starts\n\n");
		fprintf (g_neighbor_log, "# Node joined, Neighbor Log starts\n\n");

		// node ID
		fprintf (g_position_log, "# node ID\n");
		fprintf (g_position_log, "%llu\n", nodeID);
		fprintf (g_neighbor_log, "# node ID\n");
		fprintf (g_neighbor_log, "%llu\n", nodeID);
		
		fprintf (g_position_log, "# Start date/time\n"); 
		fprintf (g_position_log, "# %s", asctime (timeinfo)); 
		fprintf (g_position_log, "# GMT (hour:min:sec)\n%2d,%02d,%02d\n", 
				timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
		fprintf (g_position_log, "# second:millisec\n%d,%d\n", (int)startTime.sec (), (int)(startTime.usec () / 1000));      
		fprintf (g_neighbor_log, "# Start date/time\n"); 
		fprintf (g_neighbor_log, "#%s", asctime (timeinfo)); 
		fprintf (g_neighbor_log, "# GMT (hour:min:sec)\n%2d,%02d,%02d\n", 
				timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
		fprintf (g_neighbor_log, "# second:millisec\n%d,%d\n", (int)startTime.sec (), (int)(startTime.usec () / 1000));        
       
		// simulating which node
		fprintf (g_position_log, "# node path number simulated (-1 indicates manual control)\n");
		fprintf (g_position_log, "%d\n", g_node_no);

		// format
		fprintf (g_position_log, "\n");
		// fprintf (g_position_log, "# count,curr_sec (per second)\n"); 
		fprintf (g_position_log, "# millisec,\"posX,posY\",elapsed (per step)\n\n");
		fprintf (g_neighbor_log, "\n");
		// fprintf (g_neighbor_log, "# count,curr_sec (per second)\n"); 
		fprintf (g_neighbor_log, "# millisec,\"nodeID,posX,posY\", ... (per step)\n\n");

		fflush (g_position_log); 
		fflush (g_neighbor_log);	
    }
}

void PrintNeighbors (long long curr_msec, Vast::id_t selfID)
{
    // record neighbor position to log if joined
    if (g_state == JOINED)
    {
		//vector<Node *>& neighbors = g_self->getLogicalNeighbors ();
        vector<Node *>& neighbors = g_self->list ();
	
		printf ("Neighbors: ");

        if (g_neighbor_log != NULL)
		    fprintf (g_neighbor_log, "%lld,%llu,", curr_msec, selfID);

		for (size_t i = 0; i < neighbors.size (); i++)
		{
			printf ("[%llu] (%d, %d) ", (neighbors[i]->id), 
					(int)neighbors[i]->aoi.center.x, (int)neighbors[i]->aoi.center.y);

            if (g_neighbor_log != NULL)
            {
			    fprintf (g_neighbor_log, "\"%llu,%d,%d\"", (neighbors[i]->id), 
				    	(int)neighbors[i]->aoi.center.x, (int)neighbors[i]->aoi.center.y);			
                if (i != neighbors.size() - 1)
	    		    fprintf(g_neighbor_log, ",");
            }
		}
		
        printf ("\n");

        if (g_neighbor_log != NULL) 
        {
            fprintf (g_neighbor_log, "\n");
		    fflush (g_neighbor_log);
        }
	}
}

int main (int argc, char *argv[])
{      
    ACE::init ();

    // initialize seed
    //srand ((unsigned int)time (NULL));

    // NOTE: do not use time () as nodes starting concurrently at different sites may have 
    //       very close time () values
    ACE_Time_Value now = ACE_OS::gettimeofday ();
    printf ("Setting random seed as: %d\n", (int)now.usec ());
    srand (now.usec ());
    
    printf ("sizeof sizes:\n");
    printf ("VASTheader: %lu id_t: %lu timestamp_t: %lu length_t: %lu coord_t: %lu\nPosition: %lu Area: %lu IPaddr: %lu Addr: %lu Node: %lu\n\n",
        sizeof (VASTHeader),
        sizeof (Vast::id_t),
        sizeof (timestamp_t),
        sizeof (length_t),
        sizeof (coord_t),
        sizeof (Position),
        sizeof (Area),
        sizeof (IPaddr),
        sizeof (Addr),
        sizeof (Node));

    Position a; Area b; IPaddr c; Addr d; Node e;
    printf ("transfer sizes:\n");
    printf ("VASTheader: %lu coord_t: %lu Position: %lu Area: %lu IPaddr: %lu Addr: %lu Node: %lu\n\n",
        sizeof (VASTHeader),
        sizeof (coord_t),
        a.sizeOf (),
        b.sizeOf (),
        c.sizeOf (),
        d.sizeOf (),
        e.sizeOf ());

    // initialize parameters
    char cmd[255];
    cmd[0] = 0;
    
    for (int i=1; i < argc; i++)
    {        
        strcat (cmd, argv[i]);
        strcat (cmd, " ");
    }

    // store default gateway address
    string str ("127.0.0.1:1037");
    //g_gateway = *VASTVerse::translateAddress (str);
    g_gateway.fromString (str);
    g_gateway.host_id = ((Vast::id_t)g_gateway.publicIP.host << 32) | ((Vast::id_t)g_gateway.publicIP.port << 16) | NET_ID_RELAY;

    // initialize parameters
    SimPara simpara;
    vector<IPaddr> entries;

    bool is_gateway;

    // obtain parameters from command line and/or INI file
    if ((g_node_no = InitPara (VAST_NET_ACE, g_netpara, simpara, cmd, &is_gateway, &g_aoi, &g_gateway, &entries)) == (-1))
        exit (0);

    bool simulate_behavior = (g_node_no > 0);
    
    // make backup of AOI
    g_prev_aoi = g_aoi;

    // create VAST node factory    
    g_world = new VASTVerse (entries, &g_netpara, NULL);
    g_world->createVASTNode (g_gateway.publicIP, g_aoi, VAST_EVENT_LAYER);


    // for simulated behavior, we will use position log to move the nodes
    if (simulate_behavior)
    {
        // create / open position log file
        char filename[80];
        sprintf (filename, VAST_POSFILE_FORMAT, simpara.NODE_SIZE, simpara.WORLD_WIDTH, simpara.WORLD_HEIGHT, simpara.TIME_STEPS);

        FileClassFactory fcf;
        SectionedFile *pos_record = fcf.CreateFileClass (0);
        bool replay = true;
        if (pos_record->open (filename, SFMode_Read) == false)
        {
            replay = false;
            pos_record->open (filename, SFMode_Write);
        }
    
        // create behavior model
        g_movement.initModel (VAST_MOVEMENT_CLUSTER, pos_record, replay, 
                                Position (0,0), Position ((coord_t)simpara.WORLD_WIDTH, (coord_t)simpara.WORLD_HEIGHT),
                                simpara.NODE_SIZE, simpara.TIME_STEPS, simpara.VELOCITY);

        // close position log file
        fcf.DestroyFileClass (pos_record);

        // load initial position
        g_aoi.center = *g_movement.getPos (g_node_no, 0);

        // create logfile to record neighbors at each step
        if (is_gateway == false) 
        {
		    char poslog[] = "position";
		    char neilog[] = "neighbor";
		    g_position_log = LogFileManager::open (poslog);
		    g_neighbor_log = LogFileManager::open (neilog);    
        }
    }

    size_t count_per_sec = 0;
    int num_moves = 0;

    // how much time in millisecond
    size_t time_budget = 1000/FRAMES_PER_SECOND;
    
    long curr_sec = 0;               // current seconds since start
                                     // time of last movement

    ACE_Time_Value last_move = ACE_OS::gettimeofday(); 
    
    // record beginning of main loop    
    while (!g_finished)
    {   
        // record starting time of this cycle
        ACE_Time_Value start = ACE_OS::gettimeofday ();

        // current time in millisecond 
        long long curr_msec = (long long) (start.sec () * 1000 + start.usec () / 1000);

        g_count++;
        count_per_sec++;

        // obtain pointer to self
        Node *self = NULL;
        Vast::id_t id = 0;

        if (g_self != NULL)
        {
            self = g_self->getSelf ();
            id = g_sub_no;
        }

        if (g_state != JOINED)
            checkJoin ();

        else 
        {
#ifdef WIN32
            getInput ();
#endif
            // fix movement at STEPS_PERSEC            
            // elapsed time in microseconds
            long long elapsed = (long long)(start.sec () - last_move.sec ()) * 1000000 + (start.usec () - last_move.usec ());
                                   
            if (simulate_behavior && (elapsed > (1000000 / simpara.STEPS_PERSEC)))
            {    
                //printf ("elapsed time since last move %ld\n", elapsed);
                last_move = start;
               
                g_aoi.center = *g_movement.getPos (g_node_no, num_moves);

                // in simulated mode, we only move TIME_STEPS times
                if (num_moves >= simpara.TIME_STEPS)
                    g_finished = true;

                num_moves++;      
            }
           
            // move only if position changes
            if (!(g_prev_aoi == g_aoi))
            {
                g_prev_aoi = g_aoi;               
                g_self->move (g_sub_no, g_aoi);

                // if I'm not gateway & need to record position
                if (g_position_log != NULL)
                {
                    fprintf (g_position_log, "%lld,\"%llu,%d,%d\",%lld [%lu,%lu]\n", curr_msec, id,
                             (int)self->aoi.center.x, (int)self->aoi.center.y, elapsed,
                             g_world->getSendStat ().total, g_world->getReceiveStat ().total);
                    fflush(g_position_log);
                }

                printf ("[%llu] moves to (%d, %d)\n", id, (int)g_aoi.center.x, (int)g_aoi.center.y); 
            }

        }

        ACE_Time_Value now = ACE_OS::gettimeofday();

        // elapsed time in microseconds
        long long elapsed = (long long)(now.sec () - start.sec ()) * 1000000 + (now.usec () - start.usec ());
              
        // execute tick while obtaining time left
        size_t sleep_time = g_world->tick ((time_budget - (size_t)(elapsed / 1000))) * 1000;
       
        // do per-second things / checks
        // NOTE: we assume this takes little time and does not currently count in as time spent in cycle       
        if (start.sec () > curr_sec)
        {
            curr_sec = (long)start.sec ();
            printf ("%ld s, tick %lu, tick_persec %lu, sleep: %lu us\n", 
                     curr_sec, g_count, count_per_sec, (long) sleep_time);
            count_per_sec = 0;		

            // per second neighbor list
            //long long curr_msec = (long long) (start.sec () * 1000 + start.usec () / 1000);
            
            if (self != NULL)                
                //PrintNeighbors (curr_msec, self->id);
                PrintNeighbors (curr_msec, g_sub_no);

            // just do some per second stat collection stuff
            g_world->tickLogicalClock ();
        } 
        
        if (sleep_time > 0)
        {
            // NOTE the 2nd parameter is specified in microseconds (us) not milliseconds
            ACE_Time_Value duration (0, sleep_time);            
            ACE_OS::sleep (duration); 
        }   
    }

    // depart

    g_self->leave ();
    g_world->destroyVASTNode (g_self);
            
    delete g_world;        

    if (simulate_behavior && !is_gateway) 
    {
		LogFileManager::close (g_position_log);
		LogFileManager::close (g_neighbor_log);
	}

    return 0;
}


