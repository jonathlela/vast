
/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007-2008 Shao-Chen Chang (cscxcs at gmail.com)
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
 *	VASTATESim.cpp  (VASTATE Simulation main module)
 *
 *      main module of VASTATE simulator
 *
 */

#include "vastverse.h"
#include "vastory.h"

#include "vastatesim.h"
#include "simgame.h"
#include "statistics.h"

#include "vastutil.h"

#include <stdlib.h>

vector<simgame_node *>  g_players;
behavior *              g_model;
statistics *            g_sta;
vastate *               g_vasts     = NULL;

SimPara                 g_para;
vastverse               g_world     (VAST_MODEL_DIRECT, VAST_NET_EMULATED, 0);
arbitrator_reg          g_arb       (&g_world);

struct system_state     g_sys_state;

int                    fail_times = 0;


int UpdateFoods ();

bool ReadPara (SimPara & para, const string & conffile)
{
/*    struct {
        void * data;
        int type;
    }*/
    int *p[] =
    {
        &para.WORLD_WIDTH, 
        &para.WORLD_HEIGHT,
        &para.NODE_SIZE,
        &para.TIME_STEPS,
        &para.AOI_RADIUS,
        &para.AOI_BUFFER,
        &para.VELOCITY,
        &para.CONNECT_LIMIT,
        &para.LOSS_RATE,
        &para.FAIL_RATE, 
        &para.NUM_VIRTUAL_PEERS, 
        &para.FAIL_PRELOAD, 
        &para.FAIL_INTERVAL, 
        &para.FAIL_STYLE, 
        &para.SCC_PERIOD,
        &para.SCC_TRACE_LENGTH,
        &para.FOOD_MAX_QUANTITY,
        &para.HP_MAX,
        &para.HP_LOW_THRESHOLD,
        &para.ATTACK_POWER,
        &para.BOMB_POWER,
        &para.FOOD_POWER,
        &para.ANGST_OF_ACTION,
        &para.ANGST_OF_ATTACK,
        &para.TIREDNESS_OF_ACTION,
        &para.TIREDNESS_OF_REST,
        &para.REACTION_THR,
        &para.REACTION_STOP_THR,
        &para.STAY_MAX_STEP,
        &para.ATTRACTOR_MAX_COUNT,
        &para.ATTRACTOR_BASIC_COLDDOWN,
        &para.ATTRACTOR_MAX_COLDDOWN,
        &para.ATTRACTOR_RANGE, 
    0};

    memset (&para, 0, sizeof (SimPara));

    FILE * fp;
    if ((fp = fopen (conffile.c_str (), "r")) == NULL)
    {
        return false;
    }

    char c;
    int para_i = 0;
    while (!feof (fp) && (p[para_i] != 0))
    {
        c = fgetc (fp);
        switch (c)
        {
            case '\n':
            case '\r':
                continue;

            case '#':
                while ((c = fgetc (fp)) != '\n')
                    ;
                ungetc (c, fp);
                continue;

            default:
                ungetc (c, fp);
        }

        fscanf (fp, "%d", p[para_i]);
        para_i ++;
    }

    fclose (fp);

    if (p[para_i] != 0)
        return false;

    return true;
}

int InitVSSim (SimPara &para, int mode, const char * foodimage_filename, const char * actionimage_filename)
{
    vastory fac;
	Addr gateway;
    gateway.id = NET_ID_GATEWAY;

    srand ((unsigned int)time (NULL));

	g_para = para;
	g_para.current_timestamp = -1;
	g_para.FOOD_RATE = g_para.FOOD_MAX_QUANTITY;

	g_model = new behavior (&g_para, mode, foodimage_filename, actionimage_filename, &g_arb);
    g_sta = new statistics (&g_para, &g_players, g_model, &g_arb, &g_world);
#ifdef CONFIG_SINGLE_VASTATE
	g_vasts = fac.create(&g_world, gateway);
#endif

    memset (&g_sys_state, 0, sizeof (system_state));

	return 0;
}

int CreateNode (int capacity)
{
	// obtain current node index
    int i = g_players.size ();
    bool is_gateway = (i == 0);

    simgame_node * newnode = new simgame_node (g_vasts, capacity, g_model, &g_arb, &g_world, &g_para, is_gateway);
	g_players.push_back (newnode);
    newnode->join ();

    while (true)
    {
        // for message deliver normally
        g_world.tick ();

#ifdef CONFIG_SINGLE_VASTATE
        g_players[0]->processmsg ();
        // TODO: how to check if one single peer is joined on SINGLE_VASTATE?
        break;
#else
        for (int j=0; j<=i; ++j)
            g_players[j]->processmsg ();

        if (g_players[i]->is_gateway () ||
            g_players[i]->is_joined ())
            break;
#endif
    }


    // store node into stat class for later processing
    //g_stat.add_node (g_nodes[i]);    
    return 0;
}


int NextStep ()
{
	g_para.current_timestamp ++;

    // if need to fail node, and it's the time to fail node (or recovery node)
    if ((g_para.FAIL_RATE > 0)
        && (g_para.current_timestamp > g_para.FAIL_PRELOAD)
        && ((g_para.current_timestamp - g_para.FAIL_PRELOAD) % g_para.FAIL_INTERVAL == 0))
    {
        if (((g_para.current_timestamp - g_para.FAIL_PRELOAD) / g_para.FAIL_INTERVAL) % 2 == 1)
        {
            for (unsigned int i = 1; i < g_players.size (); i ++)
                g_players[i]->enable (true);
        }
        else
        {
            unsigned int player_count = g_players.size ();
            unsigned int fail_count = static_cast<int>(static_cast<double> (player_count) * (g_para.FAIL_RATE / 100.0));
            vector<int> node_ids;
            unsigned int i;

            // find out all possible nodes to fail (for random style is all nodes, for "only arb" style is nodes with arbitrator
            for (i = 1; i < player_count; i ++)
            {
                if (g_para.FAIL_STYLE == 0)
                    node_ids.push_back (i);
                else if (g_para.FAIL_STYLE == 1)
                    if (g_players[i]->getArbitratorInfo (NULL) > 0)
                        node_ids.push_back (i);
            }

            // randomize the sequence
            for (i = 0; i < node_ids.size (); i ++)
            {
                int r = rand () % node_ids.size ();
                int temp = node_ids[i];
                node_ids[i] = node_ids[r];
                node_ids[r] = temp;
            }
            for (i = 0; i < node_ids.size (); i ++)
            {
                int r = rand () % node_ids.size ();
                int temp = node_ids[i];
                node_ids[i] = node_ids[r];
                node_ids[r] = temp;
            }

#ifdef VASTATESIM_DEBUG
            std::ostringstream omsg;
            omsg << "vastatesim: fail node count: " << fail_count << endl;
            omsg << "vastatesim: fail ";
#endif
            // fail nodes
            for (i = 0; i < fail_count; i ++)
            {
                g_players[node_ids[i]]->enable (false);
#ifdef VASTATESIM_DEBUG
                omsg << node_ids[i] << " ";
#endif
            }

#ifdef VASTATESIM_DEBUG
            errout eo;
            eo.output (omsg.str ().c_str ());
#endif
        }
    }


	g_model->ProcessAttractor ();

    vector<simgame_node *>::iterator git = g_players.begin();
	for (; git != g_players.end (); git ++)
    {
        if ((*git)->getEnable ())
		    (*git)->NextStep ();
    }

	return 0;
}

void ProcessMsg ()
{
    g_world.tick ();

#ifdef CONFIG_SINGLE_VASTATE
    g_players[0]->processmsg ();
#else
	vector<simgame_node *>::iterator git;
	for (git = g_players.begin(); git != g_players.end (); git ++)
		(*git)->processmsg ();
#endif

	g_sta->record_step ();
    UpdateFoods ();
#ifdef CONFIG_EXPORT_NODE_POSITION_RECORD
    g_model->RecordPositions ();
#endif
}


int ShutV2sim ()
{
	while (g_players.size() > 0)
	{
		delete (simgame_node *) g_players.back ();
		g_players.pop_back ();
	}

	if (g_model != NULL)
		delete g_model;

	if (g_sta != NULL)
		delete g_sta;

	return 0;
}


// !!! Notice: call this function before all simulation done will outputting only partial statistics contents.
void refreshStatistics ()
{
    g_sta->print_all ();
}


object * GetPlayerNode (int index)
{
	return g_players[index]->GetSelfObject ();
}

const char * GetPlayerInfo (int index)
{
	return g_players[index]->toString ();
}


int GetArbitratorInfo (int index, arbitrator_info * reg)
{
	return g_players[index]->getArbitratorInfo (reg);
}

const char * GetArbitratorString (int index, int sub_index)
{
	return g_players[index]->getArbitratorString(sub_index);
}

bool GetPlayerInfo (int index, player_info * p)
{
	return g_players[index]->getPlayerInfo (p);
}

int GetAOI (int index)
{
	return g_players[index]->getAOI();
}

int GetArbAOI (int index, int sub_index)
{
	return g_players[index]->getArbAOI (sub_index);
}


bool GetInfo (int index, int sub_index, int info_type, char* buffer, size_t & buffer_size)
{
    if (index < (int) g_players.size ())
        return g_players[index]->get_info (sub_index, info_type, buffer, buffer_size);

    return false;
}


//vector<object *>& GetNeighbors (int index)
//{
//	return g_players[index]->GetObjects ();
//}


//vector<sfv::line2d>& GetEdges(int index)
//{
	// TODO
	//return g_players[index]->getedges ();
//}

int GetAttractorPosition (Position * poses)
{
	return g_model->getAttractorPosition (poses);
}

int UpdateFoods ()
{
    g_arb.food_image_clear ();

    unsigned int p, a;
    for (p = 0; p < g_players.size (); p++)
        for (a = 0; a < (unsigned) g_players[p]->getArbitratorInfo (NULL); a++)
            g_arb.update_food_image (g_players[p]->GetOwnObjects (a));

    return 0;
}

int GetFoods (food_reg * foods)
{
	if (foods == NULL)
        return g_arb._food_image.size ();
		//return g_arb.foods.size ();

	map<VAST::id_t,food_reg>::iterator it = g_arb._food_image.begin();
	int last = 0;

	for(; it != g_arb._food_image.end () ; it ++)
	{
		foods[last] = (it->second);
		last ++;
	}

	return last;
}

const char * GetFoodInfo (VAST::id_t food_id)
{
	static string _i;
	AttributeBuilder a;
	//map<VAST::id_t, food_reg *>::iterator fitr = g_arb.foods.find (food_id);
    map<VAST::id_t, food_reg>::iterator fitr = g_arb._food_image.find (food_id);
	std::stringstream ssout;

	if (fitr != g_arb._food_image.end())
	{
		food_reg & tfood = fitr->second;
		ssout
            << std::hex
            << "[" << (food_id >> 16) << "_" << (food_id & 0xFFFF) << "]"
            << std::dec
            << "(" << tfood.pos.x << "," << tfood.pos.y << ")"
			<< "  "
			<< "Count: " << tfood.count;
	}
	else
	{
		ssout << "No relevant information found (" << food_id << ").";
	}
	_i = ssout.str();
	return _i.c_str();
}

bool isArbitrator (int index)
{
	return g_players[index]->isArbitrator ();
}


voronoi * create_voronoi ()
{
	return g_world.create_voronoi();
}

void destroy_voronoi (voronoi * v)
{
	g_world.destroy_voronoi(v);
}

bool IsPlayMode ()
{
    return g_model->IsPlayMode ();
}

bool IsRecordMode ()
{
    return g_model->IsRecordMode ();
}

unsigned int GetSystemState (int parm)
{
    switch (parm)
    {
    case SYS_PROM_COUNT:
        return g_sys_state.promote_count;
    case SYS_DEM_COUNT:
        return g_sys_state.demote_count;
    }
    return 0;
}

EXPORT timestamp_t GetCurrentTimestamp ()
{
    // TODO: more efficient way to get current network timestamp?
    if (g_players.size () > 0)
        return g_players[0]->get_curr_timestamp ();
    
    return 0;
}

