
/*
 *	SimGame_Peer.h (Vastate Simulator Game _ Peer class header)
 *
 *
 */

#ifndef _VASTATE_SIMGAME_PEER_H
#define _VASTATE_SIMGAME_PEER_H

#include "vastatesim.h"
#include "behavior.h"
using namespace VAST;

class simgame_node;

class simgame_peer_logic : public peer_logic
{
public:
	simgame_peer_logic ()
		: _peer (NULL), _player (NULL)
	{
	}

    void msg_received (char *msg, size_t size);

	void obj_discovered (object *obj, bool is_self = false);
	void obj_deleted (object *obj);
        
	// callback - learn about state changes of known AOI objects
	void state_updated (id_t obj_id, int index, void *value, int length, version_t version);
	void pos_changed (id_t obj_id, Position &newpos, timestamp_t version);  
        
	// store access to peer class
	void register_interface (void *);

	object * get_self ();
	id_t getManagingArbiratorID ();
	bool getPlayerInfo (player_info * p);
	int getAOI ();
    network * get_network ();

private:
	peer               * _peer;

	object             * _player;
	vector<object*>      _objects;
	//b_register           _b_reg;
	player_behavior_reg  _behavior_r;

    static char             _ostr[MAXBUFFER_SIZE];
    static errout           _eo;
    static AttributeBuilder _ab;

	friend class simgame_node;
};


#endif /* _VASTATE_SIMGAME_PEER_H */