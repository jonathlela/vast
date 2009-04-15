
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
 * behavior.cpp (VASTATE simulation node behavior model)
 *
 *
 */

//#include "shared.h"
#include "vastate.h"
using namespace VAST;

#include "vastatesim.h"
#include "simgame.h"
#include "behavior.h"


//behavior::behavior (SimPara * para, const char * playfile, const char * recordfile)
behavior::behavior (SimPara * para, int mode, const string & food_image_file, const string & action_image_file, 
                    arbitrator_reg * arbr)
	: _para (para), _arbr(arbr), _initialized (false), _mode (VS_NORMAL_MODE)
{
	// set random seed (it should already did in vastatesim.cpp)
	//srand (time (NULL));

    if ((mode != VS_NORMAL_MODE) && (!food_image_file.empty ()) && (!action_image_file.empty ()))
    {
        int r_mode;
        if (mode == VS_RECORD_MODE)
            r_mode = RecordFileManager::MODE_WRITE;
        else if (mode == VS_PLAY_MODE)
            r_mode = RecordFileManager::MODE_READ;
        else
            return;
        if (_rfm_food.initRecordFile (r_mode, food_image_file.c_str ()) == false)
            return;
        if (_rfm_action.initRecordFile (r_mode, action_image_file.c_str ()) == false)
            return;

        // TODO:  check simulation parameters to ensure playfile is match.
	    // TODO:  Change filename when file exists automaticlly
        /*
        if (mode == VS_RECORD_MODE)
        {

            _rfm_food.writeFileHeader (para);
            _rfm_action.writeFileHeader (para);
        }
        else if (mode == VS_PLAY_MODE)
        {
            if (!_rfm_food.compareFileHeader (para))
                return;
            if (!_rfm_food.compareFileHeader (para))
                return;
        }
        */
    }

    if (mode == VS_PLAY_MODE)
    {
        if (_rfm_food.readFood (para, food_start_index, food_store) == false)
        //    return;
        //else
        {
            string s = "Reading food image file \"";
            s += food_image_file;
            s += "\" error." NEWLINE;
            _e.output (s.c_str ());
        }
    }

    if (mode == VS_RECORD_MODE || mode == VS_NORMAL_MODE)
    {
        food_start_index.reserve (_para->TIME_STEPS + 1);
        food_start_index.assign (_para->TIME_STEPS + 1, 0);
        food_start_index[0] = food_start_index[_para->TIME_STEPS] = 0;
    }

	// insert attractors
	for (int i=0; i<_para->ATTRACTOR_MAX_COUNT; i++)
		_attractor.push_back (CreateAttractor ());

    // initialize position record file
#ifdef CONFIG_EXPORT_NODE_POSITION_RECORD
    char str[256];
    sprintf (str, "%dx%ds%dn%d.pos", _para->WORLD_WIDTH, _para->WORLD_HEIGHT, _para->TIME_STEPS, _para->NODE_SIZE);
    _rfm_pos.initRecordFile (RecordFileManager::MODE_WRITE, str);
    _rfm_pos.writeFileHeader (_para);
#endif

    _initialized = true;
    _mode = mode;
}

behavior::~behavior ()
{
	// delete all attractors
	while (!_attractor.empty ())
	{
		delete _attractor.back ();
		_attractor.pop_back ();
	}
}


void behavior::InitRegister (player_behavior_reg & r)
{
	strncpy (r.last_action, "S ", 2);
	r.angst               = rand() % _para->REACTION_THR;
	r.target              = NULL;
	r.tiredness           = rand() % _para->REACTION_THR;
	r.following_attractor = NULL;
	r.waiting             = 0;
	r.dest                = GenerateNewDest ();//Position (-1,-1);
}

// ProcessAttractor ()
//		this method should be called at start or end of every step
// Rules to show attractor:
//	1.	longer time(step) attractor appear/disappear,
//		then more chance attractor disappear/appear.
//	2.  colddown means the elapsed time that the attractor appear/disappear
//  3.  attractor never change state(from appear to disappear, or inverse)
//      when colddown < ATTRACTOR_BASIC_COLDDOWN
//      , and attractor does change state
//      when colddown >= ATTRACTOR_BASIC_COLDDOWN
//
void behavior::ProcessAttractor ()
{	
	//int r;
	//int mc = _para->ATTRACTOR_MAX_COLDDOWN;
	//int bc = _para->ATTRACTOR_BASIC_COLDDOWN;

	vector<attractor_reg *>::iterator it = _attractor.begin ();
	for (; it != _attractor.end (); it++)
	{
		attractor_reg * reg = *it;
		reg->colddown --;
		if (reg->colddown <= 0)
		{
			reg->colddown = _para->ATTRACTOR_BASIC_COLDDOWN + (rand() % (_para->ATTRACTOR_MAX_COLDDOWN-_para->ATTRACTOR_BASIC_COLDDOWN));
			reg->show = (reg->show + 1) % 2;
			reg->pos = Position (rand() % _para->WORLD_WIDTH, rand() % _para->WORLD_HEIGHT);
		}
		/*
		attractor_reg * reg = *it;
		reg->colddown ++;
		r = rand() % (mc-bc);
		if (r < (reg->colddown - bc))
		{
			reg->colddown = 0;
			reg->show = (reg->show + 1) % 2;
			reg->pos = Position (rand() % _para->WORLD_WIDTH, rand() % _para->WORLD_HEIGHT);
		}
		*/
	}

	// TODO: record # of attractor into log file
}

Position& behavior::RandomPointInCircle (Position& center, int r)
{
	static double pi = acos ((double)-1);
	static Position p;
	int dis;
	int degree;

	do {
		dis = rand() % r;
		degree = rand() % 360;
		p.x =(int)( (double)center.x + cos (((double)degree/180)*pi) * (double)dis );
		p.y =(int)( (double)center.y + sin (((double)degree/180)*pi) * (double)dis );
	} while ((p.x < 0) || (p.y < 0) || (p.x > _para->WORLD_WIDTH) || (p.y > _para->WORLD_HEIGHT)) ;
	
	return p;
}

Position behavior::GenerateNewDest ()
{
	return Position((long)(rand () % (_para->WORLD_WIDTH)), (long)(rand () % (_para->WORLD_HEIGHT)));
}

Position behavior::GetInitPosition ()
{
	int act;
	b_target t;

	if (IsPlayMode ())
	{
        if (_rfm_action.readAction (act, t) != true)
		{
			t.center.x = (long)(rand () % (_para->WORLD_WIDTH ));
			t.center.y = (long)(rand () % (_para->WORLD_HEIGHT));
		}
	}
	else
	{
		t.center.x = (long)(rand () % (_para->WORLD_WIDTH ));
		t.center.y = (long)(rand () % (_para->WORLD_HEIGHT));
	}

	if (IsRecordMode ())
        _rfm_action.writeAction (act, t);

	return t.center;
}


//	action code meaning:
//		1:Move 2:Eat 3:Attack 4:Bomb 5:do nothing
//	    // actions done physically in peer in SimGame::NextStep ()

// Behavior code meaning
//      the behavior code shows last decided action for one avatar
/*
        B   food
    -       BM  moving for a food
    -       BE  eats a food
        T   to attractor
    -       TM  moving for an attractor
    -       TR  resting under an attractor
        A   attacking
    -       AM  moving for attacking someone
    -       AA  done a normal attack
    -       AB  throws a bomb (attacking a circular range
    -   S   staying
        R   random moving (by Random Waypoint)
    -       RM  random moving
    -       RS  staying between two random walks
*
*/
int behavior::NextStep (player_behavior_reg& pr, peer_object_store& os)
{
    AttributeBuilder atrbuilder;
    enum {AC_MOVE=1, AC_EAT=2, AC_ATTACK=3, AC_BOMB=4, AC_NOTHING=5};

    // do nothing by default
	int action = AC_NOTHING;

    // if _play_mode,  only need to do is read and return.
	if (IsPlayMode ())
	{
        // read action from record file
        if (_rfm_action.readAction (action, _target) != true)
		{
    		//printf ("Error on reading action.\n");
            _e.output ("Error on reading action.\n");
			action = AC_NOTHING;
		}
        else
        {
            // check if need to re-target
            object * target_object;
            switch (action)
            {
            case AC_EAT:
                target_object = NearestObject (*os.objs, _target.center, SimGame::OT_FOOD);
                if ((target_object == NULL) || (target_object->get_pos ().dist (_target.center) > _para->VELOCITY * 2))
                    action = AC_NOTHING;
                else
                    _target.object_id = target_object->get_id ();
                break;
            case AC_ATTACK:
                target_object = NearestObject (*os.objs, _target.center, SimGame::OT_PLAYER);
                if ((target_object == NULL) || (target_object->get_pos ().dist (_target.center) > _para->VELOCITY * 2))
                    action = AC_NOTHING;
                else
                    _target.object_id = target_object->get_id ();
                break;
            }
        }
	}

    // in NORMAL or RECORD mode, so make a decision
	else
	{
		object * nf;	// nearest food
		if ((atrbuilder.getPlayerHP(*os.player) <= _para->HP_LOW_THRESHOLD)
			&& ((nf = NearestObject (*os.objs, *os.player, SimGame::OT_FOOD)) != NULL))
		{
			if (Nearby(*os.player, *nf))
			{
				action            = AC_EAT;
				_target.object_id = nf->get_id ();
                _target.center    = nf->get_pos ();
				strncpy (pr.last_action, "BE", 2);
                strncpy (_target.last_action, pr.last_action, 2);
			}
			else
			{
				action            = AC_MOVE;
				_target.center    = nf->get_pos ();
				strncpy (pr.last_action, "BM", 2);
                strncpy (_target.last_action, pr.last_action, 2);
			}
			goto decision_done;
		}

		if (isTired (pr) || pr.following_attractor != NULL)
		{
			attractor_reg * na;
			if ((pr.following_attractor == NULL) || (((attractor_reg *)pr.following_attractor)->show == 0))
			{
				na = NearestAttractor(os.player->get_pos());
				
				if (na != NULL)
				{
					pr.dest = RandomPointInCircle(na->pos, _para->ATTRACTOR_RANGE);
					pr.target = NULL;
				}
				pr.following_attractor = (void *) na;
			}
			
			if (pr.following_attractor != NULL)
			{
				bool stilltired = (stillTired(pr)==1)?true:false;
				bool nearby = Nearby(os.player->get_pos(), pr.dest);
				na = (attractor_reg *) pr.following_attractor;
				
				if (nearby && !stilltired)
				{
					pr.dest = Position (-1,-1);
					pr.following_attractor = NULL;
					//pr.waiting = rand() % _para->STAY_MAX_STEP;
				}
				else
				{
					if (nearby & stilltired)
						pr.dest = RandomPointInCircle(na->pos, _para->ATTRACTOR_RANGE);
					action = AC_MOVE;
					_target.center = pr.dest;
					strncpy (pr.last_action, "TM", 2);
                    strncpy (_target.last_action, pr.last_action, 2);
                    

					if (na->pos.dist (os.player->get_pos()) <= _para->ATTRACTOR_RANGE)
					{
						pr.tiredness -= (10 *(_para->TIREDNESS_OF_REST + _para->TIREDNESS_OF_ACTION));
						strncpy (pr.last_action, "TR", 2);
						if (pr.tiredness < 0)
							pr.tiredness = 0;
					}
					
					goto decision_done;
				}
			}
		}

		if ((isAngry (pr) > 0) || pr.target != NULL)
		{
			if (pr.target == NULL)
				pr.target = NearestObject (*os.objs, *os.player, SimGame::OT_PLAYER);

			if (pr.target != NULL)
			{
				object * t = (object *) pr.target;
				bool target_nearby = (os.player->get_pos().dist (t->get_pos()) <= (_para->VELOCITY * 2));
				if (target_nearby)
				{
					int act = isAngry(pr);
					if (act == 1)
					{
						action            = AC_ATTACK;
						_target.object_id = t->get_id ();
                        _target.center    = t->get_pos ();
						strncpy (pr.last_action, "AA", 2);
                        strncpy (_target.last_action, pr.last_action, 2);
						pr.angst -= (10 * (_para->ANGST_OF_ATTACK + _para->ANGST_OF_ACTION));
					}
					else if (act == 2)
					{
						action            = AC_BOMB;
						_target.center    = RandomPointInCircle (os.player->get_pos (), _para->VELOCITY * 10);
						_target.radius    = (int) (_para->VELOCITY * 2.5);
						strncpy (pr.last_action, "AB", 2);
                        strncpy (_target.last_action, pr.last_action, 2);
						pr.angst -= (10 * (_para->ANGST_OF_ATTACK * 5 + _para->ANGST_OF_ACTION));
					}
					if (pr.angst < 0)
						pr.angst = 0;

					if (!stillAngry(pr))
						pr.target = NULL;
				}
				else
				{
					action = AC_MOVE;
					_target.center = t->get_pos();
                    _target.center.x --;
                    _target.center.y --;
					strncpy (pr.last_action, "AM", 2);
                    strncpy (_target.last_action, pr.last_action, 2);
                    // while tracking person to attack , decrease angry to prevent a "people train"
                    pr.angst -= (_para->ANGST_OF_ACTION * 4);
				}

				goto decision_done;
			}
		}

		if (pr.waiting > 0)
		{
			pr.waiting --;
			action = AC_NOTHING;
			strncpy (pr.last_action, "S ", 2);
		}
		else
		{
			// does random movement
			if ((pr.dest == Position (-1,-1))
				|| Nearby (pr.dest, os.player->get_pos()))
			{
				pr.dest = GenerateNewDest ();
				pr.waiting  = rand() % _para->STAY_MAX_STEP;
				action  = AC_NOTHING;
				strncpy (pr.last_action, "RS", 2);
			}
			else
			{
				action            = AC_MOVE;
				_target.object_id = 0;
				_target.center    = pr.dest;
				_target.radius    = 0;
				strncpy (pr.last_action, "RM", 2);
                strncpy (_target.last_action, pr.last_action, 2);
			}
		}
	}
	
decision_done:

	pr.angst += ((rand() % _para->ANGST_OF_ACTION) + 1);
	pr.tiredness += ((rand() % _para->TIREDNESS_OF_ACTION) + 1);

    // Check the need and recond the action
	if (IsRecordMode ())
    {
        _rfm_action.writeAction (action, _target);
        _rfm_action.refresh ();
    }

	/*
	 *	 return the decision
	 *     and other parameter(s) about the decision is stored in _target
	 *     can be accessed by NextTarget ()
	 */
	return action;
}

int behavior::stillAngry (player_behavior_reg & pr)
{
	return (pr.angst>=_para->REACTION_STOP_THR)?1:0;
}

int behavior::isAngry (player_behavior_reg & pr)
{
	if (pr.angst < _para->REACTION_THR)
		return 0;
	else
	{
		if (pr.angst >= (int)(_para->REACTION_THR * 1.5))
			return 2;
		else
			return 1;
	}
}

int behavior::stillTired (player_behavior_reg & pr)
{
	return (pr.tiredness >= _para->REACTION_STOP_THR)?1:0;
}

int behavior::isTired (player_behavior_reg & pr)
{
	return (pr.tiredness >= _para->REACTION_THR)?1:0;
}

int behavior::getAttractorPosition (Position * ator_array)
{
	vector<attractor_reg *>::iterator it = _attractor.begin ();
	int last = 0;

	for (; it != _attractor.end (); it++)
	{
		attractor_reg * att = *it;
		if (att->show == 1)
			ator_array[last++] = att->pos;
	}
	return last;
}

behavior::attractor_reg* behavior::CreateAttractor ()
{
	attractor_reg * reg;

	reg = new attractor_reg;
    reg->pos = Position (rand() % _para->WORLD_WIDTH, rand() % _para->WORLD_HEIGHT);
	reg->colddown = _para->ATTRACTOR_BASIC_COLDDOWN + (rand() % (_para->ATTRACTOR_MAX_COLDDOWN - _para->ATTRACTOR_BASIC_COLDDOWN));
	reg->show = rand () % 2;

	return reg;
}

bool behavior::Nearby (object& obj1, object& obj2)
{
	if (obj1.get_pos().dist (obj2.get_pos()) <= _para->VELOCITY)
		return true;
	return false;
}

bool behavior::Nearby (Position& po1, Position& po2)
{
	if (po1.dist (po2) <= 5)// _para->VELOCITY)
		return true;
	return false;
}

// find a "type" object in "objs" is the nearest to "player" (but not player)
object* behavior::NearestObject (vector<object *>& objs, object& player, int type)
{
	AttributeBuilder atrbuilder;
	vector<object *>::iterator it = objs.begin ();
	int w = _para->WORLD_WIDTH;
	int h = _para->WORLD_HEIGHT;
	double dis = sqrt ((double)(w*w + h*h));
	object * targetObject = NULL;
	Position targetPoint = player.get_pos ();

	for (; it != objs.end (); it ++)
	{
		object * tmpObject = *it;
		
		if (atrbuilder.checkType(*tmpObject,type)
			&& (tmpObject->get_id() != player.get_id())
			&& (tmpObject->get_pos ().dist(targetPoint) < dis))
		{
			targetObject = tmpObject;
			dis = tmpObject->get_pos().dist(targetPoint);
		}
	}
	return targetObject;
}

// find a "type" object in objs is the nearest to position "pos"
object * behavior::NearestObject (vector<object *> & objs, Position & pos, int type)
{
    AttributeBuilder atb;
   	vector<object *>::iterator it = objs.begin ();
	int w = _para->WORLD_WIDTH;
	int h = _para->WORLD_HEIGHT;
	double dis = sqrt ((double)(w*w + h*h));
	object * targetObject = NULL;

	for (; it != objs.end (); it ++)
	{
		object * tmpObject = *it;
		
		if (atb.checkType(*tmpObject,type)
			&& (tmpObject->get_pos ().dist(pos) < dis))
		{
			targetObject = tmpObject;
			dis = tmpObject->get_pos().dist(pos);
		}
	}

	return targetObject;
}

behavior::attractor_reg * behavior::NearestAttractor (Position& pos)
{
	vector<attractor_reg *>::iterator it = _attractor.begin ();
	attractor_reg * targetAttractor = NULL;
	int w = _para->WORLD_WIDTH;
	int h = _para->WORLD_HEIGHT;
	double dis = sqrt ((double)(w*w + h*h));

	for (; it != _attractor.end (); it++)
	{
		attractor_reg * tmpAtt = *it;
		if ((tmpAtt->show == 1)
			&& (tmpAtt->pos.dist (pos) < dis))
		{
			targetAttractor = tmpAtt;
			dis = tmpAtt->pos.dist (pos);
		}
	}
	return targetAttractor;
}

b_target & behavior::NextTarget ()
{
	return _target;
}

bool behavior::IsInitialized ()
{
    return _initialized;
}

bool behavior::IsPlayMode ()
{
    return _initialized 
        && _mode == VS_PLAY_MODE
        && _rfm_food.IsInitialized ()
        && _rfm_action.IsInitialized ();
}

bool behavior::IsRecordMode ()
{
    return _initialized
        && _mode == VS_RECORD_MODE
        && _rfm_food.IsInitialized ()
        && _rfm_action.IsInitialized ();
}

// processing about food
vector<int> & behavior::GetFoodStartIndex ()
{
	return food_start_index;
}

vector<food_creation_reg> & behavior::GetFoodStore ()
{
	return food_store;
}

int behavior::RefreshFoodImageFile ()
{
    if (IsRecordMode ())
    {
        _rfm_food.writeFood (_para, food_start_index, food_store, true);
        _rfm_food.refresh ();
    }
    return 0;
}

#ifdef CONFIG_EXPORT_NODE_POSITION_RECORD
bool behavior::RecordPositions ()
{
    StepHeader sh;
    sh.timestamp = _para->current_timestamp;

    _rfm_pos.writeStepHeader (sh);

    NodeInfo n;
    memset (&n , 0, sizeof (NodeInfo));

    // write objects
    for(map<obj_id_t, object_signature>::iterator it = _arbr->god_store.begin (); it != _arbr->god_store.end (); it ++)
    {
        object_signature & objn = it->second;
        n.obj_id = objn.id;
        n.id     = objn.peer;
        n.pos    = objn.pos;
        n.type   = (objn.peer == 0) ? 0 : 1;

        _rfm_pos.writeNode (n);
    }

    // write arbitrators
    for (map<VAST::id_t, Node>::iterator it = _arbr->arbitrators.begin (); it != _arbr->arbitrators.end (); it ++)
    {
        Node & node = it->second;
        n.obj_id = 0;
        n.id     = node.id;
        n.pos    = node.pos;
        n.type   = 2;

        _rfm_pos.writeNode (n);
    }

    // write an end record
    memset (&n, 0, sizeof (NodeInfo));
    _rfm_pos.writeNode (n);

    return true;
}
#endif


//////////////////////////////////////////////////////////////////////

	/*
	Position& behavior::GenerateNewDest (object& player, b_register& reg)
	{
		static Position po;

		if ((reg.Tiredness > _para->MIN_TIREDNESS_NEED_REST)
			|| ((reg.followingAttractor != NULL) && (reg.Tiredness > 10)))
		{
			// if hasn't an attractor or following attractor disappeared
			// then find a nearest attractor
			if ((reg.followingAttractor == NULL)
				|| ((reg.followingAttractor != NULL) && (((attractor_reg *)(reg.followingAttractor))->show == 0)))
				reg.followingAttractor = NearestAttractor (player.get_pos ());

			if (reg.followingAttractor != NULL)
			{
				attractor_reg * atr = (attractor_reg *)reg.followingAttractor;
				po = RandomPointInCircle (atr->pos, _para->ATTRACTOR_RANGE);
			}
		}
		else
		{
			reg.followingAttractor = NULL;
		}

		if (reg.followingAttractor == NULL)
		{
			po.x = (long)(rand () % (_para->WORLD_WIDTH ));
			po.y = (long)(rand () % (_para->WORLD_HEIGHT));
		}
		
		return po;
	}
	*/


	//	action code meaning:
	//		1:Move 2:Eat 3:Attack 4:Bomb 5:do nothing
	//	    // actions done physically in peer in SimGame::NextStep ()
	/*
	int behavior::NextStep (b_register& reg, object& player, vector<object *>& objs)
	{
		enum {AC_MOVE=1, AC_EAT=2, AC_ATTACK=3, AC_BOMB=4, AC_NOTHING=5};
		int action;
		b_target target;
		AttributeBuilder ab;
		

		if (_play_mode)
		{	// if in play_mode, read the action and execute
			if (readaction (action, _target) != true)
			{
				printf ("Error on reading action.\n");
				action = AC_NOTHING;
			}
		}
		else
		{	// do AI computation
			object * nf;
			if ((ab.getPlayerHP (player) < _para->HP_LOW_THRESHOLD)
				&& ((nf = NearestObject (objs, player, SimGame::OT_FOOD)) != NULL))
			{
				if (Nearby(player, *nf))
				{
					//Eat (nf);
					action            = AC_EAT;
					_target.object_id = nf->get_id ();
					_target.center    = nf->get_pos ();
					_target.radius    = 1;
					//_target.p_object = nf;
				}
				else
				{
					//MoveOneStep (nf);
					action            = AC_MOVE;
					_target.object_id = nf->get_id ();
					_target.center    = nf->get_pos ();
					_target.radius    = 1;
				}
			}
			else
			{
				int act;
				object * attack_target;

				if ((act = IsAngry (reg)) &&
					((attack_target = NearestObject (objs, player, SimGame::OT_PLAYER)) != NULL))
				{
					if (act == 1)
					{
						//AttackNearestPerson();
						action            = AC_ATTACK;
						_target.object_id = attack_target->get_id ();
						_target.center    = attack_target->get_pos ();
						_target.radius    = _para->VELOCITY +1;
					}
					else if (act == 2)
					{
						//Bomb ();
						action            = AC_BOMB;
						_target.object_id = 0;
						_target.center    = RandomPointInCircle (player.get_pos (), _para->AOI_RADIUS/2);
						_target.radius    = rand () % (_para->AOI_RADIUS / 4);
					}
				}
				else if (reg.WaitingTime != 0)
				{
					action            = AC_NOTHING;
					//_target.object_id = 0;
					//_target.center    = RandomPointInCircle (player.get_pos (), AOI_RADIUS/2);
					//_target.radius    = rand () % (AOI_RADIUS / 4);

					reg.WaitingTime --;
				}
				else if ((reg.Destination.x == -1)
					&& (reg.Destination.y == -1))
				{
					reg.Destination = GenerateNewDest (player, reg);
					//MoveOneStep (reg.Destination);
					action            = AC_MOVE;
					_target.object_id = 0;
					_target.center    = reg.Destination;
					_target.radius    = 0;
				}
				else if (Nearby (reg.Destination, player.get_pos()))
				{
					reg.WaitingTime = rand() % 8;
					reg.Destination = GenerateNewDest(player, reg);

					action = AC_NOTHING;
				}
				else
				{
					//MoveOneStep (Destination);
					action            = AC_MOVE;
					_target.object_id = 0;
					_target.center    = reg.Destination;
					_target.radius    = 0;

					if (reg.followingAttractor != NULL)
					{
						reg.Tiredness -= _para->VELOCITY;
						if (reg.Tiredness < 0)
							reg.Tiredness = 0;
					}
				}
			}
		}

		// compute tiredness
		switch (action)
		{
		case AC_MOVE:
			reg.Tiredness += _para->VELOCITY;
			break;
		case AC_EAT:
			reg.Tiredness += 2;
			break;
		case AC_ATTACK:
			reg.Tiredness += 5;
			break;
		case AC_BOMB:
			reg.Tiredness += 10;
			break;
		case AC_NOTHING:
			reg.Tiredness += 1;
			break;
		default:
			printf ("behavior::NextStep (): unknown action code.\n");
		}

		if (_record_mode)
		{
			writeaction (action, _target);
			// write actions
		}

		return action;
	}
	*/

/*
int behavior::isAngry (b_register& reg)
{
	if (reg.Angst > 0)
	{
		reg.Angst --;
		return 0;
	}
	else
	{
		reg.Angst = rand () % 6;
		int angrylevel = rand () % 12;

		if (angrylevel>=9)
			return 2;
		else
			return 1;
	}
	return 0;
}
*/

