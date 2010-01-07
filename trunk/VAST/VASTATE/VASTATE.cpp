/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007 Shun-Yun Hu (syhu@yahoo.com)
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


#include "VASTATE.h"
#include "ArbitratorImpl.h"
#include "AgentImpl.h"
//#include "StorageCS.h"

namespace Vast
{
    VASTATE::VASTATE (VASTATEPara &para, VASTPara_Net &netpara, VASTPara_Sim *simpara)
        : _para (para), _netpara (netpara), _simpara (NULL)
    {
        _state = ABSENT;

        // store parameters for VASTVerse
        if (simpara != NULL)
            _simpara = new VASTPara_Sim (*simpara);

        printf ("VASTATE init... binding port: %d\n", _netpara.port);
    }

    VASTATE::~VASTATE () 
    {
        if (_simpara != NULL)
            delete _simpara;

        // de-allocate
        size_t i;
        for (i=0; i < _arbitrators.size (); i++)
            delete _arbitrators[i];
        for (i=0; i < _agents.size (); i++)
            delete _agents[i];
        for (map<void *, VASTVerse *>::iterator it = _vastverses.begin (); 
             it != _vastverses.end (); it++)
            delete it->second;
    }  

    // create a VASTATE node (includes both an arbitrator and agent component)
    bool 
    VASTATE::createNode (Area &aoi, ArbitratorLogic *arb_logic, AgentLogic *agent_logic, std::string &password, Position *arb_pos)
    {
        // we currently only support one VASTATE node 
        // TODO: more arbitrators / agents at the same host?
        if (_arbitrators.size () > 0 || _agents.size () > 0)
        {
            printf ("VASTATE::createNode () VASTATE node already created\n");
            return false;
        }

        // store initial relay join position, if any
        if (arb_pos != NULL)
            _arb_position = *arb_pos;
        else if (_netpara.is_gateway)
        {
            printf ("VASTATE::createNode () warning: VASTATE node created as gateway but no initial position is supplied for arbitrator\n");
            return false;
        }

        // store references to logics & login password
        if (arb_logic != NULL)
            _arb_logics.push_back (arb_logic);
        if (agent_logic != NULL)
            _agent_logics.push_back (agent_logic);

        _agent_aoi = aoi;
        _pass = password;

        // we perform actual join in isLogin () as may require multiple calls 
        return true;
    }

    // destroy the current VASTATE node, reverse the createNode actions
    void 
    VASTATE::destroyNode ()
    {
        if (_state != ABSENT)
        {
            // destroy agents first, as arbitrators may need to handle certain closeoff tasks
            if (_agents.size () > 0)
                destroyAgent (_agents[0]);
        
            if (_arbitrators.size () > 0)
                destroyArbitrator (_arbitrators[0]);
        
            _arb_logics.clear ();
            _agent_logics.clear ();
            _pass.clear ();
            
            _state = ABSENT;
        }
    }

    // whether the VASTATE node has been successfully created
    bool 
    VASTATE::isLogined ()
    {
        // do not login if references to logics do not exist 
        // (that is, createNode () must be called first)
        if (_arb_logics.size () == 0 && _agent_logics.size () == 0)
            return false;
        else if (_state == JOINED)
            return true;

        // create arbitrator & agent
        if (_state == ABSENT)
        {
            // create arbitrator first (to handle incoming LOGIN request from agents)
            // but only if there are valid arbitrator logic specified
            if (_arbitrators.size () < _arb_logics.size ())
                createArbitrator (_arb_logics[0]);
        
            // we create agent AFTER arbitrator is created first
            else if (_agents.size () < _agent_logics.size ())                
            {                       
                Agent *agent = createAgent (_agent_logics[0]);
        
                if (agent != NULL)
                {   
                    // update my peer ID    
                    // TODO: make sure agent logic does it ?
                    //g_self->setSelf (g_agent->getSelf ());
        
                    agent->login (NULL, _pass.c_str (), _pass.length ());
                }
            }
        
            // if both arbitrator & agent are created successfully, move to next stage (wait for join success)
            // NOTE: arbitrator may not be created if public IP is not available at this host
            if (_arbitrators.size () == _arb_logics.size () &&
                _agents.size () == _agent_logics.size ())
                _state = JOINING;
        }

        // check if arbitrator has properly joined the network
        else if (_state == JOINING)
        {
            // see if arbitrator join location is specified
            // NOTE: Position is initialized at the origin (x = 0, y = 0), 
            bool init_pos = !(_arb_position.x == 0 && _arb_position.y == 0);
        
            // if no arbitrator logic or initial position specified, ignore arbitrator join
            // otherwise, check if arbitrator join is already completed
            if (_arb_logics.size () == 0 || 
                init_pos == false || 
                _arbitrators[0]->isJoined ())
                _state = JOINING_2;
            else                 
                _arbitrators[0]->join (_arb_position);
        }
        // check if agent has properly joined the network
        else if (_state == JOINING_2)
        {
            // if no agent logic specified or if join is already completed, 
            // then join is considered done
            if (_agent_logics.size () == 0 ||
                _agents[0]->isJoined ())
                _state = JOINED;
        
            else if (_agents[0]->isAdmitted ())
            {
                // attempt to join
                // NOTE: only the first call to join () after admitted is valid, 
                // so it can be called repetitively
                _agents[0]->setAOI (_agent_aoi.radius);    
                _agents[0]->join (_agent_aoi.center);
            }                                            
        }

        return (_state == JOINED);
    }
    
    // move logical clock forward
    int
    VASTATE::tick (int time_budget)
    {     
        // register with TimeMonitor to notify how much time is left in this tick
        TimeMonitor::instance ()->setBudget (time_budget);

        // tick all vastverses
        for (map<void *, VASTVerse *>::iterator it = _vastverses.begin (); 
             it != _vastverses.end (); it++)
             it->second->tick ();

        return TimeMonitor::instance ()->available ();
    }

    // stop operations on this factory
    void 
    VASTATE::pause ()
    {
        size_t i;

        // make sure the vnodes are departed
        for (i = 0; i < _agents.size (); i++)
        {
            _agents[i]->leave ();         
            _agents[i]->getVAST ()->leave ();
        }

        for (i = 0; i < _arbitrators.size (); i++)
        {
            _arbitrators[i]->leave ();
            _arbitrators[i]->getVAST ()->leave ();
        }
       
        // should flush and then stop the network
        map<void *, VASTVerse *>::iterator it = _vastverses.begin ();
        for (; it != _vastverses.end (); it++)
            it->second->pause ();

    }

    // resume operations on this factory
    void 
    VASTATE::resume ()
    {
        // should resume the network
        // should flush and then stop the network
        map<void *, VASTVerse *>::iterator it = _vastverses.begin ();
        for (; it != _vastverses.end (); it++)
            it->second->resume ();
    }

    // obtain the point to the created agent
    // returns NULL if not yet created
    Agent *
    VASTATE::getAgent ()
    {
        if (_state != JOINED || _agent_logics.size () == 0)
            return NULL;

        return _agents[0];
    }

    // obtain the reference to the created arbitrator
    // returns NULL if not yet created
    Arbitrator *
    VASTATE::getArbitrator ()
    {
        if (_state != JOINED || _arb_logics.size () == 0)
            return NULL;

        return _arbitrators[0];
    }

    size_t 
    VASTATE::getSendSize (const msgtype_t msgtype)
    {
        size_t total = 0;

        // collect send transmission over all created VASTVerses
        for (map<void *, VASTVerse *>::iterator it = _vastverses.begin (); it != _vastverses.end (); it++)
            total += it->second->getSendSize (msgtype);

        return total;
    }
        
    size_t 
    VASTATE::getReceiveSize (const msgtype_t msgtype)
    {
        size_t total = 0;

        // collect send transmission over all created VASTVerses
        for (map<void *, VASTVerse *>::iterator it = _vastverses.begin (); it != _vastverses.end (); it++)
            total += it->second->getReceiveSize (msgtype);

        return total;
    }

    // TODO: should check for redundent creation of same logic
    Agent *
    VASTATE::createAgent (AgentLogic *logic)
    {
        // if the VASTverse is not yet joined, then simply return
        VASTVerse *vastverse = createVASTVerse (logic);
        VAST *vnode = vastverse->createClient (); 
        if (vnode == NULL)
            return NULL;

        Agent *agent = new AgentImpl (logic, vnode);
        _agents.push_back (agent);

        // join the VAST network for SPS functions
        Topology *topology = vastverse->getTopology ();

        // we assume all agents join as "clients" on VAST network
        vnode->join (*topology->getPhysicalCoordinate (), false);

        // make sure other nodes on this host are aware of me
        recordLocalTarget (vnode->getSelf ()->id);

        return agent;
    }

    // close down an Agent
    void
    VASTATE::destroyAgent (Agent *agent)
    {
        void *logic = agent->getLogic ();
        VAST *vnode = agent->getVAST ();

        // locate the VASTVerse object
        if (_vastverses.find (logic) == _vastverses.end ())
            return;

        VASTVerse *vastverse = _vastverses[logic];
        
        // find the particular Agent
        for (size_t i=0; i<_agents.size (); ++i)
        {
            if (_agents[i] == agent)
            {
                _agents.erase (_agents.begin () + i);
                delete agent;
                            
                // remove the agent's VASTVerse
                vastverse->destroyClient (vnode);
                destroyVASTVerse (vastverse);

                return;
            }
        }
    }

    Arbitrator *
    VASTATE::createArbitrator (ArbitratorLogic *alogic)
    {           
        // if VASTverse is not yet joined, then simply return
        VASTVerse *vastverse = createVASTVerse (alogic);
        VAST *vnode = vastverse->createClient (); 
                
        if (vnode == NULL)
            return NULL;
        
        // TODO: should not allow this arbitrator be created, if public IP is not available
        Arbitrator *arbitrator = new ArbitratorImpl (alogic, vnode, _para);
        _arbitrators.push_back (arbitrator);

        // join the VAST network for SPS functions
        Topology *topology = vastverse->getTopology ();

        // we assume all arbitrators join as "relays" on VAST network
        vnode->join (*topology->getPhysicalCoordinate (), true);

        // make sure other nodes on this host are aware of me
        recordLocalTarget (vnode->getSelf ()->id);

        return arbitrator;
    }

    // close down an arbitrator
    void 
    VASTATE::destroyArbitrator (Arbitrator *arbitrator)
    {
        void *logic = arbitrator->getLogic ();
        VAST *vnode = arbitrator->getVAST ();

        // locate the VASTVerse object
        if (_vastverses.find (logic) == _vastverses.end ())
            return;

        VASTVerse *vastverse = _vastverses[logic];

        // find the particular arbitrator
        for (size_t i=0; i<_arbitrators.size (); ++i)
        {
            if (_arbitrators[i] == arbitrator)
            {
                _arbitrators.erase (_arbitrators.begin () + i);
                delete arbitrator;
                
                // remove the arbitrator's VASTVerse
                vastverse->destroyClient (vnode);
                destroyVASTVerse (vastverse);
            
                return;
            }
        }
    }

    // create an instance of VASTVerse for use by arbitrator or agent 
    VASTVerse *
    VASTATE::createVASTVerse (void *logic)
    {
        VASTVerse *vastverse;

        if (_vastverses.find (logic) != _vastverses.end ())
            vastverse = _vastverses[logic];
        else
        {
            vastverse = new VASTVerse (&_netpara, _simpara);

            // only the first VASTverse instance is gateway (even on the gateway host)
            _netpara.is_gateway = false;

            _vastverses[logic] = vastverse;
        }

        return vastverse;
    }

    void 
    VASTATE::destroyVASTVerse (VASTVerse *vastverse)
    {
        map<void *, VASTVerse *>::iterator it = _vastverses.begin ();

        for (; it != _vastverses.end (); it++)
            if (it->second == vastverse)
            {
                delete it->second;
                _vastverses.erase (it);
                break;
            }
    }

    // notify each created VASTverse of a node belonging to the same host
    void 
    VASTATE::recordLocalTarget (id_t target)
    {
        map<void *, VASTVerse *>::iterator it = _vastverses.begin ();

        for (; it != _vastverses.end (); it++)
            it->second->recordLocalTarget (target);
    }

} // namespace Vast

