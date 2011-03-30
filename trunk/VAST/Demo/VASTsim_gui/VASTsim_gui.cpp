

                            

#ifdef WIN32
//#include <vld.h>            // visual leak detector (NOTE: must download & install from
                            // http://www.codeproject.com/KB/applications/visualleakdetector.aspx

// disable warning about "identifier exceeds'255' characters"
#pragma warning(disable: 4786)
#endif


#include <windows.h>
#include <wingdi.h>
#include <vector>
#include <map>

// use VASTsim as underlying library
// must set dependency for projects "VASTsim"
#include "VASTsim.h"

// use VASTATEsim as underlying library
// must set dependency for projects "VASTATEsim"
//#include "VASTATEsim.h"

using namespace Vast;
using namespace std;

#define EN_LEVEL    1

// how often should the timer be called (in millisecond)
#define TIMER_INTERVAL      40

#define SCREEN_DIMENSION_X  800
#define SCREEN_DIMENSION_Y  600

// simulation parameters
SimPara                     g_para;

map<int, vector<Node *> *>  g_nodes;            // map of each node's AOI neighbors
map<int, vector<id_t> *>    g_ENs;              // map of each node's enclosing neighbors
map<id_t, int>              g_id2index;         // mapping from node ID to node index
int                         g_steps = 0;
//int                         CREATE_NODE_COUNTDOWN = 10;

// GUI-settings
int     NODE_RADIUS     = 10;
bool    step_mode       = true;     // toggle for step_mode
bool    follow_mode     = false;     // toggle for follow_mode
bool    paused          = false;    // toggle for pausing
bool    show_edges      = true;     // display Voronoi edges
bool    show_box        = false;     // display bounding box
bool    show_node_id    = true;     // display node IDs
bool    show_self       = true;     // display my AOI and AOI neighbors in colors
bool    pause_at_incon  = false;     // pause at inconsistency
char    last_char       = 0;        // numerical value of last key pressed
bool    finished        = false;    // simulation is done

POINTS          g_cursor;
POINT           g_origin;
int             g_selected      = 0;        // currently active node index
int             g_nodes_active  = 0;        // # of active nodes (not failed)
int             g_nodes_created = 0;        // total # of nodes created so far
length_t        g_aoi           = 0;        // aoi of current node
id_t            g_id            = 0;        // id of current node
int             g_inconsistent  = 0;        // # of inconsistent nodes
map<id_t, bool> g_show_aoi;                 // flags to whether to draw a node's AOI
int             g_create_countdown = 0;     // countdown in steps to create a new node

#define IDT_TIMER1  77

int wvalue, lvalue;

// check if a particular id is an enclosing neighbor
bool is_EN (vector<id_t>* en_list, id_t id)
{
    if (en_list == NULL)
        return false;

    for (size_t i=0; i<en_list->size (); ++i)
        if (en_list->at (i) == id)
            return true;
        
        return false;
}

// obtain current list of neighbors (to be rendered)
void RefreshNeighbors ()
{
    Node *node;
    
    for (int i=0; i < g_nodes_created; ++i)  
    {
        if ((node = GetNode (i)) != NULL)
        {
            // create mapping & pointers for neighbors & enclosing neighbors
            g_id2index[node->id] = i;
            g_nodes[i] = GetNeighbors (i);
            g_ENs[i] = GetEnclosingNeighbors (i, EN_LEVEL);
        }
    }
}

//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the scene
//-----------------------------------------------------------------------------
VOID Render (HWND hWnd)
{
    int n = g_nodes.size ();

    // Calling BeginPaint clears the update region that was set by calls
    // to InvalidateRect(). Once the update region is cleared no more
    // WM_PAINT messages will be sent to the window until InvalidateRect
    // is called again.

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint (hWnd, &ps);

    Node *self = NULL;

    // auto adjust selected node as 1st node if selected node's not found
    if (g_nodes.find (g_selected) == g_nodes.end ())
        g_selected = 0;

    // reset origin for follow mode
    if (follow_mode)
    {                
        self = GetNode (g_selected);
        g_origin.x = (long)(-(self->aoi.center.x - 400));
        g_origin.y = (long)(-(self->aoi.center.y - 300));
    }
    
    // set proper origin
    SetViewportOrgEx (hdc, g_origin.x, g_origin.y, NULL);

    // buffer for printing messages
    char str[160];

    int size_AN = 0;
    int selected_x = 0 , selected_y =0;

    HPEN hPenRed    = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
    HPEN hPenBlue   = CreatePen(PS_SOLID, 2, RGB(0, 0, 255));
    //HPEN hPenGreen  = CreatePen(PS_SOLID, 2, RGB(0, 255, 0));
    HPEN hPenOld;

    // reset # of active (not failed) nodes
    g_nodes_active = 0;
    
    int missing_count = 0;
    for (int i=0; i<n; ++i)
    {              
        self = GetNode (i);

        // we do not display failed nodes
        if (self == NULL)
            continue;

        // g_nodes is not yet obtained before 1st step is run
        if (g_nodes[i] == 0)
            break;
        
        g_nodes_active++;
        vector<Node *>&nodes = *g_nodes[i];
        vector<id_t> *en_list = g_ENs[i];

        /*
        // TODO: BUG: this is to prevent program crash under FO model, need to check out
        //            this shouldn't be 0 after initializion is completed
        if (nodes.size () == 0)
        {
            missing_count++;
            continue;
        }
        */

        int     x = (int)self->aoi.center.x;
        int     y = (int)self->aoi.center.y;
        length_t aoi = self->aoi.radius;
        
        // draw big circle

        /*
                //SelectObject (hdc, GetStockObject(GRAY_BRUSH));       
                if (j == 0 || is_EN (en_list, id))
                    hPenOld = (HPEN)SelectObject (hdc, hPenRed);
                else
                    hPenOld = (HPEN)SelectObject (hdc, hPenBlue);
        */        
                                              
        SelectObject (hdc, GetStockObject(HOLLOW_BRUSH));
        
        //SelectObject (hdc, GetStockObject(DC_PEN));
        //SetDCPenColor (hdc, RGB(0x00,0xff,0x00));     
        //hPenOld = (HPEN)SelectObject (hdc, hPen);
        //SelectObject( hdc, GetStockObject(BLACK_PEN) );

        Ellipse (hdc, x-NODE_RADIUS, y-NODE_RADIUS, x+NODE_RADIUS, y+NODE_RADIUS);
        //SelectObject (hdc, hPenOld);
        //DeleteObject (hPen);

//        SelectObject (hdc, hPenOld);
        
        // draw node id
        if (show_node_id)
        {
            //sprintf (str, "%d", (int)VASTnet::resolvePort (self->id)-GATEWAY_DEFAULT_PORT+1);
            sprintf (str, "%d", i+1);
            TextOut (hdc, x-5, y-25, str, strlen(str));
        }

        // draw node AOI
        if (g_show_aoi[i] == true)
        {
            // change color ?
            SelectObject (hdc, GetStockObject(HOLLOW_BRUSH));
            Ellipse (hdc, x-aoi, y-aoi, x+aoi, y+aoi);
        }

        if (i == g_selected)
        {
            int j;
            
            g_aoi = self->aoi.radius;
            g_id  = self->id;
            selected_x = x;
            selected_y = y;
            size_AN = nodes.size ()-1;
            
            // draw AOI
            if (show_self)
            {
                //SelectObject( hdc, GetStockObject(HOLLOW_BRUSH) );
                Ellipse (hdc, x-aoi, y-aoi, x+aoi, y+aoi);
            }

            // draw neighbor dots
            for (j=0; j<(int)nodes.size (); j++)
            {
                int x = (int)nodes[j]->aoi.center.x; 
                int y = (int)nodes[j]->aoi.center.y;
                id_t id = nodes[j]->id;

                //SelectObject(hdc,GetStockObject(DC_PEN));
                //SetDCPenColor(hdc,RGB(0x00,0xff,0x00));

                // draw small circle
                if (show_self)
                {
                    //SelectObject (hdc, GetStockObject(GRAY_BRUSH));       
                    if (j == 0 || is_EN (en_list, id))
                        hPenOld = (HPEN)SelectObject (hdc, hPenRed);
                    else
                        hPenOld = (HPEN)SelectObject (hdc, hPenBlue);
                                     
                    Ellipse (hdc, x-(NODE_RADIUS/2), y-(NODE_RADIUS/2), x+(NODE_RADIUS/2), y+(NODE_RADIUS/2));
                        
                    SelectObject (hdc, hPenOld);
                }

                // draw node id
                if (show_node_id)
                {
                    //sprintf (str, "%d", (int)VASTnet::resolvePort (nodes[j]->addr.host_id)-GATEWAY_DEFAULT_PORT+1);
                    id_t neighbor_id = nodes[j]->id;

                    /*
                    int strip = (int)(0x000000000000FFFF & neighbor_id);
                    strip = (int)(0x00000000FFFF0000 & neighbor_id);
                    strip = (int)((0x0000FFFF00000000 & neighbor_id) >> 32);
                    strip = (int)((0xFFFF000000000000 & neighbor_id) >> 32);
                    strip = (int)((0x0000FFFFFFFF0000 & neighbor_id) >> 16);
                    */

                    int index = (g_id2index.find (neighbor_id) == g_id2index.end () ? 0 : g_id2index[neighbor_id]);

                    sprintf (str, "%d", index + 1);
                    TextOut (hdc, x-5, y-25, str, strlen(str));
                }
            }

            // draw Voronoi edges
            if (show_edges && GetEdges (i) != NULL)
            {    
                vector<line2d> &lines = *GetEdges (i); 
                for (j=0; j<(int)lines.size (); j++)
                {    
                    POINT points[2];
                    
                    points[0].x = (long)lines[j].seg.p1.x;
                    points[0].y = (long)lines[j].seg.p1.y;
                    points[1].x = (long)lines[j].seg.p2.x;
                    points[1].y = (long)lines[j].seg.p2.y;
                    
                    Polyline (hdc, points, 2);
                }                
            }

            point2d min, max;

            // draw bounding box
            if (show_box && GetBoundingBox (i, min, max) == true)
            {
                vector<line2d> lines;
                lines.push_back (line2d (min.x, min.y, max.x, min.y));
                lines.push_back (line2d (max.x, min.y, max.x, max.y));
                lines.push_back (line2d (max.x, max.y, min.x, max.y));
                lines.push_back (line2d (min.x, max.y, min.x, min.y));

                for (j=0; j<(int)lines.size (); j++)
                {    
                    POINT points[2];
                    
                    points[0].x = (long)lines[j].seg.p1.x;
                    points[0].y = (long)lines[j].seg.p1.y;
                    points[1].x = (long)lines[j].seg.p2.x;
                    points[1].y = (long)lines[j].seg.p2.y;
                    
                    Polyline (hdc, points, 2);
                }   
            }
        }
    }

    // info line
    char str2[40];
    if (g_inconsistent != (-1))
        sprintf (str2, "Inconsistent: %d", g_inconsistent);
    else
        sprintf (str2, "[END]");

    sprintf (str, "step: %d (%d, %d) node: %d/%d [%d, %d] aoi: %d AN: %d %s %s %s missing: %d", 
             g_steps, 
             g_cursor.x-g_origin.x, 
             g_cursor.y-g_origin.y, 
             (int)VASTnet::resolvePort (g_id)-GATEWAY_DEFAULT_PORT+1, 
             g_nodes_active, 
             selected_x, 
             selected_y, 
             g_aoi, 
             size_AN, 
             str2, 
             (follow_mode ? "[follow]" : ""), 
             (step_mode ? "[stepped]" : ""), 
             missing_count);

    TextOut (hdc, 10-g_origin.x, 10-g_origin.y, str, strlen(str) );    
    
    // EndPaint balances off the BeginPaint call.
    EndPaint (hWnd, &ps);
}

VOID MoveOnce (HWND hWnd)
{
    if (finished == true)
        return;

    g_steps++;
    
    // create new nodes
    if (g_nodes_created < g_para.NODE_SIZE) 
    {
        if (g_create_countdown == 0)
        {
            // only make sure gateway is fully joined before creating next node
            if (CreateNode (g_nodes_created == 0) == true)
                g_show_aoi[g_nodes_created++] = false;

            g_create_countdown = g_para.JOIN_RATE;
        }
        else
            g_create_countdown--;
    }

    g_inconsistent = NextStep ();
    
    if (g_inconsistent < 0)
    {
        finished = true;           
    }

    else if (g_inconsistent > 0 && pause_at_incon)
    {
        step_mode = true;
        paused = true;
    }        
 
    if (!finished)
        RefreshNeighbors ();
      
    InvalidateRect (hWnd, NULL, TRUE);     
}    

//-----------------------------------------------------------------------------
// Name: MsgProc()
// Desc: The window's message handler
//-----------------------------------------------------------------------------

LRESULT WINAPI MsgProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_DESTROY:
            ShutSim ();
            PostQuitMessage (0);
            break;

        case WM_PAINT:
            Render (hWnd);
            ValidateRect (hWnd, NULL);
            break;

        case WM_TIMER:
            if (paused == false && !finished)
                MoveOnce (hWnd);
            if (step_mode == true)
                paused = true;
            break;
       
        case WM_CHAR:
            last_char = (char)wParam;
            // 'space' for next step
            lvalue = lParam;
            wvalue = wParam;
            
            if (wParam == ' ')
            {
                paused = !paused;
                if (paused == true)
                    step_mode = true;
            }
            // 'enter' for toggling step mode
            else if (wParam == 13)
            {
                step_mode = !step_mode;
                if (step_mode == false)
                    paused = false;
            }
            // turning on/off Voronoi edges
            else if (wParam == 'e')
            {
                show_edges = !show_edges;
                InvalidateRect (hWnd, NULL, TRUE);
            }
            // turning on/off bounding boxes
            else if (wParam == 'b')
            {
                show_box = !show_box;
                InvalidateRect (hWnd, NULL, TRUE);
            }
            // turning on/off following mode
            else if (wParam == 'f')
            {
                follow_mode = !follow_mode;
                InvalidateRect (hWnd, NULL, TRUE);
            }            
            // turning on/off pause at inconsistency
            else if (wParam == 'c' )
            {
                pause_at_incon = !pause_at_incon;                
            }            
            // turning on/off node numbering
            else if (wParam == 'n' )
            {
                show_node_id = !show_node_id;
                InvalidateRect (hWnd, NULL, TRUE);
            }                        
            // quit and write log file
            else if (wParam == 'q' )
            {
                ShutSim ();
                PostQuitMessage (0);
            }
            // move viewport UP
            else if (wParam == 'w' )
            {
                g_origin.y += 5; 
                follow_mode = false;
                InvalidateRect (hWnd, NULL, TRUE);
            }
            // move viewport DOWN
            else if (wParam == 's' )
            {
                g_origin.y -= 5;
                follow_mode = false;
                InvalidateRect (hWnd, NULL, TRUE);
            }
            // move viewport LEFT
            else if (wParam == 'a' )
            {
                g_origin.x += 5;
                follow_mode = false;
                InvalidateRect (hWnd, NULL, TRUE);
            }
            // move viewport RIGHT
            else if (wParam == 'd' )
            {
                g_origin.x -= 5;
                follow_mode = false;
                InvalidateRect (hWnd, NULL, TRUE);
            }
            // reset viewport at 'origin'
            else if (wParam == 'o' )
            {
                follow_mode = false;
                g_origin.x = g_origin.y = 0;
                InvalidateRect (hWnd, NULL, TRUE);
            }    
            // display AOI and AOI neighbors
            else if (wParam == 'i')
            {
                show_self = !show_self;
                InvalidateRect (hWnd, NULL, TRUE);
            }    
            // simply update status bar
            else
            {            
                RECT r;
                r.top = r.left = 0;
                r.right  = SCREEN_DIMENSION_X;
                r.bottom = 100;                 
                InvalidateRect (hWnd, &r, TRUE);
            }
            break;

        case WM_MOUSEMOVE:
            {
                g_cursor = MAKEPOINTS (lParam);
                RECT r;
                r.top = r.left = 0;
                r.right  = SCREEN_DIMENSION_X;
                r.bottom = 100;                 
                InvalidateRect (hWnd, &r, TRUE);
            }
            break;

        // select current active
        case WM_LBUTTONDOWN:
            {
                // select current active node
                for (int i=0; i<g_nodes_created; i++)
                {
                    Node *n = GetNode (i);

                    // skip failed nodes
                    if (n == 0)
                        continue;

                    Position pt ((coord_t)g_cursor.x-g_origin.x, (coord_t)g_cursor.y-g_origin.y);
                    
                    if (n->aoi.center.distance (pt) <= NODE_RADIUS)
                    {
                        g_selected = i;

                        // re-draw the whole screen
                        InvalidateRect (hWnd, NULL, TRUE);
                        break;
                    }
                }
            }
            break;

        // toggle nodes showing their AOI
        case WM_RBUTTONDOWN:
            {
                // select current active node
                for (int i=0; i<g_nodes_created; i++)
                {
                    Node *n = GetNode(i);

                    if (n == NULL)
                        continue;

                    Position pt ((coord_t)g_cursor.x-g_origin.x, (coord_t)g_cursor.y-g_origin.y);
                    if (n->aoi.center.distance (pt) <= NODE_RADIUS)
                    {
                        g_show_aoi[i] = !g_show_aoi[i];
                        InvalidateRect (hWnd, NULL, TRUE);
                        break;
                    }
                }
            }
            break;

        default:
            return DefWindowProc (hWnd, msg, wParam, lParam);
    }

    return 0;
}



//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: The application's entry point
//-----------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE hInst, HINSTANCE, LPSTR, INT )
{
    VASTPara_Net netpara (VAST_NET_EMULATED);

    // read parameters and initialize simulations
    InitPara (VAST_NET_EMULATED, netpara, g_para);
    
    InitSim (g_para, netpara);
    
    // Register the window class
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L,
                      hInst, 
                      LoadIcon(NULL,IDI_APPLICATION),
                      LoadCursor(NULL,IDC_ARROW),
                      (HBRUSH)(COLOR_WINDOW+1),
                      NULL,
                      "VASTSim", NULL };
    RegisterClassEx (&wc);

    // Create the application's window
    HWND hWnd = CreateWindow ("VASTSim", "VAST Simulator (GUI) v 0.1",
                              WS_OVERLAPPEDWINDOW, 100, 100, SCREEN_DIMENSION_X, SCREEN_DIMENSION_Y,
                              GetDesktopWindow (), NULL, hInst, NULL);

    // Show the window
    ShowWindow (hWnd, SW_SHOWDEFAULT);
    UpdateWindow (hWnd);
  
    // set timer
    SetTimer(hWnd,              // handle to main window 
        IDT_TIMER1,             // timer identifier 
        TIMER_INTERVAL,         // 1-second interval 
        (TIMERPROC) NULL);      // no timer callback     

    // set origin
    g_origin.x = 0;
    g_origin.y = 0;

    // Enter the message loop
    MSG msg;
    ZeroMemory (&msg, sizeof (msg));

    while (GetMessage (&msg, 0, 0, 0))
    {
        TranslateMessage (&msg);
        DispatchMessage (&msg);
    }
    
    UnregisterClass ("VASTsim", hInst);
       
    return msg.wParam;
}
