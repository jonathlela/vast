
/*
 *	SimGame.cpp  (Von-2 Simulation Simu Game _ main class implementation)
 *
 *
 *
 */

#include "vastatesim.h"
#include "vastutil.h"
#include <stdio.h>
#include <sstream>
#include <string>
#include <vector>

#include "vastory.h"
using namespace VAST;

#include "simgame.h"
#include "behavior.h"

#include <math.h>

extern struct system_state g_sys_state;

///////////////////////////////////////////////////////////////////////////////
//
//		Node
//
///////////////////////////////////////////////////////////////////////////////

char             simgame_node::_ostr [MAXBUFFER_SIZE];
errout           simgame_node::_eo;
AttributeBuilder simgame_node::_ab;

vector<object *> simgame_node::empty_o_vector;
map<id_t,object*> simgame_node::empty_io_map;

simgame_node::simgame_node (vastate * vasts_public, 
    int capacity, behavior *model, arbitrator_reg * ar_logic, vastverse * world, SimPara * para, bool is_gateway)
      : _active(true), _b_gateway (is_gateway), _vasts(NULL), _b_single_vastate(false),
        _peer(NULL), _peer_lg(NULL), _peer_capacity(capacity) , _arbitrator_reg (ar_logic), _info (NULL), _world(world), _model (model), _para (para)
{
    // gateway is ignored in VAST_NET_EMU
    if (vasts_public == NULL)
    {
	    Addr gateway;
        system_parameter_t sp;

        // setup system parameters for vastate creation
        sp.width = para->WORLD_WIDTH;
        sp.height = para->WORLD_HEIGHT;
        sp.aoi = para->AOI_RADIUS;

	    _vasts = _fac.create(world, gateway, sp);
        _b_single_vastate = false;
    }
    else
    {
        _vasts = vasts_public;
        _b_single_vastate = true;
    }
}


simgame_node::~simgame_node ()
{
    /*
    while (_ar_logic.size() > 0)
    {
	    delete _ar_logic.back();
	    _ar_logic.pop_back();
    }
    */
    _ar_logic.clear ();

    if (_peer_lg != NULL)
	    delete _peer_lg;

    if ((_vasts != NULL) && (_b_single_vastate == false))
	    _fac.destroy(_vasts);

    if (_info != NULL)
	    delete _info;
}

bool simgame_node::join ()
{
	if (_b_gateway)
	{
        vector<arbitrator_logic *> ar_l;
        vector<storage_logic *> st_l;

		int i;
		for (i = 0; i < _para->NUM_VIRTUAL_PEERS; i++)
        {
            simgame_arbitrator_logic *new_arb = new simgame_arbitrator_logic(_para, _arbitrator_reg, _model);
			_ar_logic.push_back (new_arb);
            ar_l.push_back ((arbitrator_logic *) new_arb);
            st_l.push_back ((storage_logic *) new_arb);
        }
    	
		_vasts->create_server (ar_l, st_l, _para->WORLD_WIDTH, _para->WORLD_HEIGHT, _para->NUM_VIRTUAL_PEERS);

        is_joined = true;
	}
	else
	{	
		Position p;
        Node node;
		_peer_lg = new simgame_peer_logic ();
		_model->InitRegister(_peer_lg->_behavior_r);
    	
		//p = _model->GetInitPosition();
        node.aoi = _para->AOI_RADIUS;
        node.pos = _model->GetInitPosition();

		_peer = _vasts->create_peer (_peer_lg, node, _peer_capacity);

        /*
		char auth[JOIN_REQUEST_RANDOM_CHAR_LENGTH+1];
        int i;
        // generate random visible character from '0' to 'z' (include some non-number or alphabet characters)
        // and '0' = 0x30 , 'z' = 0x7A
        for (i=0; i< JOIN_REQUEST_RANDOM_CHAR_LENGTH; i++)
            auth[i] = (rand() % (0x7A - 0x30 + 1)) + 0x30;

        _peer->join (p, _para->AOI_RADIUS, auth, JOIN_REQUEST_RANDOM_CHAR_LENGTH);
        */

        is_joined = true;
	}

    return true;
}

bool simgame_node::enable (bool state)
{
    // if has the same state as now, just return
    if (state == _active)
        return _active;

    bool o_state = _active;
    _active = state;

    if (_active == true)
    {
        if (_b_single_vastate == false)
        {
	        Addr gateway;
            system_parameter_t sp;

            sp.width = _para->WORLD_WIDTH;
            sp.height = _para->WORLD_HEIGHT;
            sp.aoi = _para->AOI_RADIUS;

	        _vasts = _fac.create(_world, gateway, sp);
        }
        join ();
    }
    else
    {
        if (_peer != NULL)
        {
            _peer->leave (0);
            delete _peer_lg;
        }

        vector<simgame_arbitrator_logic *>::iterator ait = _ar_logic.begin ();
        for (; ait != _ar_logic.end (); ait ++)
            delete (*ait);
        _ar_logic.clear ();

        _fac.destroy (_vasts);
        _vasts = NULL;
    }

    return o_state;
}

bool simgame_node::getEnable ()
{
    return _active;
}

//
//  Control AI methods
//
//	NextStep ()
//	decide and done player actions (with supporting of behavior)
//	action code meaning:
//		1:Move 2:Eat 3:Attack 4:Bomb 5:do nothing
int simgame_node::NextStep ()
{
	if ((_peer_lg == NULL) || (_peer_lg->get_self() == NULL)) return 0;

	peer_object_store object_store;
	object_store.player =   _peer_lg->get_self();
	object_store.objs   = & _peer_lg->_objects;

	//int action = _model->NextStep (_peer_lg->_b_reg, *(_peer_lg->get_self()), _peer_lg->_objects);
	int action        = _model->NextStep(_peer_lg->_behavior_r, object_store);
	b_target & target = _model->NextTarget();
	char ostr[256];
	ostr[0] = '\0';

	switch (action)
	{
	case 1:
		//Move (_model.NextTarget ());
		Move (target.center);
#ifdef VASTATESIM_DEBUG
		sprintf (ostr, "[%d] peer_logic: Made a MOVE to (%d,%d)" NEWLINE,
			_peer_lg->get_self()->peer, (int)target.center.x, (int)target.center.y);
#endif
		break;
	case 2:
		//Eat (_model.NextTarget ());
		Eat (target.object_id);
#ifdef VASTATESIM_DEBUG
		sprintf (ostr, "[%d] made a EAT on [%d]" NEWLINE,
			_peer_lg->get_self()->peer, target.object_id);
#endif
		break;
	case 3:
		//Attack (_model.NextTarget ());
		Attack (target.object_id);
#ifdef VASTATESIM_DEBUG
		sprintf (ostr, "[%d] made a ATTACK to [%d]" NEWLINE,
			_peer_lg->get_self()->peer, target.object_id);
#endif
		break;
	case 4:
		//Bomb (_model.NextTarget ());
		Bomb (target.center, target.radius);
#ifdef VASTATESIM_DEBUG
		sprintf (ostr, "[%d] made a BOMB on (%d,%d) r (%d)" NEWLINE,
			_peer_lg->get_self()->peer, (int)target.center.x, (int)target.center.y, (int)target.radius);
#endif
		break;
	case 5:
		// do nothing
#ifdef VASTATESIM_DEBUG
		sprintf (ostr, "[%d] made a NOTHING" NEWLINE,
			_peer_lg->get_self()->peer);
#endif
		break;
#ifdef VASTATESIM_DEBUG
    default:
		sprintf (ostr, "[%d] peer_logic: Behavior returns unknown action %d.\n"
				, _peer_lg->get_self()->get_id(), action);
#endif
	}

#ifdef VASTATESIM_DEBUG
	_eo.output(ostr);
#endif
	return 0;
}


void simgame_node::processmsg ()
{
	if (_vasts == NULL)
        return ;

    // process vastate messages
    _vasts->process_msg ();

    multimap<int, Node>& requests = _vasts->get_requests ();
    for (multimap<int, Node>::iterator it = requests.begin (); it != requests.end (); it ++)
    {
        int promotion = it->first;
        Node n = it->second;
        // if promote a new node
        if (promotion == 1)
        {
            // new a simgame_arbitrator_logic
            simgame_arbitrator_logic *new_arb = new simgame_arbitrator_logic(_para, _arbitrator_reg, _model);

            // push into _ar_logic
			_ar_logic.push_back (new_arb);

            // create arbitrator
            id_t peer_id;
            if (is_gateway ())
                peer_id = NET_ID_GATEWAY;
            else if (_peer_lg != NULL && _peer_lg->get_self () != NULL)
                peer_id = _peer_lg->get_self ()->peer;
            else
                _eo.output ("Internal error!");
            _vasts->create_arbitrator (peer_id, (arbitrator_logic *)new_arb, (storage_logic *)new_arb, n);

            g_sys_state.promote_count ++;
        }

        // if demote a exist node
        else if (promotion == 2)
        {
            bool demotion_succ = false;
            arbitrator_info arb_info;

            for (vector<simgame_arbitrator_logic *>::iterator it = _ar_logic.begin (); it != _ar_logic.end (); it ++)
            {
                simgame_arbitrator_logic * logic = *it;
                logic->getArbitratorInfo (&arb_info);
                if (arb_info.id == n.id)
                {
                    demotion_succ = true;
                    g_sys_state.demote_count ++;

                    _vasts->destroy_arbitrator (n.id);
                    _ar_logic.erase (it);
                    delete logic;
                    break;
                }
            }

            if (!demotion_succ)
            {
                id_t my_id;
                if (_peer_lg != NULL)
                    my_id = _peer_lg->get_self ()->peer;
                else if (_ar_logic.size () > 0)
                {
                    arbitrator_info arb_info;
                    _ar_logic[0]->getArbitratorInfo (&arb_info);
                    my_id = arb_info.id;
                }
                else
                    my_id = -1;

                sprintf (_ostr, "[%d] receives a unknown demotion to node.\r\n", my_id, n.id);
                _eo.output (_ostr);
            }
        }

        else
        {
            id_t my_id;
            if (_peer_lg != NULL)
                my_id = _peer_lg->get_self ()->peer;
            else if (_ar_logic.size () > 0)
            {
                arbitrator_info arb_info;
                _ar_logic[0]->getArbitratorInfo (&arb_info);
                my_id = arb_info.id;
            }
            else
                my_id = -1;

            sprintf (_ostr, "[%d] receives a unknown promotion request (%d).\r\n", my_id, promotion);
            _eo.output (_ostr);
        }
    }

    // clean up requests
    _vasts->clean_requests ();


    // update arbitrator image
    vector<simgame_arbitrator_logic *>::iterator ait = _ar_logic.begin ();
    for (; ait != _ar_logic.end (); ait ++)
    {
        simgame_arbitrator_logic * arb = *ait;
        arbitrator_info arb_info;
        arb->getArbitratorInfo (&arb_info);
        _arbitrator_reg->update_arbitrator (arb_info);
    }

	//int arbs = _vasts->process_msg ();
    /*
	if (arbs > 0)
	{
		for (int i=0; i<arbs; i++)
		{
			simgame_arbitrator_logic * new_arb = new simgame_arbitrator_logic (_para, _arbitrator_reg, _model);
            _ar_logic.push_back (new_arb);

			_vasts->create_arbitrator (new_arb, (storage_logic *) new_arb, NULL);

#ifdef VASTATESIM_DEBUG
            errout o;
	        char s[256];
			sprintf (s, "Peer%d: New arbitrator created" NEWLINE, GetSelfObject ()->peer);
			o.output(s);
#endif
		}
	}
    */
}

bool simgame_node::is_gateway ()
{
    return _b_gateway;
}

bool simgame_node::isArbitrator ()
{
	return (_ar_logic.size() != 0);
}

int simgame_node::getArbitratorInfo (arbitrator_info * arb_info)
{
	if (arb_info != NULL)
	{
		int tarb = _ar_logic.size();
		for (int i=0; i<tarb; i++)
		{
			simgame_arbitrator_logic& alogic = *(simgame_arbitrator_logic *)_ar_logic[i];
 			alogic.getArbitratorInfo(arb_info + i); 
		}
	}

	return _ar_logic.size();
}

const char * simgame_node::getArbitratorString (int index)
{
	if (index >= (int)_ar_logic.size())
		return NULL;

	return ((simgame_arbitrator_logic *)_ar_logic[index])->toString();
}

bool simgame_node::getPlayerInfo (player_info * p)
{
	if (_peer_lg != NULL)
		return _peer_lg->getPlayerInfo(p);
	else
		return false;
}

int simgame_node::getAOI ()
{
	if (_peer_lg == NULL) return 0;
	return _peer_lg->getAOI();
}


int simgame_node::getArbAOI (int index)
{
	if (index >= (int)_ar_logic.size())
		return 0;
	return ((simgame_arbitrator_logic*)_ar_logic[index])->getAOI();
}


void simgame_node::Move (Position& dest)
{
	event *e = _peer->create_event ();
	e->type = SimGame::E_MOVE;
	e->add ((int)_peer_lg->get_self()->get_id());
	e->add ((int)dest.x);
	e->add ((int)dest.y);
	
	_peer->send_event (e);
}

void simgame_node::Eat (id_t foodid)
{
	event *e = _peer->create_event ();
	e->type = SimGame::E_EAT;
	e->add ((int)_peer_lg->get_self()->get_id());
	e->add ((int)foodid);
	
	_peer->send_event (e);
}

void simgame_node::Attack (id_t target)
{
	event *e = _peer->create_event ();
	e->type = SimGame::E_ATTACK;
	e->add ((int)_peer_lg->get_self()->get_id());
	e->add ((int)target);
	
	_peer->send_event (e);
}

void simgame_node::Bomb (Position& center, int radius)
{
	event *e = _peer->create_event ();
	e->type = SimGame::E_BOMB;
	e->add ((int)_peer_lg->get_self()->get_id());
	e->add ((int)center.x);
	e->add ((int)center.y);
	e->add (radius);
	
	_peer->send_event (e);
}

pair<unsigned int,unsigned int> simgame_node::getArbitratorAccmulatedTransmit (int index)
{
    /*
    if ((unsigned) index < _ar_logic.size ())
    {
        network * net = ((simgame_arbitrator_logic *)_ar_logic[index])->get_network ();
        return pair<unsigned int,unsigned int>(net->sendsize (), net->recvsize ());
    }
    */

    return pair<unsigned int,unsigned int>(0, 0);
}

pair<unsigned int,unsigned int> simgame_node::getAccmulatedTransmit ()
{
    /*
	//return pair<int,int>(_vasts->get);
    if (_peer_lg != NULL)
    {
        network * net = _peer_lg->get_network ();
        return pair<unsigned int,unsigned int>(net->sendsize (),net->recvsize ());
    }
    */
    return pair<unsigned int,unsigned int>(0,0);
}

string simgame_node::PlayerObjtoString (object * obj)
{
	std::ostringstream sr;
	sr  << "  [Peer" << obj->peer << "]"
		<< _ab.objectIDToString (obj->get_id())
		<< "(" << obj->get_pos().x << "," << obj->get_pos().y << ")"
		<< "<v" << obj->pos_version << ">"
		<< "  "
		<< _ab.getPlayerHP(*obj) << "/" << _ab.getPlayerMaxHP(*obj)
		<< "<v" << obj->version << ">";
	sr  << endl;
	return sr.str();
}

const char * simgame_node::toString() 
{
	if (_info == NULL)
		_info = new string();

	//string& sr = *_info;
    
	std::ostringstream sr;

	if ((_peer_lg != NULL) && (_peer_lg->get_self() != NULL))
	{
		object * p = _peer_lg->get_self();

		sr << "Object ID: ";
        sr << _ab.objectIDToString (p->get_id()) << endl;
		//sr << "" NEWLINE;
		if (p->peer != 0)
		{
			sr << "Peer ID: " << p->peer << endl;

			sr << endl;

			sr << "Behavior Reister: " << endl;
			sr << "[" << _peer_lg->_behavior_r.last_action[0] << _peer_lg->_behavior_r.last_action[1] << "] "
				<< "dest : (" << _peer_lg->_behavior_r.dest.x << "," << _peer_lg->_behavior_r.dest.y << ")"
				<< " Waiting " << _peer_lg->_behavior_r.waiting << endl
				<< "angst: " << _peer_lg->_behavior_r.angst << " T " << _peer_lg->_behavior_r.target << endl
				<< "tire : " << _peer_lg->_behavior_r.tiredness << " A " << _peer_lg->_behavior_r.following_attractor << endl;

			sr << endl;

			sr << "Player Info: " << endl;
			sr << "  Pos: (" << p->get_pos().x << "," << p->get_pos().y << ")" 
					<< " <version: " << p->pos_version << ">" << endl;
			sr << "  HP: " << _ab.getPlayerHP(*p) << endl
			   << "  MaxHP: " << _ab.getPlayerMaxHP(*p) << endl
			   << "  <version: " << p->version << ">" << endl;

			sr << endl;

			sr << "Known Objects: " << endl;
			int to = _peer_lg->_objects.size();
            map<id_t, object*> prs, os;

			for (int i=0; i<to; i++)
			{
				object * this_object = _peer_lg->_objects[i];
                if (this_object->peer == 0)
                    os[this_object->get_id ()] = this_object;
                else
                    prs[this_object->peer] = this_object;
				//Position& this_object_pos = this_object->get_pos();
				/*
				sr << "  [Peer" << this_object->peer << "]"
				   << "[" << this_object->get_id() << "]"
				   << "(" << this_object_pos.x << "," << this_object_pos.y << ")  "
				   << ab.getPlayerHP(*this_object) << "/" << ab.getPlayerMaxHP(*this_object);
				sr << endl;
				*/
				//sr << PlayerObjtoString (this_object);
				//sr << "  " << ab.objectToString(*this_object) << endl;
			}
            map<id_t, object*>::iterator it1 = prs.begin ();
            for (; it1 != prs.end (); it1 ++)
                sr << "  " << _ab.objectToString(*(it1->second)) << endl;
            map<id_t, object*>::iterator it2 = os.begin ();
            for (; it2 != os.end (); it2 ++)
                sr << "  " << _ab.objectToString(*(it2->second)) << endl;

			sr << endl;

			sr << _peer_lg->_peer->to_string();
		}
	}
	else
	{
		sr << "No Information.";
	}

	*_info = sr.str ();
	return _info->c_str();
}

