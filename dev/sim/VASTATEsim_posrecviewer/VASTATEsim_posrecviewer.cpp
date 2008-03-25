
/*
 *  VASTATESIM_gui.cpp (GUI interface for VASTATESIM)
 *
 *
 */

#include "vastatesim.h"
#include "vastate.h"
#include "attributebuilder.h"

#include "errout.h"

#include <windows.h>
#include "dialogs.h"	// need windows.h

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <string>
#include <sstream>

// Define a fixed buffer size to speed up outputing
//   or "//" it to show all debug messages
//#define DEBUGMSG_BUFFER_SIZE 2048
#define MAX_LOADSTRING 100
const int STEP_TIME_INTERVAL = 200;  // (ms)
const int ID_INITBUTTON = 1;

// Layout definition
int LAYOUT_POSMAP_X_ORIG = 0;
int LAYOUT_POSMAP_Y_ORIG = 15;
#define POSMAP_X(x) ((int)((x)+LAYOUT_POSMAP_X_ORIG))
#define POSMAP_Y(x) ((int)((x)+LAYOUT_POSMAP_Y_ORIG))
#define GAMEPOS_X(x) ((x)-LAYOUT_POSMAP_X_ORIG)
#define GAMEPOS_Y(x) ((x)-LAYOUT_POSMAP_Y_ORIG)
/////////////////////

// linker structure for mouse selection
struct ITEM_LINK
{
	Position pos;
	int type;
	int index;
	int sub_index;
};

// UI painting parameters
POINT last_mouse_pos;

int last_mouse_pointed_index;
int selected_index;
ITEM_LINK lselected;
ITEM_LINK rselected;

vector<ITEM_LINK *> linkers;
bool bMoveUpdate;
bool bFullUpdate;
int debugAttachTo;

string imagefile_title, imagefile;

// Global Variables:
HINSTANCE hInst;								// current instance
HBRUSH hBrushes[10];
	enum {HBS_BLUE=0, HBS_RED
		, HBS_MAX};
HWND hWndMain = NULL;
HWND hWndInitButton = NULL;

TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// The title bar text
TCHAR szEditClass[MAX_LOADSTRING];			// The title bar text
const TCHAR szHelp [] = 
" *** VASTATE Simulator - position image viewer help *** \n\n"                              \
" Keyboard mappings : \n\n"                                  \
"   While un-initialize: \n"                                 \
" p -         Select PLAY file \n"                           \
" r -         Select RECORD file \n"                         \
" q -         Quit simulator \n\n"                           \
"   While initialized: \n"                                   \
" [Enter] -   Run/Stop the simulation \n"                    \
" [Space] -   Run one step \n"                               \
" v -         Show/Hide voronoi edge(s) \n"                  \
" r -         Redraw \n"                                     \
" g -         Show/Hide game behavior detail \n"             \
" f -         Show/Hide full information (food & behavior reg) \n" \
" m -         Show debug message window \n"                  \
" a/s/d/f/o - Move map to l/d/r/u/ori. \n"                   \
" h -         Show thie help \n"                             \
" q -         Quit simulator \n"                             \
" \n "                                                       \
" ************************************ "                     \
"\n\n\n";

// GUI running variables
bool bInited;
bool bRunning;
int  bShowVoronoi;
bool bRunThisStep;
bool bFinish;
int  timestamp;
bool bLButtonDown;
///////////////////////

RecordFileHeader sim_para;
vector<NodeInfo> g_nodes;

// Foward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    WndProcEdit(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

double dist (POINT p1, POINT p2)
{
    return sqrt ( pow (p1.x - p2.x, 2) + pow (p1.y - p2.y, 2) );
}


bool Nearby (Position& p1, Position& p2, int dist = 15)
{	
	if ((abs(p1.x-p2.x) <= dist)
		&& (abs(p1.y-p2.y) <= dist))
		return true;
	return false;
}

ITEM_LINK * findSelectedLinker (Position& mouse_pos)
{
	ITEM_LINK * selected = NULL;
	int &w = para.WORLD_WIDTH;
	int &h = para.WORLD_HEIGHT;
	double shortest_dis = 15;
	int tl = linkers.size();
	for (int i = 0; i < tl; i++)
	{
		Position linker_pos (POSMAP_X(linkers[i]->pos.x), POSMAP_Y(linkers[i]->pos.y));
		if (linker_pos.dist(mouse_pos) < shortest_dis)
		{
			selected = linkers[i];
			shortest_dis = linker_pos.dist(mouse_pos);
		}
	}
	
	return selected;
}

void InitPlayback ()
{
    pf.initRecord (filename);
    
    ShowWindow (hWndInitButton, false);
    
    bInited = true;
    bRunning = false;
	bRunThisStep = false;
}


VOID Render ( HWND hWnd )
{
    PAINTSTRUCT ps;
	int objc;
	Position *atts;
	food_reg *foods;
	arbitrator_info * arbs;
	int arbs_size, arbs_content_size;
	voronoi * vor;
	char msg[512], msg2[256];
	char toplineinfo [256];
	RECT rt;
	GetClientRect (hWnd, &rt);

    HDC hdc = BeginPaint(hWnd, &ps);
	SelectObject (hdc, GetStockObject (DEFAULT_GUI_FONT));

    sprintf (toplineinfo, "%s%sStep %d gamepos (%d,%d) Image file:%s",
		//  if finish show [FINISH], else check if running continually, show [RUNNING] for true.
		bFinish?"[FINISH] ":bRunning?"[RUNNING] ":"",
		bShowVoronoi?"[VORONOI] ":"",
		timestamp - 1,
		(int)GAMEPOS_X(last_mouse_pos.x), (int)GAMEPOS_Y(last_mouse_pos.y),
        imagefile_title.empty()?"(None)":imagefile_title.c_str ()
        );

	/*
	if ((bMoveUpdate == true) && (bFullUpdate == false))
	{
		bMoveUpdate = false;
		goto updatetop;
	}
	*/

	if (!bInited)
	{
		sprintf (toplineinfo, "Please selec a image file to view.");
	}

	else
	{
		SelectObject (hdc, GetStockObject (NULL_BRUSH));
		Rectangle (hdc, POSMAP_X(0), POSMAP_Y(0), POSMAP_X(para.WORLD_WIDTH), POSMAP_Y(para.WORLD_HEIGHT));

		// Draw player
		if (linkers.size() > 0)
		{
			int total = linkers.size();
			for (int i = 0; i < total; i++)
				delete (ITEM_LINK *) linkers[i];
			linkers.clear();
		}

		SelectObject (hdc, GetStockObject (NULL_BRUSH));
		for (objc = 0; objc < para.NODE_SIZE; objc ++)
		{
			object* obj = GetPlayerNode (objc);
			if (obj == NULL)
                continue;

			ITEM_LINK * new_link = new ITEM_LINK;
			new_link->pos = obj->get_pos();
			new_link->type = 0;
			new_link->index = objc;
			linkers.push_back(new_link);

			Position& p = obj->get_pos ();
			player_info pi;
			bool success_get_player_info = GetPlayerInfo (objc, &pi);

			Ellipse (hdc, (int)POSMAP_X(p.x-3), (int)POSMAP_Y(p.y-3), (int)POSMAP_X(p.x+3), (int)POSMAP_Y(p.y+3));

			if ((lselected.type == 0) && (lselected.index == objc))
			{
				int aoi = (int)(GetAOI (objc));
				Ellipse(hdc, 
					(int)POSMAP_X(p.x-aoi),
					(int)POSMAP_Y(p.y-aoi),
					(int)POSMAP_X(p.x+aoi),
					(int)POSMAP_Y(p.y+aoi));

				if (success_get_player_info)
				{
					MoveToEx(hdc, (int)POSMAP_X(p.x), (int)POSMAP_Y(p.y), NULL);
					LineTo (hdc, (int)POSMAP_X(pi.dest.x), (int)POSMAP_Y(pi.dest.y));

					if (pi.foattr.x != -1)
					{
						MoveToEx(hdc, POSMAP_X(p.x), POSMAP_Y(p.y), NULL);
						LineTo (hdc, POSMAP_X(pi.foattr.x), POSMAP_Y(pi.foattr.y));
						TextOut (hdc, POSMAP_X((p.x+pi.foattr.x)/2), POSMAP_Y((p.y+pi.foattr.y)/2), "A", 1);
					}
				}
			}
				
			if (obj->peer != 0)
			{
				AttributeBuilder ab;
				if (success_get_player_info && bShowGameDetail)
				{
					sprintf (msg, "%d %c%c", obj->peer, pi.last_action[0], pi.last_action[1]);

					if (bShowDetail)
					{
						if (pi.waiting == 0)
							sprintf (msg2, " T%d A%d", pi.tiredness, pi.angst);
						else
							sprintf (msg2, " W%d T%d A%d", pi.waiting, pi.tiredness, pi.angst);
					}
				}
				else
				{
//				string na;
//				int hp, mhp;
//				obj->get (0, na);
//				obj->get (1, hp);
//				obj->get (2, mhp);
//				sprintf (msg, "[%d](%d)\"%s\" %d/%d", obj->get_id() , obj->peer , na.c_str(), hp, mhp);
				string s = ab.getPlayerName(*obj);
//				sprintf (msg, "[%d](%d)\"%s\" %d/%d",
//				sprintf (msg, "[%d](%d) %d/%d",
//					sprintf (msg, "%d",
				sprintf (msg, "%s", 
//								obj->get_id(), 
//								obj->peer
//								"",
								s.c_str()
//								ab.getPlayerName(*obj, 1),
//								ab.getPlayerHP(*obj),
//								ab.getPlayerMaxHP(*obj)
								);
				}
				TextOut (hdc, POSMAP_X(p.x-3), POSMAP_Y(p.y+4), msg, strlen(msg));
			}
			else
			{
				sprintf (msg, "Not Player!");
				TextOut (hdc, POSMAP_X(p.x-3), POSMAP_Y(p.y+4), msg, strlen(msg));
			}

		}

		// Draw Arbitrator
		arbs = NULL;
		arbs_size = 0;
		arbs_content_size = 0;
		SelectObject (hdc, hBrushes[HBS_RED]);
		if (bShowVoronoi)
			vor = create_voronoi();

		for (objc = 0; objc < para.NODE_SIZE; objc ++)
		{
			arbs_content_size = GetArbitratorInfo (objc, NULL);
			if (arbs_content_size == 0)
				continue;

			if ((arbs == NULL) || (arbs_size < arbs_content_size))
			{
				if (arbs != NULL)
					delete[] arbs;
				arbs = new arbitrator_info[arbs_content_size];
				arbs_size = arbs_content_size;
			}
			
			GetArbitratorInfo(objc, arbs);
			for (int ari=0; ari < arbs_content_size; ari++)
			{
                if (arbs[ari].id == NET_ID_UNASSIGNED)
                    continue;

				Position &tp = arbs[ari].pos;

				ITEM_LINK * new_link = new ITEM_LINK;
				new_link->pos = tp;
				new_link->type = 1;
				new_link->index = objc;
				new_link->sub_index = ari;
				linkers.push_back(new_link);

				Ellipse (hdc, POSMAP_X(tp.x-4), POSMAP_Y(tp.y-4), 
							  POSMAP_X(tp.x+4), POSMAP_Y(tp.y+4));

				sprintf (msg, "%d", arbs[ari].id);
				TextOut (hdc, POSMAP_X(tp.x-3), POSMAP_Y(tp.y+4), msg, strlen(msg));

				if (bShowVoronoi)
					vor->insert(arbs[ari].id, arbs[ari].pos);
			}
			if (lselected.type == 1)
			{
				int aoi = (int) (GetArbAOI(lselected.index, lselected.sub_index));
				Position &tp = lselected.pos;
				SelectObject (hdc, GetStockObject(NULL_BRUSH));
				Ellipse (hdc, POSMAP_X(tp.x-aoi), POSMAP_Y(tp.y-aoi), 
					POSMAP_X(tp.x+aoi), POSMAP_Y(tp.y+aoi));
			}
		}

        if (bShowVoronoi)
		{
            POINT points[2];
            int s = vor->size ();
			vector<line2d> &edges = vor->getedges();
            for (unsigned int j = 0; j < edges.size (); j ++)
			{
                points[0].x = (long) POSMAP_X(edges[j].seg.p1.x);
                points[0].y = (long) POSMAP_Y(edges[j].seg.p1.y);
                points[1].x = (long) POSMAP_X(edges[j].seg.p2.x);
                points[1].y = (long) POSMAP_Y(edges[j].seg.p2.y);
                
                Polyline (hdc, points, 2);
			}
		}
		
		if (arbs != NULL)
		{
			delete[] arbs;
			arbs = NULL;
			arbs_size = 0;
			arbs_content_size = 0;
		}

        if (bShowVoronoi)
			destroy_voronoi (vor);

		// Draw Attractor
        if (IsPlayMode ())
            goto LABEL_OutAttractor;
		atts = new Position[para.ATTRACTOR_MAX_COUNT];
		int ac, totala = GetAttractorPosition (atts);
		for (ac = 0; ac < totala; ac++)
		{
			Position& p = atts[ac];
			SelectObject (hdc, GetStockObject (GRAY_BRUSH));
			Ellipse (hdc, POSMAP_X(p.x-3), POSMAP_Y(p.y-3), POSMAP_X(p.x+3), POSMAP_Y(p.y+3));
			SelectObject (hdc, GetStockObject (NULL_BRUSH));
			Ellipse (hdc, POSMAP_X(p.x-5), POSMAP_Y(p.y-5), POSMAP_X(p.x+5), POSMAP_Y(p.y+5));

			if ((lselected.type == 2) && (lselected.pos == p))
			{
				int dist = para.ATTRACTOR_RANGE;
				Ellipse (hdc, POSMAP_X(p.x-dist), POSMAP_Y(p.y-dist), POSMAP_X(p.x+dist), POSMAP_Y(p.y+dist));
			}

			ITEM_LINK * new_link = new ITEM_LINK;
			new_link->pos = p;
			new_link->type = 2;
			new_link->index = ac;
			new_link->sub_index = 0;
			linkers.push_back(new_link);
		}
		delete[] atts;
LABEL_OutAttractor:


		// Draw food
		int fc, totalc = GetFoods (NULL);
		foods = new food_reg [totalc];
		GetFoods (foods);

		SelectObject (hdc, hBrushes[HBS_BLUE]);
		for (fc =0; fc < totalc; fc ++)
		{
			ITEM_LINK * new_link = new ITEM_LINK;
			new_link->pos = foods[fc].pos;
			new_link->type = 3;
			new_link->index = foods[fc].id;
			new_link->sub_index = 0;
			linkers.push_back(new_link);

			Position& p = foods[fc].pos;
			Ellipse (hdc, POSMAP_X(p.x-3), POSMAP_Y(p.y-3), POSMAP_X(p.x+3), POSMAP_Y(p.y+3));

			if (bShowDetail)
			{
				sprintf (msg, "%08x", foods[fc].id);//foods[fc].count);
				TextOut (hdc, POSMAP_X(p.x-3), POSMAP_Y(p.y+5), msg, strlen(msg));
			}
		}
		delete[] foods;
	}
	bFullUpdate = false;
	bMoveUpdate = false;

//updatetop:
	//strcat (toplineinfo, "\n");
	strcat (toplineinfo, "                            ");
	DrawText (hdc, toplineinfo, strlen(toplineinfo), &rt, DT_LEFT | DT_TOP);

	errout e;
	e.output("");

	EndPaint(hWnd,&ps);
}



 int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	MSG msg;

    bInited = false;
	bRunning = false;
	bShowVoronoi = true;
	bRunThisStep = false;
	bFinish = false;

    bLButtonDown = false;

	last_mouse_pos.x = 0;
	last_mouse_pos.y = 0;
	last_mouse_pointed_index = -1;
	lselected.type = -1;
	rselected.type = -1;

    if (argc >= 2)
    {
        imagefile = argc[1];
        imagefile_title = argc[1];
        
    }

	// Initialize global strings
	strcpy (szTitle, "VASTATESIM Simulator Position Image Viewer v0.01");
	strcpy (szWindowClass, "VASTATESIMPIV");
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) 
	{
		return FALSE;
	}

	// Main message loop:
	ZeroMemory (&msg, sizeof (msg));
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		TranslateMessage(&msg);
		if (msg.message == WM_CHAR)
		{
            if (!bSimInited)
            {
                if ((msg.wParam == '\r') || (msg.wParam == ' '))
                    PostMessage (hWndMain, WM_COMMAND, MAKEWORD (ID_INITBUTTON, 0), 0);
                else if (msg.wParam == 'q')
                    PostMessage (hWndMain, WM_CLOSE, 0, 0);
            }
            else
            {
			    if (msg.wParam == '\r')
			    {
				    bRunning = !bRunning;
				    step_start = 0;
			    }
			    else if (msg.wParam == 'v')
				    bShowVoronoi = !bShowVoronoi;
			    else if (msg.wParam == ' ')
			    {
				    step_start = 0;
				    if (bRunning == true)
					    bRunning = false;
				    else
                        bRunThisStep = true;
			    }
			    else if (msg.wParam == 'r')
				    bFullUpdate = true;
			    else if (msg.wParam == 'm')
			    {
				    if (hWndLogWindow != NULL)
				    {
					    ShowWindow (hWndLogWindow, true);
					    UpdateWindow (hWndLogWindow);
					    bFullUpdate = true;
					    UpdateWindow (hWndMain);
				    }
			    }
			    else if (msg.wParam == 'g')
				    bShowGameDetail = !bShowGameDetail;
			    else if (msg.wParam == 'f')
				    bShowDetail = !bShowDetail;
			    else if (msg.wParam == 'h')
				    MessageBox (NULL, szHelp, "VASTATESIM Simulator PIV Help", MB_OK);
                else if (msg.wParam == 'q')
                    PostMessage (hWndMain, WM_CLOSE, 0, 0);
                else if (msg.wParam == 'w')
                    LAYOUT_POSMAP_Y_ORIG += 5;
                else if (msg.wParam == 's')
                    LAYOUT_POSMAP_Y_ORIG -= 5;
                else if (msg.wParam == 'a')
                    LAYOUT_POSMAP_X_ORIG += 5;
                else if (msg.wParam == 'd')
                    LAYOUT_POSMAP_X_ORIG -= 5;
                else if (msg.wParam == 'o')
                {
                    LAYOUT_POSMAP_X_ORIG = 0;
                    LAYOUT_POSMAP_Y_ORIG = 15;
                }
			    else
				    DispatchMessage(&msg);

                InvalidateRect (hWndMain, NULL, true);
            }
		}
		else
			DispatchMessage(&msg);
	}

	if (bInited)
		ShutV2sim();

	UnregisterClass ("VASTATESIMGUI", hInstance);
    UnregisterClass ("VASTATESIMGUI_DEBUG", hInstance);

	for (int obs =0; obs != HBS_MAX; obs++)
		DeleteObject (hBrushes[obs]);

	return msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage is only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(NULL, IDI_APPLICATION);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(NULL, IDI_APPLICATION);
	
	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HANDLE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance;

   // create brush(es)
   hBrushes[HBS_BLUE] = CreateSolidBrush (RGB (  0,  0, 255));
   hBrushes[HBS_RED]  = CreateSolidBrush (RGB (255,  0,   0));

   hWndMain = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, 800, 450, NULL, NULL, hInstance, NULL);

   if (!hWndMain)
   {
      return FALSE;
   }

   ShowWindow(hWndMain, nCmdShow);
   UpdateWindow(hWndMain);

   SetTimer(hWndMain,
			1,
			STEP_TIME_INTERVAL,
			(TIMERPROC) NULL);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	errout e;
	//RECT rt;
	RECT rtl, rtw;
	char ostr[256];
    char filename[256], filetitle[256];
	bool needRedraw;
	Position mouse_pos;
	ITEM_LINK * lk;
	
	switch (message) 
	{
		case WM_CREATE:
			hWndInitButton = CreateWindow (TEXT ("button"), "Init",
					WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
					0, 0, 0, 0, hWnd, (HMENU) ID_INITBUTTON, hInst, NULL);
			
			PopFileInitialize (hWnd);
			break;

		case WM_MOVE:
			attachDebug(debugAttachTo);
			break;

		case WM_SIZE:
//			MoveWindow (hWndEdit, 0, HIWORD (lParam)-LOGBOX_HEIGHT, LOWORD (lParam), LOGBOX_HEIGHT, TRUE);
			MoveWindow (hWndInitButton, LOWORD (lParam) - (65+1), 20, 60, 20, true);
			break;
		
		case WM_COMMAND:
			needRedraw = false;
			if (LOWORD (wParam) == ID_INITBUTTON)
			{
                if (PopFileSaveDlg (hWnd, filename, filetitle))
                {
                    imagefile = filename;
                    imagefile_title = filetitle;
                    InitPlayback ();
                }
			}
			else if (LOWORD (wParam) == ID_RBUTTON)
			{
                simulation_mode = VS_RECORD_MODE;
                strcpy (filename, foodfile.c_str ());
                strcpy (filetitle, foodfile.c_str ());

                BOOL food_succ, action_succ;
				if (food_succ = PopFileSaveDlg (hWnd, filename, filetitle))
				{
                    foodfile = filename;
                    foodfile_title = filetitle;
				}

                strcpy (filename, actionfile.c_str ());
                strcpy (filetitle, actionfile_title.c_str ());
                if (action_succ = PopFileSaveDlg (hWnd, filename, filetitle))
                {
                    actionfile = filename;
                    actionfile_title = filetitle;
                }

                if (!food_succ || !action_succ)
                {
                    foodfile = "";
                    foodfile_title = "";
                    actionfile = "";
                    actionfile_title = "";
                }

                if (!actionfile.empty () && !foodfile.empty ())
                    needRedraw = true;
			}
			else if (LOWORD (wParam) == ID_PBUTTON)
			{
                simulation_mode = VS_PLAY_MODE;
                strcpy (filename, foodfile.c_str ());
                strcpy (filetitle, foodfile.c_str ());

                BOOL food_succ, action_succ;
				if (food_succ = PopFileOpenDlg (hWnd, filename, filetitle))
				{
                    foodfile = filename;
                    foodfile_title = filetitle;
				}

                strcpy (filename, actionfile.c_str ());
                strcpy (filetitle, actionfile_title.c_str ());
                if (action_succ = PopFileOpenDlg (hWnd, filename, filetitle))
                {
                    actionfile = filename;
                    actionfile_title = filetitle;
                }

                if (!food_succ || !action_succ)
                {
                    foodfile = "";
                    foodfile_title = "";
                    actionfile = "";
                    actionfile_title = "";
                }

                if (!actionfile.empty () && !foodfile.empty ())
                    needRedraw = true;
			}
			
			if (needRedraw == true)
			{
				needRedraw = false;
				bFullUpdate = true;
				SetFocus(hWndMain);
				InvalidateRect (hWndMain, NULL, true);
			}

			break;

		case WM_MOUSEMOVE:
			if (!(bInited && bSimInited)) break;

            if (bLButtonDown)
            {
                POINT pt;
			    pt.x = (long) LOWORD (lParam);
			    pt.y = (long) HIWORD (lParam);

                if (dist (pt, last_mouse_pos) > 1)
                {
                    LAYOUT_POSMAP_X_ORIG += (pt.x - last_mouse_pos.x);
                    LAYOUT_POSMAP_Y_ORIG += (pt.y - last_mouse_pos.y);

                    last_mouse_pos = pt;
                    InvalidateRect (hWnd, NULL, true);
                }
            }
            /*
			GetClientRect  (hWnd, &rt);
			last_mouse_pos.x = (long) LOWORD(lParam);
			last_mouse_pos.y = (long) HIWORD(lParam);

			last_mouse_pointed_index = -1;
			if ((last_mouse_pos.x >= POSMAP_X(0)) && (last_mouse_pos.y >= POSMAP_Y(0))
				&& (last_mouse_pos.x <= POSMAP_X(para.WORLD_WIDTH))
				&& (last_mouse_pos.y <= POSMAP_Y(para.WORLD_HEIGHT)))
			{
				int tl = linkers.size();
				for (int i = 0; i < tl; i++)
				{
					Position mouse_pos (last_mouse_pos.x, last_mouse_pos.y);
					Position linker_pos (POSMAP_X(linkers[i]->pos.x), POSMAP_Y(linkers[i]->pos.y));
					if (Nearby(linker_pos, mouse_pos))
					{
						last_mouse_pointed_index = linkers[i]->index;
						break;
					}
				}
			}
			
			rt.bottom = 20;
			if (!bRunning && !bFullUpdate) bMoveUpdate = true;
			InvalidateRect (hWnd, NULL, false);
            */
			break;

		case WM_LBUTTONDOWN:
			mouse_pos.x = LOWORD(lParam);
			mouse_pos.y = HIWORD(lParam);

			lk = findSelectedLinker (mouse_pos);
			if (lk == NULL)
				lselected.type = -1;
			else
				lselected = *lk;
			
			bFullUpdate = true;
			InvalidateRect (hWndMain, NULL, true);

            bLButtonDown = true;
            last_mouse_pos.x = (long) LOWORD (lParam);
            last_mouse_pos.y = (long) HIWORD (lParam);
			break;

			//temp_selected_index = selected_index;
			//selected_index = -1;
			/*
			if ((mouse_pos.x >= POSMAP_X(0)) && (mouse_pos.y >= POSMAP_Y(0))
				&& (mouse_pos.x <= POSMAP_X(para.WORLD_WIDTH))
				&& (mouse_pos.y <= POSMAP_Y(para.WORLD_HEIGHT)))
			{
				int tl = linkers.size();
				for (int i = 0; i < tl; i++)
				{
					//Position mouse_pos (last_mouse_pos.x, last_mouse_pos.y);
					Position linker_pos (POSMAP_X(linkers[i]->pos.x), POSMAP_Y(linkers[i]->pos.y));
					if (Nearby(linker_pos, mouse_pos))
					{
						selected_index = linkers[i]->index;
						break;
					}
				}
			}
			*/
        case WM_LBUTTONUP:
            bLButtonDown = false;
            break;

		case WM_RBUTTONDOWN:
			mouse_pos.x = LOWORD(lParam); 
			mouse_pos.y = HIWORD(lParam);
			lk = findSelectedLinker (mouse_pos);
			if (lk != NULL)
			{
				switch (lk->type)
				{
				case 0: // Player
					MessageBox(NULL, GetPlayerInfo(lk->index)
						, "Player INFO", MB_OK);
					break;
				case 1: // Arbitrator
					MessageBox(NULL, GetArbitratorString (lk->index, lk->sub_index)
						, "Arb Info", MB_OK);
					break;
				case 2:	// Attractor
					break;

				case 3:	// Food
					MessageBox (NULL, GetFoodInfo (lk->index), "Food Info", MB_OK);
					break;
				default:
					;
				}
			}
            bLButtonDown = false;
			break;

		case WM_PAINT:
			Render (hWnd);
			break;

		case WM_CHAR:
			if (wParam == '\r')
				bRunning = !bRunning;

			else if (wParam == 'v')
				bShowVoronoi = !bShowVoronoi;

			else if (wParam == 'r')
				;//InvalidateRect (hWnd, NULL, true);

			else if (wParam == ' ')
				bRunThisStep = true;

//			else if (wParam == 'h')
//				;

			InvalidateRect (hWnd, NULL, true);
			break;

		case WM_TIMER:
			if (bInited && bSimInited && (!bFinish) && (bRunning || bRunThisStep))
			{
				errout eo;

				if (timestamp >= para.TIME_STEPS)
				{
					bFinish = true;

					std::ostringstream out;
					out << "[FINISHED]" << szTitle;

					strcpy (szTitle, out.str().c_str());
					SetWindowText (hWnd, szTitle);

                    refreshStatistics ();

					InvalidateRect (hWnd, NULL, true);
					break;
				}
#ifdef DEBUGMSG_CLEAN_BY_STEP
                SetWindowText (hWndEdit, "");
#endif

				long startClock, endClock, midClock;

				startClock = clock ();
				NextStep ();
				midClock = clock ();
				ProcessMsg ();
				endClock = clock ();
				proTime = endClock - startClock;
				if (timestamp == 0)
					min_proTime = max_proTime = proTime;
				if (proTime < min_proTime)
					min_proTime = proTime;
				if (proTime > max_proTime)
					max_proTime = proTime;
				total_proTime += proTime;
				InvalidateRect (hWnd, NULL, true);
                errmain.refreshOutput ();

				bRunThisStep = false;
				if (bRunning)
				{
					if (step_start != 0)
						step_time = clock () - step_start;
					else
						step_time = 0;
					step_start = clock ();
				}
				sprintf (ostr, "---------- Step %d (all %0.3lf process %0.3lf(%0.3lf+%0.3lf) secs) ----------" NEWLINE,
					timestamp,
					(double) step_time / (double) CLK_TCK, 
					(double) proTime / (double) CLK_TCK,
					(double) (midClock - startClock) / (double) CLK_TCK,
					(double) (endClock - midClock) / (double) CLK_TCK
					);
				eo.output(ostr);

                timestamp ++;
			}
			break;

		case WM_SETFOCUS:
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;
			
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}

