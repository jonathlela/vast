
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
 *  statistics.h - statistic class - class for statisticsing system operation
 *
  */

#ifndef _VASTATESIM_STATISTICS_H
#define _VASTATESIM_STATISTICS_H

#include "vastatesim.h"
#include "simgame.h"
#include <stdio.h>

// related to size of  statistics.cpp: CheckPoints[]
#define TOTAL_CHECKPOINT 6

#define STEPS_PER_SECOND    (10)
#define SNAPSHOT_INTERVAL   (100)
#define DENSITY_GRID_WIDTH  (50)

// interval between each consumption catelogies
#define BANDWIDTH_INTERVAL  (5000)

// Constants for Statistics for Bandwidth 
//    (note: last one byte is reserved for message)
//SBW_ARB, SBW_PEER, SBW_NODE, SBW_SERVER
#define SBW_ARB    (0x00100000)
#define SBW_PEER   (0x00200000)
#define SBW_NODE   (0x00400000)
#define SBW_SERVER (0x00800000)
#define SBW_AGG    (0x01000000)
//BW_SEND, BW_RECV
#define SBW_SEND   (0x00010000)
#define SBW_RECV   (0x00020000)
// AREA DENSITY
#define SDEN       (0x01000000)
#define SBW_MSG    (0x02000000)
//BW_SUM, BW_MIN, BW_MAX, BW_AVG
#define SBW_COUNT  (0x00000100)
#define SBW_SUM    (0x00001000)
#define SBW_AVG    (0x00002000)
#define SBW_MIN    (0x00004000)
#define SBW_MAX    (0x00008000)


struct stat_replica_counting
{
	int cr;	// consistent replica
	int tr;	// total replica
};

struct stat_event_record
{
	int eventid;
	int objectid;
	timestamp_t timestamp;
	timestamp_t update_version;

	int consistent_replica;
	int total_replica;

	int total_delay;
	int max_delay;

	int time_consistent [TOTAL_CHECKPOINT];
};

struct stat_arb_record
{
	int arb_id;
	int start_time;
	int end_time;
};

class stat_object_record;
/*
struct stat_object_record
{
	timestamp_t pos_version;
	timestamp_t version;

	Position pos;
	int player_aoi;

	int seem;
};
*/

class statistics
{
public:
	statistics (
		SimPara * para, vector<simgame_node *> * g_players, behavior * model, arbitrator_reg * arb_r, vastverse * world
		);
	~statistics ();
	static statistics * getInstance ();

    void print_all ();
	void print_header (FILE * fp);
	void print_snapshots (FILE * fp);
	void print_events (FILE * fp);
	void print_attractor_record (FILE * fp);
    void print_size_transmitted_v2 (FILE * fp);
    void print_type_size_transmitted (FILE * fp);

	int record_step ();

	void objectChanged (int change_type, VAST::id_t objectid);
	void createdUpdate (VAST::id_t objectid, timestamp_t timestamp, timestamp_t update_version);
	void receivedUpdate (VAST::id_t objectid, timestamp_t timestamp, timestamp_t update_version);
	void receivedReplicaChanged (int change_type, VAST::id_t objectid, timestamp_t timestamp, timestamp_t update_version);
	void attractorChanged (int iattr);
private:
    void update_count (map<unsigned int, map<unsigned int, unsigned int> > & tmap, 
        unsigned int current_slot, unsigned int node_type, unsigned int value);

    inline 
    void update_bw_count (unsigned int current_slot, unsigned int node_type, unsigned int value)
    {
        update_count (_trans_all, current_slot, node_type, value);
    }
        
    /*
    inline 
    void update_bw_bytype_count (unsigned int current_slot, unsigned int node_type, unsigned int value)
    {
        update_count (_trans_all_bytype, current_slot, node_type, value);
    }
    */

private:
    static statistics        * _inst;

	SimPara                  * _para;
	vector<simgame_node *>   * _players;
	behavior                 * _model;
    arbitrator_reg           * _arb_r;
    vastverse                * _world;

	map<VAST::id_t, int>             _number_of_replica;
	vector<stat_event_record*> _events;
	vector<stat_arb_record*>   _arbs;
	map<int,map<VAST::id_t, stat_object_record> > _snapshots;
	map<int, int>                           _discovery_consistency;
	map<int, pair<int,int>*>                _system_consistency;
	map<int,vector<int> >                   _attractor_record;

    // bandwidth statistics
    // map [timeslot][sta_type]
    // sta_type = BW_ARB, BW_PEER, BW_NODE          // node type
    //            BW_SEND, BW_RECV                  // trans type
    //            BW_SUM, BW_MIN, BW_MAX, BW_AVG    // statistics type
    //      sta_type = get a single item each group (node, trans and stat type) and "OR (|)" them 
    map<unsigned int, map<unsigned int, unsigned int> > _trans_all;
    // map [sta_type][for_what_node]
    // sta_type = BW_SEND, BW_RECV
    // last step accumulated transmission for pa and node
    map<int, map<VAST::id_t, unsigned int> > _pa_trans_ls;
    map<int, map<VAST::id_t, unsigned int> > _node_trans_ls;
    //////////////////////////////////

    map<int, unsigned int> _map_popularity;
	
	int                        _max_event;
	int                        _old_event_start;
	//  int                        _steps;

    // simulation start/end time
    time_t start_time, end_time;

	FILE                     * _fp;
};

#endif /* _VASTATESIM_STATISTICS_H */

