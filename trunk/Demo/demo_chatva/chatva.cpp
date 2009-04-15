
/*
 *  Chatva (a simple multiuser chat application to showcase VAST in Win32)
 *  
 *  version history: 2006/02/05
 *
 */

#ifdef _WINDOWS
// disable warning about "identifier exceeds'255' characters"
#pragma warning(disable: 4786)
#endif

#include <windows.h>
#include <vector>
#include <map>


#include "vastverse.h"
using namespace VAST;
using namespace std;

#ifdef ACE_DISABLED
#error "ACE needs to be enabled to build demo_chatva, please modify /include/config.h"
#endif


//#define VAST_MODEL          VAST_MODEL_FORWARD  // VAST_MODEL_DIRECT or VAST_MODEL_FORWARD
#define VAST_MODEL          VAST_MODEL_DIRECT   // VAST_MODEL_DIRECT or VAST_MODEL_FORWARD
#define TIMER_INTERVAL		50                  // interval in ms for screen & input refresh
#define UPDATE_INTERVAL     200                 // interval in ms for sending movement updates
#define INPUT_SIZE          100
#define DEFAULT_AOI         200

#define SIZE_TEXTAREA       4

int     DIM_X =             800;
int     DIM_Y =             600;


int     g_steps = 0;

// VAST variables
vastverse       g_world (VAST_MODEL, VAST_NET_ACE, 0);
vast *          g_self  = NULL;
vastid *        g_id = NULL;
Addr            g_gateway;

// GUI-settings
int     NODE_RADIUS     = 10;
bool    follow_mode     = false;    // toggle for follow_mode
bool    show_edges      = true;     // display Voronoi edges
char    last_char       = 0;        // numerical value of last key pressed
bool    finished        = false;    // simulation is done

char        g_input[INPUT_SIZE];    // input buffer (a line of typing)

POINTS      g_cursor;               // mouse cursor position
POINT       g_origin;               // viewport origin position
Position    g_pos;


vector<char *> g_chatmsg;

char   *g_GWstr;

HWND    g_activewin = 0;

#define MAX_JOIN_COUNTDOWN      (2 * (1000/UPDATE_INTERVAL))        // 3 seconds

#define IDT_TIMER1  77

//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the scene
//-----------------------------------------------------------------------------
VOID Render( HWND hWnd )
{    
    // Calling BeginPaint clears the update region that was set by calls
    // to InvalidateRect(). Once the update region is cleared no more
    // WM_PAINT messages will be sent to the window until InvalidateRect
    // is called again.

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    // g_self is not yet obtained before 1st step is run
    if (g_self == NULL)
        return;

    Node *self = g_self->getself();
    vector<Node *>&nodes = g_self->getnodes();

    //g_pos.x = self->pos.x;
    //g_pos.y = self->pos.y;

    // get screen size (?)

    if (follow_mode)
    {             
        g_origin.x = (long)(-(g_pos.x - DIM_X/2));
        g_origin.y = (long)(-(g_pos.y - DIM_Y/2));
    }        

    // set proper origin
    SetViewportOrgEx (hdc, g_origin.x, g_origin.y, NULL);
        
    char str[160];

    int n = nodes.size ();
    int size_CN = n-1;

    // draw big circle
    SelectObject (hdc, GetStockObject(HOLLOW_BRUSH));
    //SelectObject( hdc, GetStockObject(BLACK_PEN) );
    Ellipse (hdc, (int)g_pos.x-NODE_RADIUS, (int)g_pos.y-NODE_RADIUS, (int)g_pos.x+NODE_RADIUS, (int)g_pos.y+NODE_RADIUS);

    // draw AOI
    //SelectObject( hdc, GetStockObject(HOLLOW_BRUSH) );
    Ellipse (hdc, (int)g_pos.x-self->aoi, (int)g_pos.y-self->aoi, (int)g_pos.x+self->aoi, (int)g_pos.y+self->aoi);
    //Ellipse( hdc, x-aoi_buf, y-aoi_buf, x+aoi_buf, y+aoi_buf );        
            
    int j;
                
    // draw neighbor dots
    for(j=0; j< (int)nodes.size (); j++)
    {
        int size = nodes.size ();
        int   x = (int)nodes[j]->pos.x;
        int   y = (int)nodes[j]->pos.y;
        id_t id = nodes[j]->id;

        // draw small circle
        SelectObject (hdc, GetStockObject(GRAY_BRUSH));
        Ellipse (hdc, x-(NODE_RADIUS/2), y-(NODE_RADIUS/2), x+(NODE_RADIUS/2), y+(NODE_RADIUS/2));

        // draw node id
        sprintf( str, "%d", nodes[j]->id );
        TextOut( hdc, x-5, y-25, str, strlen(str) );                                    
    }

    // draw Voronoi edges
    if (show_edges) 
    {
        vector<line2d> &lines = g_self->getvoronoi ()->getedges();
        for (j=0; j< (int)lines.size (); j++)
        {    
            POINT points[2];
            
            points[0].x = (long)lines[j].seg.p1.x;
            points[0].y = (long)lines[j].seg.p1.y;
            points[1].x = (long)lines[j].seg.p2.x;
            points[1].y = (long)lines[j].seg.p2.y;
            
            Polyline (hdc, points, 2);
        }                
    }
        
    // info line
    sprintf (str, "step: %d (%d, %d) node: %d [%d, %d] aoi: %d CN: %d %s char: %d gw: %s", g_steps/(UPDATE_INTERVAL/TIMER_INTERVAL), g_cursor.x-g_origin.x, g_cursor.y-g_origin.y, self->id, (int)g_pos.x, (int)g_pos.y, self->aoi, size_CN, (follow_mode ? "[follow]" : ""), last_char, g_GWstr);
    TextOut (hdc, 10-g_origin.x, 10-g_origin.y, str, strlen(str) );    

    // draw chat text
    n = g_chatmsg.size ();
    int base_y = DIM_Y-100;
    int lines_shown = 0;

    TextOut (hdc, 10-g_origin.x, base_y-g_origin.y, g_input, strlen (g_input));
    base_y -= 30;
    while (n > 0 && lines_shown < SIZE_TEXTAREA)
    {
        n--;
        char *str = g_chatmsg[n];
        TextOut (hdc, 10-g_origin.x, base_y-g_origin.y, str, strlen (str));
        base_y -= 20;
        lines_shown++;
    }
    
    
    // EndPaint balances off the BeginPaint call.
    EndPaint(hWnd,&ps);
}

void GetMovements()
{
    // should check if i'm current active window

    // Node movement in response to arrow key presses
    if (GetAsyncKeyState(VK_LEFT) < 0)
        g_pos.x -= 5;
    else if (GetAsyncKeyState(VK_RIGHT) < 0)
        g_pos.x += 5;
    if (GetAsyncKeyState(VK_UP) < 0)
        g_pos.y -= 5;
    else if (GetAsyncKeyState(VK_DOWN) < 0)
        g_pos.y += 5;
}


void Shake()
{
    // should check if i'm current active window

    // Node movement in response to arrow key presses
    if (rand () % 2 == 0)
        g_pos.x -= 5;
    if (rand () % 2 == 0)
        g_pos.x += 5;
    if (rand () % 2 == 0)
        g_pos.y -= 5;
    if (rand () % 2 == 0)
        g_pos.y += 5;

    if (g_pos.x < 0)
        g_pos.x *= -1;
    if (g_pos.y < 0)
        g_pos.y *= -1;
    if (g_pos.x > DIM_X)
        g_pos.x = DIM_X-5;
    if (g_pos.y > DIM_Y)
        g_pos.y = DIM_Y-5;
}

bool StoreChat (char *msg)
{    
    char *str;
    int size = strlen (msg) + 1;
    if (size > INPUT_SIZE)
        size = INPUT_SIZE;

    if ((str = (char *)malloc (size)) == NULL)
        return false;
    
    memcpy (str, msg, size);         
    g_chatmsg.push_back (str);    
    
    return true;
}

VOID MoveOnce( HWND hWnd )
{
    static char buffer[INPUT_SIZE];

    if (finished == true)
        return;

    g_steps++;

    // send out current position as a move
    g_activewin = GetActiveWindow ();

    // get inputs only if I'm the current active window
    if (g_activewin != NULL && g_activewin == hWnd)
        GetMovements ();
    
    // for debug purpose
    // Shake ();

    if (g_steps % (UPDATE_INTERVAL/TIMER_INTERVAL) == 0)
        g_self->setpos (g_pos);

    //g_self->tick ();

    // check and store any incoming text messages
    id_t from;
    char *msg;

    while (g_self->recv (from, &msg) > 0)
        StoreChat (msg);
        
    InvalidateRect (hWnd, NULL, TRUE);     
}    

//-----------------------------------------------------------------------------
// Name: MsgProc()
// Desc: The window's message handler
//-----------------------------------------------------------------------------

LRESULT WINAPI MsgProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int  count=0;
    int  base_count=0;
    static int  join_countdown = 0;

    switch (msg)
    {
        case WM_DESTROY:
            PostQuitMessage (0);
            return 0;

        case WM_PAINT:
            if (g_input[0] == 0 && g_self != NULL)
            {
                id_t id = g_self->getself ()->id;
                if (id > 0)
                {
                    sprintf (g_input, "[%d]: ", id);
                    count = strlen (g_input);
                    g_input[count] = 0;
                    base_count = count;                    
                }
            }
            
            Render (hWnd);
            //ValidateRect( hWnd, NULL );
            return 0;

        case WM_TIMER:
            if (!finished)
            {
                g_self->tick ();
                if (g_self->getself ()->id == NET_ID_UNASSIGNED)
                {
                    if (g_id->getid () != NET_ID_UNASSIGNED)
                    {
                        // we do join only after obtaining an ID
                        g_self->join (g_id->getid (), DEFAULT_AOI, g_pos, g_gateway);
                        join_countdown = MAX_JOIN_COUNTDOWN;

                        // reset window
                        char title[80];
                        sprintf (title, "Chatva [%d] - tiny chat based on VAST", g_id->getid ());
                        SetWindowText (hWnd, title);
                    }
                }

                // if I'm still waiting for initial neighbors
                else if (g_self->is_joined () == false)
                {                    
                    if (join_countdown == 0)
                    {                        
                        g_self->join (g_id->getid (), DEFAULT_AOI, g_pos, g_gateway);

                        // we initiate a countdown, if countdown is reached and we're still 
                        // not joined, then another join request will be sent to gateway
                        join_countdown = MAX_JOIN_COUNTDOWN;
                    }
                    join_countdown--;
                }

                // we've joined, make a move
                else
                    MoveOnce (hWnd);
            }
            return 0;
       
        case WM_CHAR:
            last_char = wParam;

            // 'enter' for sending text chat message
            if (wParam == 13)
            {
                g_input[count++] = 0;
                                
                // send off input to AOI neighbors
				// note: self is also included as the 1st neighbor
                vector<Node *>&nodes = g_self->getnodes();
                for(int j=0; j< (int)nodes.size (); j++)
                {
                    Position pos = nodes[j]->pos;
                    id_t id   = nodes[j]->id;
                    
                    g_self->send (id, g_input, count, true);
                }

                sprintf (g_input, "[%d]: ", g_self->getself ()->id);
                count = strlen (g_input);
                g_input[count] = 0;
                base_count = count;
            }
            // 'backspace'
            else if (wParam == 8)
            {
                if (count - base_count > 0)
                    g_input[--count] = 0;
            }

            // turning on/off Voronoi edges
            else if (wParam == '-')
            {
                show_edges = !show_edges;
                InvalidateRect (hWnd, NULL, TRUE);
            }
            // turning on/off follow mode edges
            else if (wParam == '=')
            {
                follow_mode = !follow_mode;
                InvalidateRect (hWnd, NULL, TRUE);
            }            
            // ESC: quit and write log file
            else if (wParam == 27)
            {                
                PostQuitMessage (0);
            }
            // record key and update status bar
            else
            {   
                // prevent input message overload
                if (count == INPUT_SIZE-1)
                    return 0;

                g_input[count++] = wParam;
                g_input[count] = 0;

                RECT r;
                r.top = r.left = 0;
                r.right  = DIM_X;
                r.bottom = 100;                 
                InvalidateRect (hWnd, &r, TRUE);
            }
            
            return 0;

        case WM_MOUSEMOVE:
            {
                g_cursor = MAKEPOINTS( lParam );
                RECT r;
                r.top = r.left = 0;
                r.right  = 800;
                r.bottom = 100;                 
                InvalidateRect (hWnd, &r, TRUE);
            }
            return 0;

        // select current active
        case WM_LBUTTONDOWN:
            {
                /*
                // select current active node
                for (int i=0; i<g_para.NODE_SIZE; i++)
                {
                    Node *n = GetNode(i);
                    // skip failed nodes
                    if (n == 0)
                        continue;
                    Position pt(g_cursor.x-g_origin.x, g_cursor.y-g_origin.y);
                    if (n->pos.dist (pt) <= NODE_RADIUS)
                    {
                        g_selected = i;
                        InvalidateRect (hWnd, NULL, TRUE);
                        break;
                    }
                }
                */
            }
            return 0;

        // toggle nodes showing their AOI
        case WM_RBUTTONDOWN:
            {
            }
            return 0;
            
    }

    return DefWindowProc( hWnd, msg, wParam, lParam );
}


//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: The application's entry point
//-----------------------------------------------------------------------------
INT WINAPI WinMain (HINSTANCE hInst, HINSTANCE, LPSTR lpCmdLine, INT)
{    
    // create node
    srand ((unsigned int)time (NULL));
    
    g_pos.x = rand () % DIM_X;
    g_pos.y = rand () % DIM_Y;

    // set origin
    g_origin.x = g_origin.y = 0;

    // get gateway IP
    g_GWstr = lpCmdLine;
    bool is_gateway = true;
    
    IPaddr ip (lpCmdLine, 3737);
    
    g_gateway.publicIP = ip;
    
    if (g_GWstr[0] != '\0')
        is_gateway = false;
    
    // Register the window class
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L,
                      hInst, 
                      LoadIcon (NULL,IDI_APPLICATION),
                      LoadCursor (NULL,IDC_ARROW),
                      (HBRUSH)(COLOR_WINDOW+1),
                      NULL,
                      "VASTdemo", NULL };
    RegisterClassEx (&wc);

    // Create the application's window
    HWND hWnd = CreateWindow ("VASTdemo", "Chatva [unassigned] - tiny chat based on VAST",
                              WS_OVERLAPPEDWINDOW, 100, 100, DIM_X, DIM_Y,
                              GetDesktopWindow(), NULL, hInst, NULL);

    // Show the window
    ShowWindow (hWnd, SW_SHOWDEFAULT);
    UpdateWindow (hWnd);


    // create the VAST node and also a ID generator
    g_self  = g_world.create_node (g_gateway.publicIP.port, (aoi_t)((float)DEFAULT_AOI * 0.10));
    if (g_self == NULL)
    {
        MessageBox (hWnd, "VAST node init failed, please check if ACE is enabled", "", 0);
        UnregisterClass ("VASTdemo", hInst);
        return 0;
    }

    g_id    = g_world.create_id (g_self, is_gateway, g_gateway);


    // set timer
    SetTimer(hWnd,              // handle to main window 
        IDT_TIMER1,             // timer identifier 
        TIMER_INTERVAL,         // 1-second interval 
        (TIMERPROC) NULL);      // no timer callback     

    // Enter the message loop
    MSG msg;
    ZeroMemory (&msg, sizeof (msg));

    while (GetMessage (&msg, 0, 0, 0))
    {
        TranslateMessage (&msg);
        DispatchMessage (&msg);
    }
    
    UnregisterClass ("VASTdemo", hInst);

    g_world.destroy_id (g_id);
    g_world.destroy_node (g_self);
    
    return msg.wParam;
}
