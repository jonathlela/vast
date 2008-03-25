
/*
 *	SimGame_Arb.cpp  (Vastate Simulator Game _ Arbitrator class implementation)
 *
 *	
 *
 */

#include "simgame_arb.h"

char   simgame_arbitrator_logic::_ostr [MAXBUFFER_SIZE];
errout simgame_arbitrator_logic::_eo;

// callback - for authenaticing a newly joined peer
bool simgame_arbitrator_logic::join_requested (id_t from_id, char *data, int sizes)
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
        _join_requesting [_storage->query (data, sizes)] = pair<id_t, char *> (from_id, auth);
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

// callback -  for receiving a remote event (arbitrator only)
bool simgame_arbitrator_logic::event_received (id_t from_id, event &e)
{
    AttributeBuilder ab;
#ifdef VASTATESIM_DEBUG
    errout eo;
	char str[256];
#endif

    object * sender_object = NULL;

	int sender_object_id;
	e.get (0, sender_object_id);

    if (_own_objects.find (sender_object_id) == _own_objects.end ())
    {
#ifdef VASTATESIM_DEBUG
        sprintf (str, "[%d] arbitrator_logic: received event of unknown sedner object %s\n",
            (_arbitrator->self==NULL)?(0):(_arbitrator->self->id), ab.objectIDToString (sender_object_id).c_str ());
        eo.output (str);
#endif
        return false;
    }
    else
    {
        sender_object = _own_objects[sender_object_id];
    }

	Position &sender_pos = sender_object->get_pos ();

	Position dest;
	int x, y, r;
	double dx, dy;
	double ratio;
	int target_id;
	object * target_object;

	switch (e.type)
	{
	case SimGame::E_MOVE:
		e.get (1, x);
		e.get (2, y);
		dest.x = x;
		dest.y = y;

		dx = dest.x - sender_pos.x;
        dy = dest.y - sender_pos.y;
        
        // adjust deltas for constant velocity
        ratio = sqrt((dx*dx) + (dy*dy)) / (double)(_para->VELOCITY);

        if (ratio > 1)
        {
            // note that this may cause actual velocity to be less than desired
            // as any decimal places are dropped
            dx = (long)((double)dx / ratio);
            dy = (long)((double)dy / ratio);
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
		e.get (1, (int)target_id);

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
		e.get (1, (int)x);
		e.get (2, (int)y);
		e.get (3, (int)r);

		dest.x = x;
		dest.y = y;

#ifdef VASTATESIM_DEBUG
		sprintf (str, "[%d] arb_logic: bomb from [Peer%d] to (%d,%d) r %d" NEWLINE,
			_arbitrator->self->id, e.get_sender(), x,y,r);
		eo.output(str);
#endif
		
		{
			map<id_t, object*>::iterator it;
			for (it = _own_objects.begin(); it != _own_objects.end(); it++)
			{
				target_object = (*it).second;
				if (dest.dist (target_object->get_pos()) <= r)
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
		e.get (1, (int)target_id);
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
                _reg->dec_food ();
				_arbitrator->delete_obj (target_object);
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
		sprintf (ch, "Peer%d", obj->peer);
		//		bool buildresult = true;
		bool buildresult = ab.buildPlayer (*obj, ch, rand () % _para->HP_MAX, _para->HP_MAX);
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
			sprintf (ch, "[%d] arb_logic: obj_created: Has no reference data." NEWLINE,
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
    if (_arbitrator != NULL && _arbitrator->self != NULL)
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
    if (_arbitrator != NULL && _arbitrator->self != NULL)
        _reg->delete_object (_arbitrator->self->id, obj);
}

// callback - by remote arbitrator to notify their object states have changed
void simgame_arbitrator_logic::state_updated (id_t obj_id, int index, void *value, int length, version_t version)
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
    if (_arbitrator != NULL && _arbitrator->self != NULL)
        _reg->update_object (_arbitrator->self->id, _own_objects[obj_id]);
}

void simgame_arbitrator_logic::pos_changed (id_t obj_id, Position &newpos, timestamp_t version)
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
    if (_arbitrator != NULL && _arbitrator->self != NULL)
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

	map<id_t, Node> & arbs = _arbitrator->get_arbitrators ();
	for (int i = food_start_index[this_step]; i < food_start_index[this_step+1]; i++)
	{
		double food_dist = food_store[i].pos.dist (_arbitrator->self->pos);
		bool nearest = true;

		map<id_t, Node>::iterator it = arbs.begin ();
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
			object * newfood = _arbitrator->create_obj (food_store[i].pos, 0, (void *)&type);
		}
	}

	// TODO:  maybe create monster later?

    // check if the arbitartor needs help
    _detection_countdown --;
    if (_detection_countdown <= 0)
    {
        // reset detection coundown
        _detection_countdown = 10;

        // fix value for _bandwidth_usage in normal (_bandwidth_usage > 0 => -0.25
        //                                                            < 0 =>  0.25
        //                                                            = 0 =>  0
        double fix_up = (_bandwidth_usage == 0 ? 0 : ((_bandwidth_usage>0?(-1):(1)) * 0.25));
        
        if (_reg->node_transmitted[_arbitrator->self->id] > THRESHOLD_BANDWIDTH)
            fix_up = 1;
        else if (_reg->node_transmitted[_arbitrator->self->id] < THRESHOLD_LOW_BANDWIDTH)
            fix_up = -1;

        _bandwidth_usage += fix_up;

        if (_bandwidth_usage >= 3)
        {
            _arbitrator->overload ((int) _bandwidth_usage);
            //fix_up = -3;
            _bandwidth_usage = 1;

#ifdef VASTATESIM_DEBUG
            sprintf (_ostr, "[%d] call for overload help\r\n", _arbitrator->self->id);
            _eo.output (_ostr);
#endif
        }
        else if (_bandwidth_usage <= -3)
        {
            _arbitrator->underload ((int) _bandwidth_usage);
            //fix_up = 3;
            _bandwidth_usage = -1;

#ifdef VASTATESIM_DEBUG
            sprintf (_ostr, "[%d] call for underload help\r\n", _arbitrator->self->id);
            _eo.output (_ostr);
#endif
        }
    }
}


bool simgame_arbitrator_logic::getArbitratorInfo (arbitrator_info * arb_info)
{
    if ((arb_info != NULL) && (_arbitrator->self != NULL))
	{
		arb_info->id = _arbitrator->self->id;
		arb_info->pos = _arbitrator->self->pos;
		return true;
	}
    arb_info->id = NET_ID_UNASSIGNED;
    arb_info->pos = Position (0, 0);
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

	sr  << "ID: " << n->id << endl;
	sr  << "AOI: " << n->aoi << endl;
	sr << "Pos: (" << n->pos.x << "," << n->pos.y << ")" << endl;
	sr  << "Known objs: " << endl;

    // list peers first
    for (map<id_t, object *>::iterator it2 = _own_objects.begin(); it2 != _own_objects.end(); it2 ++)
    {
        if (it2->second->peer == 0)
            continue;

        sr  << "  " << ab.objectToString(*(it2->second)) ;
        if (obj_own.find (it2->second->get_id ()) != obj_own.end ())
            sr << " [owner]";
        sr  << endl;
    }

    // list normal object after
    for (map<id_t, object *>::iterator it2 = _own_objects.begin(); it2 != _own_objects.end(); it2 ++)
    {
        if (it2->second->peer != 0)
            continue;

        sr << "  " << ab.objectToString(*(it2->second));
        if (obj_own.find (it2->second->get_id ()) != obj_own.end ())
            sr << " [owner]";
        sr << endl;
    }

    sr << "Arbitrator info: " << endl;
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