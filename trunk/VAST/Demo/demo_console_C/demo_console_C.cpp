
/*
 *  demo_console_C    a VAST test console for C wrapper
 *  
 *  version:    2010/06/15  init - adopted from demo_console
 *
 */

#ifdef WIN32
// disable warning about "unsafe functions"
#pragma warning(disable: 4996)
#endif

#include "ace/ACE.h"
//#include "ace/OS.h"
#include "ace/OS_NS_unistd.h"       // ACE_OS::sleeps
#include "ace/OS_NS_sys_time.h"     // gettimeofday



// for getting keyboard inputs
#ifdef WIN32
#include <windows.h>
#include <conio.h>
#endif

#include <stdio.h>

#include "VASTWrapperC.h"

bool g_finished = false;

int   g_world_id = 1;
float g_x = 100;
float g_y = 100;
int   g_radius = 200;
char  g_lastcommand = 0;

uint16_t    g_port = 1037;
bool        g_is_gateway = false;
char        g_GWstr[80];


#ifdef WIN32

void Speak ()
{
    char str[80];
    sprintf (str, "Hello World! %d", rand ());
    printf ("publish: %s\n", str);
    VASTPublish (str, strlen (str), 0);

    /*
    // To say something mighty :)

#define BUF_SIZE 1000000

    char str[BUF_SIZE]; 
    memset (str, 'A', BUF_SIZE-1);
    str[BUF_SIZE-1] = 0;

    VASTPublish (str, BUF_SIZE, 0);   
    */
}

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

        // speak a sentence
        case 's':
            Speak ();
            break;

        // movements
        case -32:
            switch (getch ())
            {
            // LEFT
            case 75:
                g_x -= 5;
                break;
            // RIGHT
            case 77:
                g_x += 5;
                break;                            
            // UP
            case 72:
                g_y -= 5;
                break;
            // DOWN
            case 80:
                g_y += 5;
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


void Init ()
{    
            
    // if physical coordinate is not supplied, VAST will need to obtain it itself
    //g_vastnetpara.phys_coord = g_aoi.center;    

    // translate gateway string to Addr object
    InitVAST (g_is_gateway, g_GWstr);

    g_x = (float)(rand () % 100);
    g_y = (float)(rand () % 100);

    VASTJoin (g_world_id, g_x, g_y, g_radius);
}

void Loop ()
{
    size_t tick_count = 0;

    while (tick_count <= 50)
    {
        tick_count++;

        // perform tick and process for as long as necessary
        VASTTick (0);
    
        getInput ();
    
        VASTMove (g_x, g_y);
    
        // check for any received message & print it out
        const char *msg = NULL;
        size_t size;
        uint64_t from;
    
        while ((msg = VASTReceive (&from, &size)) != NULL)
        {
            printf ("received from: %llu size: %u msg: %s\n", from, size, msg);
        }
    
        // sleep a little
        // NOTE the 2nd parameter is specified in microseconds (us) not milliseconds
        ACE_Time_Value duration (0, 100000);            
        ACE_OS::sleep (duration); 
    }
}

void Shutdown ()
{
    VASTLeave ();
    ShutVAST ();
}

int main (int argc, char *argv[])
{
    // read command line parameters
    g_GWstr[0] = 0;

    // get default port
    if (argc >= 2)
    {
        g_port = (unsigned short)atoi (argv[1]);        
    }
    
    // get gateway IP
    if (argc >= 3)
    {
        if (argv[2][0] != '0')
            sprintf (g_GWstr, "%s:%d", argv[2], g_port);
    }

    // default gateway set to localhost
    if (g_GWstr[0] == 0)
    {
        g_is_gateway = true;        
        sprintf (g_GWstr, "127.0.0.1:%d", g_port);
    }    

    while (!g_finished)
    {
        Init ();

        Loop ();
        
        // we don't shutdown gateway
        //if (!g_is_gateway)
        //    Shutdown ();
    }

    Shutdown ();
   
    return 0;
}


