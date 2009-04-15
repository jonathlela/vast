
#ifdef _WINDOWS
// disable warning about "identifier exceeds'255' characters"
#pragma warning(disable: 4786)
#endif

#include <windows.h>
#include <wingdi.h>
#include <vector>
#include <map>

#include "vastverse.h"
using namespace VAST;
using namespace std;

// GUI setting
int     NODE_RADIUS     = 2;
POINT   g_origin;

bool    show_edges      = false;     // display Voronoi edges
bool    show_node_id    = false;     // display node IDs

vastverse world (VAST_MODEL_DIRECT, VAST_NET_EMULATED, 0);
voronoi *v;

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
    
    // set proper origin
    SetViewportOrgEx (hdc, g_origin.x, g_origin.y, NULL);

    char str[160];

    int n = v->size ();    

    int selected_x = 0 , selected_y =0;

    HPEN hPenRed = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
    HPEN hPenBlue = CreatePen(PS_SOLID, 2, RGB(0, 0, 255));
//    HPEN hPenOld;
   
    // loop & print each node
    for (int i=1; i<=n; ++i)
    {              
        Position pos = v->get (i);

        double     x = pos.x;
        double     y = pos.y;
        
        // draw big circle
        SelectObject (hdc, GetStockObject(HOLLOW_BRUSH));
        Ellipse (hdc, (int)x-NODE_RADIUS, (int)y-NODE_RADIUS, (int)x+NODE_RADIUS, (int)y+NODE_RADIUS);

        // draw node id
        if (show_node_id)
        {
            sprintf (str, "%d", i);
            TextOut (hdc, (int)(x-5), (int)(y-25), str, (int)strlen(str));
        }
    }

    
    // draw Voronoi edges
    if (show_edges) 
    {
        vector<line2d> &lines = v->getedges ();
        for(int j=0; j<(int)lines.size (); j++)
        {    
            POINT points[2];
            
            points[0].x = (long)lines[j].seg.p1.x;
            points[0].y = (long)lines[j].seg.p1.y;
            points[1].x = (long)lines[j].seg.p2.x;
            points[1].y = (long)lines[j].seg.p2.y;
            
            Polyline (hdc, points, 2);                
        }                
    }
   
    sprintf (str, "total nodes: %d", n);
    TextOut (hdc, (int)10-g_origin.x, (int)10-g_origin.y, str, (int)strlen(str) );    
    
    // EndPaint balances off the BeginPaint call.
    EndPaint(hWnd,&ps);
}


//-----------------------------------------------------------------------------
// Name: MsgProc()
// Desc: The window's message handler
//-----------------------------------------------------------------------------


LRESULT WINAPI MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch (msg)
    {
        case WM_DESTROY:
            PostQuitMessage( 0 );
            return 0;

        case WM_PAINT:
            Render( hWnd );
            //ValidateRect( hWnd, NULL );
            return 0;

        case WM_TIMER:
            return 0;
       
        case WM_CHAR:
            
            // turning on/off Voronoi edges
            if( wParam == 'e' )
            {
                show_edges = !show_edges;
                InvalidateRect (hWnd, NULL, TRUE);
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
                PostQuitMessage (0);
            }
            // move viewport UP
            else if( wParam == 'w' )
            {
                g_origin.y += 5; 
                InvalidateRect (hWnd, NULL, TRUE);
            }
            // move viewport DOWN
            else if( wParam == 's' )
            {
                g_origin.y -= 5;
                InvalidateRect (hWnd, NULL, TRUE);
            }
            // move viewport LEFT
            else if( wParam == 'a' )
            {
                g_origin.x += 5;
                InvalidateRect (hWnd, NULL, TRUE);
            }
            // move viewport RIGHT
            else if( wParam == 'd' )
            {
                g_origin.x -= 5;
                InvalidateRect (hWnd, NULL, TRUE);
            }
            // set origin
            else if( wParam == 'o' )
            {
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
                //g_cursor = MAKEPOINTS (lParam);
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
INT WINAPI WinMain (HINSTANCE hInst, HINSTANCE, LPSTR, INT)
{

    // create the Voronoi class        
    v = world.create_voronoi ();

    // read the input data set
    FILE *fp;
    char inputfile[] = "input.txt";

    if ((fp = fopen (inputfile, "rt")) == NULL)
    {
        printf ("cannot read input file '%s'\n", inputfile);
        exit (0);
    }

	// get interest radius
	double interest_radius = 0;
    
    double x, y;
    int n = 0;
    while (fscanf (fp, "%lf %lf\n", &x, &y) != EOF)
    {
        Position pt (x, y);
        printf ("reading site [%d] at (%lf, %lf)\n", ++n, pt.x, pt.y);

        // insert the points and build the voronoi diagram
        v->insert  (n, pt);
    }


   
    // Register the window class
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L,
                      hInst, 
                      LoadIcon(NULL,IDI_APPLICATION),
                      LoadCursor(NULL,IDC_ARROW),
                      (HBRUSH)(COLOR_WINDOW+1),
                      NULL,
                      "VisVoronoi", NULL };
    RegisterClassEx (&wc);

    // Create the application's window
    HWND hWnd = CreateWindow ("VisVoronoi", "Voronoi Visualizer",
                              WS_OVERLAPPEDWINDOW, 100, 100, 800, 800,
                              GetDesktopWindow (), NULL, hInst, NULL);

    // Show the window
    ShowWindow (hWnd, SW_SHOWDEFAULT);
    UpdateWindow (hWnd);
   
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
    
    UnregisterClass ("VisVoronoi", hInst);
    
    return (int)msg.wParam;
}
