
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
#include "ace/OS.h"


// for getting keyboard inputs
#ifdef WIN32
#include <windows.h>
#include <conio.h>
#endif

#include <stdio.h>

#include "VASTWrapperC.h"

bool g_finished = false;

float g_x = 100;
float g_y = 100;
int   g_radius = 200;
char  g_lastcommand = 0;

#ifdef WIN32

void Speak ()
{
    char str[]="Hello World!\0";

    VASTPublish (str, strlen (str), 0);
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

int main (int argc, char *argv[])
{
    uint16_t port = 1037;
    bool is_gateway = false;

    char GWstr[80];
    GWstr[0] = 0;

    // get default port
    if (argc >= 2)
    {
        port = (unsigned short)atoi (argv[1]);        
    }
    
    // get gateway IP
    if (argc >= 3)
    {
        if (argv[2][0] != '0')
            sprintf (GWstr, "%s:%d", argv[2], port);
    }

    // default gateway set to localhost
    if (GWstr[0] == 0)
    {
        is_gateway = true;        
        sprintf (GWstr, "127.0.0.1:%d", port);
    }        
        
    // if physical coordinate is not supplied, VAST will need to obtain it itself
    //g_vastnetpara.phys_coord = g_aoi.center;    

    // translate gateway string to Addr object
    InitVAST (is_gateway, port);

    g_x = (float)(rand () % 100);
    g_y = (float)(rand () % 100);

    VASTJoin (g_x, g_y, g_radius);

    while (!g_finished)
    {
        // perform tick and process for as long as necessary
        VASTTick (0);

        getInput ();

        VASTMove (g_x, g_y);

        // check for any received message & print it out
        VAST_C_Msg *msg;
        while ((msg = VASTReceive ()) != NULL)
        {
            char str[80];
            strcpy (str, msg->msg);
            printf ("received from: %llu msg: %s\n", msg->from, str);
        }

        // sleep a little
        // NOTE the 2nd parameter is specified in microseconds (us) not milliseconds
        ACE_Time_Value duration (0, 100000);            
        ACE_OS::sleep (duration); 
    }

    VASTLeave ();
    ShutVAST ();
    
    return 0;
}


