
#ifdef _WINDOWS
// disable warning about "identifier exceeds'255' characters"
#pragma warning(disable: 4786)
#endif

#include <windows.h>
#include <wingdi.h>
#include <vector>
#include <map>

#include "vastsim.h"
using namespace Vast;
using namespace std;

#define EN_LEVEL    1

//#define TIMER_INTERVAL		20
#define TIMER_INTERVAL		40

// simulation parameters
VASTsimPara                g_para;

map<int, vector<Node *> *> g_nodes;
map<int, vector<id_t> *>   g_ENs;
int                        g_steps = 0;

// GUI-settings
int     NODE_RADIUS     = 10;
bool    step_mode       = true;     // toggle for step_mode
bool    follow_mode     = false;     // toggle for follow_mode
bool    paused          = false;    // toggle for pausing
bool    show_edges      = true;     // display Voronoi edges
bool    show_node_id    = true;     // display node IDs
bool    pause_at_incon  = false;     // pause at inconsistency
char    last_char       = 0;        // numerical value of last key pressed
bool    finished        = false;    // simulation is done

POINTS  g_cursor;
POINT   g_origin;
int     g_selected      = 0;        // currently active node index
int     g_nodes_active  = 0;
length_t   g_aoi           = 0;
id_t    g_id            = 0;
int     g_inconsistent  = 0;
map<id_t, bool> g_show_aoi;

#define IDT_TIMER1  77

int wvalue, lvalue;

// check if a particular id is an enclosing neighbor
bool is_EN (vector<id_t>&en_list, id_t id)
{
    for (int i=0; i<(int)en_list.size (); ++i)
        if (en_list[i] == id)
            return true;
        
        return false;
}

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

    Node *self = GetNode (g_selected);

    if (follow_mode)
    {        
        vector<Node *>&nodes = *g_nodes[g_selected];
        g_origin.x = (long)(-(self->pos.x - 400));
        g_origin.y = (long)(-(self->pos.y - 300));
    }
    
    // set proper origin
    SetViewportOrgEx (hdc, g_origin.x, g_origin.y, NULL);

    char str[160];

    int n = g_nodes.size ();
    int size_AN = 0;

    int selected_x = 0 , selected_y =0;

    HPEN hPenRed = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
    HPEN hPenBlue = CreatePen(PS_SOLID, 2, RGB(0, 0, 255));
    HPEN hPenOld;

    g_nodes_active = 0;
    
    int missing_count = 0;
    for (int i=0; i<n; ++i)
    {              
        self = GetNode (i);

        // g_nodes is not yet obtained before 1st step is run
        if (g_nodes[i] == 0)
            break;
        
        // we do not display failed nodes
        if (self == NULL)
            continue;

        g_nodes_active++;
        vector<Node *>&nodes = *g_nodes[i];
        vector<id_t>&en_list = *g_ENs[i];

        // TODO: BUG: this is to prevent program crash under FO model, need to check out
        //            this shouldn't be 0 after initializion is completed
        if (nodes.size () == 0)
        {
            missing_count++;
            continue;
        }

        int     x = (int)self->pos.x;
        int     y = (int)self->pos.y;
        length_t aoi = self->aoi;
        
        // draw big circle

        SelectObject (hdc, GetStockObject(HOLLOW_BRUSH));
        //SelectObject (hdc, GetStockObject(DC_PEN));
        //SetDCPenColor (hdc, RGB(0x00,0xff,0x00));     
        //hPenOld = (HPEN)SelectObject (hdc, hPen);
        //SelectObject( hdc, GetStockObject(BLACK_PEN) );

        Ellipse (hdc, x-NODE_RADIUS, y-NODE_RADIUS, x+NODE_RADIUS, y+NODE_RADIUS);
        //SelectObject (hdc, hPenOld);
        //DeleteObject (hPen);
        
        // draw node id
        if (show_node_id)
        {
            sprintf (str, "%d", EXTRACT_NODE_ID (self->id));
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
            
            g_aoi = self->aoi;
            g_id  = self->id;
            selected_x = x;
            selected_y = y;
            size_AN = nodes.size ()-1;
            
            // draw AOI
            //SelectObject( hdc, GetStockObject(HOLLOW_BRUSH) );
            Ellipse (hdc, x-aoi, y-aoi, x+aoi, y+aoi);
            //Ellipse( hdc, x-aoi_buf, y-aoi_buf, x+aoi_buf, y+aoi_buf );
            
            // draw neighbor dots
            for(j=0; j<(int)nodes.size (); j++)
            {
                int x = (int)nodes[j]->pos.x; 
                int y = (int)nodes[j]->pos.y;
                id_t id = nodes[j]->id;

                //SelectObject(hdc,GetStockObject(DC_PEN));
                //SetDCPenColor(hdc,RGB(0x00,0xff,0x00));

                // draw small circle
                
                //SelectObject (hdc, GetStockObject(GRAY_BRUSH));       
                if (j == 0 || is_EN (en_list, id))
                    hPenOld = (HPEN)SelectObject (hdc, hPenRed);
                else
                    hPenOld = (HPEN)SelectObject (hdc, hPenBlue);
                
                Ellipse (hdc, x-(NODE_RADIUS/2), y-(NODE_RADIUS/2), x+(NODE_RADIUS/2), y+(NODE_RADIUS/2));
                SelectObject (hdc, hPenOld);                    

                // draw node id
                if (show_node_id)
                {
                    sprintf (str, "%d", EXTRACT_NODE_ID (nodes[j]->id));
                    TextOut (hdc, x-5, y-25, str, strlen(str));
                }
            }

            // draw Voronoi edges
            if (show_edges) 
            {
                vector<line2d> &lines = GetEdges (i);
                for(j=0; j<(int)lines.size (); j++)
                {    
                    POINT points[2];
                    
                    points[0].x = (long)lines[j].seg.p1.x;
                    points[0].y = (long)lines[j].seg.p1.y;
                    points[1].x = (long)lines[j].seg.p2.x;
                    points[1].y = (long)lines[j].seg.p2.y;
                    
                    Polyline( hdc, points, 2 );                
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

    //sprintf( str, "step: %d (%d, %d) node: %d/%d [%d, %d] aoi: %d AN: %d %s %s %s char: [%d, %d]", g_steps, g_cursor.x-g_origin.x, g_cursor.y-g_origin.y, g_id, g_nodes_active, selected_x, selected_y, g_aoi, size_AN, str2, (follow_mode ? "[follow]" : ""), (step_mode ? "[stepped]" : ""), wvalue, lvalue);
    sprintf( str, "step: %d (%d, %d) node: %d/%d [%d, %d] aoi: %d AN: %d %s %s %s missing: %d", g_steps, g_cursor.x-g_origin.x, g_cursor.y-g_origin.y, g_id, g_nodes_active, selected_x, selected_y, g_aoi, size_AN, str2, (follow_mode ? "[follow]" : ""), (step_mode ? "[stepped]" : ""), missing_count);
    TextOut( hdc, 10-g_origin.x, 10-g_origin.y, str, strlen(str) );    
    
    // EndPaint balances off the BeginPaint call.
    EndPaint(hWnd,&ps);
}

VOID MoveOnce( HWND hWnd )
{
    if (finished == true)
        return;

    g_steps++;
    g_inconsistent = NextStep ();
    
    if (g_inconsistent < 0)
        finished = true;    
    else if (g_inconsistent > 0 && pause_at_incon)
    {
        step_mode = true;
        paused = true;
    }        
    
    // obtain positions of my current neighbors
    for (int i=0; i<g_para.NODE_SIZE; ++i)  
    {
        g_nodes[i] = &GetNeighbors (i);
        g_ENs[i] = &GetEnclosingNeighbors (i, EN_LEVEL);
    }
    
    InvalidateRect (hWnd, NULL, TRUE);     
}    

//-----------------------------------------------------------------------------
// Name: MsgProc()
// Desc: The window's message handler
//-----------------------------------------------------------------------------


LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
        case WM_DESTROY:
            ShutVASTsim ();
            PostQuitMessage( 0 );
            return 0;

        case WM_PAINT:
            Render( hWnd );
            //ValidateRect( hWnd, NULL );
            return 0;

        case WM_TIMER:
            if( paused == false && !finished )
                MoveOnce( hWnd );
            if( step_mode == true )
                paused = true;
            return 0;
       
        case WM_CHAR:
            last_char = wParam;
            // 'space' for next step
            lvalue = lParam;
            wvalue = wParam;
            
            if( wParam == ' ' )
            {
                paused = !paused;
                if( paused == true )
                    step_mode = true;
            }
            // 'enter' for toggling step mode
            else if( wParam == 13 )
            {
                step_mode = !step_mode;
                if (step_mode == false)
                    paused = false;
            }
            // turning on/off Voronoi edges
            else if( wParam == 'e' )
            {
                show_edges = !show_edges;
                InvalidateRect (hWnd, NULL, TRUE);
            }
            // turning on/off Voronoi edges
            else if( wParam == 'f' )
            {
                follow_mode = !follow_mode;
                InvalidateRect (hWnd, NULL, TRUE);
            }            
            // turning on/off Voronoi edges
            else if( wParam == 'c' )
            {
                pause_at_incon = !pause_at_incon;                
            }            
            // turning on/off node numbering
            else if( wParam == 'n' )
            {
                show_node_id = !show_node_id;
                InvalidateRect (hWnd, NULL, TRUE);
            }                        
            // quit and write log file
            else if( wParam == 'q' )
            {
                ShutVASTsim ();
                PostQuitMessage (0);
            }
            // move viewport UP
            else if( wParam == 'w' )
            {
                g_origin.y += 5; 
                follow_mode = false;
                InvalidateRect (hWnd, NULL, TRUE);
            }
            // move viewport DOWN
            else if( wParam == 's' )
            {
                g_origin.y -= 5;
                follow_mode = false;
                InvalidateRect (hWnd, NULL, TRUE);
            }
            // move viewport LEFT
            else if( wParam == 'a' )
            {
                g_origin.x += 5;
                follow_mode = false;
                InvalidateRect (hWnd, NULL, TRUE);
            }
            // move viewport RIGHT
            else if( wParam == 'd' )
            {
                g_origin.x -= 5;
                follow_mode = false;
                InvalidateRect (hWnd, NULL, TRUE);
            }
            // move viewport RIGHT
            else if( wParam == 'o' )
            {
                follow_mode = false;
                g_origin.x = g_origin.y = 0;
                InvalidateRect (hWnd, NULL, TRUE);
            }    
            // simply update status bar
            else
            {            
                RECT r;
                r.top = r.left = 0;
                r.right  = 800;
                r.bottom = 100;                 
                InvalidateRect (hWnd, &r, TRUE);
            }
            return 0;

        case WM_MOUSEMOVE:
            {
                g_cursor = MAKEPOINTS (lParam);
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
            }
            return 0;

        // toggle nodes showing their AOI
        case WM_RBUTTONDOWN:
            {
                // select current active node
                for (int i=0; i<g_para.NODE_SIZE; i++)
                {
                    Node *n = GetNode(i);
                    Position pt(g_cursor.x-g_origin.x, g_cursor.y-g_origin.y);
                    if (n->pos.dist (pt) <= NODE_RADIUS)
                    {
                        g_show_aoi[i] = !g_show_aoi[i];
                        InvalidateRect (hWnd, NULL, TRUE);
                        break;
                    }
                }
            }
            return 0;            
    }

    return DefWindowProc( hWnd, msg, wParam, lParam );
}



//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: The application's entry point
//-----------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE hInst, HINSTANCE, LPSTR, INT )
{
    // set default parameters if not available through config file
    if (ReadPara (g_para) == false)
    {
        g_para.VAST_MODEL      =  1;        // 1: Direct connection 2: Forwarding
        g_para.NET_MODEL       =  1;        // 1: emulated 2: emulated with bandwidth limit
        g_para.MOVE_MODEL      =  1;        // 1: random 2: cluster
        g_para.WORLD_WIDTH     =  800;
        g_para.WORLD_HEIGHT    =  600;
        g_para.NODE_SIZE       =  50;
        g_para.TIME_STEPS      =  1000;
        g_para.AOI_RADIUS      =  100;
        g_para.AOI_BUFFER      =  15;
        g_para.CONNECT_LIMIT   =  0;
        g_para.VELOCITY        =  5;
        g_para.LOSS_RATE       =  0;
        g_para.FAIL_RATE       =  0;        
    }
    
    InitVASTsim (g_para);
    
    // create nodes
    for (int i=0; i<g_para.NODE_SIZE; ++i)
    {
        CreateNode ();
        g_show_aoi[i] = false;
    }
   
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
                              WS_OVERLAPPEDWINDOW, 100, 100, 800, 600,
                              GetDesktopWindow (), NULL, hInst, NULL);

    // Show the window
    ShowWindow (hWnd, SW_SHOWDEFAULT);
    UpdateWindow (hWnd);
  

    // set timer
    SetTimer(hWnd,              // handle to main window 
        IDT_TIMER1,             // timer identifier 
        TIMER_INTERVAL,                     // 1-second interval 
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
