

/*
 *	SimGame.h  (Vastate Simulator Game _ node class header)
 *
 */

#ifndef _VASTATE_SIMGAME_H
#define _VASTATE_SIMGAME_H

#ifdef WIN32
// disable warning about "identifier exceeds'255' characters"
#pragma warning(disable: 4786)
#endif

#include <map>
#include <vector>

#include "vastory.h"
#include "behavior.h"
#include "arbitrator_reg.h"
#include "simgame_arb.h"
#include "simgame_peer.h"

class simgame_node
{
public:
    simgame_node (
        vastate * vasts_public,
		int capacity, behavior *model, arbitrator_reg * ar_logic, vastverse * world, SimPara * para, bool is_gateway = false
        );
	~simgame_node ();

	bool simgame_node::join ();
    
    //
	//  Player Action methods
	//
    bool enable (bool state);
    bool getEnable ();

	int NextStep ();
	void processmsg ();
	void Move (Position& dest);
	void Eat (id_t foodid);
	void Attack (id_t target);
	void Bomb (Position& center, int radius);

	//
	//	Data member access
	//
	inline object * GetSelfObject () { 
		return (_peer_lg != NULL)?(_peer_lg->get_self ()):(NULL) ; 
	}
	inline vector<object *>& GetObjects () { 
		return (_peer_lg != NULL)?(_peer_lg->_objects):empty_o_vector;
	}
    /*
	inline map<id_t,object*>& GetOwnObjects () {
		return (_arbitrator_lg != NULL)?(_arbitrator_lg->_own_objects):empty_io_map;
	}
    */
	inline map<id_t,object*>& GetOwnObjects (int index) {
		if (index < (int)_ar_logic.size())
			return ((simgame_arbitrator_logic *)_ar_logic[index])->_own_objects;
		else
			return empty_io_map;
	}
    bool is_gateway ();
	bool isArbitrator ();
	int getArbitratorInfo (arbitrator_info * arb_info);
	const char * getArbitratorString (int index);
	bool getPlayerInfo (player_info * p);
	int getAOI ();
	int getArbAOI (int index);
    pair<unsigned int,unsigned int> getArbitratorAccmulatedTransmit (int index);
	pair<unsigned int,unsigned int> getAccmulatedTransmit ();
	

	const char * toString ();
private:
	string PlayerObjtoString (object * obj);

	//
	//	Data Members
	//
private:
    bool                  _active;
    bool                  _b_gateway;

	vastory               _fac;
	vastate             * _vasts;
    bool                  _b_single_vastate;
	peer                * _peer;
	simgame_peer_logic  * _peer_lg;
    int                   _peer_capacity;

    // store all arbitrator logic pointer
	vector<simgame_arbitrator_logic *> _ar_logic;

    arbitrator_reg      * _arbitrator_reg;
	string              * _info;

    bool is_joined;
    vastverse           * _world;
	behavior            * _model;
	SimPara             * _para;

    static char             _ostr [MAXBUFFER_SIZE];
    static errout           _eo;
    static AttributeBuilder _ab;

	// private vars for returning value
	static vector<object *> empty_o_vector;
	static map<id_t,object*> empty_io_map;
};


#endif /* _VASTATE_SIMGAME_H */

