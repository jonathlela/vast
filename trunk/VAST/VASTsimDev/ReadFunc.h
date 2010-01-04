
#ifndef READ_FUNC_H
#define READ_FUNC_H

#include <time.h>

#include "VASTsim.h"



using namespace std;
using namespace Vast;

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

//EXPORT bool                 ReadLatency (int &lat_size, vector<NodeList>& pp_lat_table);
EXPORT bool                 ReadLatency (int &lat_size, vector<vector<float>> &tg_lat_table);
EXPORT bool                 ReadBandwidth (vector<SimBand> &band);
//EXPORT void                 SetBandwidth (vector<SimBand> &band);
EXPORT int                  ReadTopGenLatency (vector<vector<float>> &tg_lat_table);
EXPORT int                  ReadPairPingLatency (vector<NodeList> &pp_lat_table);

#endif