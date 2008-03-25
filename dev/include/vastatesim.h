

/*
 *	VASTATESim.h		(VASTATE Simulation main module header)
 *
 *	Here is the definition of basic method of VASTATE simulation
 *
 */


#ifndef _VASTATESIM_H
#define _VASTATESIM_H

#ifdef WIN32
// disable warning about "identifier exceeds'255' characters"
#pragma warning(disable: 4786)
#endif
	
#define __combiner(x) #x
#ifdef WIN32
#define NEWLINE __combiner(\r\n)
#else
#define NEWLINE __combiner(\n)
#endif

#include <vector>
#include "vastory.h"

using namespace VAST;
using namespace std;

// Configure section //////////////////
// Noice single vastate has problem with load-balance function
//#define CONFIG_SINGLE_VASTATE
#define CONFIG_SHOW_DETAILED_BANDWIDTH
#ifdef DEBUG
//#define VASTATESIM_DEBUG
#endif
///////////////////////////////////////

#define MAXBUFFER_SIZE          1024
#define STABLE_COLDDOWN_STEPS   20
#define THRESHOLD_BANDWIDTH     8000
#define THRESHOLD_LOW_BANDWIDTH 4000

typedef struct {
	// System Parameter
    int     WORLD_WIDTH;
    int     WORLD_HEIGHT;
    int     NODE_SIZE;
    int     TIME_STEPS;
    int     AOI_RADIUS;
    int     AOI_BUFFER;
    int     CONNECT_LIMIT;
    int     VELOCITY;
    int     LOSS_RATE;
    int     FAIL_RATE;
	int     NUM_VIRTUAL_PEERS;
    int     FAIL_PRELOAD;
    int     FAIL_INTERVAL;
    int     FAIL_STYLE;

	// Statistics Parameter
	int     SCC_PERIOD;
	int     SCC_TRACE_LENGTH;

	// Game Parameter
	int     FOOD_MAX_QUANTITY;
	int     FOOD_RATE;

	int     HP_MAX;
	int     HP_LOW_THRESHOLD;
	int     ATTACK_POWER;
	int     BOMB_POWER;
	int     FOOD_POWER;

	int     ANGST_OF_ACTION;
	int     ANGST_OF_ATTACK;
	int     TIREDNESS_OF_ACTION;
	int     TIREDNESS_OF_REST;
	int     REACTION_THR;
	int     REACTION_STOP_THR;
	int     STAY_MAX_STEP;
	//int     MIN_TIREDNESS_NEED_REST;

	int     ATTRACTOR_MAX_COUNT;
	int     ATTRACTOR_BASIC_COLDDOWN;
	int     ATTRACTOR_MAX_COLDDOWN;
	int     ATTRACTOR_RANGE;

	// Simulation Variables
	int     current_timestamp;
	int     food_rate;
    int     fail_count;
} SimPara;

struct system_state
{
    unsigned int promote_count;
    unsigned int demote_count;
};

enum {
    SYS_PROM_COUNT, SYS_DEM_COUNT
};

// modes defination for record file
enum {
    VS_NORMAL_MODE,       // reserved (?)
    VS_RECORD_MODE,     // record mode
    VS_PLAY_MODE        // play back mode
};

struct player_info
{
	Position foattr;
	Position dest;
	char last_action [2];
	int angst;
	int tiredness;
	int waiting;
};

struct food_reg
{
	id_t id;
	Position pos;
	int count;
    version_t version;
};

struct arbitrator_info
{
	Position pos;
	id_t id;
};

EXPORT int          InitVSSim (SimPara &para, int mode, const char * foodimage_filename, const char * actionimage_filename);
EXPORT bool         ReadPara (SimPara & para, const string & conffile);
EXPORT void         ProcessMsg ();
EXPORT int          CreateNode (int capacity);
EXPORT int          NextStep ();
EXPORT int          ShutV2sim ();
// !!! Notice: call this function before all simulation done will outputting only partial statistics contents.
EXPORT void         refreshStatistics ();

EXPORT voronoi *    create_voronoi ();
EXPORT void         destroy_voronoi (voronoi * v);

EXPORT object *     GetPlayerNode (int index);
EXPORT int          GetAttractorPosition (Position * poses);
EXPORT int          GetFoods (food_reg * foods);
EXPORT const char * GetFoodInfo (id_t food_id);
EXPORT bool         isArbitrator (int index);
EXPORT const char * GetPlayerInfo (int index);
EXPORT bool         GetPlayerInfo (int index, player_info * p);

EXPORT int          GetArbitratorInfo (int index, arbitrator_info * reg);
EXPORT const char * GetArbitratorString (int index, int sub_index);
EXPORT int          GetAOI (int index);
EXPORT int          GetArbAOI (int index, int sub_index);

EXPORT bool         IsPlayMode ();
EXPORT bool         IsRecordMode ();

EXPORT unsigned int GetSystemState (int parm);

#endif /* _VASTATESIM_H */
