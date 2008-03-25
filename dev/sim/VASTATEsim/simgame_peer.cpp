
/*
 *	SimGame_Peer.cpp (Vastate Simulator Game _ Peer class implementation)
 *
 *
 */

#include "simgame_peer.h"
#include "vastutil.h"

char             simgame_peer_logic::_ostr[MAXBUFFER_SIZE];
errout           simgame_peer_logic::_eo;
AttributeBuilder simgame_peer_logic::_ab;

void simgame_peer_logic::msg_received (char *msg, size_t size)
{

}

void simgame_peer_logic::obj_discovered (object *obj, bool is_self)
{
	/*
	AttributeBuilder ab;
	switch (obj->type)
	{
	case SimGame::OT_PLAYER:
		ab.buildPlayer (*obj, "", 0, 0);
		break;
	case SimGame::OT_FOOD:
		ab.buildFood (*obj, 4);
		break;
	}*/
	
	if (is_self)
		_player = obj;

	_objects.push_back (obj);

//		statistics * sta = statistics::getInstance ();
//		sta->receivedReplicaChanged (1, obj->get_id(), 0, obj->version);

#ifdef VASTATESIM_DEBUG
	sprintf (_ostr, "[%d] peer_logic: obj_discovered: id: %s  %s" NEWLINE,
        (get_self() == NULL)?-1:get_self()->peer, _ab.objectIDToString (*obj).c_str (), (is_self?"[Self]":""));
	_eo.output (_ostr);
#endif
}

void simgame_peer_logic::obj_deleted (object *obj)
{
	vector<object*>::iterator it = _objects.begin();
	for (; it != _objects.end (); it++)
	{
		if (*it == obj)
		{
			_objects.erase (it);
			break;
		}
	}

//		statistics * sta = statistics::getInstance ();
//		sta->receivedReplicaChanged (2, obj->get_id(), 0, obj->version);
#ifdef VASTATESIM_DEBUG
	sprintf (_ostr, "[%d] peer_logic: obj_deleted: id: %s" NEWLINE,
        this->get_self()->peer, _ab.objectIDToString (obj->get_id()).c_str ());
	_eo.output (_ostr);
#endif
}
    
// callback - learn about state changes of known AOI objects
void simgame_peer_logic::state_updated (id_t obj_id, int index, void *value, int length, version_t version)
{
	// in client: redraw screen.
	
//		statistics * sta = statistics::getInstance ();
//		sta->receivedUpdate (obj_id, 0, version);

#ifdef VASTATESIM_DEBUG
	sprintf (_ostr, "[%d] peer_logic: state_updated: id: %s index: %d value: %d version: %d" NEWLINE,
		(get_self()==NULL)?-1:get_self()->peer, _ab.objectIDToString (obj_id).c_str (), index, *(int *)(value), version);
	_eo.output(_ostr);
#endif
}

void simgame_peer_logic::pos_changed (id_t obj_id, Position &newpos, timestamp_t version)
{
#ifdef VASTATESIM_DEBUG
	sprintf (_ostr, "[%d] peer_logic: pos_changed: [%d] new pos: (%d,%d) version: %d" NEWLINE,
        (get_self()==NULL)?-1:get_self()->peer, _ab.objectIDToString ((obj_id_t) obj_id).c_str (), (int)newpos.x, (int)newpos.y, version);
	_eo.output(_ostr);
#endif
}
    
// store access to peer class
void simgame_peer_logic::register_interface (void *p_peer)
{
	_peer = (peer *) p_peer;
}

object * simgame_peer_logic::get_self ()
{
	return _player;
}

id_t simgame_peer_logic::getManagingArbiratorID ()
{
	return 0;
}

bool simgame_peer_logic::getPlayerInfo (player_info * p)
{
	if (p != NULL)
	{
		p->dest = _behavior_r.dest;
		strncpy (p->last_action, _behavior_r.last_action, 2);
		p->angst = _behavior_r.angst;
		p->tiredness = _behavior_r.tiredness;
		p->waiting = _behavior_r.waiting;

		p->foattr = Position (-1,-1);
		if (_behavior_r.following_attractor != NULL)
		{
			behavior::attractor_reg * attr = (behavior::attractor_reg *) _behavior_r.following_attractor;
			p->foattr = (attr->show == 1)?attr->pos:Position(-1,-1);
		}
		else if (_behavior_r.target != NULL)
		{
			object * obj = (object *) _behavior_r.target;
			p->foattr = obj->get_pos();
		}
		return true;
	}
	return false;
}

int simgame_peer_logic::getAOI ()
{
	return _peer->get_aoi();
}
/*
network * simgame_peer_logic::get_network ()
{
    return _peer->get_network ();
}

*/