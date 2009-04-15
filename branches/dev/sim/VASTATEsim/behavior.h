
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
 *  behavior.h (VASTATE simulation node behavior model Header)
 *
 *
 */

#ifndef _VASTATESIM_BEHAVIOR_H
#define _VASTATESIM_BEHAVIOR_H

/*
 *  Class behavior
 *
 *  running in two modes
 *	record mode: records all events in the form (id, event, para) in log file
 *	play mode: read all event from file, and read by v2simgame
 *
 */

#include "attributebuilder.h"
#include "recordfilemanager.h"
#include "arbitrator_reg.h"

#include <time.h>
#include <math.h>
#include <stdlib.h>

struct b_register
{
	Position Destination;
	int WaitingTime;
	int Angst;
	int Tiredness;
	// should be attractor_reg *
	void * followingAttractor;	// NULL for following nobody
};

/*
 *	struct player_behavior_reg
 *  store information about making/displaying player action decision
 */
struct player_behavior_reg
{
	/*
	 *  last_action
	 *	this should be last activity of the player
	 *  Code representation:
	 *    BM BE    Need more HP   , BM=Moving to food      BE=Eat the food
	 *    AM AA    Attacking      , AM=Moving to target    AA=Attacking the target
	 *    TM TR    Tired          , TM=Moving to attractor TR=Resting
	 *    S        Staying        ,
	 *    RM RS    Random movement, RM=Random movement     RS=Gerenate new random movement
	 */
	char last_action [2];

	/*
	 *	angst, target
	 *  decide if player attacks another player
	 *  angst: increase on any action, decrease on attack(or bomb)
	 *  target: (object *) the target want to attack
	 */
	int angst;
	void * target;

	/*
	 *	tiredness, following_attractor
	 *  decide if player need to rest
	 *  tiredness: increase on any action, decrease on moving around attractor(s)
	 *  following_attactor: (attractor_reg *) the attractor is following
	 */
	int tiredness;
	void * following_attractor;

	/*
	 *  waiting
	 *	time to staying same place, low priority to attack or rest, high to random movement
	 */
	int waiting;

	/*
	 *	dest
	 *  destination of random movement
	 */
	Position dest;
};


struct peer_object_store
{
	object * player;
	vector<object *> * objs;
};

class behavior
{
public:
	struct attractor_reg
	{
		Position pos;
		int colddown;
		int show;
	};

public:
	//behavior (SimPara * para, const char * playfile, const char * recordfile);
    // SimPara, record mode, food image filename, action image filename
    behavior (SimPara * para, int mode, const string & food_image_file, const string & action_image_file,
            arbitrator_reg * arbr);
	~behavior ();
	
	Position& RandomPointInCircle (Position& center, int r);
	Position  GenerateNewDest ();
	Position& GenerateNewDest (object& player, b_register& reg);

	Position GetInitPosition ();

	//void InitRegister (b_register & reg);
	//int NextStep (b_register& reg, object& player, vector<object *>& objs);
	void      InitRegister (player_behavior_reg & r);
	int       NextStep     (player_behavior_reg & pr, peer_object_store& os);
	b_target& NextTarget   ();

	void ProcessAttractor ();

	//int isAngry (b_register& reg);
	int  isAngry           (player_behavior_reg & pr);
	int  stillAngry        (player_behavior_reg & pr);
	int  isTired           (player_behavior_reg & pr);
	int  stillTired        (player_behavior_reg & pr);

	// processing about food
	vector<int> &               GetFoodStartIndex ();
	vector<food_creation_reg> & GetFoodStore ();
    int                         RefreshFoodImageFile ();

    bool IsInitialized ();
	bool IsPlayMode ();
	bool IsRecordMode ();

	int getAttractorPosition (Position * ator_array);

#ifdef CONFIG_EXPORT_NODE_POSITION_RECORD
    bool RecordPositions ();
#endif

private:
	attractor_reg* CreateAttractor ();
	bool Nearby (object& obj1, object& obj2);
	bool Nearby (Position& po1, Position& po2);
	object * NearestObject (vector<object *> & objs, object & player, int type);
    object * NearestObject (vector<object *> & objs, Position & pos, int type);
	attractor_reg * NearestAttractor (Position& pos);

    /*
	inline bool readaction (int& action, b_target& target);
	inline void writeaction (const int& action, const b_target& target);
	inline void writeEOL ();
    */

private:
	SimPara         * _para;
    arbitrator_reg  * _arbr;
	b_target          _target;

	//RecordFileManager _rfm_play, _rfm_rec;
    RecordFileManager _rfm_food, _rfm_action;
#ifdef CONFIG_EXPORT_NODE_POSITION_RECORD
    RecordFileManager _rfm_pos;
#endif
    bool _initialized;     // use only in play mode
    int _mode;
	vector<attractor_reg *> _attractor;

	// step # create food food_store[food_start_index[#]] .. food_store[food_start_index[#+1]]
	vector<int> food_start_index;
	vector<food_creation_reg> food_store;

    errout _e;
};


#endif /* _VASTATESIM_BEHAVIOR_H */

