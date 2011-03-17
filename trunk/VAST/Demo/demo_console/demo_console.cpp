
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
//#include "ace/OS.h"
#include "ace/OS_NS_unistd.h"       // ACE_OS::sleeps
#include "ace/OS_NS_sys_time.h"     // gettimeofday

#include <stdio.h>

#ifdef WIN32
#include <conio.h>      // for getting keyboard inputs
#endif

#include "Movement.h"

// use VAST for functions
#include "VASTVerse.h"
#include "VASTUtil.h"
#include "VASTCallback.h"       // for creating callback handler

#define VAST_EVENT_LAYER    1                   // layer ID for sending events 
#define VAST_UPDATE_LAYER   2                   // layer ID for sending updates

// user-defined message types
#define DEMO_MSGTYPE_BANDWIDTH_REPORT   1
#define DEMO_MSGTYPE_KEYPRESS           2

using namespace Vast;
using namespace std;

#ifdef ACE_DISABLED
#error "ACE needs to be enabled to build demo_console, please modify /common/Config.h"
#endif


// number of seconds elasped before a bandwidth usage is reported to gateway
const int REPORT_INTERVAL        = 10;     

// global
Area        g_aoi;              // my AOI (with center as current position)
Area        g_prev_aoi;         // previous AOI (to detect if AOI has changed)
world_t     g_world_id = 0;     // world ID
bool        g_finished = false; // whether we're done for this program execution
char        g_lastcommand = 0;  // last keyboard character typed 

// VAST-specific variables
VASTVerse *     g_world = NULL;     // factory for creating a VAST node
VAST *          g_self  = NULL;     // pointer to VAST
Vast::id_t      g_sub_id = 0;       // subscription # for my client (peer)  

// socket-specific variables
Vast::id_t      g_socket_id = NET_ID_UNASSIGNED;    // socket ID

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

        // join matcher
        case 'j':
            // ESSENTIAL: must specify which world to join
            g_world->createVASTNode (g_world_id, g_aoi, VAST_EVENT_LAYER);
            break;

        // leave matcher
        case 'l':
            // ESSENTIAL: before we leave must clean up resources
            g_world->destroyVASTNode (g_self);
            break;

        // send a socket message
        case 's':
            {
                if (g_socket_id == NET_ID_UNASSIGNED)
                {
                    // store gateway's IP & port for later use (make socket connection)
                    IPaddr gateway = g_world->getGateway ();

                    g_socket_id = g_world->openSocket (gateway);
                    printf ("obtain socket_id: [%llu]\n", g_socket_id);
                }

                if (g_socket_id != NET_ID_UNASSIGNED)
                {
                    char teststr[] = "hello world!\0";
                    g_world->sendSocket (g_socket_id, teststr, strlen (teststr)+1);
                }
            }
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


// print out current list of observed neighbors
void printNeighbors (unsigned long long curr_msec, Vast::id_t selfID, bool screen_only = false)
{
    // if we havn't joined successfully
    if (g_self == NULL)
        return;
    
    vector<Node *>& neighbors = g_self->list ();
	
	printf ("Neighbors:");
	for (size_t i = 0; i < neighbors.size (); i++)
	{
        if (i % 2 == 0)
            printf ("\n");

		printf ("[%llu] (%d, %d) ", (neighbors[i]->id), (int)neighbors[i]->aoi.center.x, (int)neighbors[i]->aoi.center.y);            
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

    // process incoming messages
    bool processMessage (Message &msg)
    {
        // gateway-task
        // right now we only recognize msgtype = 1 (bandwidth report)
        if (msg.msgtype == DEMO_MSGTYPE_BANDWIDTH_REPORT)
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

            LogManager::instance ()->writeLogFile ("[%llu] %s send: (%u,%u,%.2f) recv: (%u,%u,%.2f)", msg.from, (type == GATEWAY ? "GATEWAY" : (type == MATCHER ? "MATCHER" : "CLIENT")), sendstat.minimum, sendstat.maximum, sendstat.average, recvstat.minimum, recvstat.maximum, recvstat.average);            
        }

        // origin matcher's keypress
        else if (msg.msgtype == DEMO_MSGTYPE_KEYPRESS)
        {
            char str[80];

            // specify '0' as size to indicate we don't know the size
            size_t size = msg.extract (str, 0);
            str[size] = 0;

            printf ("ORIGIN_MATCHER receives: %s\n", str);
        }

        // means that we can handle another message
        return true;
    }

    // process a plain socket message from the network by current node, 
    // returns whether we can process more
    bool processSocketMessage (id_t socket, const char *msg, size_t size)
    {
        printf ("from [%llu] size: %d msg: %s\n", socket, size, msg);

        /*
        // send back acknowledgment if not the same message
        char teststr[] = "hello world back!\0";
        if (strlen (teststr) + 1 != size)
        {
            g_world->sendSocket (socket, teststr, strlen (teststr)+1);
        }
        */

        // re-direct client message to server
        if (_clients.find (socket) != _clients.end ())
        {
            g_world->sendSocket (_clients[socket], msg, size);
        }
        // re-direct server message to its client
        else if (_servers.find (socket) != _servers.end ())
        {
            g_world->sendSocket (_servers[socket], msg, size);
        }
        // otherwise it's a new client, establish server for it & check authentication
        else 
        {
            // establish connection to server
            id_t server_socket_id = g_world->openSocket (_socket_server);

            // establish client <-> server mapping
            _clients[socket] = server_socket_id;
            _servers[server_socket_id] = socket;

            // forward 1st message to server (to authenticate)
            // TODO: check if we need to wait first (for socket connection to establish)
            g_world->sendSocket (server_socket_id, msg, size);
        }

        return true;
    }

    // perform some per-second tasks
    void performPerSecondTasks (timestamp_t now)
    {
        _report_countdown--;
                    
        // client-task
        // check if we should report to gateway bandwidth usage 
        if (g_self && _report_countdown <= 0)
        {
            _report_countdown = REPORT_INTERVAL;

            // report bandwidth usage stat to gateway
            // message type 1 = bandwidth
            Message msg (DEMO_MSGTYPE_BANDWIDTH_REPORT);
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
        
            g_self->reportGateway (msg);
        
            // reset stat collection (interval as per second)
            g_world->clearStat ();
        }
                        
        // show current neighbors (even if we have no movmment) debug purpose
        if (g_world->isGateway () == false)
            printNeighbors (now, g_sub_id, true);                
    }

    // perform some per-tick tasks
    void performPerTickTasks ()
    {
        // obtain keyboard movement input
        getInput ();

        if (g_self == NULL)
            return;
        
        // perform movements if position changes (due to input or simulated movement)
        // NOTE we don't necessarily move in every tick (# of ticks > # of moves per second)
        if (!(g_prev_aoi == g_aoi))
        {
            g_prev_aoi = g_aoi;               
            g_self->move (g_sub_id, g_aoi);
    
            // print out movement 
            printf ("[%llu] moves to (%d, %d)\n", g_sub_id, (int)g_aoi.center.x, (int)g_aoi.center.y);
        }

        // send keypress to origin matcher
        if (g_lastcommand != 0)
        {
            if (isalpha (g_lastcommand))
            {
                // build message string
                char str[80];
                sprintf (str, "[%llu] has pressed '%c'\0", g_sub_id, g_lastcommand);
    
                Message msg (DEMO_MSGTYPE_KEYPRESS);
                msg.store (str, strlen (str), true);
    
                // send to origin matcher
                g_self->reportOrigin (msg);
            }
    
            g_lastcommand = 0;
        }    
    }

    // notify of successful connection with gateway
    void gatewayConnected (id_t host_id)
    {
        printf ("gatewayConnected () hostID: [%llu]\n", host_id);
    }

    // notify of successful connection with gateway
    void gatewayDisconnected ()
    {
        printf ("gatewayDisonnected ()\n");
    }

    // notify the successful join of the VAST node
    void nodeJoined (VAST *vastnode)
    {
        g_self = vastnode;

        // obtain subscription ID, so that movement can be correct
        g_sub_id = g_self->getSubscriptionID ();

        printf ("nodeJoined () id: [%llu]\n", g_sub_id);
    }

    void nodeLeft ()
    {
        printf ("nodeLeft () id: [%llu]\n", g_sub_id);
        g_self = NULL;
        g_sub_id = 0;
    }

private:
    int _report_countdown;      // # of seconds before reporting to gateway
    
    IPaddr          _socket_server; // IP address of socket server
    map<id_t, id_t> _clients;    // record of active clients
    map<id_t, id_t> _servers;    // record of active servers
};

int main (int argc, char *argv[])
{   
    // 
    // Initialization
    //
    
    // for using ACE (get current time in platform-independent way)
    ACE::init ();

    // initialize random seed
    // NOTE: do not use time () as nodes at different sites may have very close time () values
    ACE_Time_Value now = ACE_OS::gettimeofday ();
    printf ("Setting random seed as: %d\n", (int)now.usec ());
    srand (now.usec ());
                      
    //
    // set default values
    //
    g_world_id     = VAST_DEFAULT_WORLD_ID;

    g_aoi.center.x = (coord_t)(rand () % 100);
    g_aoi.center.y = (coord_t)(rand () % 100);
    g_aoi.radius   = 200;

    // make backup of AOI, to detect position change so we can move the client
    g_prev_aoi = g_aoi;

    // set network parameters
    VASTPara_Net netpara (VAST_NET_ACE);
    netpara.port = GATEWAY_DEFAULT_PORT;

    //
    // load command line parameters, if any
    //
    char GWstr[80];     // gateway string
    GWstr[0] = 0;

    // port
    if (argc > 1)        
        netpara.port = (unsigned short)atoi (argv[1]);
    
    // gateway's IP 
    if (argc > 2)        
        sprintf (GWstr, "%s:%d", argv[2], netpara.port);

    // world id
    if (argc > 3)
        g_world_id = (world_t)atoi (argv[3]);

    //
    // setup gateway
    //

    bool is_gateway = false;

    // default gateway set to localhost
    if (GWstr[0] == 0)
    {
        is_gateway = true;
        netpara.is_entry = true;
        sprintf (GWstr, "127.0.0.1:%d", netpara.port);
    }
    
    // force all clients to default join at world 2, unless specified other than 2
    if (!is_gateway && g_world_id == VAST_DEFAULT_WORLD_ID)
    {
        g_world_id = VAST_DEFAULT_WORLD_ID + 1;
    }
                
    // ESSENTIAL: create VAST node factory and pass in callback
    StatHandler handler;
    g_world = new VASTVerse (is_gateway, string (GWstr), &netpara, NULL, &handler, 20);

    //
    // main loop (do nothing, or can be a windows loop)
    //
    while (!g_finished)
    {
        // NOTE the 2nd parameter is specified in microseconds (us) not milliseconds
        ACE_Time_Value duration (1, 0);            
        ACE_OS::sleep (duration); 
    }
    
    //
    // depart & clean up
    //

    delete g_world;        

    return 0;
}
