
/*
 *  demo_console    a console-based minimal VAST application 
 *  
 *  version:    2009/06/12      init
 *              2010/09/01      simplfied from test_console (removing all loging capability)
 */


#ifdef WIN32
// disable warning about "unsafe functions"
#pragma warning(disable: 4996)
#endif

#include "ace/ACE.h"    // for sleep functions
#include "ace/OS.h"

#include <stdio.h>

#ifdef WIN32
#include <conio.h>      // for getting keyboard inputs
#endif

#include "Movement.h"

// use VAST for functions
#include "VASTVerse.h"
#include "VASTUtil.h"
#include "VASTsim.h"
#include "VASTCallback.h"       // for creating callback handler

#define VAST_EVENT_LAYER    1                   // layer ID for sending events 
#define VAST_UPDATE_LAYER   2                   // layer ID for sending updates

using namespace Vast;
using namespace std;

#ifdef ACE_DISABLED
#error "ACE needs to be enabled to build demo_console, please modify /common/Config.h"
#endif

// target cycles per second
const int FRAMES_PER_SECOND      = 20;

// number of seconds elasped before a bandwidth usage is reported to gateway
const int REPORT_INTERVAL        = 10;     

// global
Area        g_aoi;              // my AOI (with center as current position)
Area        g_prev_aoi;         // previous AOI (to detect if AOI has changed)
Addr        g_gateway;          // address for gateway
world_t     g_world_id = 0;     // world ID
bool        g_finished = false; // whether we're done for this program execution
NodeState   g_state = ABSENT;   // the join state of this node
char        g_lastcommand = 0;  // last keyboard character typed 
size_t      g_tick = 0;         // # of ticks so far (# of times the main loop has run)
int         g_node_no = (-1);   // which node to simulate (-1 indicates none, manual control)

VASTPara_Net g_netpara;         // network parameters

MovementGenerator g_movement;

// VAST-specific variables
VASTVerse *     g_world = NULL;
VAST *          g_self  = NULL;
Vast::id_t      g_sub_id = 0;        // subscription # for my client (peer)  


// obtain keyboard input, currently only available under Win32
void getInput ()
{

#ifdef WIN32

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
#endif

}

void checkJoin ()
{    
    // create the VAST node
    switch (g_state)
    {
    case ABSENT:
        if ((g_self = g_world->getVASTNode ()) != NULL)
        {   
            g_sub_id = g_self->getSubscriptionID (); 
            g_state = JOINED;
        }
        break;

    default:
        break;
    }
}

// print out current list of observed neighbors
void printNeighbors (unsigned long long curr_msec, Vast::id_t selfID, bool screen_only = false)
{
    if (g_state != JOINED)
        return;
    
    vector<Node *>& neighbors = g_self->list ();
	
	printf ("Neighbors:");
	for (size_t i = 0; i < neighbors.size (); i++)
	{
        if (i % 2 == 0)
            printf ("\n");

		printf ("[%llu] (%d, %d) ", (neighbors[i]->id), 
                                    (int)neighbors[i]->aoi.center.x, 
                                    (int)neighbors[i]->aoi.center.y);            
	}
    printf ("\n");	
}

class StatHandler : public VASTCallback
{
public:
    StatHandler () 
    {
        // # of seconds before reporting to gateway
        _report_countdown = REPORT_INTERVAL;
    }

    bool processMessage (Message &msg)
    {
        // gateway-task
        // right now we only recognize msgtype = 1 (bandwidth report)
        if (msg.msgtype == 1)
        {
            StatType sendstat, recvstat;

            // extract node type
            listsize_t type;
            msg.extract (type);

            // send size
            msg.extract (sendstat);         
            msg.extract (recvstat);

            sendstat.calculateAverage ();
            recvstat.calculateAverage ();

            LogManager::instance ()->writeLogFile ("[%llu] %s send: (%u,%u,%.2f) recv: (%u,%u,%.2f)", msg.from, (type == GATEWAY ? "GATEWAY" : (type == MATCHER ? "MATCHER" : "CLIENT")), 
                                                    sendstat.minimum, sendstat.maximum, sendstat.average, recvstat.minimum, recvstat.maximum, recvstat.average);
            
        }

        // means that we can handle another message
        return true;
    }

    // perform some per-second tasks
    void performPerSecondTasks (timestamp_t now)
    {
        _report_countdown--;
                    
        // check if we should report to gateway bandwidth usage (client-task)
        if (g_self && _report_countdown <= 0)
        {
            _report_countdown = REPORT_INTERVAL;

            // report bandwidth usage stat to gateway
            // message type 1 = bandwidth
            Message msg (1);
            StatType sendstat = g_world->getSendStat (true);
            StatType recvstat = g_world->getReceiveStat (true);
                    
            // determine what type of node am I (NOTE: each time may be different)
            // type 1: origin, 2: matcher, 3: client
            listsize_t self_type = CLIENT;
            if (g_world->isMatcher ())
            {
                self_type = MATCHER;
                if (g_world->isGateway ())
                    self_type = GATEWAY;
            }

            msg.store (self_type);
            msg.store (sendstat);
            msg.store (recvstat);
        
            g_self->report (msg);
        
            // reset stat collection (interval as per second)
            g_world->clearStat ();
        }

        ACE_Time_Value curr_time = ACE_OS::gettimeofday ();
                
        // current time in milliseconds
        unsigned long long curr_msec = (unsigned long long) (curr_time.sec () * 1000 + curr_time.usec () / 1000);
        
        // show current neighbors (even if we have no movmment) debug purpose
        if (g_world->isGateway () == false)
            printNeighbors (curr_msec, g_sub_id, true);                
    }

private:
    int _report_countdown;      // # of seconds before reporting to gateway
};

int main (int argc, char *argv[])
{   
    // 
    // Initialization
    //

    ACE::init ();

    // initialize random seed
    // NOTE: do not use time () as nodes at different sites may have very close time () values
    ACE_Time_Value now = ACE_OS::gettimeofday ();
    printf ("Setting random seed as: %d\n", (int)now.usec ());
    srand (now.usec ());
  
    // combine command line parameters
    char cmd[255];
    cmd[0] = 0;
    
    for (int i=1; i < argc; i++)
    {        
        strcat (cmd, argv[i]);
        strcat (cmd, " ");
    }
    
    // initialize parameters
    SimPara simpara;
    vector<IPaddr> entries;

    bool is_gateway;

    // obtain parameters from command line and/or INI file
    if ((g_node_no = InitPara (VAST_NET_ACE, g_netpara, simpara, cmd, &is_gateway, &g_world_id, &g_aoi, &g_gateway, &entries)) == (-1))
        exit (0);
    
    // force all clients to default join at world 2, unless specified other than 2
    if (!is_gateway && g_world_id == 1)
    {
        g_world_id = 2;
    }

    // if g_node_no is specified, this node will simulate user movements
    bool simulate_move = (g_node_no > 0 && is_gateway == false);
    
    // make backup of AOI, to detect position change so we need to move the client
    g_prev_aoi = g_aoi;
            
    // create VAST node factory and pass in callback
    StatHandler handler;
    g_world = new VASTVerse (entries, &g_netpara, NULL);
    g_world->createVASTNode (g_gateway.publicIP, g_aoi, VAST_EVENT_LAYER, g_world_id, &handler);

    //
    // create simulated movement model
    //

    // for simulated behavior, we will use position log to move the nodes
    if (simulate_move)
    {
        // create a file-based movement model
        g_movement.initModelFromFile (simpara);

        // load initial position
        g_aoi.center = *g_movement.getPos (g_node_no, 0);
    }
    
    //
    // main loop
    //
    
    int num_moves = 0;                                              // which movement

    size_t time_budget = 1000000/FRAMES_PER_SECOND;                 // how much time in microseconds for each frame
    size_t microsec_per_move = (1000000 / (simpara.STEPS_PERSEC));  // microseconds elapsed for one move    

    ACE_Time_Value last_movement = ACE_OS::gettimeofday();  // time of last movement 

    size_t  sleep_time = 0;             // time to sleep (in microseconds)
    int     time_left = 0;              // how much time left for ticking VAST, in microseconds

    size_t tick_per_sec = 0;            // tick count per second


    // entering main loop
    // NOTE we don't necessarily move in every loop (# of loops > # of moves per second)
    while (!g_finished)
    {   
        g_tick++;
        tick_per_sec++;

        // make sure this cycle doesn't exceed the time budget
        TimeMonitor::instance ()->setBudget (time_budget);

        ACE_Time_Value curr_time = ACE_OS::gettimeofday ();

        // elapsed time in microseconds
        long long elapsed = (long long)(curr_time.sec () - last_movement.sec ()) * 1000000 + (curr_time.usec () - last_movement.usec ());

        // if we should move in this frame
        bool to_move = false;
        if (elapsed >= microsec_per_move)
        {
            // re-record last_movement
            last_movement = curr_time;
            to_move = true;
        }

        // perform join check or movement
        if (g_state != JOINED)
        {            
            checkJoin ();
        }
        else 
        {
            // obtain keyboard movement input
            getInput ();
                        
            // check if automatic movement should be performed                                   
            if (simulate_move && to_move)
            {                                            
                g_aoi.center = *g_movement.getPos (g_node_no, num_moves++);
                
                // in simulated mode, we only move TIME_STEPS times
                if (num_moves > simpara.TIME_STEPS)
                    g_finished = true;
            }
           
            // perform movements, but move only if position changes
            if (!(g_prev_aoi == g_aoi))
            {
                g_prev_aoi = g_aoi;               
                g_self->move (g_sub_id, g_aoi);

                // print out movement 
                printf ("[%llu] moves to (%d, %d)\n", g_sub_id, (int)g_aoi.center.x, (int)g_aoi.center.y);
            }
        }       
    
        // whether per-sec tasks were performed
        bool per_sec = false;

        // execute tick while obtaining time left, sleep out the remaining time
        time_left = TimeMonitor::instance ()->available ();
        sleep_time = g_world->tick (time_left, &per_sec);

        // show message to indicate liveness
        if (per_sec)
        {            
            printf ("%ld s, tick %lu, tick_persec %lu, sleep: %lu us, time_left: %d\n", (long)curr_time.sec (), g_tick, tick_per_sec, sleep_time, time_left);
            tick_per_sec = 0;
        }

        if (sleep_time > 0)
        {
            // NOTE the 2nd parameter is specified in microseconds (us) not milliseconds
            ACE_Time_Value duration (0, sleep_time);            
            ACE_OS::sleep (duration); 
        }
    }

    //
    // depart & clean up
    //

    g_self->leave ();
    g_world->tick ();
    
    g_world->destroyVASTNode (g_self);
            
    delete g_world;        

    return 0;
}
