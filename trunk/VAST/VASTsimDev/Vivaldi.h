

#ifndef VASTSIM_VIVALDI_H
#define VASTSIM_VIVALDI_H

#include <time.h>

#include "VASTUtil.h"
#include "Movement.h"
#include "VASTVerse.h"
#include "VASTsim.h"
//#include "ReadFunc.h"

using namespace std;
using namespace Vast;

typedef struct 
{
	int DOWNLOAD;
	int UPLOAD;
} Bandwidth;

typedef struct
{
	int     DOWNLINK;
	int     UPLINK;
	int     RANGEBASE;          // random range for peer's bandwidth
} SimBand;

typedef struct
{
	Position   COORD;           // physical coordinate
	float      ERROR;           // node predict error its self
	float      E_RATE;          // error estimate of node for adaptive timestep
} PhysCoord;

typedef struct
{
	char DEST_IP[16];
	float MIN_PING;
	float AVG_PING;
	float MAX_PING;
} PairPing;                     // pair ping per node

typedef struct
{
	char SOUR_IP[16];
	vector<PairPing> DEST_NODE;
} NodeList;                     // node's ping list


/************************************************************************/
/*                        Vivaldi Algorithm                             */
/************************************************************************/

class EXPORT Vivaldi
{
public:

	Vivaldi(){};
	Vivaldi (SimPara &para);

	~Vivaldi ();

	void proc_vivaldi_simple ();                                   // simple algorithm

	void proc_vivaldi_adaptive ();                                 // adaptive algorithm for convergence

	bool release_phys_coord (id_t node_id);                        // release used coordinate

	Position get_phys_coord (id_t node_id);                        // given a physical coordinate

	int get_down_bandwidth (id_t node_id);

	int get_up_bandwidth (id_t node_id);

	float get_latency (id_t src_node, id_t dest_node);

private:

	void  init_physcoord ();

	void  unit_vector (Position& V, Position &coord_i, Position &coord_j);

	bool  read_para ();

	float rand24 ();                                               // RAND24_MAX = 16777215 

	float rel_err_sample (float distance, float rtt);

	bool  read_latency ();

	bool  read_bandwidth ();

	int   read_tg_latency ();

	void  create_bandwidth ();


	//bool                 ReadLatency (int &lat_size, vector<NodeList>& pp_lat_table);
	//int   ReadPairPingLatency ();

	// basic parameter
	SimPara                       _para;
	vector<SimBand>               _band_table;                     // bandwidth cumulative distribution fraction
	vector<PhysCoord>             _nodes;                          // physical nodes position
	vector<Bandwidth>             _band;                           // nodes bandwidth
	vector<vector<float>>         _l_table;                        // gentop latency table
	map<id_t, int>                _lat_list;                       // latency list
	map<id_t, int>                _band_list;                      // nodes bandwidth list
	map<id_t, int>                _join_list;                      // record for join nodes
	map<id_t, int>                _leave_list;                     // record for leave nodes

	// general mesurement use
	//vector<NodeList>              _pp_l_table;

	// for more accurate measurement use
	//vector<NodeList>              _l_table_pp;                     // pair ping latency table

	unsigned int                  _lat_index;                      // latency index has been used
	unsigned int                  _band_index;                     // the bandwidth index has been used
	unsigned int                  _phys_index;                     // the phys_coordinate index has been used
	unsigned int                  _node_size;                      // for latency or bandwidth table size
	unsigned int                  _phys_map_size;                  // physical coordinate system map size
	float                         _error;                          // total error in physical coordinate
	float                         _tolerance;                      // the error value we can allow of this coordinate
	float                         _con_frac;                       // Cc, constant fraction of a node's estimated error
	float                         _con_error;                      // Ce, constant for tuning parameter
	float                         _dtime;                          // determine how far a node moves at each time interval
};

#endif