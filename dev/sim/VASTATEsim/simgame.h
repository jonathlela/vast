
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
 *  Simulator for VSM Implementation (vastatesim)
 *  simgame.h - header of simgame_node: a "node" in simulation
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

	bool join ();
    
    //
	//  Player Action methods
	//
    bool enable (bool state);
    bool getEnable ();

	int NextStep ();
	void processmsg ();
	void Move (Position& dest);
	void Eat (VAST::id_t foodid);
	void Attack (VAST::id_t target);
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
	inline map<VAST::id_t,object*>& GetOwnObjects () {
		return (_arbitrator_lg != NULL)?(_arbitrator_lg->_own_objects):empty_io_map;
	}
    */
	inline map<VAST::id_t,object*>& GetOwnObjects (int index) {
		if (index < (int)_ar_logic.size())
			return ((simgame_arbitrator_logic *)_ar_logic[index])->_own_objects;
		else
			return empty_io_map;
    }

    // return the status of if the peer joined
    bool is_joined ()
    {
        if (_peer == NULL)
            return false;

        return _peer->is_joined ();
    }

    bool is_gateway ();
	bool isArbitrator ();
	int getArbitratorInfo (arbitrator_info * arb_info);
	const char * getArbitratorString (int index);
	bool getPlayerInfo (player_info * p);
	int getAOI ();
	int getArbAOI (int index);
    bool get_info (int index, int info_type, char* buffer, size_t & buffer_size);
    timestamp_t get_curr_timestamp ();

    // return accmulated transmission size for arbitrators hosted by the node
    pair<unsigned int,unsigned int> getArbitratorAccmulatedTransmit (int index);

    // return accmulated transmission size for peer on the node
	pair<unsigned int,unsigned int> getAccmulatedTransmit ();

    // return accmulated transmission by message type for arbitrators hosted by the node
    pair<unsigned int,unsigned int> getArbitratorAccmulatedTransmit_bytype (int index, msgtype_t msgtype);

    // return accmulated transmission by message type for peer on the node
	pair<unsigned int,unsigned int> getAccmulatedTransmit_bytype (msgtype_t msgtype);

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

    bool                  _joined;
    vastverse           * _world;
	behavior            * _model;
	SimPara             * _para;

    static char             _ostr [MAXBUFFER_SIZE];
    static errout           _eo;
    static AttributeBuilder _ab;

	// private vars for returning value
	static vector<object *> empty_o_vector;
	static map<VAST::id_t,object*> empty_io_map;
};


#endif /* _VASTATE_SIMGAME_H */

