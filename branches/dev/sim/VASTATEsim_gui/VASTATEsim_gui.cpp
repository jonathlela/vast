
/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007-2008 Shao-Jhen Chang (cscxcs at gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*
 *  VASTATESIM_gui.cpp (GUI interface for VASTATESIM)
 *
 *
 */

#include "vastatesim.h"
#include "vastate.h"
#include "attributebuilder.h"

#include "vastutil.h"

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
#define DEBUGMSG_CLEAN_BY_STEP
#define DEFAULT_STEP_TIME_INTERVAL (200)
#define DEFAULT_WINDOW_TITLE "VASTATESIM Simulator GUI v0.02"
#define MAX_BUFFER_SIZE (10240)

// UI painting parameters
#define LOGBOX_HEIGHT 150
POINT last_mouse_pos;

int STEP_TIME_INTERVAL = DEFAULT_STEP_TIME_INTERVAL;  // (ms)

// an ITEM_LINK is that an item show on the map, including peers, objects, arbitrators, ...
class ITEM_LINK
{
public:
    ITEM_LINK ()
    {
        memset (this, 0, sizeof(ITEM_LINK));
    }
    ~ITEM_LINK () {}

	Position pos;
	int type;
	int index;
	int sub_index;
};

int last_mouse_pointed_index;
//ITEM_LINK * last_mouse_pointed;
int selected_index;
ITEM_LINK lselected;
ITEM_LINK rselected;
int       lselect_skip, rselect_skip;

// the linkers list stores all linkers on the map, and will be refreshed on each redraw
// used to find on which user clicks
vector<ITEM_LINK *> linkers;
bool bMoveUpdate;
bool bFullUpdate;
//bool bDebugAttached;
int debugAttachTo;

int simulation_mode;
char *SIMULATION_MODE_STR[] = {"NORMAL", "RECORD", "PLAY"};
string actionfile, actionfile_title;
string foodfile, foodfile_title;

string sideinfo;

// process time
long proTime;
long max_proTime, min_proTime, total_proTime;
// total running time (in continues running mode)
long step_time;
long step_start;
//////////////////////

// Layout definition
///////////////////////////////////////
// Macros
#define POSMAP_X(x) ((int)(((x)+LAYOUT_POSMAP_X_ORIG)*LAYOUT_POSMAP_RATE))
#define POSMAP_Y(x) ((int)(((x)+LAYOUT_POSMAP_Y_ORIG)*LAYOUT_POSMAP_RATE))
#define GAMEPOS_X(x) ((int)(((x)-LAYOUT_POSMAP_X_ORIG)/LAYOUT_POSMAP_RATE))
#define GAMEPOS_Y(x) ((int)(((x)-LAYOUT_POSMAP_Y_ORIG)/LAYOUT_POSMAP_RATE))
// Variables
int LAYOUT_POSMAP_X_ORIG = 0;
int LAYOUT_POSMAP_Y_ORIG = 15;
int last_orig_x, last_orig_y;
double LAYOUT_POSMAP_RATE = 1.0;

bool LAYOUT_HUD_FLOAT = true;
int LAYOUT_HUD_FLOAT_X = 280;
int LAYOUT_HUD_FLOAT_Y = -15;

bool RUNMODE_RELAX = false;
bool RUNMODE_TOSTEP = false;
// Data buffers
///////////////////////////////////////
arbitrator_info * arbs = NULL;
int arbs_size, arbs_content_size = 0;
char * d_buffer = NULL;
size_t d_buffer_size, d_buffer_maxsize = 0;

// Global Variables:
HINSTANCE hInst;								// current instance
HBRUSH hBrushes[10];
HPEN hPens[10];
enum {HBS_BLUE=0, HBS_RED, HBS_DGRAY, HBS_LGRAY, HBS_ORANGE, 
		  HBS_MAX};

COLORREF colors[] ={RGB (  0,  0,255),  // HBS_BLUE
                    RGB (255,  0,  0),  // HBS_RED
                    RGB ( 96, 96, 96),  // HBS_DGRAY
                    RGB (160,160,160),  // HBS_LGRAY
                    RGB (192, 64,  0),  // HBS_ORANGE
                    RGB (0, 0, 0)};     // HBS_MAX, just put a record here to prevent possibility of array out of range

int widths[]      ={1,  // HBS_BLUE
                    1,  // HBS_RED
                    1,  // HBS_DGRAY
                    1,  // HBS_LGRAY
                    2,  // HBS_ORANGE
                    1}; // HBS_MAX, just put a record here to prevent possibility of array out of range

HWND hWndMain = NULL;
HWND hWndInitButton = NULL;
HWND hWndRecordButton = NULL;
HWND hWndPlayButton = NULL;

HWND hWndLogWindow = NULL;
HWND hWndEdit = NULL;

TCHAR strbuf[MAX_BUFFER_SIZE];
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// The title bar text
TCHAR szEditClass[MAX_LOADSTRING];			// The title bar text
const TCHAR szHelp [] = 
" *** VSM (vastate) GUI Simulator *** \n\n"           \
" Keyboard mappings : \n\n"                           \
" While un-initialize: \n"                            \
"   p -         Select PLAY file \n"                  \
"   r -         Select RECORD file \n"                \
"\n"                                                  \
" While initialized: \n"                              \
"   [Enter] -   Run/Stop the simulation \n"           \
"   [Space] -   Run one step \n"                      \
"   v -         Show/Hide voronoi edge(s) \n"         \
"   r -         Redraw \n"                            \
"   g -         Show/Hide game behavior detail \n"    \
"   f -         Show/Hide full information \n"        \
"   m -         Show debug message window \n"         \
"   a/s/d/w/o - Move map to l/d/r/u/ori. \n"          \
"   u -         Switch floating object info. \n"      \
"   A/S/D/W   - Move floating object info position\n" \
"   t -         run to step. \n                     " \
" \n"                                                 \
" Global: \n"                                         \
"   h -         Show the help \n"                     \
"   q -         Quit \n"                              \
" \n "                                                \
" ************************************ "              \
"\n\n\n";

SimPara para;
bool bInited;
bool bSimInited;
bool bRunning;
int  bShowVoronoi;
bool bShowGameDetail;
bool bShowDetail;
// bool bShowLocalView;
bool bRunThisStep;
bool bFinish;
int  timestamp;
int  timestamp_runto;
int errmsg_no;

bool bLButtonDown;


const int ID_INITBUTTON = 1;
const int ID_RBUTTON = 2;
const int ID_PBUTTON = 3;

// Foward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    WndProcEdit(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);


static 
string windowlized_string (const string &instr)
{
    string tstr = instr, ostr;
    while (tstr.size () > 0)
    {
        int index = tstr.find ('\n');

        // if already "\r\n", skip and find next
        while (index != -1 && tstr[index-1] == '\r')
            index = tstr.find ('\n', index+1);

        // replace "\n" to "\r\n"
        ostr.append (tstr.substr (0, index));
        ostr.append ("\r\n");

        if (index == (-1))
            tstr.clear ();
        else
            tstr = tstr.substr (index+1);
    }

    return ostr;
}


class editout : public errout
{
public:
	editout ()
	{
		errout::setout (this);
	}

	~editout ()
	{
	}

    void refreshOutput ()
    {
        int length, queuelen;

		if (queueMessage == NULL)
			return ;
		else
			queuelen = strlen (queueMessage);

        if (hWndEdit == NULL)
			length = 0;
		else
			length = GetWindowTextLength(hWndEdit);

		int maxl = queuelen+length+2;
		char * strdest = new char [maxl];
		if (strdest == NULL)
		{
			MessageBox (NULL, "Memory allocation error", "VASTATEsim_gui", MB_OK);
			PostQuitMessage (0);
			return ;
		}
		strdest[0] = '\0';
		strdest[1] = '\0';

		GetWindowText (hWndEdit, strdest, maxl);
		if (queueMessage != NULL)
		{
			strcat (strdest, queueMessage);
			delete [] queueMessage;
			queueMessage = NULL;
		}

#ifdef DEBUGMSG_BUFFER_SIZE
		SetWindowText (hWndEdit, (maxl>DEBUGMSG_BUFFER_SIZE) ? (strdest+(maxl-DEBUGMSG_BUFFER_SIZE)) : strdest);
#else
		SetWindowText (hWndEdit, strdest);
#endif

		int iCount = SendMessage (hWndEdit, EM_GETLINECOUNT, 0, 0);
		PostMessage(hWndEdit, EM_LINESCROLL, 0, iCount);

		delete[] strdest;
    }

	void outputHappened (const char * str)
	{
		if (str == NULL && queueMessage == NULL)
			return;

		if ((str == NULL || str[0] == '\0') && (queueMessage != NULL))
			refreshOutput ();
        else
        {
            string processedstr = windowlized_string (string (str));

            int len;
			if (queueMessage == NULL)
                len = processedstr.size () + 2;
			else
                len = processedstr.size () + strlen (queueMessage) + 2;

			char * newstr = new char[len];
			if (newstr == NULL)
			{
				MessageBox (NULL, "Memory allocation error", "VASTATESIM", MB_OK);
				PostQuitMessage (0);
				return ;
			}

			if (queueMessage == NULL)
				newstr[0] = newstr[1] = '\0';
			else
			{
				strcpy (newstr, queueMessage);
				delete[] queueMessage;
			}

            strcat (newstr, processedstr.c_str ());
			queueMessage = newstr;
		}
	}

	static char *queueMessage;
};

char * editout::queueMessage = NULL;
editout errmain;


double dist (POINT p1, POINT p2)
{
    return sqrt (pow ((double)(p1.x - p2.x), 2) + pow ((double)(p1.y - p2.y), 2) );
}


bool Nearby (Position& p1, Position& p2, int dist = 15)
{	
	if ((abs(p1.x-p2.x) <= dist)
		&& (abs(p1.y-p2.y) <= dist))
		return true;
	return false;
}

// find a nearest linker to mouse_pos, if multiple selected, skip the first skip_count items
/*
 *  selection has two strategies, (listed by its priority)
 *  1) inside the circle centered by mouse_pos and radius of touch_dis
 *  2) nearest to mouse_pos
 */
ITEM_LINK * findSelectedLinker (Position& mouse_pos, int skip_count = 0)
{
    // static var and system parameters
    static vector<ITEM_LINK *> inrange_items;
	int &w = para.WORLD_WIDTH;
	int &h = para.WORLD_HEIGHT;

    // linker variables
    ITEM_LINK * nearest = NULL;
	double nearest_dist = 15;

    inrange_items.clear ();
    const double touch_dis = 6;

    int tl = linkers.size();
	for (int i = 0; i < tl; i++)
	{
		Position linker_pos (POSMAP_X(linkers[i]->pos.x), POSMAP_Y(linkers[i]->pos.y));
		if (linker_pos.dist(mouse_pos) < nearest_dist)
		{
			nearest = linkers[i];
			nearest_dist = linker_pos.dist(mouse_pos);
		}

        if (linker_pos.dist(mouse_pos) <= touch_dis)
            inrange_items.push_back (linkers[i]);
	}

    if (inrange_items.size () != 0)
        return inrange_items[skip_count % inrange_items.size ()];

    return nearest;
}

int DebugAttached ()
{
	const int verynear = 12;
	RECT rtw, rtl;
	GetWindowRect (hWndMain, &rtw);
	GetWindowRect(hWndLogWindow, &rtl);
	if (Nearby (Position(rtw.right,rtw.top), Position(rtl.left,rtl.top), verynear))
		return 1;
	else if (Nearby(Position(rtw.left,rtw.bottom), Position(rtl.left,rtl.top), verynear))
		return 2;
	/*
	else if (abs(rtl.bottom-rtw.top) <= verynear)
		return 3;
	else if (abs(rtl.right-rtw.left) <= verynear)
		return 4;
	*/
	return 0;
}

void attachDebug (int nowattach)
{
	RECT rt, rtw;
	GetWindowRect (hWndMain, &rtw);
	GetWindowRect (hWndLogWindow, &rt);

	int w = rt.right - rt.left;
	int h = rt.bottom - rt.top;

	switch (nowattach)
	{
	case 1:
		MoveWindow(hWndLogWindow, rtw.right, rtw.top, w, h, true);
		break;
	case 2:
		MoveWindow (hWndLogWindow, rtw.left, rtw.bottom, w, h, true);
		break;
	case 3:
		break;
	case 4:
		break;
	default:
		break;
	}
}


void InitSimulation ()
{
	errout eo;
	//char strmsg[128];

    InitVSSim (para, simulation_mode, foodfile.c_str (), actionfile.c_str ());
	
	int n;
	for (n=0; n<para.NODE_SIZE; n++)
    {
		CreateNode (85);
        sprintf (strbuf, "Initializing - %d / %d\n", n+1, para.NODE_SIZE);
        eo.output (strbuf);
    }

	/*
	for (n=0; n<5; n++)
	    ProcessMsg ();
    */

	ShowWindow (hWndInitButton, false);
	ShowWindow (hWndRecordButton, false);
	ShowWindow (hWndPlayButton, false);
	
	sprintf (strbuf, "---------- Initialized ----------" NEWLINE);
	eo.output(strbuf);
    errmain.refreshOutput ();

	bRunning = false;
	bRunThisStep = false;
	bSimInited = true;
}



VOID Render ( HWND hWnd )
{
    int objc;
    Position *atts;
	food_reg *foods;
	voronoi * vor;
    id_t      vor_id;
	char msg[512], msg2[256];
	char toplineinfo  [512];
    char toplineinfo2 [512] = {'\0'} ;
	RECT rt;
	GetClientRect (hWnd, &rt);

    // get and initialize DC
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
	SelectObject (hdc, GetStockObject (DEFAULT_GUI_FONT));

	if (!bInited)
	{
		//sprintf (msg, "Can't open parameter file.\n");
		//DrawText (hdc, msg, strlen(msg), &rt, DT_CENTER);
		sprintf (toplineinfo, "Can't open parameter file.");
	}
	else if (!bSimInited)
	{
        sprintf (toplineinfo, "::: PREPARE :::  Mode: %s Foodimage:%s Actionimage:%s",
            SIMULATION_MODE_STR[simulation_mode],
            foodfile.empty ()?"(none)":foodfile_title.c_str (),
            actionfile.empty ()?"(none)":actionfile_title.c_str ()
            );
		rt.top = 20;
		DrawText (hdc, szHelp, strlen(szHelp), &rt, DT_LEFT);
		GetClientRect (hWnd, &rt);
	}
	else
	{
        // make top line information string
        sprintf (toplineinfo, "%s%s%sStep %d (%d) (proc time %0.3lf, STEP INTERVAL%s %d) pro/de (%d,%d) gamepos (%d,%d) Mode: %s Food/Action image file:\"%s\"/\"%s\" --",
		    //  if finish show [FINISH], else check if running continually, show [RUNNING] for true.
		    bFinish?"[FINISH] ":bRunning?"[RUNNING] ":"",                       // first status string
		    bShowVoronoi?"[VORONOI] ":"",                                       // second status string
            (timestamp_runto > 0) && (timestamp_runto > timestamp) ? "[RUNTO]" : "",
                                                                                // run to step info
		    timestamp - 1,                                                      // timestamp
            GetCurrentTimestamp (),                                             // internal clock timestamp
		    (double) proTime / (double) CLK_TCK,                                // last step process time
            (RUNMODE_RELAX) ? "[R]" : "",                                       // if running in relax mode
            STEP_TIME_INTERVAL,                                                 // step interval
            GetSystemState (SYS_PROM_COUNT), GetSystemState (SYS_DEM_COUNT),    // promote / demote count
		    (int)GAMEPOS_X(last_mouse_pos.x), (int)GAMEPOS_Y(last_mouse_pos.y), // gamemap position mouse last directed
            SIMULATION_MODE_STR[simulation_mode],                               // simulation mode string
            foodfile_title.empty()?"(none)":foodfile_title.c_str (),            // food record filename
            actionfile_title.empty()?"(none)":actionfile_title.c_str ()         // action record filename
		);

        if (RUNMODE_TOSTEP ||
            ((timestamp_runto > 0) && (timestamp_runto > timestamp)))
        {
            sprintf (toplineinfo2, "RUNTO: %d --", timestamp_runto);
        }

        // Draw edge rectangle of the VE
		SelectObject (hdc, GetStockObject (NULL_BRUSH));
        SelectObject (hdc, GetStockObject (BLACK_PEN));
		Rectangle (hdc, POSMAP_X(0), POSMAP_Y(0), POSMAP_X(para.WORLD_WIDTH), POSMAP_Y(para.WORLD_HEIGHT));

        // refresh side info string
        switch (lselected.type)
		{
		case 0: // Player
            sideinfo = "Player\n";
            sideinfo.append (GetPlayerInfo(lselected.index));
			break;
		case 1: // Arbitrator
            sideinfo = "Arbitrator\n";
            sideinfo.append (GetArbitratorString (lselected.index, lselected.sub_index));
			break;
		case 2:	// Attractor
			break;
		case 3:	// Food
            sideinfo = "Food\n";
            sideinfo.append (GetFoodInfo (lselected.index));
			break;
		default:
            sideinfo.clear ();
		}

        // print side info line by line (TODO: any more efficient way?)
        if (sideinfo.size () > 0)
        {
            int pos_x, pos_y;

            if (LAYOUT_HUD_FLOAT)
            {
                pos_x = rt.right - LAYOUT_HUD_FLOAT_X;
                pos_y = rt.top   - LAYOUT_HUD_FLOAT_Y;
            }
            else
            {
                pos_x = POSMAP_X(para.WORLD_WIDTH)+10;
                pos_y = POSMAP_Y(0);
            }

            for (string st = sideinfo; st.size () > 0; pos_y += 12)
            {
                int index = st.find ('\n');
                TextOut (hdc, pos_x, pos_y, st.c_str (), (index>=0) ? index : st.size ());
                if (index == (-1))
                    st = "";
                else
                    st = st.substr (index+1);
            }
        }

        // reset linker map to next round use
		if (linkers.size() > 0)
		{
			int total = linkers.size();
			for (int i = 0; i < total; i++)
				delete (ITEM_LINK *) linkers[i];
			linkers.clear();
		}

        // Draw player
		SelectObject (hdc, GetStockObject (NULL_BRUSH));
        SelectObject (hdc, GetStockObject (BLACK_PEN));
		for (objc = 0; objc < para.NODE_SIZE; objc ++)
		{
			object* obj = GetPlayerNode (objc);
			if (obj == NULL)
                continue;

			ITEM_LINK * new_link = new ITEM_LINK ();
			new_link->pos = obj->get_pos();
			new_link->type = 0;
			new_link->index = objc;
			linkers.push_back(new_link);

			Position& p = obj->get_pos ();
			player_info pi;
			bool success_get_player_info = GetPlayerInfo (objc, &pi);

			Ellipse (hdc, (int)POSMAP_X(p.x)-5, (int)POSMAP_Y(p.y)-5, (int)POSMAP_X(p.x)+5, (int)POSMAP_Y(p.y)+5);

			if ((lselected.type == 0) && (lselected.index == objc))
			{
                SelectObject (hdc, GetStockObject(NULL_BRUSH));
                SelectObject (hdc, hPens[HBS_DGRAY]);

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

                SelectObject (hdc, GetStockObject (BLACK_PEN));
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
		//arbs = NULL;
		//arbs_size = arbs_content_size = 0;
		SelectObject (hdc, GetStockObject (NULL_BRUSH));
        SelectObject (hdc, hPens[HBS_RED]);

		if (bShowVoronoi)
        {
			vor = create_voronoi();
            vor_id = 0;
        }

		for (objc = 0; objc < para.NODE_SIZE; objc ++)
		{
            // for each node, check if any arbitrator run on it
			arbs_content_size = GetArbitratorInfo (objc, NULL);
			if (arbs_content_size == 0)
				continue;

            // check space of arbs is sufficient
			if ((arbs == NULL) || (arbs_size < arbs_content_size))
			{
				if (arbs != NULL)
					delete[] arbs;
				arbs = new arbitrator_info[arbs_content_size];
				arbs_size = arbs_content_size;
			}
			
            // get arbs, and for all arbs in the list
			GetArbitratorInfo(objc, arbs);
			for (int ari=0; ari < arbs_content_size; ari++)
			{
                if (arbs[ari].id == NET_ID_UNASSIGNED ||
                    arbs[ari].joined == false)
                    continue;

				Position &tp = arbs[ari].pos;

				ITEM_LINK * new_link = new ITEM_LINK ();
				new_link->pos = tp;
				new_link->type = 1;
				new_link->index = objc;
				new_link->sub_index = ari;
				linkers.push_back(new_link);

                if (arbs[ari].is_aggr)
                {
                    SelectObject (hdc, hPens[HBS_ORANGE]);

                    Ellipse (hdc, POSMAP_X(tp.x)-7, POSMAP_Y(tp.y)-7, POSMAP_X(tp.x)+7, POSMAP_Y(tp.y)+7);
                    sprintf (msg, "Ag%d", arbs[ari].id);
                    TextOut (hdc, POSMAP_X(tp.x-3), POSMAP_Y(tp.y+4), msg, strlen(msg));
                    // status string
                    if (arbs[ari].status[0] != '\0')
                        TextOut (hdc, POSMAP_X(tp.x+4), POSMAP_Y(tp.y-8), arbs[ari].status, 1);

                    d_buffer_size = d_buffer_maxsize;
                    if (GetInfo (objc, ari, 1, d_buffer, d_buffer_size) == false)
                    {
                        if (d_buffer_size > d_buffer_maxsize)
                        {
                            if (d_buffer != NULL)
                                delete[] d_buffer;
                            d_buffer = new char[d_buffer_size];
                            d_buffer_maxsize = d_buffer_size;

                            if (GetInfo (objc, ari, 1, d_buffer, d_buffer_size) == false)
                                d_buffer_size = 0;
                        }
                    }

                    if (d_buffer_size > 0)
                    {
                        char * d_buffer_p = d_buffer + 1;
                        for (int nc = 0; nc < d_buffer[0]; nc ++, d_buffer_p += sizeof (Node))
                        {
                            Node * cnode = (Node *) d_buffer_p;
                            Position & cpos = cnode->pos;
                            //Ellipse (hdc, POSMAP_X(cnode->pos.x-3), POSMAP_Y(cnode->pos.y-3), POSMAP_X(cnode->pos.x+3), POSMAP_Y(cnode->pos.y+3));
                            MoveToEx (hdc, POSMAP_X(cpos.x), POSMAP_Y(cpos.y)-3, NULL);
                            LineTo (hdc, POSMAP_X(cpos.x)+3, POSMAP_Y(cpos.y)+3);
                            LineTo (hdc, POSMAP_X(cpos.x)-3, POSMAP_Y(cpos.y)+3);
                            LineTo (hdc, POSMAP_X(cpos.x), POSMAP_Y(cpos.y)-3);
                            sprintf (msg, "%d", cnode->id);
                            TextOut (hdc, POSMAP_X(cnode->pos.x-3), POSMAP_Y(cnode->pos.y+4), msg, strlen(msg));

                            if (bShowVoronoi)
                                vor->insert (vor_id ++, cnode->pos);
                        }
                    }

                    SelectObject (hdc, hPens[HBS_RED]);
                }
                else
                {
                    Ellipse (hdc, POSMAP_X(tp.x)-7, POSMAP_Y(tp.y)-7, POSMAP_X(tp.x)+7, POSMAP_Y(tp.y)+7);
                    // node id
                    sprintf (msg, "%d", arbs[ari].id);
    				TextOut (hdc, POSMAP_X(tp.x)-3, POSMAP_Y(tp.y)+4, msg, strlen(msg));
                    // status string
                    if (arbs[ari].status[0] != '\0')
                        TextOut (hdc, POSMAP_X(tp.x)+4, POSMAP_Y(tp.y)-8, arbs[ari].status, 1);

                    if (bShowVoronoi)
					    vor->insert(vor_id ++, arbs[ari].pos);
                }

                if (lselected.type == 1 && 
                    lselected.index == objc && 
                    lselected.sub_index == ari)
                {
                    int aoi = (int) arbs[ari].aoi;
                    Position &tp = arbs[ari].pos;
		            SelectObject (hdc, GetStockObject(NULL_BRUSH));
                    SelectObject (hdc, hPens[HBS_DGRAY]);
		            Ellipse (hdc, POSMAP_X(tp.x-aoi), POSMAP_Y(tp.y-aoi), 
			            POSMAP_X(tp.x+aoi), POSMAP_Y(tp.y+aoi));
                    if (arbs[ari].aoi_b != 0)
                    {
                        aoi = arbs[ari].aoi_b;
                        Ellipse (hdc, POSMAP_X(tp.x-aoi), POSMAP_Y(tp.y-aoi), 
			                POSMAP_X(tp.x+aoi), POSMAP_Y(tp.y+aoi));
                    }

                    SelectObject (hdc, GetStockObject (NULL_BRUSH));
                    SelectObject (hdc, hPens[HBS_RED]);
                }
			}
		}

        // delete temporary memory
        /* continue use to the end of the program
		if (arbs != NULL)
		{
			delete[] arbs;
			arbs = NULL;
			arbs_size = arbs_content_size = 0;
		}
        */

        // draw AOI radius for selected arbitrator
        /*
		if (lselected.type == 1)
		{
			int aoi = (int) (GetArbAOI(lselected.index, lselected.sub_index));
			Position &tp = lselected.pos;
			SelectObject (hdc, GetStockObject(NULL_BRUSH));
            SelectObject (hdc, hPens[HBS_DGRAY]);
			Ellipse (hdc, POSMAP_X(tp.x-aoi), POSMAP_Y(tp.y-aoi), 
				POSMAP_X(tp.x+aoi), POSMAP_Y(tp.y+aoi));
		}*/

        // draw Voronoi diagram if option selected
        if (bShowVoronoi)
		{
            SelectObject (hdc, GetStockObject (BLACK_PEN));

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
		

        if (bShowVoronoi)
			destroy_voronoi (vor);

		// Draw Attractor (inproper in Playback mode)
        if (!IsPlayMode ())
        {
            SelectObject (hdc, GetStockObject (NULL_BRUSH));
            SelectObject (hdc, GetStockObject (BLACK_PEN));
		    atts = new Position[para.ATTRACTOR_MAX_COUNT];
		    int ac, totala = GetAttractorPosition (atts);
		    for (ac = 0; ac < totala; ac++)
		    {
			    Position& p = atts[ac];
			    //SelectObject (hdc, GetStockObject (GRAY_BRUSH));
			    //Ellipse (hdc, POSMAP_X(p.x-3), POSMAP_Y(p.y-3), POSMAP_X(p.x+3), POSMAP_Y(p.y+3));
			    Ellipse (hdc, POSMAP_X(p.x-5), POSMAP_Y(p.y-5), POSMAP_X(p.x+5), POSMAP_Y(p.y+5));
                // Draw a cross
                MoveToEx (hdc, POSMAP_X(p.x-5), POSMAP_Y(p.y-5), NULL);
                LineTo (hdc, POSMAP_X(p.x+5), POSMAP_Y(p.y+5));
                MoveToEx (hdc, POSMAP_X(p.x+5), POSMAP_Y(p.y-5), NULL);
                LineTo (hdc, POSMAP_X(p.x-5), POSMAP_Y(p.y+5));

			    if ((lselected.type == 2) && (lselected.pos == p))
			    {
				    int dist = para.ATTRACTOR_RANGE;
				    Ellipse (hdc, POSMAP_X(p.x-dist), POSMAP_Y(p.y-dist), POSMAP_X(p.x+dist), POSMAP_Y(p.y+dist));
			    }

			    ITEM_LINK * new_link = new ITEM_LINK ();
			    new_link->pos = p;
			    new_link->type = 2;
			    new_link->index = ac;
			    new_link->sub_index = 0;
			    linkers.push_back(new_link);
		    }
		    delete[] atts;
        }

		// Draw foods
		int fc, totalc = GetFoods (NULL);
		foods = new food_reg [totalc];
		GetFoods (foods);

        SelectObject (hdc, GetStockObject (NULL_BRUSH));
        SelectObject (hdc, hPens[HBS_BLUE]);
		for (fc =0; fc < totalc; fc ++)
		{
			ITEM_LINK * new_link = new ITEM_LINK ();
			new_link->pos = foods[fc].pos;
			new_link->type = 3;
			new_link->index = foods[fc].id;
			new_link->sub_index = 0;
			linkers.push_back(new_link);

			Position& p = foods[fc].pos;
			Ellipse (hdc, POSMAP_X(p.x)-5, POSMAP_Y(p.y)-5, POSMAP_X(p.x)+5, POSMAP_Y(p.y)+5);

			if (bShowDetail)
			{
				sprintf (msg, "%08x", foods[fc].id);//foods[fc].count);
				TextOut (hdc, POSMAP_X(p.x)-3, POSMAP_Y(p.y)+5, msg, strlen(msg));
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

    if (toplineinfo2[0] != '\0')
    {
        TextOut (hdc, 0, 20, toplineinfo2, strlen(toplineinfo));
    }

	errout e;
	e.output("");

	EndPaint(hWnd,&ps);
}



int APIENTRY 
WinMain 
(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // for message outputing
	MSG msg;
	errout eo;

    // initial variables
    bInited = false;
	bRunning = false;
	bShowVoronoi = true;
	bRunThisStep = false;
	bSimInited = false;
	bFinish = false;
	bShowGameDetail = false;

    bLButtonDown = false;
    /*
	RECORDFILE[0] = '\0';
	PLAYFILE[0] = '\0';
	recordFileTitle[0] = '\0';
	playFileTitle[0] = '\0';
    */
    simulation_mode = VS_NORMAL_MODE;

	last_mouse_pos.x = 0;
	last_mouse_pos.y = 0;
	last_mouse_pointed_index = -1;
	lselected.type = -1;
	rselected.type = -1;
    lselect_skip   =  0;
	proTime = min_proTime = max_proTime = total_proTime = 0;
	step_time = step_start = 0;
    timestamp_runto = -1;

	// Load para
    string filename ("VASTATEsim.ini");
	if (ReadPara (para, filename))
	{
		bInited = true;
		timestamp = 0;
	}

	srand ((unsigned int)time (NULL));

	// Initialize global strings
	strcpy (szTitle, "VASTATESIM Simulator GUI v0.02");
	strcpy (szWindowClass, "VASTATESIMGUI");
	strcpy (szEditClass, "VASTATESIMGUI_DEBUG");
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
                else if (msg.wParam == 'r')
                    PostMessage (hWndMain, WM_COMMAND, MAKEWORD (ID_RBUTTON, 0), 0);
                else if (msg.wParam == 'p')
                    PostMessage (hWndMain, WM_COMMAND, MAKEWORD (ID_PBUTTON, 0), 0);
                else if (msg.wParam == 'q')
                    PostMessage (hWndMain, WM_CLOSE, 0, 0);
            }
            else
            {
                if (RUNMODE_TOSTEP)
                {
                    if (msg.wParam >= '0' && msg.wParam <= '9')
                        timestamp_runto = timestamp_runto * 10 + (msg.wParam - '0');
                    else if (msg.wParam == 0x08) // Backspace
                        timestamp_runto /= 10;

                    else if (msg.wParam == '\r')
                    {
                        RUNMODE_TOSTEP = false;
                        bRunning = true;
                    }
                    else if (msg.wParam == 0x1B) // ESC
                    {
                        RUNMODE_TOSTEP = false;
                        timestamp_runto = -1;
                    }
                }

                // normal mode
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
				        MessageBox (NULL, szHelp, "VSM(vastate) Simulator Help", MB_OK);
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
                    else if (msg.wParam == '+')
                        LAYOUT_POSMAP_RATE += 0.25;
                    else if (msg.wParam == '-')
                        LAYOUT_POSMAP_RATE -= 0.25;
                    else if (msg.wParam == 'u')
                        LAYOUT_HUD_FLOAT = !LAYOUT_HUD_FLOAT;
                    else if (msg.wParam == 'W')
                        LAYOUT_HUD_FLOAT_Y += 5;
                    else if (msg.wParam == 'S')
                        LAYOUT_HUD_FLOAT_Y -= 5;
                    else if (msg.wParam == 'A')
                        LAYOUT_HUD_FLOAT_X += 5;
                    else if (msg.wParam == 'D')
                        LAYOUT_HUD_FLOAT_X -= 5;
                    else if (msg.wParam == 'R')
                    {
                        RUNMODE_RELAX = !RUNMODE_RELAX;
                        if (!RUNMODE_RELAX)
                            STEP_TIME_INTERVAL = DEFAULT_STEP_TIME_INTERVAL;
                    }
                    else if (msg.wParam == 't')
                    {
                        RUNMODE_TOSTEP = true;
                        timestamp_runto = 0;
                    }

			        else
				        DispatchMessage(&msg);
                }

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
    {
		DeleteObject (hBrushes[obs]);
        DeleteObject (hPens[obs]);
    }

    if (arbs != NULL)
        delete[] arbs;
    if (d_buffer != NULL)
        delete[] d_buffer;

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

	
	WNDCLASSEX wcexe;
	
	wcexe.cbSize = sizeof(WNDCLASSEX); 
	
	wcexe.style			= CS_HREDRAW | CS_VREDRAW;
	wcexe.lpfnWndProc	= (WNDPROC)WndProcEdit;
	wcexe.cbClsExtra    = 0;
	wcexe.cbWndExtra    = 0;
	wcexe.hInstance     = hInstance;
	wcexe.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wcexe.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wcexe.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wcexe.lpszMenuName  = NULL;
	wcexe.lpszClassName = szEditClass;
	wcexe.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

	
	return RegisterClassEx(&wcex) && RegisterClassEx(&wcexe);
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

    // create brushes and pens
    for (int obs =0; obs != HBS_MAX; obs++)
    {
        hBrushes[obs] = CreateSolidBrush (colors[obs]);
        hPens   [obs] = CreatePen (PS_SOLID, widths[obs], colors[obs]);
    }

    hWndMain = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 800, 450, NULL, NULL, hInstance, NULL);

    if (!hWndMain)
        return FALSE;

    ShowWindow(hWndMain, nCmdShow);
    UpdateWindow(hWndMain);

    SetTimer(hWndMain, 1, STEP_TIME_INTERVAL, (TIMERPROC) NULL);

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
	static errout e;
    static Position lmouse_pos, rmouse_pos;
    static char ostr[256];
	//RECT rt;

    Position mouse_pos;
	RECT rtl, rtw;
    char filename[256], filetitle[256];
	bool needRedraw;
	ITEM_LINK * lk;
	
	switch (message) 
	{
		case WM_CREATE:
			hWndInitButton = CreateWindow (TEXT ("button"), "Init",
					WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
					0, 0, 0, 0, hWnd, (HMENU) ID_INITBUTTON, hInst, NULL);
			hWndRecordButton = CreateWindow (TEXT ("button"), "Select\nRec file",
					WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_MULTILINE | BS_FLAT,
					0, 0, 0, 0, hWnd, (HMENU) ID_RBUTTON, hInst, NULL);
			hWndPlayButton = CreateWindow (TEXT ("button"), "Select\nPlay file",
					WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_MULTILINE | BS_FLAT,
					0, 0, 0, 0, hWnd, (HMENU) ID_PBUTTON, hInst, NULL);

            sprintf (strbuf, "%s - Debug", DEFAULT_WINDOW_TITLE);
			hWndLogWindow = CreateWindow(szEditClass, strbuf, WS_BORDER | WS_POPUP | WS_CAPTION |WS_SIZEBOX ,//WS_OVERLAPPEDWINDOW ^ WS_SYSMENU,
				CW_USEDEFAULT, 0, 800, 150, hWnd, NULL, hInst, NULL);
			ShowWindow(hWndLogWindow, true);
			UpdateWindow(hWndLogWindow);
			GetWindowRect (hWnd, &rtw);
			GetWindowRect (hWndLogWindow, &rtl);
			MoveWindow (hWndLogWindow, rtw.right, rtw.top, (rtl.right-rtl.left), (rtl.bottom-rtl.top), true);

            // set debug window is attached to bottom of main window
			debugAttachTo = 2;
			
            // initialize Popup dialogs (OpenFile and SaveFile)
			PopFileInitialize (hWnd);
			break;

		case WM_MOVE:
            // check if debug window moves together
			attachDebug(debugAttachTo);
			break;

		case WM_SIZE:
            // move buttons
			MoveWindow (hWndInitButton, LOWORD (lParam) - (65+1), 20, 60, 20, true);
			MoveWindow (hWndRecordButton, LOWORD (lParam) - (70+1), 45, 70, 40, true);
			MoveWindow (hWndPlayButton, LOWORD (lParam) - (70+1), 85, 70, 40, true);

            // move debug window
			attachDebug(debugAttachTo);
			break;
		
		case WM_COMMAND:
			needRedraw = false;
			if (LOWORD (wParam) == ID_INITBUTTON)
			{
                simulation_mode = VS_NORMAL_MODE;
				InitSimulation();
				needRedraw = true;
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
                else
                    break;

                strcpy (filename, actionfile.c_str ());
                strcpy (filetitle, actionfile_title.c_str ());
                if (action_succ = PopFileSaveDlg (hWnd, filename, filetitle))
                {
                    actionfile = filename;
                    actionfile_title = filetitle;
                }
                else
                {
                    foodfile = "";
                    foodfile_title = "";
                }
                
                if (food_succ && action_succ)
                {
                    InitSimulation ();
                    needRedraw = true;
                }
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
                else
                    break;

                strcpy (filename, actionfile.c_str ());
                strcpy (filetitle, actionfile_title.c_str ());
                if (action_succ = PopFileOpenDlg (hWnd, filename, filetitle))
                {
                    actionfile = filename;
                    actionfile_title = filetitle;
                }
                else
                {
                    foodfile = "";
                    foodfile_title = "";
                }

                if (food_succ && action_succ)
                {
                    InitSimulation ();
                    needRedraw = true;
                }
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
                    LAYOUT_POSMAP_X_ORIG = last_orig_x + (int)((pt.x - last_mouse_pos.x) / LAYOUT_POSMAP_RATE);
                    LAYOUT_POSMAP_Y_ORIG = last_orig_y + (int)((pt.y - last_mouse_pos.y) / LAYOUT_POSMAP_RATE);

                    //last_mouse_pos = pt;
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
            {
                Position old_mouse_pos = lmouse_pos;
			    lmouse_pos.x = LOWORD(lParam);
			    lmouse_pos.y = HIWORD(lParam);

                // check if clicked on same position multiple times
                if (old_mouse_pos.dist (lmouse_pos) <= 3.0)
                    lselect_skip ++;
                else
                    lselect_skip = 0;

                // find the linker clicked
			    lk = findSelectedLinker (lmouse_pos, lselect_skip);
			    if (lk == NULL)
				    lselected.type = -1;
			    else
				    lselected = *lk;

			    bFullUpdate = true;
			    InvalidateRect (hWndMain, NULL, true);

                bLButtonDown = true;
                last_mouse_pos.x = LOWORD(lParam);
			    last_mouse_pos.y = HIWORD(lParam);
                last_orig_x = LAYOUT_POSMAP_X_ORIG;
                last_orig_y = LAYOUT_POSMAP_Y_ORIG;
            }
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
            {
                Position old_pos = rmouse_pos;
			    rmouse_pos.x = LOWORD(lParam); 
			    rmouse_pos.y = HIWORD(lParam);

                // check if clicked on same position multiple times
                if (old_pos.dist (rmouse_pos) <= 3.0)
                    rselect_skip ++;
                else
                    rselect_skip = 0;

                // find the linker clicked
			    lk = findSelectedLinker (rmouse_pos, rselect_skip);
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
            }
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
            if (bRunning && timestamp == timestamp_runto - 1)
            {
                bRunning = false;
                bRunThisStep = true;
            }

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

                    if (RUNMODE_RELAX)
                    {
                        int old_step_interval = STEP_TIME_INTERVAL;
                        if (STEP_TIME_INTERVAL - proTime <= 100)
                            STEP_TIME_INTERVAL += 50;
                        else if (STEP_TIME_INTERVAL - proTime >= 200)
                            STEP_TIME_INTERVAL -= 50;

                        if (STEP_TIME_INTERVAL != old_step_interval)
                            SetTimer(hWndMain, 1, STEP_TIME_INTERVAL, (TIMERPROC) NULL);
                    }
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
			ShowWindow (hWndLogWindow, true);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;
			
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}


LRESULT CALLBACK WndProcEdit(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	RECT rt;
//	RECT rtparent;
	HDC hdc;
	PAINTSTRUCT ps;

	switch (message) 
	{
		case WM_CREATE:
			hWndEdit = CreateWindow (TEXT ("edit"), NULL,
				WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL |
				WS_BORDER | ES_LEFT | ES_MULTILINE |
				ES_AUTOHSCROLL | ES_AUTOVSCROLL,
				200, 100, 100, 100, hWnd, (HMENU) 0,
				hInst, NULL) ;
			break;

		case WM_MOVE:
			debugAttachTo = DebugAttached ();
			
			if (debugAttachTo > 0)
				attachDebug (debugAttachTo);
			break;

		case WM_SIZE:
			GetClientRect (hWnd, &rt);
			MoveWindow (hWndEdit, 0, 0, rt.right, rt.bottom, true);

			break;
			
		case WM_PAINT:
			hdc = BeginPaint (hWnd, &ps);

			EndPaint (hWnd, &ps);
			break;

		case WM_CLOSE:
			ShowWindow (hWndLogWindow, false);
			bFullUpdate = true;
			UpdateWindow (hWndMain);
			return 0;
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
