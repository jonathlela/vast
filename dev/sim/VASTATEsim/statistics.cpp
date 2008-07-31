
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
 *  statistics.cpp (VASTATE simulation statistics module implementation)
 *
 *
 */

#include "shared.h"
using namespace VAST;

#include "stat_object_record.h"
#include "statistics.h"
#include "vastutil.h"
#include "attributebuilder.h"

#include <string>
#include <sstream>
#include <algorithm>
using namespace std;

#define self_max(a,b) ((a)>(b)?(a):(b))
#define self_min(a,b) ((a)<(b)?(a):(b))

statistics * statistics::_inst = NULL;
static int CheckPoints [] = {1, 2, 3, 4, 5, 6};

// System Running status (declared in vastatesim.cpp)
extern struct system_state g_sys_state;

// section retrieved from vast_dc.h
    typedef enum VAST_DC_Message
    {
        DC_QUERY = 10,          // find out host to contact for initial neighbor list
        DC_HELLO,              // initial connection request
        DC_EN,                 // query for missing enclosing neighbors
        DC_MOVE,               // position update to normal peers
        DC_MOVE_B,             // position update to boundary peers
        DC_NODE,               // discovery of new nodes          
        DC_OVERCAP,            // disconnection requiring action: shrink AOI or refuse connection
        DC_UNKNOWN
    };

// Network message type be tracked on section tracking transmission size by type
static msgtype_t msg_tracked [] = {
    // network layer messages ==
    ID, IPQUERY, // 2
        // -- total count above: 2
    // vast_dc messages ==
    DC_QUERY, DC_HELLO, DC_EN, DC_MOVE, DC_MOVE_B, DC_NODE, // 6
        // -- total count above: 8
    // vastate messages ==
    JOIN, ENTER, OBJECT, STATE, ARBITRATOR, 
    EVENT, OVERLOAD_I, UNDERLOAD, PROMOTE, TRANSFER, 
    TRANSFER_ACK, AGGREGATOR, MIGRATE, MIGRATE_ACK // 14
        // -- total count above: 22
};
static int msg_tracked_count = 22;
static char *msg_tracked_str[] = {
    // network layer messages ==
    "ID", "IPQUERY", 
    // vast_dc messages ==
    "DC_QUERY", "DC_HELLO", "DC_EN", "DC_MOVE", "DC_MOVE_B", "DC_NODE", 
    // vastate messages ==
    "JOIN", "ENTER", "OBJECT", "STATE", "ARBIT", 
    "EVENT", "OVER_L", "UNDER_L", "PROMOTE", "TRANS", 
    "TRANSACK", "AGGR", "MIGR", "MIGR_ACK"
};


statistics::statistics (
	SimPara * para, vector<simgame_node *> * g_players, behavior * model, arbitrator_reg * arb_r, vastverse * world)
	: _para (para), _players (g_players), _model(model), _arb_r (arb_r), _world (world)
	, _max_event (0), _old_event_start(0)
{
	FILE * fp = NULL;
	char filename[256];
	int count = 1;

	while (true)
	{
		sprintf (filename, "W%dx%d_N%04d_S%d_A%d_V%02d_L%03d_F%03d-%d.log",
			_para->WORLD_WIDTH, _para->WORLD_HEIGHT,
			_para->NODE_SIZE, _para->TIME_STEPS, _para->AOI_RADIUS, 
			_para->VELOCITY, _para->LOSS_RATE, _para->FAIL_RATE, count);

		if ((fp = fopen (filename, "rt")) == NULL)
		{
			if ((_fp = fopen (filename, "w+t")) == NULL)
				printf ("cannot open log file '%s'\n", filename);
			break;                
		}
            
		fclose (fp);
		count++;
	}

    // record time of simulation starts
    start_time = time (NULL);

    _inst = this;
}

statistics::~statistics ()
{
    if (_fp != NULL)
        print_all ();

	// delete _events
	while (!_events.empty())
	{
		delete (stat_event_record *)_events.back();
		_events.pop_back();
	}

	// delete system_consistency
	map<int,pair<int,int>*>::iterator it = _system_consistency.begin();
	for (; it != _system_consistency.end(); it ++)
	{
		delete[] it->second;
	}
	_system_consistency.clear();
}


void statistics::print_all ()
{
    // record end time of simulation
    end_time = time (NULL);

	// output simulation result
    print_header (_fp);
	print_attractor_record (_fp);
	print_snapshots (_fp);
    print_size_transmitted_v2 (_fp);
    print_type_size_transmitted (_fp);

	fclose (_fp);
    _fp = NULL;
}

void statistics::print_header (FILE * fp)
{
    if (fp == NULL)
        return;

    time_t dt = (time_t) difftime (end_time, start_time);

    // 
    // basic simulation info
    //
    fprintf (fp, "Simulation Parameters\n");
    fprintf (fp, "-------------------------------\n");
    /*
    fprintf (fp, "Data Structure Sizes (in bytes)\n");
    fprintf (fp, "        id_t:%d aoi_t:%d msgtype_t:%d timestamp_t:%d Position:%d Node:%d Addr:%d\n", 
        sizeof(VAST::id_t), sizeof(aoi_t), sizeof(msgtype_t), sizeof(timestamp_t), sizeof(Position), sizeof(Node), sizeof(Addr));
	fprintf (fp, "\n");
    */
    
    fprintf (fp, "Vastate parameters\n");
    fprintf (fp, "        %s: %d  Nodes: %d  World: (%d, %d) \n", 
        (_para->FAIL_RATE > 0 ? "Scenarios" : "Steps"), _para->TIME_STEPS, _para->NODE_SIZE, _para->WORLD_WIDTH, _para->WORLD_HEIGHT);
    fprintf (fp, "        AOI: %d (buffer: %d)  Connlimit: %d  Velocity: %d  Lossrate: %d  Failrate: %d\n", 
        _para->AOI_RADIUS, _para->AOI_BUFFER, _para->CONNECT_LIMIT, _para->VELOCITY, _para->LOSS_RATE, _para->FAIL_RATE);
	fprintf (fp, "        Virtual peers: %d\n", _para->NUM_VIRTUAL_PEERS);
    fprintf (fp, "        Prmotion     : %d     Demotion : %d \n", g_sys_state.promote_count, g_sys_state.demote_count);
    fprintf (fp, "        Start time   : %s", ctime (&start_time));
    fprintf (fp, "        End time     : %s", ctime (&end_time));
    fprintf (fp, "        Time elipsed : total %d d %d h %d m %d s.\n", 
        (int) (dt / 86400), (int) ((dt % 86400) / 3600), (int) ((dt % 3600) / 60), (int) (dt % 60));
    fprintf (fp, "\n");

	fprintf (fp, "SimGame Parameters\n");
	fprintf (fp, "        SCC PERIOD: %d LENGTH: %d\n", _para->SCC_PERIOD, _para->SCC_TRACE_LENGTH);
	//fprintf (fp, "HP_MAX: %d THR: %d FOOD MAX: %d NeedRest: %d Attack: %d Bomb: %d Food: %d\n", _para->HP_MAX, _para->HP_LOW_THRESHOLD, _para->FOOD_MAX_QUALITY, _para->MIN_TIREDNESS_NEED_REST, _para->ATTACK_POWER, _para->BOMB_POWER, _para->FOOD_POWER);
	//fprintf (fp, "ATTRACTOR: max: %d colddown basic: %d max: %d range: %d\n", _para->MAX_ATTRACTOR, _para->ATTRACTOR_BASIC_COLDDOWN, _para->ATTRACTOR_MAX_COLDDOWN, _para->ATTRACTOR_RANGE);

	fprintf (fp, "\n\n");
}

void statistics::print_attractor_record (FILE * fp)
{
}

void statistics::print_snapshots (FILE * fp)
{
	if (fp == NULL)
		return ;

    int totalsnapshots = (int)(_para->current_timestamp / _para->SCC_PERIOD);// + 1;
	int si, td;
    long total_discovery_consistency = 0;

	// print header
	fprintf (fp, "System Consistency (pos,attr)\n");
	fprintf (fp, "-----------------------------\n");
	fprintf (fp, "      Discovery ");
	for (td = 0; td < _para->SCC_TRACE_LENGTH; td ++)
		fprintf (fp, " %4d            ", td);
	fprintf (fp, "\n");

	// content
	for (si = 0; si < totalsnapshots; si ++)
	{
		fprintf(fp, "%4d", si);

		fprintf (fp, "    %3d.%02d   ", _discovery_consistency[si] / 100, _discovery_consistency[si] % 100);
        total_discovery_consistency += _discovery_consistency[si];

		for (td = 0; td < _para->SCC_TRACE_LENGTH; td ++)
		{
			//if (si+td >= _steps)
			//	fprintf(fp, "               ",);				// width:13
            if (si+td < _para->current_timestamp) //_steps)
			{
				fprintf(fp, "   ");
				if (_system_consistency[si][td].first == 10000)
					fprintf(fp, "100.00 ");
				else
					fprintf(fp, "%3d.%02d ", _system_consistency[si][td].first / 100, _system_consistency[si][td].first % 100);
				if (_system_consistency[si][td].second == 10000)
					fprintf(fp, "100.00");
				else
					fprintf(fp, "%3d.%02d", _system_consistency[si][td].second / 100, _system_consistency[si][td].second % 100);
				fprintf(fp, " ");
			}
		}
		fprintf (fp, "\n");
	}

    if (totalsnapshots > 0)
        total_discovery_consistency /= totalsnapshots;
    fprintf (fp, " AVG    %3ld.%02ld   ", total_discovery_consistency / 100, total_discovery_consistency % 100);

	fprintf (fp, "\n\n\n");
}

void statistics::print_events (FILE * fp)
{
	int cc;
	fprintf(fp,"  Update Consistency\n");
	fprintf(fp," --------------------\n");
	fprintf(fp," Check Points : ");
	for (cc = 0; cc < TOTAL_CHECKPOINT; cc++)
		fprintf (fp, " %d", CheckPoints[cc]);
	fprintf(fp,"\n");

	fprintf(fp," EventID  ObjectID  TimeStamp  Version  MaxDelay  AvgDelay*  TC \n");
	fprintf(fp," [0000]    000000    000000     000000    0000      0000     0000\n");
	//fprintf(fp," [%04d]    %6d    %6d     %6d    %4d      %4d   ");

	vector<stat_event_record*>::iterator eit = _events.begin();
	for (; eit != _events.end() ; eit ++)
	{
		stat_event_record * ev = *eit;
		fprintf(fp," [%04d]    %6d    %6d     %6d    %4d      %4d   ",
		ev->eventid,
		ev->objectid,
		ev->timestamp,
		ev->update_version,
		ev->max_delay,
		(int)(((double)ev->total_delay / (double)ev->total_replica)*10000)
		);
		for (cc = 0; cc < TOTAL_CHECKPOINT; cc++)
			fprintf(fp,"  %4d", ev->time_consistent[cc]);
		fprintf(fp,"\n");
	}
}

void statistics::print_size_transmitted_v2 (FILE * fp)
{
    fprintf (fp, "Bandwidth consumption \n"
                 "----------------------\n");
    /*

    Bandwidth consumption
    ----------------------
    AVERAGE
           ARBITRATOR          PEER                NODE
               SEND     RECV       SEND     RECV       SEND     RECV     
    0000   88888888 88888888   88888888 88888888   88888888 88888888 

    MAXIMUM

    */

    unsigned int itype[] = {SBW_ARB, SBW_PEER, SBW_SERVER, SBW_NODE, SBW_AGG, 0};
    unsigned int ctype[] = {SBW_AVG, SBW_MAX, SBW_SUM, 0}; //SBW_MIN,
    unsigned int ttype[] = {SBW_SEND, SBW_RECV, 0};
    static char *itype_name[] = {"ARBITRATOR", "PEER", "SERVER", "NODE", "AGGREGATOR"};
    static char *ctype_name[] = {"AVERAGE", "MAXIMUM", "SUM", "MINIMUM"};
    static char *ttype_name[] = {"SEND", "RECV"};
    int ic, cc, tc, st;
    int stm = (_para->current_timestamp + 1) / STEPS_PER_SECOND;

    // for each statistics type
    for (cc = 0; ctype[cc] != 0; cc ++)
    {
        // print table header
        fprintf (fp, "%s\n", ctype_name[cc]);
        fprintf (fp, "       ");
        for (ic = 0; itype[ic] != 0; ic ++)
            fprintf (fp, "%-20s", itype_name[ic]);
        fprintf (fp, "\n");
        fprintf (fp, "       ");
        for (ic = 0; itype[ic] != 0; ic ++)
        {
            for (tc = 0; ttype[tc] != 0; tc ++)
                fprintf (fp, "    %4s ", ttype_name[tc]);
            fprintf (fp, "  ");
        }
        fprintf (fp, "    DENS ");
        fprintf (fp, "\n");

        // for all time slots
        for (st = 0; st < stm; st ++)
        {
            fprintf (fp, "%4d   ", (st + 1) * STEPS_PER_SECOND);

            // for all node type
            for (ic = 0; itype[ic] != 0; ic ++)
            {
                // for send/recv
                for (tc = 0; ttype[tc] != 0; tc ++)
                    fprintf (fp, "%8u ", _trans_all [st][itype[ic] | ctype[cc] | ttype[tc]]);
                fprintf (fp, "  ");
            }

            // for area density
            fprintf (fp, "%8u ", _trans_all [st][SDEN | ctype[cc]]);
            fprintf (fp, "\n");
        }
        fprintf (fp, "\n");
    }
    fprintf (fp, "\n\n");
}



void statistics::print_type_size_transmitted (FILE * fp)
{
#define print_type_size_transmitted_SUM

    fprintf (fp, "Catelogized bandwidth consumption \n"
                 "----------------------\n");
    /*

    Catelogized bandwidth consumption
    ----------------------
    ARBITRATOR
           AVERAGE                                 ...  MAXIMUM
           DC_NODE             OBJECT              ...  DC_NODE
               SEND     RECV       SEND     RECV   ...      SEND     RECV     
    0000   88888888 88888888   88888888 88888888        88888888 88888888 

    MAXIMUM

    */

    map<unsigned int, unsigned int> overall_bytype;
    unsigned int                    overall_transmitted = 0;

    unsigned int itype[] = {SBW_ARB, SBW_PEER, SBW_AGG, 0};
    static char *itype_name[] = {"ARBITRATOR", "PEER", "AGGREGATOR"};

    unsigned int ctype[] = {SBW_AVG, 0};//SBW_AVG, SBW_MAX, 0};
    static char *ctype_name[] = {"AVERAGE"};//"AVERAGE", "MAXIMUM"};

    // msg type

#ifndef print_type_size_transmitted_SUM
    unsigned int ttype[] = {SBW_SEND, SBW_RECV, 0};
    static char *ttype_name[] = {"SEND", "RECV"};
    int tc;
#endif
    int ic, cc, mc, st;
    int stm = (_para->current_timestamp + 1) / STEPS_PER_SECOND;

    char temp_str[50];

    // for node type
    for (ic = 0; itype[ic] != 0; ++ ic)
    {
        // print table header
        fprintf (fp, "%s\n", itype_name[ic]);
        fprintf (fp, "       ");
#ifndef print_type_size_transmitted_SUM
        sprintf (temp_str, "%%-%ds", 20 * msg_tracked_count);
#else
        sprintf (temp_str, "%%-%ds", 11 * msg_tracked_count);
#endif
        for (cc = 0; ctype[cc] != 0; ++ cc)
            fprintf (fp, temp_str, ctype_name[cc]);
        fprintf (fp, "\n");
        fprintf (fp, "       ");
        for (cc = 0; ctype[cc] != 0; ++ cc)
            for (mc = 0; mc < msg_tracked_count; ++ mc)
#ifndef print_type_size_transmitted_SUM
                fprintf (fp, "%-20s", msg_tracked_str[mc]);
#else
                fprintf (fp, "%-11s", msg_tracked_str[mc]);
#endif
        fprintf (fp, "\n");
#ifndef print_type_size_transmitted_SUM
        fprintf (fp, "       ");
        for (cc = 0; ctype[cc] != 0; ++ cc)
            for (mc = 0; mc < msg_tracked_count; ++ mc)
            {
                for (tc = 0; ttype[tc] != 0; tc ++)
                    fprintf (fp, "    %4s ", ttype_name[tc]);
                fprintf (fp, "  ");
            }
        fprintf (fp, "\n");
#endif

        // for all time slots
        for (st = 0; st < stm; st ++)
        {
            fprintf (fp, "%4d   ", (st + 1) * STEPS_PER_SECOND);
            for (cc = 0; ctype[cc] != 0; ++ cc)
            {
                for (mc = 0; mc < msg_tracked_count; ++ mc)
                {
#ifndef print_type_size_transmitted_SUM
                    for (tc = 0; ttype[tc] != 0; tc ++)
                    {
                        unsigned int value = _trans_all [st][itype[ic] | ctype[cc] | SBW_MSG | msg_tracked[mc] | ttype[tc]];
                        fprintf (fp, "%8u ", value);
                        overall_bytype[ctype[cc] | SBW_MSG | msg_tracked[mc] | ttype[tc]] += value;
                    }
#else
                    unsigned int value = _trans_all [st][itype[ic] | ctype[cc] | SBW_MSG | msg_tracked[mc] | SBW_SEND]
                                        + _trans_all [st][itype[ic] | ctype[cc] | SBW_MSG | msg_tracked[mc] | SBW_RECV];
                    fprintf (fp, "%8u ", value);
                    overall_bytype[ctype[cc] | SBW_MSG | msg_tracked[mc]] += value;
#endif
                    fprintf (fp, "  ");
                }
            }
            fprintf (fp, "\n");
        }
        //fprintf (fp, "\n");

        fprintf (fp, " SUM   ");
        for (cc = 0; ctype[cc] != 0; ++ cc)
        {
            for (mc = 0; mc < msg_tracked_count; ++ mc)
            {
#ifndef print_type_size_transmitted_SUM
                for (tc = 0; ttype[tc] != 0; tc ++)
                {
                    unsigned int value = overall_bytype[ctype[cc] | SBW_MSG | msg_tracked[mc] | ttype[tc]];
                    fprintf (fp, "%8u ", value);
                    overall_transmitted += value;
                }
#else
                unsigned int value = overall_bytype[ctype[cc] | SBW_MSG | msg_tracked[mc]];
                fprintf (fp, "%8u ", value);
                overall_transmitted += value;
#endif
                fprintf (fp, "  ");
            }
        }
        fprintf (fp, "\n");

        fprintf (fp, "       ");
        for (cc = 0; ctype[cc] != 0; ++ cc)
        {
            for (mc = 0; mc < msg_tracked_count; ++ mc)
            {
#ifndef print_type_size_transmitted_SUM
                unsigned int ratio = (int) (((double) overall_bytype[ctype[cc] | SBW_MSG | msg_tracked[mc] | ttype[tc]] * 100.0) 
                                            / (double) overall_transmitted + 0.5);
                for (tc = 0; ttype[tc] != 0; tc ++)
                    fprintf (fp, "(%5u%%) ", ratio);
#else
                unsigned int ratio = (int) (((double) overall_bytype[ctype[cc] | SBW_MSG | msg_tracked[mc]] * 100.0) 
                                                / (double) overall_transmitted + 0.5);
                fprintf (fp, "   (%3u%%)", ratio);
#endif
                fprintf (fp, "  ");
            }
        }
        fprintf (fp, "\n");

        overall_bytype.clear ();
        overall_transmitted = 0;
    }

    fprintf (fp, "\n\n");
}


statistics * statistics::getInstance ()
{
	return _inst;
}

/*
 *	System Consistency calculation (scc)
 *		para.SCC_PERIOD: how many time steps get a snapshot
 *      para.SCC_TRACE_LENGTH: how many snapshots after a snapshot is taken will be compare to.
 *	  note: it is include the time step the snapshot is taken in SCC_TRACE_LENGTH
 *
 *  ex:
 *    PERIOD = 3, TRACE_LENGTH = 4
 *    time steps  0 1 2 3 4 5 6 7 8 9 10 11 12
 *                s     s     s     s        s
 *    "s"  means take a snapshot
 *    and  snapshot at 0 will have a consistency % at 0 3 6 9  (total:4)
 *                     3                              3 6 9 12
 *    ...
 *
 */
int statistics::record_step ()
{
    std::stringstream ssout;
    AttributeBuilder abr;

    if (_para->current_timestamp < 0)
        return 0;

	// game para
    int & steps = _para->current_timestamp;

    /* bandwidth version 2 */
    {
        // if this is the last step in the timeslot
        if (steps % STEPS_PER_SECOND == STEPS_PER_SECOND - 1)
        {
            // count which slot that this step belongs to
            int current_timeslot = _para->current_timestamp / STEPS_PER_SECOND;
            // temp variable for count bandwidth
            unsigned int sendsize, recvsize;
            unsigned int flag;

            // for all nodes
            vector<simgame_node *>::iterator it = _players->begin ();
            for (; it != _players->end () ; it ++)
            {
                simgame_node * tp = *it;
                map<unsigned int, unsigned int> this_node_trans;
                this_node_trans[SBW_SEND] = 0;
                this_node_trans[SBW_RECV] = 0;

                // check if arbitrator exists for this node
                int arb_count;
                if ((arb_count = tp->getArbitratorInfo (NULL)) > 0)
                {
                    // get information about arbitrators
                    arbitrator_info * arbi = new arbitrator_info [arb_count];
                    tp->getArbitratorInfo (arbi);

                    // for each arbitrator hosted by the node
                    for (int arbc = 0; arbc < arb_count; arbc ++)
                    {
                        VAST::id_t & arb_id = arbi[arbc].id;

                        // FOR OVERALL TRANSMISSION SIZE
                        ///////////////////////////////////////
                        // fetch accmulated transmitted
                        std::pair<unsigned int, unsigned int> arb_transmitted = tp->getArbitratorAccmulatedTransmit (arbc);
                        unsigned int acc_sendsize = arb_transmitted.first;
                        unsigned int acc_recvsize = arb_transmitted.second;

                        // process send size
                        if (_pa_trans_ls[SBW_SEND].find (arbi[arbc].id) == _pa_trans_ls[SBW_SEND].end ())
                            sendsize = acc_sendsize;
                        else
                            sendsize = acc_sendsize - _pa_trans_ls[SBW_SEND][arbi[arbc].id];
                        if (arbi[arbc].is_aggr)
                            update_bw_count (current_timeslot, SBW_AGG | SBW_SEND, sendsize);
                        else
                            update_bw_count (current_timeslot, SBW_ARB | SBW_SEND, sendsize);
                        this_node_trans[SBW_SEND] += sendsize;
                        
                        // process receive size
                        if (_pa_trans_ls[SBW_RECV].find (arbi[arbc].id) == _pa_trans_ls[SBW_RECV].end ())
                            recvsize = acc_recvsize;
                        else
                            recvsize = acc_recvsize - _pa_trans_ls[SBW_RECV][arbi[arbc].id];
                        if (arbi[arbc].is_aggr)
                            update_bw_count (current_timeslot, SBW_AGG | SBW_RECV, recvsize);
                        else
                            update_bw_count (current_timeslot, SBW_ARB | SBW_RECV, recvsize);
                        this_node_trans[SBW_RECV] += recvsize;

                        _arb_r->node_transmitted [arbi[arbc].id] = sendsize + recvsize;

                        // update last step transmitted
                        _pa_trans_ls[SBW_SEND][arbi[arbc].id] = acc_sendsize;
                        _pa_trans_ls[SBW_RECV][arbi[arbc].id] = acc_recvsize;

                        // FOR MSGTYPE-CATEGORIZED TRANSMISSION SIZE
                        ///////////////////////////////////////
                        // for all message types to counting for
                        for (int mt_i = 0; mt_i < msg_tracked_count; ++ mt_i)
                        {
                            unsigned int th_msgtype = msg_tracked[mt_i] | SBW_MSG;

                            // get node transmission information
                            std::pair<unsigned int, unsigned int> type_transed = tp->getArbitratorAccmulatedTransmit_bytype (arbc, msg_tracked[mt_i]);
                            unsigned int & at_sendsize = type_transed.first;
                            unsigned int & at_recvsize = type_transed.second;

                            // check node type
                            flag = ((arbi[arbc].is_aggr) ? SBW_AGG : SBW_ARB) | th_msgtype;

                            // process sending size
                            map<VAST::id_t, unsigned int> & m_send_ls = _pa_trans_ls[SBW_SEND | th_msgtype];
                            if (m_send_ls.find (arb_id) == m_send_ls.end ())
                                sendsize = at_sendsize;
                            else
                                sendsize = at_sendsize - m_send_ls[arb_id];
                            update_bw_count (current_timeslot, flag | SBW_SEND, sendsize);

                            // process receiving size
                            map<VAST::id_t, unsigned int> & m_recv_ls = _pa_trans_ls[SBW_RECV | th_msgtype];
                            if (m_recv_ls.find (arb_id) == m_recv_ls.end ())
                                recvsize = at_recvsize;
                            else
                                recvsize = at_recvsize - m_recv_ls[arb_id];
                            update_bw_count (current_timeslot, flag | SBW_RECV, recvsize);

                            // update last step transmitted
                            _pa_trans_ls [SBW_SEND | th_msgtype][arb_id] = at_sendsize;
                            _pa_trans_ls [SBW_RECV | th_msgtype][arb_id] = at_recvsize; 
                        }
                    }

                    delete[] arbi;
                }

                // check if peer exists for this node
                if (tp->GetSelfObject () != NULL)
                {
                    // fetch peer information
                    object * obj = tp->GetSelfObject ();
                    pair<unsigned int, unsigned int> peer_transmitted = tp->getAccmulatedTransmit ();
                    unsigned int acc_sendsize = peer_transmitted.first;
                    unsigned int acc_recvsize = peer_transmitted.second;
                    //size_t acc_sendsize = 0, acc_recvsize = 0;

                    // process send size
                    if (_pa_trans_ls[SBW_SEND].find (obj->get_id ()) == _pa_trans_ls[SBW_SEND].end ())
                        sendsize = acc_sendsize;
                    else
                        sendsize = acc_sendsize - _pa_trans_ls[SBW_SEND][obj->get_id ()];
                    update_bw_count (current_timeslot, SBW_PEER | SBW_SEND, sendsize);
                    this_node_trans[SBW_SEND] += sendsize;

                    // process receive size
                    if (_pa_trans_ls[SBW_RECV].find (obj->get_id ()) == _pa_trans_ls[SBW_RECV].end ())
                        recvsize = acc_recvsize;
                    else
                        recvsize = acc_recvsize - _pa_trans_ls[SBW_RECV][obj->get_id ()];
                    update_bw_count (current_timeslot, SBW_PEER | SBW_RECV, recvsize);
                    this_node_trans[SBW_RECV] += recvsize;

                    // update last step transmitted
                    // pontential BUG: use object id as keys, may collision with node ids used by arbitrators' statistics?
                    _pa_trans_ls[SBW_SEND][obj->get_id ()] = acc_sendsize;
                    _pa_trans_ls[SBW_RECV][obj->get_id ()] = acc_recvsize;

                    // FOR MSGTYPE-CATEGORIZED TRANSMISSION SIZE
                    ///////////////////////////////////////
                    // for all message types to counting for
                    for (int mt_i = 0; mt_i < msg_tracked_count; ++ mt_i)
                    {
                        unsigned int th_msgtype = msg_tracked[mt_i] | SBW_MSG;

                        // get node transmission information
                        std::pair<unsigned int, unsigned int> type_transed = tp->getAccmulatedTransmit_bytype (msg_tracked[mt_i]);
                        unsigned int & at_sendsize = type_transed.first;
                        unsigned int & at_recvsize = type_transed.second;

                        // check node type
                        flag = SBW_PEER | th_msgtype;

                        // process sending size
                        map<VAST::id_t, unsigned int> & m_send_ls = _pa_trans_ls[SBW_SEND | th_msgtype];
                        if (m_send_ls.find (obj->get_id ()) == m_send_ls.end ())
                            sendsize = at_sendsize;
                        else
                            sendsize = at_sendsize - m_send_ls[obj->get_id ()];
                        update_bw_count (current_timeslot, flag | SBW_SEND, sendsize);

                        // process receiving size
                        map<VAST::id_t, unsigned int> & m_recv_ls = _pa_trans_ls[SBW_RECV | th_msgtype];
                        if (m_recv_ls.find (obj->get_id ()) == m_recv_ls.end ())
                            recvsize = at_recvsize;
                        else
                            recvsize = at_recvsize - m_recv_ls[obj->get_id ()];
                        update_bw_count (current_timeslot, flag | SBW_RECV, recvsize);

                        // update last step transmitted
                        _pa_trans_ls [SBW_SEND | th_msgtype][obj->get_id ()] = at_sendsize;
                        _pa_trans_ls [SBW_RECV | th_msgtype][obj->get_id ()] = at_recvsize; 
                    }
                }

                // count transmit size for this node
                if (tp->is_gateway ())
                    flag = SBW_SERVER;
                else
                    flag = SBW_NODE;

                update_bw_count (current_timeslot, flag | SBW_SEND, this_node_trans[SBW_SEND]);
                update_bw_count (current_timeslot, flag | SBW_RECV, this_node_trans[SBW_RECV]);
            } /* for all nodes */
        }
    } /* bandwidth version 2 */

	/* event consistency */ /*{
		vector<stat_event_record*>::iterator eit = _events.begin () + _old_event_start;
		int cc;
		for (; eit != _events.end() ; eit ++)
		{
			stat_event_record * ev = *eit;
			for (cc = 0; cc < TOTAL_CHECKPOINT; cc++)
			{
				if (steps == (ev->timestamp + CheckPoints[cc]))
				{
					ev->time_consistent[cc] = (int)(((double)ev->consistent_replica / (double)ev->total_replica) * 10000.0);
					break;
				}
				else if ((cc==0) && (steps < (ev->timestamp + CheckPoints[0])))
				{
					eit = _events.end ();
					break;
				}
				else if ((cc==5) && (steps > (ev->timestamp + CheckPoints[TOTAL_CHECKPOINT-1])))
				{
					_old_event_start = ev->eventid + 1;
				}
			}
		}
	}*/ /* event consistency */

	/* system consistency */ {
		int s_period = _para->SCC_PERIOD;
		int s_length = _para->SCC_TRACE_LENGTH;
		int currentstep = steps / s_period;
		
		if (steps % s_period == 0)
		{
			// find all primary copy
			// for all arbitractor(s) in player(s), in its object store
			//   , find biggest version  is  a object's PRIMARY COPY
            /*
			vector<simgame_node *>::iterator it = _players->begin ();
			for (; it != _players->end () ; it ++)
			{
				simgame_node * player = *it;
				int ar, arcount = player->getArbitratorInfo(NULL);
				for (ar=0 ; ar < arcount ; ar++)
				{
					map<VAST::id_t,object*> & objs = player->GetOwnObjects(ar);
					map<VAST::id_t,object*>::iterator itown = objs.begin ();
					for (; itown != objs.end(); itown ++)
					{
						object * o = itown->second;
						if (_snapshots[currentstep].find(o->get_id()) == _snapshots[currentstep].end())
						{
							_snapshots[currentstep][o->get_id()].pos_version = o->pos_version;
							_snapshots[currentstep][o->get_id()].version     = o->version;
							_snapshots[currentstep][o->get_id()].pos         = o->get_pos ();
							_snapshots[currentstep][o->get_id()].player_aoi  = _para->AOI_RADIUS;
						}
						else
						{
							_snapshots[currentstep][o->get_id()].pos_version = self_max(o->pos_version,_snapshots[currentstep][o->get_id()].pos_version);
							_snapshots[currentstep][o->get_id()].version     = self_max(o->version,_snapshots[currentstep][o->get_id()].version);
							if (o->pos_version > _snapshots[currentstep][o->get_id()].pos_version)
								_snapshots[currentstep][o->get_id()].pos     = o->get_pos ();
						}
					}
				}

                vector<object *>::iterator it = player->GetObjects ().begin ();
                for (; it != player->GetObjects ().end (); it ++)
                    _snapshots [currentstep][(*it)->get_id ()].k_peers.push_back (player->GetSelfObject ()->peer);
			}
            */

            map<obj_id_t, object_signature> & god_store = _arb_r->god_store;
            for (map<obj_id_t, object_signature>::iterator it = god_store.begin (); it != god_store.end (); it ++)
            {
                object_signature & os = it->second;
                _snapshots[currentstep][it->first].pos         = os.pos;
                _snapshots[currentstep][it->first].pos_version = os.pos_version;
                _snapshots[currentstep][it->first].version     = os.version;
            }

			int i;
			int *total_replica_count = new int[s_length];
			int *attr_consistent_replica_count = new int[s_length];
			int *pos_consistent_replica_count = new int[s_length];
			int total_objects_count = 0;
			int correct_discovered_object = 0;
			
			for (i=0; i < s_length; i++)
			{
				total_replica_count [i] = 0;
				attr_consistent_replica_count [i] = 0;
				pos_consistent_replica_count [i] = 0;
			}

			int ss;
			int ss_start = self_max (0, currentstep-s_length+1);
			vector<simgame_node *>::iterator pit = _players->begin ();
			for (; pit != _players->end () ; pit ++)
            {
				simgame_node * player = *pit;
				if (player->GetSelfObject() == NULL)
					continue;

                //ssout << "[sta] update_consistency: peer[" << player->GetSelfObject ()->peer << "]";

				vector<object*> & objs = player->GetObjects ();
				_snapshots[currentstep][player->GetSelfObject ()->get_id ()].player_aoi = player->getAOI ();

				// Count Discovery Consistency
				map<VAST::id_t, stat_object_record>::iterator sit = _snapshots[currentstep].begin ();
				stat_object_record * self_record = & _snapshots[currentstep][player->GetSelfObject()->get_id()];

                // Loop through every objects in god_store
				for (; sit != _snapshots[currentstep].end (); sit++)
				{
					stat_object_record * processing_record = & sit->second;

                    // find if the object are known by peer
					vector<object*>::iterator poit = objs.begin();
					for (; (poit != objs.end ()) && ((*poit)->get_id() != sit->first); poit ++)
						;

                    // add total objects count
					total_objects_count ++;

                    // record should be see or not by peer for later use
					if (self_record->pos.dist(processing_record->pos) <= self_record->player_aoi)
						processing_record->seem = 1;
					else
						processing_record->seem = 0;

                    // check if "I should see and known" or "I should not see and unknown"
                    //     counts as it's consistent
                    if (((self_record->pos.dist(processing_record->pos) <= _para->AOI_RADIUS)
						&& (poit != objs.end ()))
						||
                        ((self_record->pos.dist(processing_record->pos) > _para->AOI_RADIUS)
						&& (poit == objs.end ())))
						correct_discovered_object ++;
#ifdef VASTATESIM_STATISTICS_DEBUG
                    else
                    {
                        ssout << currentstep << " [INCONSISTENT] discovery for node [" << player->GetSelfObject ()->peer << "]";
                        if (poit != objs.end ())
                            ssout << " obj_id overlooked " << abr.objectIDToString ((*poit)->get_id ()) << endl;
                        else
                            ssout << " obj_id less looked " << abr.objectIDToString (sit->first) << endl;
                    }
#endif
				}

				// Count Update Consistency
				vector<object*>::iterator poit = objs.begin();
				for (; poit != objs.end (); poit++)
				{
					object * processing_obj = *poit;
                    if (_snapshots[currentstep][processing_obj->get_id()].seem != 1)
                        continue;

                    // for each previous steps
                    for (ss = ss_start; ss <= currentstep; ss++)
					{
                        //ssout << "[ss:" << ss << "]";
                        //vector<VAST::id_t> & kp = _snapshots[ss][processing_obj->get_id ()].k_peers;
                        //vector<VAST::id_t>::iterator itkp = find (kp.begin (), kp.end (), player->GetSelfObject ()->peer);
                        
                        // check if i know the object consistent
                        // only on
                        //      1. the object are not a deleted object (in that case, I shouldn't know the object
                        //                                          , but program runs here means I knew.
                        //      2. I should know the object at that time (refer AOI and my position at that time)
                        if (_snapshots[ss].find (processing_obj->get_id ()) != _snapshots[ss].end ()
                            && _snapshots[ss][processing_obj->get_id ()].pos_version != 0
                            && _snapshots[ss].find (player->GetSelfObject ()->get_id ()) != _snapshots[ss].end ()
                            && _snapshots[ss][player->GetSelfObject ()->get_id ()].pos.dist (processing_obj->get_pos()) <= _para->AOI_RADIUS)
                            //&& (itkp != kp.end ()))
                        {
                            total_replica_count [ss-ss_start] ++;
                            if (processing_obj->pos_version >= _snapshots[ss][processing_obj->get_id ()].pos_version)
								pos_consistent_replica_count[ss-ss_start] ++;
#ifdef VASTATESIM_STATISTICS_DEBUG
                            else
                            {
                                ;//ssout << abr.objectIDToString (*processing_obj) << "pos";
                                if (ss == ss_start)
                                    ssout << currentstep << " [INCONSISTENT] position of obj_id [" << abr.objectIDToString (*processing_obj) << "] "
                                          << " node [" << player->GetSelfObject ()->peer << "]" 
                                          << endl;
                            }
#endif

                            if (processing_obj->version >= _snapshots[ss][processing_obj->get_id ()].version)
								attr_consistent_replica_count[ss-ss_start] ++;
#ifdef VASTATESIM_STATISTICS_DEBUG
                            else
                            {
                                ;//ssout << abr.objectIDToString (*processing_obj) << "att";
                                if (ss == ss_start)
                                    ssout << currentstep << " [INCONSISTENT] state of obj_id [" << abr.objectIDToString (*processing_obj) << "] "
                                          << " node [" << player->GetSelfObject ()->peer << "]" 
                                          << endl;
                            }
#endif 
                        }
                        //ssout << " ";
                    }
                }

                // ssout << "" NEWLINE;
			}

			if ((currentstep - s_length + 1) >= 0)
				_snapshots.erase (currentstep - s_length);

            _discovery_consistency [currentstep] = 
                (int)(((double) correct_discovered_object / (double) total_objects_count) * 10000);
            //ssout << "[sta] update_consistency: "
            //	  << correct_discovered_object << " / " << total_objects_count << " = " << _discovery_consistency [currentstep] << "" NEWLINE;

			_system_consistency [currentstep] = new pair<int,int>[s_length];
			//ssout << "[sta] update_consistency:" ;
			for (ss = ss_start; ss <= currentstep; ss++)
			{
				/*ssout << " [ss:" << ss << "] "
					  << " total:" << total_replica_count[ss-ss_start]
					  << " pos:" << pos_consistent_replica_count[ss-ss_start]
					  << " attr:" << attr_consistent_replica_count[ss-ss_start];*/
				_system_consistency [ss][currentstep-ss].first = (int)
					( ( (double) pos_consistent_replica_count [ss-ss_start] / (double) total_replica_count [ss-ss_start]) * 10000 );
				_system_consistency [ss][currentstep-ss].second = (int)
					( ( (double) attr_consistent_replica_count [ss-ss_start] / (double) total_replica_count [ss-ss_start]) * 10000 );
                /*
				if (currentstep-ss > 0)
				{
					if (_system_consistency[ss][currentstep-ss].first < _system_consistency [ss][currentstep-ss-1].first)
						puts("test");
					if (_system_consistency[ss][currentstep-ss].second < _system_consistency [ss][currentstep-ss-1].second)
						puts("test");
				}
                */
			}
			/*ssout << "" NEWLINE;*/

#ifdef VASTATESIM_STATISTICS_DEBUG
			errout eo;
			string s = ssout.str();
			eo.output(s.c_str());
#endif

			delete[] total_replica_count;
			delete[] attr_consistent_replica_count;
			delete[] pos_consistent_replica_count;
		}
	} /* system consistency */

    /* area popularity / density */ {
        if (steps % STEPS_PER_SECOND == STEPS_PER_SECOND - 1)
        {
            // count which slot that this step belongs to
            int current_timeslot = _para->current_timestamp / STEPS_PER_SECOND;
            int grid_width = _para->AOI_RADIUS * 2;
            int grid_count = (_para->WORLD_HEIGHT / grid_width) * (_para->WORLD_WIDTH / grid_width);
            int grid_column_count = _para->WORLD_WIDTH / grid_width;

            // object count
            map<int,unsigned int> map_areas;

            // initialize
            for (int i = 0; i < grid_count; i ++)
                map_areas[i] = 0;

            // for each object
            map<obj_id_t, object_signature> & god_store = _arb_r->god_store;
            for (map<obj_id_t,object_signature>::iterator it = god_store.begin (); it != god_store.end (); it ++)
            {
                int grid_x = (int)(it->second.pos.x / (double) grid_width);
                int grid_y = (int)(it->second.pos.y / (double) grid_width);

                // count of the area increase by one
                map_areas[grid_y * grid_column_count + grid_x] ++;
            }

            // update counting record
            for (int i = 0; i < grid_count; i ++)
                update_bw_count (current_timeslot, SDEN, map_areas[i]);
        }
    }

	return 0;
}

void 
statistics::update_count (map<unsigned int, map<unsigned int, unsigned int> > & tmap, 
                              unsigned int current_slot, unsigned int node_type, unsigned int value)
{
    if (tmap[current_slot].find (node_type | SBW_SUM) == tmap[current_slot].end ())
    {
        tmap[current_slot][node_type | SBW_SUM]   = value;
        tmap[current_slot][node_type | SBW_AVG]   = value;
        tmap[current_slot][node_type | SBW_MIN]   = value;
        tmap[current_slot][node_type | SBW_MAX]   = value;
        tmap[current_slot][node_type | SBW_COUNT] = 1;
    }
    else
    {
        tmap[current_slot][node_type | SBW_COUNT] += 1;
        tmap[current_slot][node_type | SBW_SUM]   += value;
        tmap[current_slot][node_type | SBW_AVG]    = tmap[current_slot][node_type | SBW_SUM] / tmap[current_slot][node_type | SBW_COUNT];
        tmap[current_slot][node_type | SBW_MIN]    = self_min (value, tmap[current_slot][node_type | SBW_MIN]);
        tmap[current_slot][node_type | SBW_MAX]    = self_max (value, tmap[current_slot][node_type | SBW_MAX]);
    }

}

/*
void statistics::update_bw_count (unsigned int current_slot, unsigned int node_type, unsigned int value)
{
    if (_trans_all[current_slot].find (node_type | SBW_SUM) == _trans_all[current_slot].end ())
    {
        _trans_all[current_slot][node_type | SBW_SUM] = value;
        _trans_all[current_slot][node_type | SBW_AVG] = value;
        _trans_all[current_slot][node_type | SBW_MIN] = value;
        _trans_all[current_slot][node_type | SBW_MAX] = value;
        _trans_all[current_slot][node_type | SBW_COUNT] = 1;
    }
    else
    {
        _trans_all[current_slot][node_type | SBW_COUNT] += 1;
        _trans_all[current_slot][node_type | SBW_SUM]   += value;
        _trans_all[current_slot][node_type | SBW_AVG]    = _trans_all[current_slot][node_type | SBW_SUM] / _trans_all[current_slot][node_type | SBW_COUNT];
        _trans_all[current_slot][node_type | SBW_MIN]    = self_min (value, _trans_all[current_slot][node_type | SBW_MIN]);
        _trans_all[current_slot][node_type | SBW_MAX]    = self_max (value, _trans_all[current_slot][node_type | SBW_MAX]);
    }
}
*/

// 1 for create_obj, 2 for delete_obj
void statistics::objectChanged (int change_type, VAST::id_t objectid)
{
	switch (change_type)
	{
	case 1:
		_number_of_replica[objectid] = 0;
		break;

	case 2:
		_number_of_replica.erase (objectid);
		break;
	default:
		;
	}
}

void statistics::createdUpdate (VAST::id_t objectid, timestamp_t timestamp, timestamp_t update_version)
{
	stat_event_record * newev;

	newev = new stat_event_record;
	memset (newev, 0, sizeof(stat_event_record));
	newev->eventid            = _events.size ();
	newev->objectid           = objectid;
    newev->timestamp          = _para->current_timestamp; //_steps; //timestamp;
	newev->update_version     = update_version;

	newev->consistent_replica = 0;
	newev->total_replica      = _number_of_replica[objectid];

	_events.push_back (newev);
}

void statistics::receivedUpdate (VAST::id_t objectid, timestamp_t timestamp, timestamp_t update_version)
{
	vector<stat_event_record*>::iterator eit = _events.begin ();
	for(; eit != _events.end () ; eit ++)
	{
		stat_event_record * ev = *eit;
		if (((unsigned) ev->objectid == objectid) && (ev->update_version == update_version))
		{
			//int difftime = timestamp - ev->timestamp;
			//int difftime = _steps - ev->timestamp;
            int difftime = _para->current_timestamp - ev->timestamp;
			ev->consistent_replica ++;
			if (difftime > ev->max_delay)
				ev->max_delay = difftime;
		}
	}
}

void statistics::receivedReplicaChanged (int change_type, VAST::id_t objectid, timestamp_t timestamp, timestamp_t update_version)
{
	if (change_type == 1)
		_number_of_replica[objectid] = _number_of_replica[objectid] + 1;
	else
		_number_of_replica[objectid] = _number_of_replica[objectid] - 1;

	vector<stat_event_record*>::iterator eit = _events.begin();
	for (; eit != _events.end () ; eit ++)
	{
		stat_event_record * evred = *eit;
		if ((unsigned) evred->objectid == objectid)
		{
			if ((change_type == 1) && (update_version < evred->update_version))
				evred->total_replica ++;
			else if ((change_type == 2) && (update_version < evred->update_version))
				evred->total_replica --;
		}
	}
}

void statistics::attractorChanged (int iattr)
{
    _attractor_record[iattr].push_back (_para->current_timestamp);//_steps);
}

