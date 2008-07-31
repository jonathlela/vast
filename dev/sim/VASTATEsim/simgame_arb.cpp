
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
 *	SimGame_Arb.cpp  (Vastate Simulator Game _ Arbitrator class implementation)
 *
 *	
 *
 */

#include "simgame_arb.h"

char   simgame_arbitrator_logic::_ostr [MAXBUFFER_SIZE];
errout simgame_arbitrator_logic::_eo;
AttributeBuilder simgame_arbitrator_logic::atb;

// callback - for authenaticing a newly joined peer
bool simgame_arbitrator_logic::join_requested (VAST::id_t from_id, char *data, int sizes)
{
#ifdef VASTATESIM_DEBUG
    errout eo;
    std::ostringstream oss;
    oss << "[ ] simgame_arb_logic::join_reuested (" << from_id << ", data size: " << sizes << "" NEWLINE;
    eo.output (oss.str ().c_str ());
#endif

    // 
    _arbitrator->insert_peer (from_id, NULL, 0);

    return true;

    /*

    if (_storage == NULL)
    {
        eo.output ("[err] simgame_arbitrator_logic::join_requested: _storage is NULL !" NEWLINE);
        return false;
    }

    if (sizes == JOIN_REQUEST_RANDOM_CHAR_LENGTH)
    {
        char *auth = new char[JOIN_REQUEST_RANDOM_CHAR_LENGTH + 1];
        strncpy (auth, data, JOIN_REQUEST_RANDOM_CHAR_LENGTH);
        _join_requesting [_storage->query (data, sizes)] = pair<VAST::id_t, char *> (from_id, auth);
    }
    else
    {
        eo.output ("[err] simgame_arbitrator_logic::join_requested: Incorrect join request received.");
        return false;
    }

    return true;
    */
}

    // callback - process a particular query for certain data
bool simgame_arbitrator_logic::query_received (int query_id, char *query, size_t size)
{
    _storage->respond (query_id, query, size);
    return true;
}

// callback - process the response to a particular query
bool simgame_arbitrator_logic::reply_received (int query_id, char *reply, size_t size)
{
    errout eo;

    if (_join_requesting.find (query_id) == _join_requesting.end ())
        return false;
    if ((size == JOIN_REQUEST_RANDOM_CHAR_LENGTH) && !strncmp (reply, _join_requesting [query_id].second, JOIN_REQUEST_RANDOM_CHAR_LENGTH))
    {
#ifdef VASTATESIM_DEBUG
        std::ostringstream oss;
        oss << "[" << _arbitrator->self->id << "] insert_peer [" << _join_requesting [query_id].first << "] called." NEWLINE;
        eo.output (oss.str ().c_str ());
#endif

        _arbitrator->insert_peer (_join_requesting [query_id].first, NULL, 0);
        delete[] (char *) _join_requesting [query_id].second;
        _join_requesting.erase (query_id);
        return true;
    }

    return false;
}

// callback - for processing a query returned by the shared storage
bool simgame_arbitrator_logic::query_returned (query_id_t query_id, char *data, int size)
{
    return true;
}

const std::vector<VAST::obj_id_t> &
    simgame_arbitrator_logic::event_affected (VAST::id_t from_id, event &e)
{
    // return value
    static std::vector<VAST::obj_id_t> ret;
    ret.clear ();

    int i_value;

    // get sender object id
	e.get (0, i_value);
    VAST::id_t sender_object_id = (VAST::id_t) i_value;

    // get sender object
    object * sender_object = NULL;
    if ((sender_object = get_object (sender_object_id)) == NULL)
        return ret; 

    // check for events
    switch (e.type)
    {
        case SimGame::E_MOVE:
            ret.push_back (sender_object->get_id ());
            break;

        case SimGame::E_ATTACK:
        {
            e.get (1, i_value);
            VAST::id_t target_id = (VAST::id_t) i_value;

            object *target_object;
            if ((target_object = get_object (target_id)) == NULL)
                break;
            
            if (target_object->get_pos ().dist (sender_object->get_pos ()) <= _para->VELOCITY * 2)
                ret.push_back (target_id);
        }
        break;

        case SimGame::E_BOMB:
        {
            int x, y, r;
		    e.get (1, x);
		    e.get (2, y);
		    e.get (3, r);

            Position dest = Position ((double) x, (double) y);

#ifdef VASTATESIM_DEBUG
            sprintf (_ostr, "[%d] INFO: arb_logic: bomb from [Peer%d] to (%d,%d) r %d" NEWLINE,
            			_arbitrator->self->id, e.get_sender(), x,y,r);
		    _eo.output(_ostr);
#endif

			map<VAST::id_t, object*>::iterator it;
			for (it = _own_objects.begin(); it != _own_objects.end(); it ++)
			{
				object * obj = it->second;
                if (dest.dist (obj->get_pos ()) <= (double) r)
                    ret.push_back (obj->get_id ());
            }
		}
		break;

	    case SimGame::E_EAT:
        {
	        e.get (1, i_value);
            VAST::id_t target_id = (VAST::id_t) i_value;

            object * food;
            if ((food = get_object (target_id)) == NULL)
                break;

            if (food->get_pos ().dist (sender_object->get_pos ()) <= _para->VELOCITY * 2)
	        {
                ret.push_back (sender_object_id);
                ret.push_back (food->get_id ());
	        }
        }
        break;

        default:
            sprintf (_ostr, "[%lu] ERROR: simgame_arbitrator_logic::event_affected (): received unknown event type (%d)", this->_arbitrator->self->id, 
                            e.type);
            _eo.output (_ostr);
    }

    return ret;
}

// callback -  for receiving a remote event (arbitrator only)
bool simgame_arbitrator_logic::event_received (VAST::id_t from_id, event &e)
{
    AttributeBuilder ab;
#ifdef VASTATESIM_DEBUG
    errout eo;
	char str[256];
#endif

    int sender_object_id;
	e.get (0, sender_object_id);

    // get sender object
    object * sender_object = get_object (sender_object_id);
    if (sender_object == NULL)
        return false;

    Position &sender_pos = sender_object->get_pos ();

	Position dest;
	//double x, y, r;
    float fx, fy, fr;
	double dx, dy;
	double ratio;
	int target_id;
	object * target_object;

	switch (e.type)
	{
	case SimGame::E_MOVE:
		e.get (1, fx);
		e.get (2, fy);
		dest.x = fx;
		dest.y = fy;

		dx = dest.x - sender_pos.x;
        dy = dest.y - sender_pos.y;
        
        // adjust deltas for constant velocity
        ratio = sqrt((dx*dx) + (dy*dy)) / (double)(_para->VELOCITY);

        if (ratio > 1)
        {
            // note that this may cause actual velocity to be less than desired
            // as any decimal places are dropped
            dx = ((double)dx / ratio);
            dy = ((double)dy / ratio);
        }

		dest = sender_pos;
		dest.x += dx;
		dest.y += dy;
		// sender_object->set_pos (dest);
		_arbitrator->change_pos(sender_object, dest);

#ifdef VASTATESIM_DEBUG
		sprintf (str, "[%d] arb_logic: pos_change_event: objectid: %s peer %d pos to: (%d,%d)" NEWLINE,
			_arbitrator->self->id,
            ab.objectIDToString (sender_object->get_id()).c_str (), sender_object->peer, (int)dest.x, (int)dest.y);
		eo.output(str);
#endif
		break;

	case SimGame::E_ATTACK:
		e.get (1, target_id);

        if (_own_objects.find (target_id) == _own_objects.end ())
        {
#ifdef VASTATESIM_DEBUG
            sprintf (str, "[%d] arbitrator_logic: received event from %s attacks to unknown object %s\n",
                (_arbitrator->self==NULL)?(0):(_arbitrator->self->id), ab.objectIDToString(sender_object_id).c_str (), ab.objectIDToString (target_id).c_str ());
            eo.output (str);
#endif
            return false;
        }

		if (_own_objects[target_id]->get_pos().dist (sender_pos) <= _para->VELOCITY * 2)
		{
			AttributeBuilder ab;
			int userhp = ab.getPlayerHP(*_own_objects[target_id]);
			userhp -= _para->ATTACK_POWER;
			if (userhp < 0)
				userhp = 0;
			//ab.setPlayerHP(*_own_objects[target_id], userhp);
			_arbitrator->update_obj(_own_objects[target_id], SimGame::PlayerHP
				, VASTATE_ATT_TYPE_INT, &userhp);
#ifdef VASTATESIM_DEBUG
			sprintf (str, "[%d] arb_logic: Attack from [Peer%d] to %s(hp %d -> %d)" NEWLINE,
                _arbitrator->self->id, e.get_sender(), ab.objectIDToString(target_id).c_str (),
				ab.getPlayerHP(*_own_objects[target_id]), userhp);
			eo.output(str);
#endif
		}
#ifdef VASTATESIM_DEBUG
        else
		{
			sprintf (str, "[%d] arb_logic: Invalid attack from [Peer%d] to [%d]" NEWLINE,
				_arbitrator->self->id, e.get_sender(), target_id);
			eo.output(str);
		}
#endif
		break;

	case SimGame::E_BOMB:
		e.get (1, fx);
		e.get (2, fy);
		e.get (3, fr);

		dest.x = fx;
		dest.y = fy;

#ifdef VASTATESIM_DEBUG
		sprintf (str, "[%d] arb_logic: bomb from [Peer%d] to (%d,%d) r %d" NEWLINE,
			_arbitrator->self->id, e.get_sender(), (int) fx, (int) fy, (int) fr);
		eo.output(str);
#endif
		
		{
			map<VAST::id_t, object*>::iterator it;
			for (it = _own_objects.begin(); it != _own_objects.end(); it++)
			{
				target_object = (*it).second;
				if (dest.distsqr (target_object->get_pos()) <= (double) (fr*fr))
				{
					int userhp = ab.getPlayerHP(*target_object);
					userhp -= _para->BOMB_POWER;
					if (userhp < 0)
						userhp = 0;
					ab.setPlayerHP(*target_object, userhp);
					_arbitrator->update_obj(target_object, SimGame::PlayerHP
						, VASTATE_ATT_TYPE_INT, &userhp);
#ifdef VASTATESIM_DEBUG
					sprintf (str, "[%d] arb_logic: bomb from [Peer%d] to %d(hp %d -> %d)" NEWLINE,
                        _arbitrator->self->id, e.get_sender(), ab.objectIDToString(target_object->get_id()).c_str (),
						ab.getPlayerHP(*target_object)	, userhp);
					eo.output(str);
#endif
				}
			}
		}
		break;

	case SimGame::E_EAT:
		e.get (1, target_id);
		if (_own_objects.find (target_id) == _own_objects.end ())
			break;

		target_object = _own_objects[target_id];

		if (target_object->get_pos().dist (sender_pos) <= _para->VELOCITY * 2)
		{
			int userhp = ab.getPlayerHP(*_own_objects[sender_object_id]);
			int usermaxhp = ab.getPlayerMaxHP (*_own_objects[sender_object_id]);
			userhp += _para->FOOD_POWER;
			if (userhp > usermaxhp)
				userhp = usermaxhp;
			//ab.setPlayerHP(*_own_objects[target_id], userhp);
			_arbitrator->update_obj(_own_objects[sender_object_id], SimGame::PlayerHP,
				VASTATE_ATT_TYPE_INT, &userhp);

			int quality = ab.getFoodCount (*target_object);
			quality --;
			if (quality >0)
			{
				// _reg->foods[target_object->get_id()]->count = quality;
				// ab.setFoodCount (*target_object, quality);
				_arbitrator->update_obj(target_object, SimGame::FoodCount
					, VASTATE_ATT_TYPE_INT, &quality);
			}
			else
			{
				//_reg->deletefood (target_object->get_id ());
				if (_arbitrator->delete_obj (target_object))
                    _reg->dec_food ();
			}
		}
#ifdef VASTATESIM_DEBUG
        else
		{
			sprintf (str, "Invalid eatting from  %d  to  %d.\n"
				, e.get_sender(), sender_object->get_pos().x, sender_object->get_pos().y
				, target_id, target_object->get_pos().x, target_object->get_pos().y
				);
			eo.output(str);

		}
#endif
		break;

	default:
		;
	}

	// called by arbitrator::processmsg automatically
	//_arbitrator->update_states ();
	return true;
}
    
// callback - by remote arbitrator to create objects

void simgame_arbitrator_logic::obj_created (object *obj, void *ref, size_t size)
{
	errout e;
	char ch[200];
	AttributeBuilder ab;

	if (obj->peer != 0)
	{
		//		statistics * sta = statistics::getInstance ();

		// TODO: read datas from server
		sprintf (ch, "Peer%lu", obj->peer);
		//		bool buildresult = true;
		//bool buildresult = ab.buildPlayer (*obj, ch, rand () % _para->HP_MAX, _para->HP_MAX);
		ab.buildPlayer (*obj, ch, rand () % _para->HP_MAX, _para->HP_MAX);
		//		sta->objectChanged (1, player_object->get_id ());
		
#ifdef VASTATESIM_DEBUG
		sprintf (ch, "[Peer%d][%d] joined." NEWLINE, 
			obj->peer, obj->get_id());
		e.output(ch);
#endif		
		//_own_objects[obj->get_id()] = obj;
	}
	else
	{
		if (ref == NULL)
		{
			sprintf (ch, "[%lu] arb_logic: obj_created: Has no reference data.\n",
				_arbitrator->self->id);
			e.output(ch);
		}
		else
		{
			ab.buildFood(*obj, 3);
			//_reg->newfood (obj->get_id (), obj->get_pos(), 3);
            
		}
	}
}

//void simgame_arbitrator_logic::obj_created (object *obj)
void simgame_arbitrator_logic::obj_discovered (object *obj)
{
//		statistics * sta = statistics::getInstance ();

	/*
	if (obj->peer != 0)
	{
		AttributeBuilder ab;
		char ch[200];
		errout e;

		// TODO: read datas from server
		sprintf (ch, "Peer%d", obj->get_id());
		bool buildresult = ab.buildPlayer (*obj, ch, _para->HP_MAX, _para->HP_MAX);
//			sta->objectChanged (1, player_object->get_id ());

		sprintf (ch, "Peer:%d  ObjectID:%d  build:%d  joined." NEWLINE, obj->get_id()
			, obj->get_id(), buildresult);
		e.output(ch);
	}
	*/

    if (obj == NULL)
        return;
	
    AttributeBuilder ab;
#ifdef VASTATESIM_DEBUG
	errout eo;
    char ostr[256];
	sprintf (ostr, "[%d] arb_logic: obj_created: id: %s" NEWLINE,
        _arbitrator->self->id, ab.objectIDToString(*obj).c_str ());
	eo.output (ostr);
#endif

	_own_objects[obj->get_id()] = obj;

    // update god's object store
    if (_arbitrator != NULL && _arbitrator->self != NULL &&
        _arbitrator->is_obj_owner (obj->get_id ()))
        _reg->update_object (_arbitrator->self->id, obj);
}

void simgame_arbitrator_logic::obj_deleted (object *obj)
{
#ifdef VASTATESIM_DEBUG
	errout eo;
    AttributeBuilder ab;
	char ostr[256];
	sprintf (ostr, "[%d] arb_logic: obj_deleted: id: %s" NEWLINE,
        _arbitrator->self->id, ab.objectIDToString (*obj).c_str ());
	eo.output (ostr);
#endif
	
	_own_objects.erase (obj->get_id ());

    // update god's object store
    if (_arbitrator != NULL && _arbitrator->self != NULL &&
        _arbitrator->is_obj_owner (obj->get_id ()))
        _reg->delete_object (_arbitrator->self->id, obj);
}

// callback - by remote arbitrator to notify their object states have changed
void simgame_arbitrator_logic::state_updated (VAST::id_t obj_id, int index, void *value, int length, version_t version)
{
#ifdef VASTATESIM_DEBUG
	errout eo;
    AttributeBuilder ab;
	char ostr[256];
	sprintf (ostr, "[%d] arb_logic: state_updated: id: %s index: %d value: %d version: %d" NEWLINE,
		_arbitrator->self->id, ab.objectIDToString(obj_id).c_str (), index, *(int *)(value), version);
	eo.output(ostr);
#endif

    // received a unknown object's update, ignore it (should do this?)
    if (_own_objects.find (obj_id) == _own_objects.end ())
        return ;

    // update god's object store
    if (_arbitrator != NULL && _arbitrator->self != NULL &&
        _arbitrator->is_obj_owner (obj_id))
        _reg->update_object (_arbitrator->self->id, _own_objects[obj_id]);
}

void simgame_arbitrator_logic::pos_changed (VAST::id_t obj_id, Position &newpos, timestamp_t version)
{
#ifdef VASTATESIM_DEBUG
    AttributeBuilder ab;
    errout eo;
    char ostr[256];
    sprintf (ostr, "[%d] arb_logic: pos_changed: id: %s new pos: (%d,%d) version: %d" NEWLINE,
        _arbitrator->self->id, ab.objectIDToString(obj_id).c_str (), (int)newpos.x, (int)newpos.y, version);
	eo.output(ostr);
#endif

    // received a unknown object's update, ignore it (should do this?)
    if (_own_objects.find (obj_id) == _own_objects.end ())
        return ;

    // update god's object store
    if (_arbitrator != NULL && _arbitrator->self != NULL &&
        _arbitrator->is_obj_owner (obj_id))
        _reg->update_object (_arbitrator->self->id, _own_objects[obj_id]);
}

void simgame_arbitrator_logic::tick ()	
{
	int this_step = _para->current_timestamp;

	vector<int> & food_start_index = _model->GetFoodStartIndex ();
	vector<food_creation_reg> & food_store = _model->GetFoodStore ();

    if (!_model->IsPlayMode ())
    {
        // flag for refreshing food image file
        bool need_refresh = false;
	    // if (_reg->getFoodcount() < _para->FOOD_MAX_QUANTITY)

        // process for food creation
        if (_reg->getFoodCount () < _para->FOOD_MAX_QUANTITY)
	    {
		    int r = rand() % _para->FOOD_MAX_QUANTITY;
		    if (r >= _reg->getFoodCount ())
		    {
                Position pos = Position (rand () % _para->WORLD_WIDTH, rand () % _para->WORLD_HEIGHT);
                _reg->inc_food ();

                // food_start_index [_para->TIME_STEPS];
                int this_index = food_start_index [_para->TIME_STEPS];
                food_store.push_back (food_creation_reg ());
                food_store[this_index].count = 3;
                food_store[this_index].pos = pos;
                food_start_index [_para->TIME_STEPS] ++;

                need_refresh = true;
		    }
	    }

        if (this_step <= _para->TIME_STEPS - 3)
            food_start_index [this_step + 2] = food_start_index [_para->TIME_STEPS];

        if (need_refresh)
            _model->RefreshFoodImageFile ();
    }

    if (this_step < 0)
		return;

	map<VAST::id_t, Node> & arbs = _arbitrator->get_arbitrators ();
	for (int i = food_start_index[this_step]; i < food_start_index[this_step+1]; i++)
	{
		double food_dist = food_store[i].pos.dist (_arbitrator->self->pos);
		bool nearest = true;

		map<VAST::id_t, Node>::iterator it = arbs.begin ();
		for (; it != arbs.end (); it ++)
		{
			if (food_dist > it->second.pos.dist (food_store[i].pos))
			{
				nearest = false;
				break;
			}
		}

		if (nearest == true)
		{
			int type = 1;
			//object * newfood = _arbitrator->create_obj (food_store[i].pos, 0, (void *)&type);
			_arbitrator->create_obj (food_store[i].pos, 0, (void *)&type);
		}
	}

	// TODO:  maybe create monster later?

    // check if the arbitartor needs help
    _detection_countdown --;
    if (_detection_countdown <= 0)
    {
        // reset detection coundown
        // TODO: change to normal value
        _detection_countdown = 5;

        // fix value for _bandwidth_usage in normal (_bandwidth_usage > 0 => -0.1
        //                                                            < 0 =>  0.1
        //                                                            = 0 =>  0
        double fix_up = 0.0;
        if (_bandwidth_usage > 1)
            fix_up = -1.0;
        else if (_bandwidth_usage < -1)
            fix_up = 1.0;
        else 
            fix_up = -1.0 * _bandwidth_usage;

        if (_reg->node_transmitted[_arbitrator->self->id] >= THRESHOLD_BANDWIDTH * (_promoted ? RATIO_CAPABLE : 1.0))
            fix_up = 1.0;
        else if (_reg->node_transmitted[_arbitrator->self->id] <= THRESHOLD_LOW_BANDWIDTH * (_promoted ? RATIO_CAPABLE : 1.0))
            fix_up = -1.0;

        _bandwidth_usage += fix_up;

#ifndef RUNNING_MODE_NO_AGGREGATOR
#ifndef RUNNING_MODE_CLIENT_SERVER
        // TODO: change to normal value
        if (_bandwidth_usage >= 1.5)
        {
            _arbitrator->overload ((int) (_bandwidth_usage + 0.5));
            _bandwidth_usage = 0;

#ifdef VASTATESIM_DEBUG
            sprintf (_ostr, "[%d] call for overload help\n", _arbitrator->self->id);
            _eo.output (_ostr);
#endif
        }
        else if (_bandwidth_usage <= -1.5)
        {
            _arbitrator->underload ((int) (_bandwidth_usage - 0.5));
            _bandwidth_usage = 0;

#ifdef VASTATESIM_DEBUG
            sprintf (_ostr, "[%d] call for underload help\n", _arbitrator->self->id);
            _eo.output (_ostr);
#endif
        }
#endif /* RUNNING_MODE_CLIENT_SERVER */
#endif /* RUNNING_MODE_NO_AGGREGATOR */
    }
}


VAST::id_t simgame_arbitrator_logic::_my_id () const
{
    if (_arbitrator == NULL || _arbitrator->self == NULL)
        return NET_ID_UNASSIGNED;

    return _arbitrator->self->id;
}

object *
simgame_arbitrator_logic::get_object (obj_id_t object_id)
{
    if (_own_objects.find (object_id) == _own_objects.end ())
    {
#ifdef VASTATESIM_DEBUG
        sprintf (_ostr, "[%d] arbitrator_logic: received event of unknown sedner object %s\n",
            _my_id (), atb.objectIDToString (object_id).c_str ());
        _eo.output (_ostr);
#endif
        return NULL;
    }
    else
        return _own_objects[object_id];
}


bool simgame_arbitrator_logic::getArbitratorInfo (arbitrator_info * arb_info)
{
    if ((arb_info != NULL) && (_arbitrator->self != NULL))
	{
		arb_info->id      =  _arbitrator->self->id;
		arb_info->pos     =  _arbitrator->self->pos;
        arb_info->aoi     =  _arbitrator->self->aoi;
        arb_info->joined  =  _arbitrator->is_joined ();
        arb_info->is_aggr = _arbitrator->is_aggregator ();
        arb_info->aoi_b   = (_arbitrator->is_aggregator ()) ? (_arbitrator->get_aggnode ()->aoi_b) : (0);
        strcpy (arb_info->status, "");
        if (_bandwidth_usage >= 1)
            strcpy (arb_info->status, "+");
        else if (_bandwidth_usage <= -1)
            strcpy (arb_info->status, "-");

        return true;
	}
    arb_info->id = NET_ID_UNASSIGNED;
    arb_info->pos = Position (0, 0);
    arb_info->aoi = arb_info->aoi_b = 0;
    arb_info->joined = arb_info->is_aggr = false;

	return false;
}

string simgame_arbitrator_logic::PlayerObjtoString (object * obj)
{
	AttributeBuilder ab;
	std::ostringstream sr;
	sr  << "  [Peer" << obj->peer << "]"
		<< "[" << obj->get_id() << "]"
		<< "(" << obj->get_pos().x << "," << obj->get_pos().y << ")"
		<< "<v" << obj->pos_version << ">"
		<< "  "
		<< ab.getPlayerHP(*obj) << "/" << ab.getPlayerMaxHP(*obj)
		<< "<v" << obj->version << ">";
	sr  << endl;
	return sr.str();
}

const char * simgame_arbitrator_logic::toString ()
{
	if (_info == NULL)
		_info = new string ();

	AttributeBuilder ab;
	std::ostringstream sr;
	Node * n = _arbitrator->self;
    map<obj_id_t,bool> & obj_own = _arbitrator->get_owned_objs ();

    if (_arbitrator->is_aggregator ())
        sr << "[Aggregator]" << endl;
    sr  << "Joined: " << _arbitrator->is_joined () << endl;
	sr  << "ID: " << n->id << endl;
	sr  << "AOI: " << n->aoi;
    // output second aoi if is aggregator
    if (_arbitrator->is_aggregator ())
    {
        AggNode * ag = _arbitrator->get_aggnode ();
        if (ag != NULL)
            sr << "    AOI_b: " << ag->aoi_b;
    }
    sr << endl;
	sr << "Pos: (" << n->pos.x << "," << n->pos.y << ")" << endl;
	sr  << "Known objs: " << endl;

    // list peers first
    for (map<VAST::id_t, object *>::iterator it2 = _own_objects.begin(); it2 != _own_objects.end(); it2 ++)
    {
        if (it2->second->peer == 0)
            continue;

        sr  << "  " << ab.objectToString(*(it2->second)) ;
        if (obj_own.find (it2->second->get_id ()) != obj_own.end ())
            sr << " [owner]";
        sr  << endl;
    }

    // list normal object after
    for (map<VAST::id_t, object *>::iterator it2 = _own_objects.begin(); it2 != _own_objects.end(); it2 ++)
    {
        if (it2->second->peer != 0)
            continue;

        sr << "  " << ab.objectToString(*(it2->second));
        if (obj_own.find (it2->second->get_id ()) != obj_own.end ())
            sr << " [owner]";
        sr << endl;
    }

    //sr << "arbitrators: " << endl;
    sr << _arbitrator->to_string ();

	*_info = sr.str();
	return _info->c_str();
}

int simgame_arbitrator_logic::getAOI ()
{
	return _arbitrator->self->aoi;
}

network * simgame_arbitrator_logic::get_network ()
{
    return _arbitrator->get_vnode ()->getnet ();
}

// store access to arbitrator class
void simgame_arbitrator_logic::register_interface (void *p_arb)
{
	_arbitrator = (arbitrator *) p_arb;
}

// store access to storage class
void simgame_arbitrator_logic::register_storage (void * p_sto)
{
    _storage = (storage *) p_sto;
}

bool 
simgame_arbitrator_logic::get_info (int info_type, char* buffer, size_t & buffer_size)
{
    return _arbitrator->get_info (info_type, buffer, buffer_size);
}

// Recycle area /////////////////////////////////////////////////////////// 

/*
// callback -  to learn about creation of avatar objects
void simgame_arbitrator_logic::peer_entered (object *player_object)
{
//		statistics * sta = statistics::getInstance ();
	AttributeBuilder ab;
	char ch[200];
	errout e;

	// TODO: read datas from server
	sprintf (ch, "Peer%d", player_object->get_id());
//		bool buildresult = true;
	bool buildresult = ab.buildPlayer (*player_object, ch, _para->HP_MAX, _para->HP_MAX);
//		sta->objectChanged (1, player_object->get_id ());

	sprintf (ch, "[Peer%d][%d] joined." NEWLINE, 
		player_object->peer, player_object->get_id());
	e.output(ch);
	
	_own_objects[player_object->get_id()] = player_object;
}

void simgame_arbitrator_logic::peer_left (object * player_object)
{
//		statistics * sta = statistics::getInstance ();

//		sta->objectChanged (2, player_object->get_id ());
	// TODO: do some saving action (ex: send final data to server)
}
*/

// tick ()
    /*
	int r;

    // TODO: check if food is created within my region
	// create food
	if (_reg->getFoodcount() < _para->FOOD_MAX_QUALITY)
	{
		r = rand() % _para->FOOD_MAX_QUALITY;
		if (r >= _reg->getFoodcount ())
		{
			//AttributeBuilder ab;
			int x = rand() % _para->WORLD_WIDTH;
			int y = rand() % _para->WORLD_HEIGHT;
			Position p(x, y);

            // syhu: add variable 'type'
			int type = 1;
            object * newfood = _arbitrator->create_obj (p, 0, (void *)&type);
			// statistics * sta = statistics::getInstance ();
			// sta->objectChanged (1, newfood->get_id ());
		}
	}
	*/

