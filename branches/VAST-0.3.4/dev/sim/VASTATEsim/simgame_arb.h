
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
 *	SimGame_Arb.h  (Vastate Simulator Game _ Arbitrator class header)
 *	
 *
 */

#ifndef _VASTATE_SIMGAME_ARBITRATOR_H
#define _VASTATE_SIMGAME_ARBITRATOR_H

#include "vastatesim.h"
using namespace VAST;
#include "arbitrator_reg.h"
#include "behavior.h"

#define JOIN_REQUEST_RANDOM_CHAR_LENGTH 4

class simgame_node;

class simgame_arbitrator_logic : public arbitrator_logic, public storage_logic
{
public:
	simgame_arbitrator_logic (SimPara * para, arbitrator_reg * ar_logic, behavior * model, bool promoted = false)
        : _promoted (promoted)
        , _storage (NULL), _arbitrator (NULL), _reg (ar_logic)
        , _para (para), _info (NULL), _model (model)
        , _detection_countdown (STABLE_COLDDOWN_STEPS), _bandwidth_usage (0)
	{
	}

	~simgame_arbitrator_logic ()
	{
		if (_info != NULL)
			delete _info;

        if (_join_requesting.size () > 0)
        {
            map <query_id_t, pair<VAST::id_t, char *> >::iterator it = _join_requesting.begin ();
            for (; it != _join_requesting.end (); it ++)
                delete[] (char *) (it->second.second);
            _join_requesting.clear ();
        }
	}

    // callback - for authenaticing a newly joined peer
    bool join_requested (VAST::id_t from_id, char *data, int sizes);

    // callback - for processing a query returned by the shared storage
    bool query_returned (query_id_t query_id, char *data, int size);

    // callback - for determining affecting objects by event
    //      note: a static vector may be declared in event_affected for returning value
    const std::vector<VAST::obj_id_t> &
         event_affected (VAST::id_t from_id, event &e);

	// callback -  for receiving a remote event (arbitrator only)
	bool event_received (VAST::id_t from_id, event &e);
        
	// callback - by remote arbitrator to create objects
	void obj_created    (object *obj, void *ref, size_t size);
	void obj_discovered (object *obj);
	void obj_deleted    (object *obj);

	// callback - by remote arbitrator to notify their object states have changed
	void state_updated (VAST::id_t obj_id, int index, void *value, int length, version_t version);
	void pos_changed (VAST::id_t obj_id, Position &newpos, timestamp_t version);

	// callback -  by every processmsg called
	void tick ();

	bool getArbitratorInfo (arbitrator_info * arb_info);
	string PlayerObjtoString (object * obj);
	const char * toString ();
	int getAOI ();
    network * get_network ();
    bool get_info (int info_type, char* buffer, size_t & buffer_size);

	// store access to arbitrator class
    void register_interface (void *);

    // callback - process a particular query for certain data
    bool query_received (int query_id, char *query, size_t size);

    // callback - process the response to a particular query
    bool reply_received (int query_id, char *reply, size_t size);

    // store access to storage class
    void register_storage (void *);

protected:
    // get my_id
    VAST::id_t     _my_id () const;

    // get object from object_store
    object * get_object (obj_id_t object_id);

private:
    bool                    _promoted;
    storage               * _storage;
	arbitrator  	      * _arbitrator;
	arbitrator_reg        * _reg;

	map<VAST::id_t, object *>     _own_objects;
	SimPara               * _para;
	string                * _info;
	behavior              * _model;

    int                     _detection_countdown;
    double                  _bandwidth_usage;

    //int                     _last_loadlevel;

    map<query_id_t, pair<VAST::id_t, char *> > _join_requesting;

    static char   _ostr [MAXBUFFER_SIZE];
    static errout _eo;
    static AttributeBuilder atb;

	friend class simgame_node;
};

#endif /* _VASTATE_SIMGAME_ARBITRATOR_H */

