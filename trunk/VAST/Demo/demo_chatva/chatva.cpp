
/*
 *  Chatva (a simple multiuser chat application to showcase VAST in Win32)
 *  
 *  version history:	2006/02/05  begin
 *					    2009/07/01  converted to use VASTATE
 *                      2010/03/12  adopts VAST 0.4.3 relay-matcher-client design
 *
 */

#ifdef _WIN32
// disable warning about "identifier exceeds'255' characters"
#pragma warning(disable: 4786)
#endif

#include <windows.h>
#include <vector>
#include <map>
#include <time.h>

#define USE_VAST

#ifdef USE_VAST
// use VAST for functions
#include "VASTVerse.h"
#include "VASTsim.h"            // for InitPara
#else
// use VASTATE for main functions
#include "VASTATE.h"
#include "VASTATEsim.h"       
#endif


using namespace Vast;
using namespace std;

#ifdef ACE_DISABLED
#error "ACE needs to be enabled to build demo_chatva, please modify /common/Config.h"
#endif

#define TIMER_INTERVAL		50                  // interval in ms for screen & input refresh
#define UPDATE_INTERVAL     100                 // interval in ms for sending movement updates
#define INPUT_SIZE          200                 // size for chat message

#define VAST_EVENT_LAYER    1                   // layer ID for sending events 
#define VAST_UPDATE_LAYER   2                   // layer ID for sending updates

#define SIZE_TEXTAREA       4

int     g_steps = 0;                            // number of time-steps elapsed

#ifdef USE_VAST
// VAST-specific variables
VASTVerse *     g_world = NULL;
VAST *          g_self  = NULL;
id_t            g_sub_no = 0;       // subscription # for my client (peer)
#else
// VASTATE-specific variables
VASTATE *       g_world = NULL;
SimAgent *      g_self  = NULL;
SimArbitrator * g_arbitratorlogic = NULL;
Agent *         g_agent = NULL;
Arbitrator *    g_arbitrator = NULL;

#endif

// common variables
int             g_node_no = (-1);   // which node to simulate (-1 indicates none, manual control)
Area            g_aoi;              // my AOI (with center as current position)
world_t         g_world_id = 0;
Vast::NodeState g_state = ABSENT;
Voronoi      *  g_Voronoi = NULL;   // access to Voronoi class
VASTPara_Net    g_netpara (VAST_NET_ACE);          // network parameters

// GUI-settings
int     NODE_RADIUS     = 10;
bool    follow_mode     = false;    // toggle for follow_mode
bool    show_edges      = true;     // display Voronoi edges
char    last_char       = 0;        // numerical value of last key pressed
bool    finished        = false;    // simulation is done

char        g_input[INPUT_SIZE];    // input buffer (a line of typing)

POINTS      g_cursor;               // mouse cursor position
POINT       g_origin;               // viewport origin position

vector<string> g_chatmsg;

//char    g_gatewaystr[80];
Addr    g_gateway;                       // address to gateway
HWND    g_activewin = 0;

#define IDT_TIMER1  77

//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the scene
//-----------------------------------------------------------------------------
VOID Render (HWND hWnd)
{    
    // Calling BeginPaint clears the update region that was set by calls
    // to InvalidateRect(). Once the update region is cleared no more
    // WM_PAINT messages will be sent to the window until InvalidateRect
    // is called again.

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint (hWnd, &ps);

    // g_self is not yet obtained before 1st step is run
    if (g_self == NULL)
        return;

    Node *self = g_self->getSelf ();
    if (self == NULL)
        return;

#ifdef USE_VAST
    vector<Node *>&nodes = g_self->list ();
    g_Voronoi = g_world->getMatcherVoronoi ();
#else
    vector<Node *>&nodes = g_self->getNeighbors ();    
#endif

    // get screen size (?)

    HPEN hPenRed = CreatePen (PS_SOLID, 2, RGB(255, 0, 0));
    HPEN hPenOld;

    if (follow_mode)
    {             
        g_origin.x = (long)(-(g_aoi.center.x - DIM_X/2));
        g_origin.y = (long)(-(g_aoi.center.y - DIM_Y/2));
    }        

    // set proper origin
    SetViewportOrgEx (hdc, g_origin.x, g_origin.y, NULL);
        
    char str[160];

    int n = nodes.size ();

    // draw big circle
    SelectObject (hdc, GetStockObject(HOLLOW_BRUSH));
    //SelectObject( hdc, GetStockObject(BLACK_PEN) );
    Ellipse (hdc, (int)g_aoi.center.x-NODE_RADIUS, (int)g_aoi.center.y-NODE_RADIUS, (int)g_aoi.center.x+NODE_RADIUS, (int)g_aoi.center.y+NODE_RADIUS);

    // draw AOI
    //SelectObject( hdc, GetStockObject(HOLLOW_BRUSH) );
    Ellipse (hdc, (int)g_aoi.center.x - g_aoi.radius, (int)g_aoi.center.y - g_aoi.radius, (int)g_aoi.center.x + g_aoi.radius, (int)g_aoi.center.y + g_aoi.radius);
    //Ellipse( hdc, x-aoi_buf, y-aoi_buf, x+aoi_buf, y+aoi_buf );        

    // draw matcher AOI, if available
    Area *matcher_aoi = g_world->getMatcherAOI ();
    if (matcher_aoi != NULL)
    {
        hPenOld = (HPEN)SelectObject (hdc, hPenRed);
        Ellipse (hdc, (int)matcher_aoi->center.x - matcher_aoi->radius, (int)matcher_aoi->center.y - matcher_aoi->radius, (int)matcher_aoi->center.x + matcher_aoi->radius, (int)matcher_aoi->center.y + matcher_aoi->radius);
        SelectObject (hdc, hPenOld);
    }

    size_t j;
                
    // draw neighbor dots
    for(j=0; j< nodes.size (); j++)
    {
        //int size = nodes.size ();
        int   x = (int)nodes[j]->aoi.center.x;
        int   y = (int)nodes[j]->aoi.center.y;
        id_t id = nodes[j]->id;
        //id_t id = nodes[j]->addr.host_id;

        // draw small circle
        SelectObject (hdc, GetStockObject(GRAY_BRUSH));
        Ellipse (hdc, x-(NODE_RADIUS/2), y-(NODE_RADIUS/2), x+(NODE_RADIUS/2), y+(NODE_RADIUS/2));

        // draw node id
        sprintf (str, "%d", (int)VASTnet::resolvePort (id) - g_netpara.port + 1);
        TextOut (hdc, x-5, y-25, str, strlen(str));
    }

    // draw Voronoi edges
    if (show_edges && g_Voronoi != NULL) 
    {
        vector<line2d> &lines = g_Voronoi->getedges ();
        for (j=0; j< lines.size (); j++)
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
    char join_state[80];
    if (g_state == ABSENT)
        strcpy (join_state, "[ABSENT]");
    else if (g_state == JOINED)
        strcpy (join_state, "[JOINED]");
    else
        strcpy (join_state, "[JOINING]");

    string GWstr;
    g_gateway.toString (GWstr);

    sprintf (str, "step: %d (%d, %d) node: %d [%d, %d] %s aoi: %d CN: %d %s char: %d gw: %s", 
             g_steps/(UPDATE_INTERVAL/TIMER_INTERVAL), 
             g_cursor.x-g_origin.x, g_cursor.y-g_origin.y, 
             (int)VASTnet::resolvePort (self->id)-g_netpara.port+1, (int)g_aoi.center.x, (int)g_aoi.center.y, 
             join_state,
             (matcher_aoi == NULL ? self->aoi.radius : matcher_aoi->radius), 
             n, (follow_mode ? "[follow]" : ""), 
             last_char,
             GWstr.c_str ());

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
        string &chat = g_chatmsg[n];
        size_t size = chat.size ();
        TextOut (hdc, 10-g_origin.x, base_y-g_origin.y, chat.c_str (), size);
        base_y -= 20;
        lines_shown++;
    }
        
    // EndPaint balances off the BeginPaint call.
    EndPaint(hWnd,&ps);
}

void GetMovements ()
{
    // should check if i'm current active window

    // Node movement in response to arrow key presses
    if (GetAsyncKeyState(VK_LEFT) < 0)
        g_aoi.center.x -= 5;
    else if (GetAsyncKeyState(VK_RIGHT) < 0)
        g_aoi.center.x += 5;
    if (GetAsyncKeyState(VK_UP) < 0)
        g_aoi.center.y -= 5;
    else if (GetAsyncKeyState(VK_DOWN) < 0)
        g_aoi.center.y += 5;
}


void Shake ()
{
    // should check if i'm current active window

    // Node movement in response to arrow key presses
    if (rand () % 2 == 0)
        g_aoi.center.x -= 5;
    if (rand () % 2 == 0)
        g_aoi.center.x += 5;
    if (rand () % 2 == 0)
        g_aoi.center.y -= 5;
    if (rand () % 2 == 0)
        g_aoi.center.y += 5;

    if (g_aoi.center.x < 0)
        g_aoi.center.x *= -1;
    if (g_aoi.center.y < 0)
        g_aoi.center.y *= -1;
    if (g_aoi.center.x > DIM_X)
        g_aoi.center.x = (coord_t)DIM_X-5;
    if (g_aoi.center.y > DIM_Y)
        g_aoi.center.y = (coord_t)DIM_Y-5;
}

bool StoreChat (std::string &msg)
{    
    if (msg.size () == 0)
        return false;

    // TODO: we never release the allocated chat msg       
    g_chatmsg.push_back (msg);
    
    return true;
}

VOID MoveOnce (HWND hWnd)
{
    static char buffer[INPUT_SIZE];

    if (finished == true)
        return;

    // send out current position as a move
    g_activewin = GetActiveWindow ();

    // get inputs only if I'm the current active window
    if (g_activewin != NULL && g_activewin == hWnd)
        GetMovements ();
    
    // for debug purpose
    // Shake ();

    // we don't want to move too fast (not as fast as frame rate)
    if (g_steps % (UPDATE_INTERVAL/TIMER_INTERVAL) == 0)
    {
#ifdef USE_VAST
        g_self->move (g_sub_no, g_aoi);
#else
        g_agent->move (g_aoi.center);
#endif
    }

    // check and store any incoming text messages    
    char recv_buf[INPUT_SIZE];
    size_t size = 0;
    
    do
    {        
#ifdef USE_VAST
        Message *msg = NULL;
        if ((msg = g_self->receive ()) != NULL)
        {
            size = msg->extract (recv_buf, 0);
            recv_buf[size]=0;
        }
        else
            size = 0;
#else
        size = g_self->getChat (recv_buf);
#endif
        if (size > 0)
        {
            string chatmsg (recv_buf, size);
            StoreChat (chatmsg);
        }
    }
    while (size > 0);
        
    InvalidateRect (hWnd, NULL, TRUE);     
}    

// make sure we've joined the network
VOID CheckJoin ()
{
#ifdef USE_VAST
    if (g_state == ABSENT)
    {
        if ((g_self = g_world->getVASTNode ()) != NULL)
        {
            g_sub_no = g_self->getSubscriptionID ();
            g_state = JOINED;
        }
    }

#else
    // created the agent & arbitrator
    if (g_state == ABSENT)
    {
        string password ("abc\0");
        
        // only 1st node join as arbitrator
        g_world->createNode (g_aoi, g_arbitratorlogic, g_self, password, (g_netpara.is_gateway ? &g_aoi.center : NULL));
        
        // all nodes join as arbitrator
        //g_world->createNode (g_aoi, g_arbitratorlogic, g_self, password, &g_aoi.center);
        
        g_state = JOINING;
    }
            
    // check if our agent & arbitrator have properly joined the network
    else if (g_state == JOINING && g_world->isLogined ())
    {
        g_arbitrator = g_world->getArbitrator ();
        g_agent = g_world->getAgent ();

        // update my peer ID                
        //g_self->setSelf (g_agent->getSelf ());

        g_state = JOINED;        
    }
#endif

}

//-----------------------------------------------------------------------------
// Name: MsgProc()
// Desc: The window's message handler
//-----------------------------------------------------------------------------


LRESULT WINAPI MsgProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static int  count=0;
    static int  base_count=0;
    
    switch (msg)
    {
        case WM_DESTROY:
            finished = true;
#ifdef USE_VAST
            if (g_self != NULL)
                g_self->leave ();
#else
            
            if (g_agent != NULL)
            {
                g_agent->leave ();
                g_agent->logout ();
            }
                        
#endif
            g_world->tick (0);
            
            PostQuitMessage (0);
            return 0;

        case WM_PAINT:
            if (g_input[0] == 0 && g_self != NULL && g_self->getSelf () != NULL)
            {
                Node *self = g_self->getSelf ();
                id_t id = self->id;
                if (id > 0)
                {
                    sprintf (g_input, "[%d]: ", (int)VASTnet::resolvePort (id)-g_netpara.port+1);
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
                g_steps++;
                               
                // NOTE that we will process for as long as needed 
                // (so possibly will run out of the time budget)
                g_world->tick (0);

                if (g_state == JOINED)            
                    MoveOnce (hWnd);
                else
                {              
                    CheckJoin ();

                    // update window title if we've just joined
                    if (g_state == JOINED)
                    {
                        // reset window
                        char title[80];
                        sprintf (title, "Chatva [%d] - tiny chat based on VAST", g_self->getSelf ()->id);
                        SetWindowText (hWnd, title);

                        // replace gateway IP with detected
                    }
                }
            }
            return 0;
       
        case WM_CHAR:
            last_char = (char)wParam;

            // 'enter' for sending text chat message
            if (wParam == 13)
            {
                g_input[count] = 0;

#ifdef USE_VAST
                // send off input to AOI neighbors
				// note: self is also included as the 1st neighbor                
                Message send_msg (12);

                send_msg.clear (123);
                send_msg.store (g_input, count, true);

                /* SEND-based */
                vector<Node *>&nodes = g_self->list ();
                //vector<Node *>&nodes = g_self->getLogicalNeighbors ();
                //vector<Node *>&nodes = g_self->getPhysicalNeighbors ();
                
                for (size_t j=0; j < nodes.size (); j++)
                {
                    Position pos = nodes[j]->aoi.center;
                    id_t id      = nodes[j]->id;

                    send_msg.addTarget (id);
                }

                // send away message
                g_self->send (send_msg);
                

                // PUBLISH-based                
                //g_self->publish (g_aoi, VAST_EVENT_LAYER, send_msg);

#else
                // process CREATE message
                if (strstr (g_input, "createfood") != 0)
                {
                    // create a food at some nearby location
                    SimPeerAction action = CREATE_FOOD;
                    Event *e = g_agent->createEvent ((msgtype_t)action);
                    Position pos (g_aoi.center);
                    pos.x += 5;
                    pos.y += 5;

                    e->add (pos);
                    g_agent->act (e);
                }
                else
                {
                    // VASTATE-based would be to update attribute
                    SimPeerAction action = TALK;
                    Event *e = g_agent->createEvent ((msgtype_t)action);
                    e->add (std::string (g_input, count));
                    g_agent->act (e);
                }
#endif
                // clear buffer
                g_input[0] = 0;

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

                g_input[count++] = (char)wParam;
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
    srand ((unsigned int)time (NULL));
    
    // set origin
    g_origin.x = g_origin.y = 0;

    // store default gateway address
    char GWstr[80];
    strcpy (GWstr, "127.0.0.1:1037");
    
    //g_gateway = *VASTVerse::translateAddress (str);

    // initialize parameters
    SimPara simpara;
    vector<IPaddr> entries;

    bool is_gateway;

    // obtain parameters from command line and/or INI file
    if ((g_node_no = InitPara (VAST_NET_ACE, g_netpara, simpara, lpCmdLine, &is_gateway, &g_world_id, &g_aoi, GWstr)) == (-1))
        exit (0);

    //bool simulate_behavior = (g_node_no > 0);

    /*
    // if physical coordinate is assigned, then no inference will be made, 
    // only do it for gateway (as localhost ping is not accurate can physical coord may not converge)
    if (g_netpara.is_gateway)
        g_netpara.phys_coord = g_aoi.center;
    */
    
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

#ifdef USE_VAST

    // create VAST node factory    
    g_world = new VASTVerse (is_gateway, GWstr, &g_netpara, NULL);
    g_world->createVASTNode (1, g_aoi, VAST_EVENT_LAYER);

#else
    VASTATEPara para;
    para.default_aoi    = DEFAULT_AOI;
    para.world_height   = DIM_Y;
    para.world_width    = DIM_X;
    para.overload_limit = 0;

    g_world = new VASTATE (para, g_netpara, NULL);    
    g_self  = CreateSimAgent ();
    g_arbitratorlogic = CreateSimArbitrator ();

#endif
    
    //if (g_self == NULL)
    //{
    //    MessageBox (hWnd, "VAST node init failed, please check if ACE is enabled", "", 0);
    //    UnregisterClass ("VASTdemo", hInst);
    //    return 0;
    //}

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

#ifdef USE_VAST
    g_world->destroyVASTNode (g_self);

#else
    
    g_world->destroyNode ();
    
    if (g_self != NULL)
        DestroySimAgent (g_self);

    if (g_arbitratorlogic != NULL)
        DestroySimArbitrator (g_arbitratorlogic);
    
#endif
        
    delete g_world;
    
    return msg.wParam;
}
