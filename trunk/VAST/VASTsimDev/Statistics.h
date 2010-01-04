/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2006 Shun-Yun Hu (syhu@yahoo.com)
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
 *  Statistics.h -- methods to collect simulation statistics
 *
 */

#ifndef VASTSIM_STATISTICS_H
#define VASTSIM_STATISTICS_H

#include <time.h>
#include <vector>
#include <map>
#include <algorithm>
#include "vast.h"
#include "VASTsim.h"
#include "SimNode.h"

#define SNAPSHOT_INTERVAL   (100)

using namespace std;
using namespace Vast;

class statistics
{
public:
                
    statistics ()
    {	
        _steps = _steps_stablized = 0;
        _AN_actual_accumulated = _AN_visible_accumulated = 0;
        _AN_actual_first_interval = _AN_visible_first_interval =0;
        _fp = NULL;
    }

    ~statistics ()
    {
        if (_fp != NULL)
            fclose (_fp);        
    }

    void init_variables ()
    {
        // zero out stat variables
        _total_AN_actual = _total_AN_visible = 0;        
        _max_AN = 0; 
        
        _max_drift = _total_drift = _drift_nodes = 0;
        _max_RS = _recovery_steps = _recovery_count = _inconsistent_count = 0;       
        
//        _last_consistent.clear ();
        _last_consistent_map.clear();
/*
        for (int i=0; i<_simnodes.size(); ++i)
            _last_consistent.push_back (_steps);
*/
		int num_nodes = _simnodes.size();
		for (int i=0; i< num_nodes; i++)
		{
			if (is_failed(i))
			{
				continue;
			}	
			_last_consistent_map[_simnodes[i]->get_id()] = _steps;
		}
		
    }

    void init_timer (SimPara &para)
    {
        _starttime = time (NULL);
        _para = para;

        // open output file, and avoid overwrite existing log files
        char filename[80];
                
        int count = 1;

        while (true)
        {
            sprintf (filename, "N%04d_S%d_A%d_C%02d_V%02d_L%03d_F%03d-%d.log", _para.NODE_SIZE, _para.TIME_STEPS, _para.AOI_RADIUS, _para.CONNECT_LIMIT, _para.VELOCITY, _para.LOSS_RATE, _para.FAIL_RATE, count);
            
            if ((_fp = fopen (filename, "rt")) == NULL)
            {
                if ((_fp = fopen (filename, "w+t")) == NULL)
                    printf ("cannot open log file '%s'\n", filename);
                break;                
            }
            
            fclose (_fp);
            count++;
        }
        
        print_header ();
        init_variables ();        
    }

    // keep a reference of a newly created node
    void add_node (SimNode *node)
    {
        _simnodes.push_back (node);
//		_last_consistent.push_back(_steps);
		_last_consistent_map[node->get_id()] = _steps;
    }


	void remove_node (SimNode *node)
	{
		vector<SimNode*>::iterator sit = _simnodes.begin();
		vector<SimNode*>::const_iterator end_it = _simnodes .end();

		for (;sit != end_it; sit++)
		{
			if ((*sit)->get_id() == node->get_id())
			{
				//_last_consistent_map.erase((*sit)->get_id());
				//_simnodes.erase(sit);
				node->clear_variables ();
				break;
			}			
		}
		
	}
	// calculate the current un-fail nodes 
	inline int num_unfail_nodes()
	{
		int num_nodes = 0;
		vector<SimNode*>::iterator it = _simnodes.begin();
		vector<SimNode*>::const_iterator end_it = _simnodes.end();		
		for (; it != end_it; it++)
		{
			if ((*it)->state != FAILED)
			{
				num_nodes++;
			}			
		}		
		return num_nodes;
	}

	inline bool is_failed (size_t index)
	{
		return _simnodes[index]->state == FAILED;
	}


	// if nodes has join the _simnode list return true else return false
	inline bool is_rec (id_t bid)
	{	
		vector<SimNode*>::iterator       sit = _simnodes.begin();
		vector<SimNode*>::const_iterator end_it = _simnodes.end();
		
		for (; sit != end_it; sit++)
		{
			if ((*sit)->getBehId()  == bid)
			{
				return true;
			}			
		}	
		return false;
	}

    void calc_consistency (size_t i, size_t &AN_actual, size_t &AN_visible, size_t &total_drift, size_t &max_drift, size_t &drift_nodes)
    {		
		
        size_t n = _simnodes.size ();

        Node *neighbor;
        AN_actual = AN_visible = total_drift = max_drift = drift_nodes = 0;

        // loop through all nodes
        for (size_t j=0; j<n; ++j)
        {                
            // skip self-check
            if (i == j || is_failed(j))
                continue;

            // visible neighbors
            if (_simnodes[i]->in_view (_simnodes[j]))
            {
                AN_actual++;
                
                if ((neighbor = _simnodes[i]->knows (_simnodes[j])) != NULL)
                {
                    AN_visible++;           
                   
                    // calculate drift distance (except self)
                    // NOTE: drift distance is calculated for all known AOI neighbors
                    drift_nodes++;

                    size_t drift = (size_t)neighbor->aoi.center.distance (_simnodes[j]->get_pos ());
                    total_drift += drift;
                    
                    if (max_drift < drift)
                    {
                        max_drift = drift;
#ifdef DEBUG_DETAIL
                        printf ("%4d - max drift updated: [%d] info on [%d] drift: %d\n", _steps+1, (int)_simnodes[i]->get_id (), (int)neighbor->id, (int)drift);
#endif
                    }                        
                }
            }
        } // end looping through all other nodes


    }

    // NOTE: some variable abbreviation
    //
    //      AN = AOI neighbor
    //      CN = connected neighbor
    //
    // returns: number of nodes having inconsistent views
    //
    // what we need to collect:
    //      1. consistency data (global vs. local view)
    //      2. # of AN/CN
    //      3. transmission size
    //
    int record_step ()
    {        
        _steps++;
            
        // init variables          
        size_t n = _simnodes.size ();
        size_t inconsistent_nodes = 0;
        size_t i;
        size_t AN_actual, AN_visible;
        size_t total_drift, max_drift, drift_nodes;

        size_t recovery_steps;

        // loop through all nodes to calculate statistics
        for (i=0; i<n; ++i)
        {           
			if (is_failed(i))
			{
				continue;
			}
			
            // find actual AOI neighbor (from a global view)
            calc_consistency (i, AN_actual, AN_visible, total_drift, max_drift, drift_nodes);

            // record stat
            _drift_nodes += drift_nodes;
            _total_drift += total_drift;

            if (_max_drift < max_drift)
                _max_drift = max_drift;

            if (_max_AN < AN_visible)
                _max_AN = AN_visible;

            _total_AN_visible += AN_visible;
            _total_AN_actual  += AN_actual;
			

            //
            // check if 100% consistency is achieved & record
            // and if any node has recovered from topology inconsistency
            //

			id_t lc_index = _simnodes[i]->get_id();
            if (AN_visible == AN_actual)
            {
                //recovery_steps = _steps - _last_consistent[i] - 1;
                recovery_steps = _steps - _last_consistent_map[lc_index] - 1;
                // if there has been some inconsistency
                if (recovery_steps > 0)
                {                    
                    _recovery_count++;
                    _recovery_steps += recovery_steps;
                    
                    if (recovery_steps > _max_RS)
                        _max_RS = recovery_steps;
                }
                
               // _last_consistent[i] = _steps;                
				_last_consistent_map[lc_index] = _steps;
            }
            else
            {
                inconsistent_nodes++;

#ifdef RECORD_INCONSISTENT_NODES                
                pair<size_t, Vast::id_t> *in_node = new pair<size_t, Vast::id_t>(_steps, _simnodes[i]->get_id ());
                _inconsistent_nodes.push_back (in_node);
#endif                
                // record the onset of an inconsistency
                //if (_last_consistent[i] == (_steps-1))
				if (_last_consistent_map[lc_index] == (_steps-1))
                    _inconsistent_count++;                
            }
        }
                   
        // record transmission size per simulated second
        if (_steps % _para.STEPS_PERSEC == 0)
        {
            for (i=0; i<n; ++i)        
			{
				if (is_failed(i))
				{
					continue;
				}
				
                _simnodes[i]->record_stat_persec ();    
			}
        }        
        
        // take snapshot records of current stat
        if (_steps % SNAPSHOT_INTERVAL == 0)
            print_snapshot ();
        
        // record 1st 100% TC point
        if (_steps_stablized == 0 && inconsistent_nodes == 0)
        {
            _steps_stablized = _steps;
            _AN_actual_first_interval = _total_AN_actual;
            _AN_visible_first_interval = _total_AN_visible;
        }

        return inconsistent_nodes;
    }

    void print_header ()
    {
        if (_fp == NULL)
            return;
        // 
        // basic simulation info
        //
        fprintf (_fp, "Data Structure Sizes (in bytes)\n");
        fprintf (_fp, "-------------------------------\n");
        fprintf (_fp, "id_t:%d length_t:%d msgtype_t:%d timestamp_t:%d Point:%d Node:%d Addr:%d\n\n\n", sizeof(Vast::id_t), sizeof(length_t), sizeof(msgtype_t), sizeof(timestamp_t), sizeof(Position), sizeof(Node), sizeof(Addr));
        
        fprintf (_fp, "Simulation Parameters\n");
        fprintf (_fp, "---------------------\n");
        fprintf (_fp, "nodes: %d world: (%d, %d) %s: %d\naoi: %d buffer: %d connlimit: %d velocity: %d lossrate: %d failrate: %d\n\n\n", _para.NODE_SIZE, _para.WORLD_WIDTH, _para.WORLD_HEIGHT, (_para.FAIL_RATE > 0 ? "scenarios" : "steps"), _para.TIME_STEPS, _para.AOI_RADIUS, _para.AOI_BUFFER, _para.CONNECT_LIMIT, _para.VELOCITY, _para.LOSS_RATE, _para.FAIL_RATE);
        
        
        fprintf (_fp, "Simulation Snapshots                                                                                          \n");
        fprintf (_fp, "--------------------                                                                                          \n");
        fprintf (_fp, "TC:             Topology Consistency                                                                          \n");
        fprintf (_fp, "Send/Recv:      Transmission per node per second (assuming %d updates / second)                               \n", _para.STEPS_PERSEC);
        fprintf (_fp, "DD:             Drift Distance                                                                                \n");
        fprintf (_fp, "AOI:            AOI-radius                                                                                    \n");
        fprintf (_fp, "CN:             Connected Neighbor                                                                            \n");
        fprintf (_fp, "AN:             AOI Neighbor                                                                                  \n");
        fprintf (_fp, "RS:             Recovery Steps                                                                                \n");
        fprintf (_fp, "RC:             Number of Successful Recovery Cases                                                           \n");
        fprintf (_fp, "                                                                                                              \n");
        fprintf (_fp, "For each metric, first number is the average, the second is either the maximum or minimum.                    \n");
        fprintf (_fp, "                                                                                                              \n");
        fprintf (_fp, "Snapshot interval: %d time-steps                                                                              \n", SNAPSHOT_INTERVAL);
        fprintf (_fp, "                                                                                                              \n");
		//if (_para.VAST_MODEL != VAST_MODEL_MULTICAST)
		//fprintf (_fp, "TC              Send (max)         Recv (max)     RS (max)   DD (max)   AOI (min)   CN (max)    AN (max)     RC (total)         TC raw data      MOVE latency (min/max/avg)   PUBLISH latency (min/max/avg)\n");
        fprintf (_fp, "TC              Send (max)         Recv (max)     RS (max)   DD (max)   AOI (min)   CN (max)    AN (max)     RC (total)         TC raw data      MOVE latency (min/max/avg)   PUBLISH latency (min/max/avg)  Peer-size (min/max/avg)\n");
        //else
        //    fprintf (_fp, "TC              Send (max)         Recv (max)       Send-d (max)       Recv-d (max)     RS (max)   DD (max)   AOI (min)   CN (max)    AN (max)    RC (total)         TC raw data\n");
        
    }

    void print_snapshot ()
    {
        if (_fp == NULL)
            return;

        int num_nodes   = _simnodes.size ();
        int num_samples = (_steps % SNAPSHOT_INTERVAL) * num_unfail_nodes();
        if (num_samples == 0)
            num_samples = SNAPSHOT_INTERVAL * num_nodes;
                
        long min_aoi = _para.AOI_RADIUS;
        float total_aoi = 0;
        int total_send = 0, total_recv = 0;
        //int max_send = 0, max_recv = 0;   

		size_t max_send_per_sec = 0, max_recv_per_sec = 0;
        int max_CN = 0;
        float total_CN = 0;

		StatType move_latency;
		StatType publish_latency;
        
        // do a final recovery-steps calculation 
        // (some nodes have not yet recovered and they should add to the recovery_steps calculation)        
        // need to add them to '_inconsistant_count' as well as '_recovery_steps'
        int i = 0;
		id_t lc_index;
        for (; i<num_nodes; ++i)
        {
			if (is_failed(i))
			{
				continue;
			}			
			lc_index = _simnodes[i]->get_id();
            //if (_last_consistent[i] < _steps)
			if(_last_consistent_map[lc_index] < _steps)
            {
                _inconsistent_count++;            
                
                // TODO, BUG: should we add this?
                _recovery_count++;
                //_recovery_steps += (_steps - _last_consistent[i] - 1);
				_recovery_steps += (_steps - _last_consistent_map[lc_index] - 1);
            }
        }        

        // collect stat from all simnodes
        for (i=0; i<num_nodes; ++i)
        {   
			if (is_failed(i))
			{
				continue;
			}	

            // aoi
            if (_simnodes[i]->min_aoi () < min_aoi)
                min_aoi = _simnodes[i]->min_aoi ();
            total_aoi += _simnodes[i]->avg_aoi ();
            
            // transmission
            total_send += (int)_simnodes[i]->avg_send ();
            total_recv += (int)_simnodes[i]->avg_recv ();

            
            // transmission per second
            if (_simnodes[i]->max_send_persec > max_send_per_sec)
                max_send_per_sec = _simnodes[i]->max_send_persec;
            if (_simnodes[i]->max_recv_persec > max_recv_per_sec)
                max_recv_per_sec = _simnodes[i]->max_recv_persec;
 
			// CN
            if (_simnodes[i]->max_CN () > max_CN)
                max_CN = _simnodes[i]->max_CN ();
            total_CN += _simnodes[i]->avg_CN ();

			// latency
			//VAST_Message msgtype = MOVE; // (equals 19)

			StatType *stat = _simnodes[i]->vnode->getMessageLatency (19);
			if (stat != NULL)
			{
				if (stat->minimum < move_latency.minimum)
					move_latency.minimum = stat->minimum;
				if (stat->maximum > move_latency.maximum)
					move_latency.maximum = stat->maximum;

				move_latency.total += stat->total;
				move_latency.num_records += stat->num_records;
			}

			// publish latency
			stat = _simnodes[i]->vnode->getMessageLatency (18);
			if (stat != NULL)
			{
				if (stat->minimum < publish_latency.minimum)
					publish_latency.minimum = stat->minimum;
				if (stat->maximum > publish_latency.maximum)
					publish_latency.maximum = stat->maximum;

				publish_latency.total += stat->total;
				publish_latency.num_records += stat->num_records;
			}

            // clean per node record
            _simnodes[i]->clear_variables ();
			_simnodes[i]->vnode->getMessageLatency (0);
        }

        // calculate Topology Consistency
        double consistency = (double)_total_AN_visible / (double)_total_AN_actual;                
        
        // print snapshots of stat
        
        // TC
        fprintf (_fp, "%3.4f%%\t", (float)consistency * 100);

        // Send / Recv
        fprintf (_fp, "%8d %-8d  ", total_send/num_nodes, max_send_per_sec);
        fprintf (_fp, "%8d %-8d  ", total_recv/num_nodes, max_recv_per_sec);

		// RS
        fprintf (_fp, "%3.3f %-3d  ", (_recovery_count > 0 ? (double)_recovery_steps/(double)_recovery_count : 0), _max_RS);
        
        // drift distance
        fprintf (_fp, "%3.3f %-3d  ", (double)_total_drift / (double)_drift_nodes, (int)_max_drift);        

        // AOI
        fprintf (_fp, "%3.2f %-3d  ", (double)total_aoi / (double)num_nodes, (int)min_aoi);
        
        // CN
        fprintf (_fp, "%3.3f %-3d  ", (double)total_CN/(double)num_nodes, max_CN);

        // AN
        fprintf (_fp, "%3.3f %-3d  ", (double)_total_AN_visible/(double)num_samples, _max_AN);
        
        // RC
        fprintf (_fp, "%3d %3d %3.2f%%  ", _recovery_count, _inconsistent_count, (_inconsistent_count > 0 ? ((double)_recovery_count/(double)_inconsistent_count*100): 0));
                             
        // TC actual data
        fprintf (_fp, " %7d %7d ", _total_AN_visible, _total_AN_actual);

		// latencies
		move_latency.calculateAverage ();
		publish_latency.calculateAverage ();
		fprintf (_fp, "(%d %d %f) ", move_latency.minimum, move_latency.maximum, move_latency.average);
		fprintf (_fp, "(%d %d %f) \n", publish_latency.minimum, publish_latency.maximum, publish_latency.average);

        // store accumulated data
        _AN_visible_accumulated += _total_AN_visible;
        _AN_actual_accumulated += _total_AN_actual;
        
        /* accumulated
        fprintf (_fp, "\ntransmission size (total # of bytes over %d steps)\n", _steps);
        fprintf (_fp, "[ avg] sent: %10d recv: %10d\n"  , total_send/num_nodes, total_recv/num_nodes);
        fprintf (_fp, "[ max] sent: %10d recv: %10d\n\n", max_send, max_recv);
        */

        // reset stat
        init_variables ();
    }
    
    void print_stat ()
    {
        if (_fp == NULL)
            return;
        
        printf ("writing results...\n");

        // if we abort prematurely, then take a snapshot
        if (_steps % SNAPSHOT_INTERVAL != 0)
        {
            fprintf (_fp, "\n");  
            print_snapshot ();
        }

        size_t send_accumulated = 0, recv_accumulated = 0, i;

		for (i=0; i<_simnodes.size (); ++i)
        {
			if (_simnodes[i]->state == FAILED)
			{
				continue;
			}			
            send_accumulated += _simnodes[i]->accumulated_send ();
            recv_accumulated += _simnodes[i]->accumulated_recv ();		
        }

        double consistency = (double)_AN_visible_accumulated / (double)_AN_actual_accumulated;

        fprintf (_fp, "\n\nSummary\n-------\n");        
        fprintf (_fp, "Total elapsed time (seconds): %d  stablized_steps: %d  run_steps: %d\n", (int)(time (NULL)-_starttime), _steps_stablized, _steps);        
        fprintf (_fp, "Topology Consistency (overall):         %3.4f%%\n", consistency * 100);
        consistency = (double)(_AN_visible_accumulated-_AN_visible_first_interval) / (double)(_AN_actual_accumulated - _AN_actual_first_interval);
        fprintf (_fp, "Topology Consistency (after stablized): %3.4f%%\n\n", consistency * 100);

        fprintf (_fp, "transmission size (bytes/second, assuming %d updates/second)\n", _para.STEPS_PERSEC);

        fprintf (_fp, "[ avg] sent: %u recv %u\n", (size_t)((float)send_accumulated / (float)_simnodes.size () / (float)_steps * _para.STEPS_PERSEC), (int)((float)recv_accumulated / (float)_simnodes.size () / (float)_steps * _para.STEPS_PERSEC));


        //
        // transmission size stat
        //
        
        fprintf (_fp, "\ntransmission size (total # of bytes over %d steps)\n\n", _steps);
		//if (_para.VAST_MODEL == VAST_MODEL_DIRECT)
		fprintf (_fp, "             send       recv   DISCONNCT       ID    QUERY    HELLO       EN     MOVE   MOVE_B     NODE  OVERCAP  PAYLOAD\n");
		//else if (_para.VAST_MODEL == VAST_MODEL_MULTICAST)
		//	fprintf (_fp, "	     send	recv   def_send   def_recv     DISCONNCT    QUERY INITLIST   REQNBR   ACKNBR  REQADDR  ACKADDR	  HELLO	EXCHANGE     MOVE    RELAY    UNKNOWN\n");

		for (i=0; i<_simnodes.size (); i++)
		{
			SimNode *node = _simnodes[i];
			StatType *peersize = _simnodes[i]->vnode->getPeerStat ();
			fprintf (_fp, "[%4d] %u %u\t%s\t", (int)node->get_id (), node->accumulated_send (), node->accumulated_recv (), node->vnode->getStat ());

			if (peersize != NULL)
				fprintf (_fp, "(%d, %d, %f)", peersize->minimum, peersize->maximum, peersize->average);

			fprintf (_fp, "\n");
		}

        //
        // records of nodes having inconsistency
        //

#ifdef RECORD_INCONSISTENT_NODES    
        fprintf (_fp, "\nInconsistent nodes\n");
        int n = _inconsistent_nodes.size ();
        int last_step = 0, step;
        Vast::id_t id;          
        for (i=0; i<n; ++i)
        {
            step = _inconsistent_nodes[i]->first;
            id = _inconsistent_nodes[i]->second;
            
            if (step != last_step)
            {
                fprintf (_fp, "\n%4d - ", step);
                last_step = step;
            }
            fprintf (_fp, "[%d] ", (int)id);
            
            delete _inconsistent_nodes[i];
        }
        fprintf (_fp, "\n\n");
        _inconsistent_nodes.clear ();        
#endif
    }

private:
    time_t          _starttime;     // system time when starting the simulation
    size_t          _steps;         // total # of simulation time-steps	
    size_t          _steps_stablized;    // # of steps before actual simulation
    SimPara         _para;
    
    //vector<size_t>  _last_consistent;   // last 100% consistency timestep    
	map<id_t, size_t> _last_consistent_map; // last 100% consistency timestep    
    
    FILE *_fp;                          // file pointer for output

    // internal record keeping structs
    vector<SimNode *> _simnodes;

    // 
    // statistics record
    //

    // recording consistency
    size_t          _total_AN_actual, _total_AN_visible;
    size_t          _AN_actual_accumulated, _AN_visible_accumulated;
    size_t          _AN_actual_first_interval, _AN_visible_first_interval;

    // AN
    size_t          _max_AN;
    
    // drift distance
    size_t          _max_drift, _total_drift, _drift_nodes;

    // recovery (back to 100% consistency for a particular node)
    size_t          _max_RS, _recovery_steps, _recovery_count, _inconsistent_count;

    vector<pair<size_t, Vast::id_t> *> _inconsistent_nodes;    // node ids
};


#endif // VASTSIM_STATISTICS_H





















