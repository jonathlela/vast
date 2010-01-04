/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2009 Shun-Yun Hu  (syhu@yahoo.com)
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
 *  Topology.h  
 *
 *  locate my physical coordinate on a network topology through landmark nodes
 *
 *  history 2009/04/17      starts 1st implementation
 *
 */



#ifndef _VAST_Topology_H
#define _VAST_Topology_H

#include "Config.h"
#include "VASTTypes.h"
#include "MessageHandler.h"
#include "Vivaldi.h"

// number of ticks before a new round of queries is sent for neighbors' coordinates
#define COORD_QUERY_RETRY_COUNTDOWN        5

#define TOPOLOGY_CONSTANT_ERROR      (0.1f)     
#define TOPOLOGY_CONSTANT_FRACTION   (0.1f)      
#define TOPOLOGY_TOLERANCE           (0.5f)     // how much local error is considered okay
#define TOPOLOGY_DEFAULT_ERROR       (1.0f)     // default value for local error


using namespace std;

namespace Vast
{

    typedef enum 
    {
        LOCATE = VON_MAX_MSG,   // try to obtain a position    
        COORDINATE,             // return an identified location
        REGISTER,               // register myself as a landmark
        LANDMARK,               // list of known landmarks
        BADLANDMARK,            // invalid landmark reporting
        PING,                   // query to measure latency
        PONG,                   // reponse to PING
        PONG_2                  // reponse to PONG

    } TOPOLOGY_Message;

    class EXPORT Topology : public MessageHandler
    {

    public:

        // constructor for physical topology class, 
        // may supply an artifical physical coordinate
        Topology (id_t host_id, Position *phys_coord = NULL);

        ~Topology ();
        
        // obtain my physical coordinate, returns NULL if not yet obtained
        Position*   getPhysicalCoordinate ();

        // send a message to a remote host in order to obtain round-trip time
        bool ping (id_t target);

        // response to a PING message
        bool pong (id_t target, timestamp_t querytime, bool first = false);

        // get IP address of the host
        //Addr&       getAddress ();

		// set the Vivaldi 
		//Vivaldi * getVivaldi();
		//void setVivaldi(Vivaldi * val);

    private:

        // perform initialization tasks for this handler (optional)
        // NOTE that all internal variables (such as handler_no) have been set at this point
        void initHandler ();

        // returns whether the message was successfully handled
        bool handleMessage (Message &in_msg);

        // performs tasks after all messages are handled
        void postHandling ();

        //
        //  process functions
        //

        // re-calculate my coordinate, 
        // given a neighbor j's position xj & error estimate ej
        void vivaldi (float rtt, Position &xi, Position &xj, float &ei, float &ej);

        //
        //  private variables
        //
        id_t        _host_id;        
        Position   *_phys_coord;        // default physical coordinate
        Position    _temp_coord;        // still in-progress coorindate
        float       _error;             // error estimate for the local node
		//Vivaldi	   *_vivaldi;		
        int         _counter;           // countdown counter to send query
        int         _request_times;   

        map<id_t, Node> _neighbors;     // list of contact neighbors
        map<id_t, bool> _pending;       // list of pending PING requests sent

        //map<id_t, timestamp_t>  _latencies;     // latency look-up table    
        //map<id_t, Position>     _coords;        // physical coordinates of known neighbors 
	};

} // namespace Vast

#endif
