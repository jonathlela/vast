
/*
 *  statistics.h (VASTATE simulation statistics module header)
 *
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


// Constants for Statistics for Bandwidth
//SBW_ARB, SBW_PEER, SBW_NODE, SBW_SERVER
#define SBW_ARB    0x00001000
#define SBW_PEER   0x00002000
#define SBW_NODE   0x00004000
#define SBW_SERVER 0x00008000
//BW_SEND, BW_RECV
#define SBW_SEND   0x00000100
#define SBW_RECV   0x00000200
// AREA DENSITY
#define SDEN       0x00010000
//BW_SUM, BW_MIN, BW_MAX, BW_AVG
#define SBW_COUNT  0x00000001
#define SBW_SUM    0x00000010
#define SBW_AVG    0x00000020
#define SBW_MIN    0x00000040
#define SBW_MAX    0x00000080

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

	int record_step ();

	void objectChanged (int change_type, id_t objectid);
	void createdUpdate (id_t objectid, timestamp_t timestamp, timestamp_t update_version);
	void receivedUpdate (id_t objectid, timestamp_t timestamp, timestamp_t update_version);
	void receivedReplicaChanged (int change_type, id_t objectid, timestamp_t timestamp, timestamp_t update_version);
	void attractorChanged (int iattr);
private:
    void update_bw_count (unsigned int current_slot, unsigned int node_type, unsigned int value);

private:
    static statistics        * _inst;

	SimPara                  * _para;
	vector<simgame_node *>   * _players;
	behavior                 * _model;
    arbitrator_reg           * _arb_r;
    vastverse                * _world;

	map<id_t, int>             _number_of_replica;
	vector<stat_event_record*> _events;
	vector<stat_arb_record*>   _arbs;
	map<int,map<id_t, stat_object_record> > _snapshots;
	map<int, int>                           _discovery_consistency;
	map<int, pair<int,int>*>                _system_consistency;
	map<int,vector<int> >                   _attractor_record;

    // new bandwidth statistics coding
    //BW_ARB, BW_PEER, BW_NODE
    //BW_SEND, BW_RECV
    //BW_SUM, BW_MIN, BW_MAX, BW_AVG
    // map [timeslot][sta_type]
    map<unsigned int, map<unsigned int, unsigned int> > _trans_all;
    // map [sta_type][for_what_node]
    // sta_type = BW_SEND, BW_RECV
    map<int, map<id_t, unsigned int> > _pa_trans_ls;
    map<int, map<id_t, unsigned int> > _node_trans_ls;
    //////////////////////////////////

    map<int, unsigned int> _map_popularity;
	
	int                        _max_event;
	int                        _old_event_start;
	//  int                        _steps;
	FILE                     * _fp;
};

#endif /* _VASTATESIM_STATISTICS_H */

