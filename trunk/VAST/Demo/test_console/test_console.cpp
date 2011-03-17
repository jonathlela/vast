
/*
 *  test_console    a VAST experimental node
 *  
 *  version:    2010/06/15  init - adopted from demo_console
 *              2010/07/12  added bandwidth stat reporting to gateway
 *              2010/07/28  randomize the path to move
 *              2010/08/04  print joining and joined time in position.log
 */

//#include <vld.h>            // visual leak detector (NOTE: must download & install from
                            // http://www.codeproject.com/KB/applications/visualleakdetector.aspx


#ifdef WIN32
// disable warning about "unsafe functions"
#pragma warning(disable: 4996)
#endif

#include "ace/ACE.h"    // for sleep functions
//#include "ace/OS.h"

#include "ace/OS_NS_unistd.h"       // ACE_OS::sleeps
#include "ace/OS_NS_sys_time.h"     // gettimeofday


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

// number of seconds elasped before a bandwidth usage is reported to gateway
const int REPORT_INTERVAL        = 10;     

// global
Area        g_aoi;              // my AOI (with center as current position)
Area        g_prev_aoi;         // previous AOI (to detect if AOI has changed)
world_t     g_world_id = 0;     // world ID
bool        g_finished = false; // whether we're done for this program execution
NodeState   g_state = ABSENT;   // the join state of this node
char        g_lastcommand = 0;  // last keyboard character typed 
size_t      g_count = 0;        // # of ticks so far (# of times the main loop has run)
int         g_node_no = (-1);   // which node to simulate (-1 indicates none, manual control)
unsigned long long   joining_msec;       // record the time for begining to join  by lee
unsigned long long   joined_msec;        // record the time for joined  by lee
unsigned long long   leave_msec;

VASTPara_Net g_netpara (VAST_NET_ACE);   // network parameters

MovementGenerator g_movement;
FILE       *g_position_log = NULL;     // logfile for node positions
FILE	   *g_neighbor_log = NULL;	   // logfile for node neighbors
FILE	   *g_message_log  = NULL;	   // logfile for gateway

// VAST-specific variables
VASTVerse *     g_world = NULL;
VAST *          g_self  = NULL;
Vast::id_t      g_sub_no = 0;        // subscription # for my client (peer)  

// Node types
    char NODE_TYPE[][20] = 
    {
        "UNKNOWN",
        "GATEWAY",
        "RELAY_MATCHER",
        "MATCHER",
        "RELAY_CLIENT",
        "CLIENT"
    };


#ifdef WIN32
void getInput ()
{
    // get input
    while (kbhit ())
    {
        char c = (char)getch ();

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

// record the time this node joins
bool recordJoinTime (FILE *fp, Vast::id_t nodeID)
{
    if (fp == NULL || nodeID == 0)
        return false;

	// record join time
	time_t rawtime;          
	time (&rawtime);

	tm *timeinfo = gmtime (&rawtime);
    
	ACE_Time_Value startTime = ACE_OS::gettimeofday();
	fprintf (fp, "# Node joined, Log starts\n\n");

	// node ID
	fprintf (fp, "# node ID\n");
	fprintf (fp, "%llu\n", nodeID);
	
	fprintf (fp, "# Start date/time\n"); 
	fprintf (fp, "# %s", asctime (timeinfo)); 
	fprintf (fp, "# GMT (hour:min:sec)\n%2d,%02d,%02d\n", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
	fprintf (fp, "# second:millisec\n%d,%d\n", (int)startTime.sec (), (int)(startTime.usec () / 1000));      
    fflush (fp);

    return true;
}

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
            // record the time joined   by lee
            // current time in milliseconds
            
            //ACE_Time_Value joined_time = ACE_OS::gettimeofday ();
            //joined_msec = (unsigned long long) (joined_time.sec () * 1000 + joined_time.usec () / 1000);              
            joined_msec = g_world->getTimestamp ();
        }
        break;

    default:
        break;
    }

    // do things immediately when join is successful
    if (g_state == JOINED)
    {
        Node *self;
        Vast::id_t nodeID;

        self = g_self->getSelf ();
        nodeID = g_self->getSubscriptionID ();

        recordJoinTime (g_position_log, nodeID);
        recordJoinTime (g_neighbor_log, nodeID);
        recordJoinTime (g_message_log, nodeID);
       
        if (g_position_log)
        {
            // simulating which node
            fprintf (g_position_log, "# node path number simulated (-1 indicates manual control)\n");
            fprintf (g_position_log, "%d\n", g_node_no);
        
            // format
            fprintf (g_position_log, "\n");
            fprintf (g_position_log, "# millisec,\"posX,posY\",elapsed (per step)\n\n");

            // joinining & joined time
            fprintf (g_position_log, "%llu,\"%llu,joining\",%d\n",joining_msec,nodeID,g_node_no);
            fprintf (g_position_log, "%llu,\"%llu,joined\",%d\n",joined_msec,nodeID,g_node_no);

            fflush (g_position_log);
        }

        if (g_neighbor_log)
        {
            // format
            fprintf (g_neighbor_log, "\n");
            fprintf (g_neighbor_log, "# millisec,\"nodeID,posX,posY\", ... (per step)\n\n");		

            // join & joining time
            fprintf(g_neighbor_log, "%llu,\"%llu,joining\"\n",joining_msec,nodeID);
            fprintf(g_neighbor_log, "%llu,\"%llu,joined\"\n",joined_msec,nodeID);

            fflush(g_neighbor_log);
        }
    }
}

// print out current list of observed neighbors
void printNeighbors (unsigned long long curr_msec, Vast::id_t selfID, bool screen_only = false)
{
    // record neighbor position to log if joined
    if (g_state == JOINED)
    {
		//vector<Node *>& neighbors = g_self->getLogicalNeighbors ();
        vector<Node *>& neighbors = g_self->list ();
	
		printf ("Neighbors:");

        if (g_neighbor_log != NULL && screen_only == false)
		    fprintf (g_neighbor_log, "%llu,%llu,", curr_msec, selfID);

		for (size_t i = 0; i < neighbors.size (); i++)
		{
            if (i%2 == 0)
                printf ("\n");

			printf ("[%llu] (%d, %d) ", (neighbors[i]->id), 
					(int)neighbors[i]->aoi.center.x, (int)neighbors[i]->aoi.center.y);
            
            if (g_neighbor_log && screen_only == false)
            {
			    fprintf (g_neighbor_log, "\"%llu,%d,%d\"", (neighbors[i]->id), 
				    	(int)neighbors[i]->aoi.center.x, (int)neighbors[i]->aoi.center.y);			

                if (i != neighbors.size() - 1)
	    		    fprintf(g_neighbor_log, ",");
            }
		}
		
        printf ("\n");

        if (g_neighbor_log && screen_only == false)
        {
            fprintf (g_neighbor_log, "\n");
		    fflush (g_neighbor_log);
        }
	}
}

void printSizes ()
{    
    printf ("sizeof sizes:\n");
    printf ("bool: %u, VASTheader: %u id_t: %u timestamp_t: %u length_t: %u coord_t: %u\nPosition: %u Area: %u IPaddr: %u Addr: %u Node: %u\n\n",
        sizeof (bool),
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
    printf ("VASTheader: %u coord_t: %u Position: %u Area: %u IPaddr: %u Addr: %u Node: %u\n\n",
        sizeof (VASTHeader),
        sizeof (coord_t),
        a.sizeOf (),
        b.sizeOf (),
        c.sizeOf (),
        d.sizeOf (),
        e.sizeOf ());
}

int main (int argc, char *argv[])
{   
    // 
    // Initialization
    //

    // print out size of different data types, useful for debug transmit sizes
    //printSizes ();

    //ACE::init ();

    // initialize random seed

    // NOTE: do not use time () as nodes at different sites may have very close time () values
    ACE_Time_Value now = ACE_OS::gettimeofday ();
    printf ("Setting random seed as: %d\n", (int)now.usec ());
    srand (now.usec ());    
 
    // initialize parameters
    char cmd[255];
    cmd[0] = 0;
    
    for (int i=1; i < argc; i++)
    {        
        strcat (cmd, argv[i]);
        strcat (cmd, " ");
    }
    
    // store default gateway address
    char GWstr[80];
    sprintf (GWstr, "127.0.0.1:1037");

    // initialize parameters
    SimPara simpara;

    bool is_gateway;

    // # of seconds to pause before joining, default is no pause
    int interval = 0;

    // obtain parameters from command line and/or INI file
    if ((g_node_no = InitPara (VAST_NET_ACE, g_netpara, simpara, cmd, &is_gateway, &g_world_id, &g_aoi, GWstr, &interval)) == (-1))
        exit (0);

    // randomize selected join path  by lee
   /* if (g_node_no != 0 && !is_gateway)
    {
        g_node_no = rand() % (simpara.NODE_SIZE-1)+1;    
    }*/
    
    // force all clients to default join at world 2, unless specified other than 2
    if (!is_gateway && g_world_id == 1)
    {
        g_world_id = 2;
    }

    // if g_node_no is specified, then this node will simulate a client movement
    bool simulate_behavior = (g_node_no > 0);
    
    // make backup of AOI, so we can detect whehter position has changed and we need to move the client
    g_prev_aoi = g_aoi;

    // process interval
    
    // sleep a little to let nodes move at different time  by lee
    ACE_Time_Value tv (0, 1000000 * interval * g_node_no);
    printf ("interval to pause is: %d seconds\n", interval * g_node_no); 
    
    ACE_OS::sleep (tv);
    
    // create VAST node factory
    g_world = new VASTVerse (is_gateway, GWstr, &g_netpara, NULL, NULL, 40);
    g_world->createVASTNode (g_world_id, g_aoi, VAST_EVENT_LAYER);

    // record "begin to join" in position.log  by lee
    // current time in milliseconds
    
    ACE_Time_Value joining_time = ACE_OS::gettimeofday ();
    joining_msec = (unsigned long long) (joining_time.sec () * 1000 + joining_time.usec () / 1000);
    
    // NOTE: g_world likely is not initialized yet (to provide time)
    //joining_msec = g_world->getTimestamp ();
        
    //
    // open logs
    //

    // for simulated behavior, we will use position log to move the nodes
    if (simulate_behavior && is_gateway == false)
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
		char poslog[] = "position";
		char neilog[] = "neighbor";
		g_position_log = LogManager::open (poslog);
		g_neighbor_log = LogManager::open (neilog);    
    }
    
    // open message log (both gateway & client)
    if (is_gateway)
        g_message_log = LogManager::open ("gateway", "stat");
    //else
        //g_message_log = LogManager::open ("client", "stat");

    //LogManager::instance ()->setLogFile (g_message_log);

    //
    // main loop
    //
    
    size_t tick_per_sec = 0;                                        // tick count per second
    int seconds_to_report = REPORT_INTERVAL;                        // # of seconds before reporting to gateway
    int num_moves = 0;                                              // which movement
    size_t time_budget = 1000000/FRAMES_PER_SECOND;                 // how much time in microseconds for each frame
    size_t ms_per_move = (1000000 / (simpara.STEPS_PERSEC));        // microseconds elapsed for one move

    long curr_sec = 0;                                      // current seconds since start

    ACE_Time_Value last_movement = ACE_OS::gettimeofday();  // time of last movement 
    
    map<Vast::id_t, long long> last_update;                 // last update time for a neighbor
    size_t sleep_time = 0;                                  // time to sleep (in microseconds)
    int    time_left = 0;                                   // how much time left for ticking VAST, in microseconds

    // entering main loop
    // NOTE we don't necessarily move in every loop (# of loops > # of moves per second)
    while (!g_finished)
    {   
        g_count++;
        tick_per_sec++;

        TimeMonitor::instance ()->setBudget (time_budget);

        ACE_Time_Value curr_time = ACE_OS::gettimeofday ();

        // current time in milliseconds
        unsigned long long curr_msec = (unsigned long long) (curr_time.sec () * 1000 + curr_time.usec () / 1000);

        // check whether we need to perform per-second task in this cycle
        bool persec_task = false;
        if (curr_time.sec () > curr_sec)
        {
            curr_sec = (long)curr_time.sec ();
            persec_task = true;
        }

        // elapsed time in microseconds
        long long elapsed = (long long)(curr_time.sec () - last_movement.sec ()) * 1000000 + (curr_time.usec () - last_movement.usec ());

        // if we should move in this frame
        bool to_move = false;
        if (elapsed >= ms_per_move)
        {
            // re-record last_movement
            last_movement = curr_time;
            to_move = true;
        }

        // obtain pointer to self
        Node *self = NULL;
        Vast::id_t id = 0;

        if (g_self != NULL)
        {
            self = g_self->getSelf ();
            id = g_sub_no;
        }

        // determine what type of node am I (NOTE: each time may be different)
        // type 1: origin, 2: matcher, 3: client
        listsize_t self_type = CLIENT;
        if (g_world->isMatcher ())
        {
            self_type = MATCHER;
            if (g_world->isGateway ())
                self_type = GATEWAY;
            else if (g_self && g_self->isRelay () > 0)
                self_type = RELAY_MATCHER;
        }
        else if (g_self && g_self->isRelay () > 0)
        {
            self_type = RELAY_CLIENT;
        } 


        // perform join check or movement
        if (g_state != JOINED)
        {            
            checkJoin ();
        }
        else 
        {
            // generate movement (either from input or from movement model, for simulated behavior)
#ifdef WIN32
            getInput ();
#endif
                        
            // check if automatic movement should be performed                                   
            if (simulate_behavior && to_move)
            {                                            
                g_aoi.center = *g_movement.getPos (g_node_no, num_moves);
                
                // in simulated mode, we only move TIME_STEPS times
                if (num_moves >= simpara.TIME_STEPS)
                    g_finished = true;

                num_moves++;      
            }
           
            // perform movements, but move only if position changes
            if (!(g_prev_aoi == g_aoi))
            {
                g_prev_aoi = g_aoi;               
                g_self->move (g_sub_no, g_aoi);

                // if I'm not gateway & need to record position
                if (g_position_log != NULL)
                {
                    fprintf (g_position_log, "%llu,\"%llu,%d,%d\",%lld,%s,[%lu,%lu]\n", 
                             curr_msec, 
                             id,
                             (int)self->aoi.center.x, (int)self->aoi.center.y, 
                             elapsed, NODE_TYPE[self_type],
                             g_world->getSendStat ().total, g_world->getReceiveStat ().total);
                    fflush (g_position_log);
                }

                //print neighbor by lee
                if (g_neighbor_log != NULL)                
                    printNeighbors (curr_msec, g_sub_no);

                // print out movement once per second
                if (num_moves % simpara.STEPS_PERSEC == 0)
                    printf ("[%llu] moves to (%d, %d)\n", id, (int)g_aoi.center.x, (int)g_aoi.center.y);
            }

            // process input messages, if any
            Message *msg;

            while ((msg = g_self->receive ()) != NULL)
            {
                // right now we only recognize msgtype = 1 (bandwidth report)
                if (msg->msgtype == 1)
                {
                    StatType sendstat, recvstat;
                    listsize_t client_size = 0;

                    // extract node type
                    listsize_t type;
                    msg->extract (type);

                    // send size
                    msg->extract (sendstat);         
                    msg->extract (recvstat);

                    // extract # of clients connected
                    msg->extract (client_size);

                    sendstat.calculateAverage ();
                    recvstat.calculateAverage ();

                    LogManager::instance ()->writeLogFile ("[%llu] %s clients: %u send: (%u,%u,%.2f) recv: (%u,%u,%.2f)", 
                                                            msg->from, NODE_TYPE[type], client_size, 
                                                            sendstat.minimum, sendstat.maximum, sendstat.average, 
                                                            recvstat.minimum, recvstat.maximum, recvstat.average);

                    // record last update time for this node
                    last_update[msg->from] = curr_msec;
                }                
            }
        }
       
        // do other per-second things / checks                        
        if (persec_task)
        {            
            seconds_to_report--;
                        
            // do some per second stat collection stuff
            g_world->tickLogicalClock ();

            // check if we should report to gateway bandwidth usage
            if (g_self && seconds_to_report <= 0)
            {
                seconds_to_report = REPORT_INTERVAL;
                
                listsize_t client_size = (listsize_t)g_self->isRelay ();
                printf ("reporting to gateway, client_size: %u\n", client_size);

                // report bandwidth usage stat to gateway
                // message type 1 = bandwidth
                Message msg (1);
                StatType sendstat = g_world->getSendStat (true);
                StatType recvstat = g_world->getReceiveStat (true);
                        
                msg.store (self_type);
                msg.store (sendstat);
                msg.store (recvstat);
                msg.store (client_size);
            
                g_self->reportGateway (msg);
            
                // reset stat collection (interval as per second)
                g_world->clearStat ();
            }

            // remove obsolete entries in last_update (if I'm gateway), 
            // this keeps the concurrent count accurate
            if (is_gateway)
            {
                map<Vast::id_t, long long>::iterator it = last_update.begin ();
                vector<Vast::id_t> remove_list;
                for ( ; it != last_update.end (); it++)
                {
                    if ((curr_msec - it->second) > (REPORT_INTERVAL * 1000 * 1.5))
                        remove_list.push_back (it->first);
                }

                for (size_t i=0; i < remove_list.size (); i++)
                    last_update.erase (remove_list[i]);

                LogManager::instance ()->writeLogFile ("GW-STAT: %lu concurrent at %llu\n", last_update.size (), curr_msec);
            }

            // show current neighbors (even if we have no movmment) debug purpose
            if (self != NULL && is_gateway == false)                
                printNeighbors (curr_msec, g_sub_no, true);

            // show message to indicate liveness
            printf ("%ld s, tick %lu, tick_persec %lu, last_sleep: %lu us, time_left: %d\n", curr_sec, g_count, tick_per_sec, sleep_time, time_left);
            tick_per_sec = 0;
        }
    
        // execute tick while obtaining time left, sleep out the remaining time
        time_left = TimeMonitor::instance ()->available ();
        sleep_time = g_world->tick (time_left);

        if (sleep_time > 0)
        {
            // NOTE the 2nd parameter is specified in microseconds (us) not milliseconds
            ACE_Time_Value duration (0, sleep_time);            
            ACE_OS::sleep (duration); 
        }
    }

    //
    // depart & clean up
    // current time in milliseconds
    ACE_Time_Value leave_time = ACE_OS::gettimeofday ();
    leave_msec = (unsigned long long) (leave_time.sec () * 1000 + leave_time.usec () / 1000);
    if(g_position_log != NULL)
        {
            
            fprintf(g_position_log, 
                "%llu,\"%llu,leave\"\n",leave_msec,g_sub_no);
            fflush(g_position_log);
        }
      
        if(g_neighbor_log != NULL)
        {
            
            fprintf(g_neighbor_log, 
                "%llu,\"%llu,leave\"\n",leave_msec,g_sub_no);
            fflush(g_neighbor_log); 
        }

    g_self->leave ();
    g_world->tick ();

    // sleep a little to let LEAVE message be sent out
    ACE_Time_Value tv2 (0, 2000000); 
    ACE_OS::sleep (tv2);              
    
    g_world->destroyVASTNode (g_self);
            
    delete g_world;        

    if (g_position_log) 
    {
        LogManager::close (g_position_log);
        g_position_log = NULL;
    }

    if (g_neighbor_log)
    {
        LogManager::close (g_neighbor_log);
        g_neighbor_log = NULL;
    }

    if (g_message_log)
    {
        LogManager::instance ()->unsetLogFile ();
		LogManager::close (g_message_log);
        g_message_log = NULL;
    }

    LogManager::terminateInstance ();

    //ACE::fini ();

    return 0;
}
