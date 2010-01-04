/*
 *  
 */

#include "vast_mc.h"
#include "vor_SF.h"
#include <time.h>

//#ifndef DEBUG_DETAIL
//#define DEBUG_DETAIL

namespace VAST
{
	// all of the message types in Non-Redundant model
	char vast_mc_MESSAGE[][20] =
	{
		"QUERY",		// To find initial acceptor
		"INITLIST",		// To get joined initial neighbor list
		"REQNBR",		// when join success, we notify EN our existance
		"ACKNBR",		// send back to request node a neighbor list
		"REQADDR",
		"ACKADDR",
		"HELLO",		// To initiate connection with new neighbor
		"EXCHANGE",		// To exchange 2-hop neighbor lists
		"MOVE",			// Position updates to AOI neighbors
		"RELAY",		// To help for relay other's position update
		"UNKNOWN",
	};

	// constructor
	vast_mc::vast_mc (network *netlayer, aoi_t detect_buffer)
		:_detection_buffer (detect_buffer)
	{
		this->setnet (netlayer);
		_net->start ();
		_voronoi = new vor_SF ();

		// to count the total neighbors in AOI for calculating the density
		_total_neighbor = 0;

		// THE EYE OF GOD
		_NM_total = _NM_known = 0;
        // init counter for messages received (for simulation)
        for (int i=0; i<VAST_MSG_SIZE; i++)
            _msg_stat.push_back (0);

//#ifdef DEBUG_DETAIL
//		printf ("NODE id: %d construct success! POS: %d, %d \n", (int)_self.id, (int)_self.pos.x, (int)_self.pos.y);
//#endif
	}

	// destructor cleans up any previous allocation
	vast_mc::~vast_mc ()
	{
		if (is_joined () == true)
			this->leave ();

		delete _voronoi;
		_net->stop ();

//#ifdef DEBUD_DETAIL
//		printf ("NODE id: %d deconstruct success! POS: %d, %d \n", _self.id, (int)_self.pos.x, (int)_self.pos.y);
//#endif

	}

	// join VON to obtain unique id
	// NOTE: join is considered complete ('_joined' is set) only after node id is obtained
	bool vast_mc::join (id_t id, aoi_t AOI, Position &pos, Addr &gateway)
	{
#ifdef DEBUG_DETAIL            
        printf ("[%d] attempt to join at (%d, %d)\n", (int)id, (int)pos.x, (int)pos.y);
#endif
		if (is_joined () == true)
			return false;

		// setup self information
		_self.id = id;
		_self.aoi = AOI;
		_self.pos = pos;

		_nbr_change = true;
		_addr_index = 0;
		_exponent = 0;
		_f_count = 0;
		_last_f_count = -1;
		_now_hop = 0;
		_min_hop = _max_hop = 2;

		// this should be done in vastid::hanlemsg while receiving an ID from gateway
		_net->register_id (id);

		insert_node (_self); //, _net->getaddr (_self.id));

		// the first node is automatically considered joined
		if (id == NET_ID_GATEWAY)
			_joined = S_JOINED;
		else
		{
			// send query to find acceptor if I'm a regular peer
			Msg_Query info (_self, _net->getaddr (_self.id));
			if (_net->connect (gateway) == (-1))
				return false;

			// NOTE: gateway will disconnect me immediately after receving	QUERY
			_net->sendmsg (NET_ID_GATEWAY, MC_QUERY, (char *)&info, sizeof (Msg_Query), true, true);
		}


		return true;
	}

	// quit VON
	void vast_mc::leave ()
	{
		// remove & disconnect all connected nodes (include myself)
		vector<id_t> remove_list;
		map<id_t, Node>::iterator it = _id2node.begin ();
		for (; it != _id2node.end (); it++)
			if (it->first != _self.id)
				remove_list.push_back (it->first);

		int size = remove_list.size ();
		for (int i = 0; i < size; ++i)
			delete_node (remove_list[i]);

		_joined = S_INIT;
	}

	//
	// Node related functions
	//
	void vast_mc::setAOI (aoi_t radius)
	{
		_self.aoi = radius;
	}

	aoi_t vast_mc::getAOI ()
	{
		return _self.aoi;
	}

	char * vast_mc::getstat (bool clear)
	{
		static char str[VAST_BUFSIZ];
		char buf[80];
		str[0] = 0;

		for (int i = 0; i < 12; i++)
        {
            sprintf (buf, "%8d ", _msg_stat[i]);
            strcat (str, buf);
        }

		if (_NM_total != 0)
		{
			// print out NODE message ratio
			sprintf (buf, " NM ratio: %6d/%6d = %f", _NM_known, _NM_total, (float)_NM_known/(float)_NM_total);
			strcat (str, buf);
		}

		return str;
	}

	// move to a new position, returns actual position
	Position & vast_mc::setpos (Position &pos)
	{
		// do not move if we have not joined
		// (no neighbors are known unless I'm gateway)
		if (is_joined () == true)
		{

#ifdef DEBUG_DETAIL
			printf ("Node [%d] into setpos process. Current position is (%d, %d)\n", (int)_self.id, (int)_self.pos.x, (int)_self.pos.y);
#endif

			// update location information
			_self.pos = pos;
			_self.time = _net->get_curr_timestamp ();
			// redraw the voronoi diagram
			update_node (_self);

#ifdef DEBUG_DETAIL
			printf ("Node [%d] set position to (%d , %d)\n", (int)_self.id, (int)_self.pos.x, (int)_self.pos.y);
#endif

			// notify all connected neighbor
			vector<id_t> mv_ex_list;
			mv_ex_list = _voronoi->get_en (_self.id, 1);
			exchange_ENs (mv_ex_list);
			// policy
			MC_Node pos_update (_self, _now_hop, 0);
			for (int i = 0; i < (int)mv_ex_list.size (); ++i)
			{
				_net->sendmsg (mv_ex_list[i], MC_RELAY, (char *)&pos_update, sizeof (MC_Node), true, true);
			}
		}
		return _self.pos;
	}

	// process a single message in queue
	bool vast_mc::handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size)
	{
		// clear previous storage
		_contentstore.clear ();
		_msgstore.clear();

		if (msgtype == 0)
			_msg_stat[(int)msgtype] += size;
		else if (msgtype >= 30 && msgtype <= 40)
			_msg_stat[(int)msgtype - 29] += size;

		switch ((VAST_MC_Message)msgtype)
		{
		case MC_QUERY:
			if (size == sizeof (Msg_Query))
			{

#ifdef DEBUG_DETAIL
			printf ("Node [%d] get QUERY from [%d] \n", (int)_self.id, (int)from_id);
#endif
				// create a Msg_Query n with given msg
				Msg_Query n (msg);
				// let this msgnode be a new joiner
				Node &joiner = n.node;
				vector<id_t> list;
				int i, nbr_size;
				id_t tmp_id;

				_id2addr[n.node.id] = n.addr;

				// to prevent gateway server from over-connction
				if (_self.id == NET_ID_GATEWAY)
					_net->disconnect (from_id);

				if (_voronoi->contains (_self.id, joiner.pos))
				{
					// I'm the acceptor
					insert_node (joiner, n.addr);
					vector<id_t> th_list = TwoHopNbr_maintain ();
					// add all relevent nodes in the initial list
					map<id_t, Node>::iterator it;
					for (it = _id2node.begin (); it != _id2node.end (); it++)
					{
						tmp_id = it->first;
						if (tmp_id == joiner.id)
							continue;

						if (_voronoi->overlaps (tmp_id, joiner.pos, joiner.aoi) ||
							is_inlist (tmp_id, th_list))
							list.push_back (tmp_id);
					}
					
					// send INITLIST to joiner
					send_nodes (joiner.id, list, true);
				}
				else
				{
					// I'm not the acceptor
					// find the closest node from ENs to relay the QUERY
					nbr_size = _neighbors.size ();
					if (nbr_size != 0)
					{
						Node nbr_node;
						id_t target = _self.id;
						double dist = _self.pos.dist (joiner.pos);
						for (i = 0; i < nbr_size; i++)
						{
							nbr_node = *_neighbors[i];
							if (nbr_node.pos.dist (joiner.pos) < dist)
							{
								target = nbr_node.id;
								dist = nbr_node.pos.dist (joiner.pos);
							}
						}

						if (_net->is_connected (target) == false)
						{
                            // May not work, only fit to new connect model on the face
							//_net->connect (target, _net->getaddr (target));
                            _net->connect (_net->getaddr (target));
							_addition_conn.push_back (target);
						}
						_net->sendmsg (target, MC_QUERY, msg, size, true, true);

#ifdef DEBUG_DETAIL
						printf ("	Node [%d] relay QUERY to Node [%d] asked by Node [%d] \n", (int)_self.id, (int)target, (int)joiner.id);
#endif
					}
					else
					{
						printf ("\nNode [%d] has voronoi diagram ERROR\n", (int)_self.id);
					}
				}
			}
			break;

		case MC_INITLIST:
			// check the message size is legal/correct
			if ((size - 1) % sizeof (Msg_Node) == 0)
			{
#ifdef DEBUG_DETAIL
				printf ("Node [%d] got INITLIST from [%d]\n", (int)_self.id, (int)from_id);
#endif
				int n = (int)msg[0];
				char *p = msg + 1;
				Msg_Node newnode;

#ifdef DEBUG_DETAIL
			printf ("	The INITLIST is include");
#endif

				int i;
				for (i = 0; i < n; i++)
				{
					memcpy (&newnode, p, sizeof (Msg_Node));
					if (is_neighbor (newnode.node.id) == false)
						insert_node (newnode.node);//, newnode.addr);
					else
						update_node (newnode.node);

					p += sizeof (Msg_Node);
#ifdef DEBUG_DETAIL
					printf ("  [%d]", (int)newnode.node.id);
#endif

				}

#ifdef DEBUG_DETAIL
			printf ("\n	then prepare REQNBR (");
#endif

				// prepare send REQNBR to get more neighbors
				Msg_Node hello (_self); //, _net->getaddr (_self.id));
				vector<id_t> EN_list = _voronoi->get_en (_self.id, 1);
				int ENlist_size = EN_list.size ();
				int info_count = 0;
				char *q = _reqnbr_buf + 1;

				memcpy (q, &hello, sizeof (Msg_Node));
				q += sizeof (Msg_Node);
				info_count++;

				// add ENs to the list
				for (i = 0; i < ENlist_size; i++)
				{
					newnode.set (_id2node[EN_list[i]]); //, _net->getaddr (EN_list[i]));
					memcpy (q, &newnode, sizeof (Msg_Node));
					info_count++;
					q += sizeof (Msg_Node);
#ifdef DEBUG_DETAIL
					printf (" [%d]", (int)EN_list[i]);
#endif
					
				}
#ifdef DEBUG_DETAIL
			printf (") to");
#endif
				// send to ENs to notify them and do neighbor discovery
				_reqnbr_buf[0] = (unsigned char)info_count;
				for (i = 0; i < ENlist_size; i++)
				{
					if (EN_list[i] == from_id)
						continue;

					_net->sendmsg (EN_list[i], MC_REQNBR, (char *)&_reqnbr_buf, 1 + info_count * sizeof (Msg_Node), true, true);
					
#ifdef DEBUG_DETAIL
					printf ("  [%d]", (int)EN_list[i]);
#endif
				}

#ifdef DEBUG_DETAIL
				printf ("\n");
#endif
				// got initial list, join success
				_joined = S_JOINED;
			}
			break;

		case MC_REQNBR:
			if ((size - 1) % sizeof (Msg_Node) == 0)
			{
#ifdef DEBUG_DETAIL
				printf ("Node [%d] got REQNBR from Node [%d] (", (int)_self.id, (int)from_id);
#endif

				int i, info_count = 0;
				int n = (int)msg[0];
				char *p = msg + 1;
				//id_t tmp_id;

				vector<id_t> from_list;
				Msg_Node newnode;
				Msg_Node backnode;
				
				for (i = 0; i < n; ++i)
				{
					memcpy (&newnode, p, sizeof (Msg_Node));

#ifdef DEBUG_DETAIL
					printf (" [%d]", (int)newnode.node.id);
#endif

					from_list.push_back (newnode.node.id);
					if (is_neighbor (newnode.node.id))
						update_node (newnode.node);
					else
						insert_node (newnode.node); //, newnode.addr);

					p += sizeof (Msg_Node);
				}

#ifdef DEBUG_DETAIL
				printf (")\n	The ACKNBR content to Node [%d] is", (int)from_id);
#endif

				char *q = _acknbr_buf + 1;
				vector<id_t> compare_list = _voronoi->get_en (from_id);

				// compare from_list and compare_list to find out the lack nodes
				for (i = 0; i < (int)compare_list.size (); i++)
				{
					if (is_inlist (compare_list[i], from_list))
						continue;

					backnode.set (_id2node[compare_list[i]]); //, _net->getaddr (compare_list[i]));
					memcpy (q, &backnode, sizeof (Msg_Node));
					q += sizeof (Msg_Node);
					info_count++;
#ifdef DEBUG_DETAIL
					printf (" [%d]", (int)backnode.node.id);
#endif
				}
				_acknbr_buf[0] = (unsigned char)info_count;
				
				// send back Nbr list to new joiner
				if (info_count > 0)
				{
					_net->sendmsg (from_id, MC_ACKNBR, (char *)&_acknbr_buf, 1 + info_count * sizeof (Msg_Node), true, true);
				}

#ifdef DEBUG_DETAIL
				printf ("\n");
#endif
			}
			break;

		case MC_ACKNBR:
			if ((size - 1) % sizeof (Msg_Node) == 0)
			{
#ifdef DEBUG_DETAIL
				printf ("Node [%d] got ACKNBR from Node [%d]\n	The ACKNBR content is", (int)_self.id, (int)from_id);
#endif
				vector<id_t> before_en = _voronoi->get_en (_self.id, 1);
				vector<id_t> now_en;
				vector<id_t> new_en;
				int b_size = before_en.size ();
				int n_size = 0;
				int new_size = 0;
				int n = (int)msg[0];
				char *p = msg + 1;
				int i;
				Msg_Node newnode;

				// insert/update receive nodes info
				for (i = 0; i < n; i++, p += sizeof (Msg_Node))
				{
					memcpy (&newnode, p, sizeof (Msg_Node));
					if (is_neighbor (newnode.node.id))
						update_node (newnode.node);
					else
						insert_node (newnode.node); //, newnode.addr);
#ifdef DEBUG_DETAIL
					printf ("  [%d]", (int)newnode.node.id);
#endif
				}
#ifdef DEBUG_DETAIL
				printf ("\n	send REQNBR (");
#endif
				now_en = _voronoi->get_en (_self.id, 1);
				n_size = now_en.size ();
				int info_count = 0;
				// compare with before_en and now_en and count the difference
				if (b_size == n_size)
				{
					for (i = 0; i < n_size; i++)
					{
						if (is_inlist (now_en[i], before_en))
							continue;
						new_en.push_back (now_en[i]);
					}
				}
				else
				{
					for (i = 0; i < n_size; i++)
					{
						if (is_inlist (now_en[i], before_en))
							continue;
						new_en.push_back (now_en[i]);
					}
				}
				new_size = new_en.size ();

				if (new_size != 0)
				{
					char *q = _reqnbr_buf + 1;
					for (i = 0; i < new_size; i++)
					{
						newnode.set (_id2node[new_en[i]]); //, _net->getaddr (new_en[i]));
						memcpy (q, &newnode, sizeof (Msg_Node));
						q += sizeof (Msg_Node);
						info_count++;
#ifdef DEBUG_DETAIL
						printf (" [%d]", (int)newnode.node.id);
#endif
					}
					_reqnbr_buf[0] = (unsigned char)info_count;

					for (i = 0; i < new_size; i++)
					{
						_net->sendmsg (new_en[i], MC_REQNBR, (char *)&_reqnbr_buf, 1 + info_count * sizeof (Msg_Node), true, true);
					}
#ifdef DEBUG_DETAIL
						printf ("\n");
#endif
				}
			}
			break;

		case MC_REQADDR:
			if ((size - 1) % sizeof (id_t) == 0)
			{
				int i, info_count = 0;
				id_t tmp_id;
				id_addr tmp_addr;
				vector<id_t> query_list;
				char *p = msg + 1;
				int n = (int)msg[0];
				for (i = 0; i < n; i++, p += sizeof (id_t))
				{
					memcpy (&tmp_id, p, sizeof (id_t));
					query_list.push_back (tmp_id);
				}

				char *q = _ackaddr_buf + 1;
				for (i = 0; i < n; i++)
				{
					map<id_t, Addr>::iterator it;
					it = _id2addr.find (query_list[i]);
					if (it != _id2addr.end ())
					{
						tmp_addr.id = it->first;
						tmp_addr.addr = it->second;
						memcpy (q, &tmp_addr, sizeof (id_addr));
						info_count++;
						q += sizeof (id_addr);
					}
				}
				_ackaddr_buf[0] = (unsigned char)info_count;
				if (_net->is_connected (from_id) == false)
				{
                    // May not work, only fit to new connect model on the face
					//_net->connect (from_id, _id2addr[from_id]);
                    _net->connect (_id2addr[from_id]);
					_addition_conn.push_back (from_id);
				}
				_net->sendmsg (from_id, MC_ACKADDR, (char *)&_ackaddr_buf, 1 + info_count * sizeof (id_addr), true, true);
			}
			break;

		case MC_ACKADDR:
			if ((size - 1) % sizeof (id_addr) == 0)
			{
				id_addr rcv_data;
				char *p = msg + 1;
				int i, n = (int)msg[0];
				for (i = 0; i < n; i++, p += sizeof (id_addr))
				{
					memcpy (&rcv_data, p, sizeof (id_addr));
					_id2addr[rcv_data.id] = rcv_data.addr;
				}
			}
			break;

		case MC_HELLO:
			if (size == sizeof (Msg_Node))
			{
#ifdef DEBUG_DETAIL
				printf ("Node [%d] got HELLO from Node [%d]\n", (int)_self.id, (int)from_id);
#endif
				Msg_Node newnode(msg);

				if (is_neighbor (from_id))
				{
					//update_node (newnode.node);
					_voronoi->update (newnode.node.id, newnode.node.pos);
					_id2node[newnode.node.id].update (newnode.node);
				}
				else
				{
					insert_node (newnode.node); //, newnode.addr);
				}
			}
			break;

		case MC_EXCHANGE:
			//if ((size - 1) % sizeof (Msg_Node) == 0)
			if ((size - 1) % sizeof (Node) == 0)
			{
#ifdef DEBUG_DETAIL
				printf ("Node [%d] got EXCHANGE from [%d]\n", (int)_self.id, (int)from_id);
				printf ("	The EXCHANGE content is");
#endif
				
				int i;
				int n = (int)msg[0];
				char *p = msg + 1;
				Node newnode;

				// update receivee nodes info
				for (i = 0; i < n; ++i)
				{
					memcpy (&newnode, p, sizeof (Node));
					p += sizeof (Node);
					//memcpy (&newnode, p, sizeof (Msg_Node));
					//p += sizeof (Msg_Node);

#ifdef DEBUG_DETAIL
					//printf ("  [%d]", (int)newnode.node.id);
					printf (" [%d]", (int)newnode.id);
#endif

					// there is some case that the 2-hop node are not in self's AOI
					if (newnode.id == _self.id)
						continue;
					else if (is_neighbor (newnode.id))
						update_node (newnode);
					else
						insert_node (newnode); //, _id2addr[newnode.id]);
				}
#ifdef DEBUG_DETAIL
			printf ("\n");
#endif
			}
			break;

		case MC_MOVE:
			if (size == sizeof (MC_Node))
			{
				MC_Node *newnode = (MC_Node *)msg;

				// if the remote node has just been disconnected,
				// then no need to process MOVE message in queue
				if (update_node ((*newnode).node) == false)
					break;

#ifdef DEBUG_DETAIL
				printf ("Node [%d] got MOVE that [%d] move to (%d, %d)\n", (int)_self.id, (int)(*newnode).node.id, (int)(*newnode).node.pos.x, (int)(*newnode).node.pos.y);
#endif

				// check if the node already in self's neighbor
				if (is_neighbor ((*newnode).node.id))
				{
					// select next hop to relay
					// check self' EN to find which is in source node's AOI
					vector<id_t> nexthop_list;
					nexthop_list = select_next ((*newnode).node);
					int list_size = nexthop_list.size ();
					if (list_size != 0)
					{
#ifdef DEBUG_DETAIL
						printf ("	and realy to");
						for (int i = 0; i < list_size; i++)
							printf ("  [%d]", (int)nexthop_list[i]);
#endif
						// relay message to the nodes which are in the list
						relay_nodes (nexthop_list, *newnode, true, true);
					}
#ifdef DEBUG_DETAIL
					else
						printf ("This Node [%d] is Leaf", (int)_self.id);
#endif
				}
			}
			break;

		case MC_RELAY:
			// neighbor discovery is made by select_next ()
			if (size == sizeof (MC_Node))
			{
#ifdef DEBUG_DETAIL
			printf ("Node [%d] got RELAY from [%d]\n", (int)_self.id, (int)from_id);
#endif

				MC_Node *newnode = (MC_Node *)msg;

				// record the received _max_hop
				if (newnode->dest_hop == 0)
				{
					if (newnode->hop_count > _max_hop)
						_max_hop = newnode->hop_count;
				}
				(*newnode).hop_count += 1;

				// record the packet come from id and turn it to zero
				if (_msg_count_down.find ((*newnode).node.id) == _msg_count_down.end ())
					_msg_count_down[(*newnode).node.id] = 0;
				else
					_msg_count_down[(*newnode).node.id] = 0;

				// it means this node is a normal node, just need update node
				if (is_neighbor ((*newnode).node.id))
				{
					if (update_node ((*newnode).node) == false)
						return false;
				}
				else
					insert_node ((*newnode).node); //, _id2addr[(*newnode).node.id]);

				// decide if continue relay the packet
				if ((*newnode).dest_hop <= (*newnode).hop_count && (*newnode).dest_hop != 0)
					break;
				else
				{
					vector<id_t> nexthop_list = select_next ((*newnode).node);
					int next_size = nexthop_list.size ();
					if (next_size != 0)
					{
#ifdef DEBUG_DETAIL
						printf ("	continue realy message to");
						for (int i = 0; i < next_size; i++)
						{
							printf ("  [%d]", (int)nexthop_list[i]);
						}
						printf ("\n");
#endif
						relay_nodes (nexthop_list, *newnode, true, true);
					}
#ifdef DEBUG_DETAIL
					else
						printf ("	This node is Leaf\n");
#endif
				}

			}
			break;
		case DISCONNECT:
            if ((size-1) % sizeof (id_t) == 0)
            {
                int n = msg[0];
                char *p = msg + 1;
                id_t id;

                for (int i = 0; i < n; ++i, p+=sizeof (id_t))
                {
                    memcpy (&id, p, sizeof (id_t));
                    
                    // see if it's a remote node disconnecting me
                    if (id == _self.id && is_neighbor (id) == false)
                        delete_node (from_id, false);                        
                }
            }
            // allow other handlers to handle the DISCONNECT message
            return false;

		default:
			return false;
		}
		return true;
	}

	// This function will be called one times per-round
	// collect all infos from gathering messages
	void vast_mc::post_processmsg ()
	{

#ifdef DEBUG_DETAIL
		printf ("Node [%d] is into post_pro\n", (int)_self.id);
#endif

		// if already joined then did following process
		if (is_joined () == true)
		{

#ifdef DEBUG_DETAIL
			printf ("Node [%d] is already joined.\n", (int)_self.id);
#endif

			id_t tmp_id;
			int i;

			// check if all the addresses in AOI already be known -----------------------------------------------------
			int count = 0, now_index;
			char *p = _reqaddr_buf + 1;
			int addr_size = 0;

			// list the ids which are not having address
			map<id_t, Node>::iterator it;
			for (it = _id2node.begin (); it != _id2node.end (); it++)
			{
				tmp_id = it->first;
				if (_id2addr.find (tmp_id) == _id2addr.end ())
				{
					memcpy (p, &tmp_id, sizeof (id_t));
					p += sizeof (id_t);
					addr_size++;
				}
			}

			// send to all neighbors by 6 nodes per step
			now_index = _addr_index;
			while (count < 6)
			{
				// avoid when known nodes is less than 6
				if ((_addr_index + 1) % (int)_neighbors.size () == now_index)
					break;

				// syhu: seems like a check was missing, _addr_index would go out of range
				if (_neighbors.size () <= (unsigned)_addr_index)
					break;

				tmp_id = _neighbors[_addr_index]->id;
				if (tmp_id != _self.id &&
					_id2node.find (tmp_id) != _id2node.end () &&
					_id2addr.find (tmp_id) != _id2addr.end ())
				{
					if (_net->is_connected (tmp_id) == false)
					{
                        // May not work, only fit to new connect model on the face
						//_net->connect (tmp_id, _id2addr[tmp_id]);
                        _net->connect (_id2addr[tmp_id]);
						_addition_conn.push_back (tmp_id);
					}
					_net->sendmsg (tmp_id, MC_REQADDR, (char *)&_reqaddr_buf, 1 + addr_size * sizeof (id_t), true, true);
					count++;
				}
				_addr_index = (_addr_index + 1) % (int)_neighbors.size ();
			}
			// --------------------------------------------------------------------------------------------------------

			// policy -------------------------------------------------------------------------------------------------
			
			// if the value of _now_hop bigger than MAX next time
			if ((_now_hop + _f_count) > (_max_hop + MAX_HOP_COUNT_BUF))
			{
				// go back to initial value
				_f_count = 0;
				_last_f_count = -1;
				_now_hop = 0;

				// redetect the value of _max_hop
				if ((_max_hop * 0.7) <= _min_hop)
					_max_hop = _min_hop;
				else
					_max_hop = (int)(_max_hop * 0.7);
			}
			else
			{
				_now_hop = (_now_hop + _f_count);

				// set the lower bound of _now_hop
				if (_now_hop < _min_hop)
					_now_hop += _min_hop;
				else
				{
					if (_last_f_count == (-1) && _f_count == 0)
					{
						_last_f_count = 0;
					}
					else if (_last_f_count == 0 && _f_count == 0)
					{
						_f_count = 1;
					}
					else
					{
						int temp = _f_count;
						_f_count = _f_count + _last_f_count;
						_last_f_count = temp;
					}
				}
			}

			/*else	// exponential series
			{
				_exponent++;
				_f_count = (int)pow (2, _exponent);
			}*/
			// --------------------------------------------------------------------------------------------------------

			// all msg count down adds 1 ------------------------------------------------------------------------------
			for (map<id_t, int>::iterator iter = _msg_count_down.begin (); iter != _msg_count_down.end (); iter++)
			{
				iter->second += 1;
			}
			// --------------------------------------------------------------------------------------------------------

			vector<id_t> en_Nbr = _voronoi->get_en (_self.id, 1);
			int en_size = en_Nbr.size ();

#ifdef DEBUG_DETAIL
			printf ("Node [%d] got en_Nbr success. en_size is %d\n", (int)_self.id, en_size);
#endif

			// to check connect and disconnect
			// we only make ENs connect, the others will disconnect
			Msg_Node hello;
			hello.set (_self); //, _net->getaddr (_self.id));

			vector<id_t> en_en_list;

#ifdef DEBUG_DETAIL
				printf ("	send HELLO to EN  ");
#endif

				for (i = 0; i < en_size; ++i)
				{
					if (_net->is_connected (en_Nbr[i]) == false)
					{
                        // May not work, only fit to new connect model on the face
						//_net->connect (en_Nbr[i], _id2addr[en_Nbr[i]]);
                        _net->connect (_id2addr[en_Nbr[i]]);
#ifdef DEBUG_DETAIL
				printf ("(h)");
#endif
					}
#ifdef DEBUG_DETAIL
				printf ("[%d]  ", (int)en_Nbr[i]);
#endif
				}

#ifdef DEBUG_DETAIL
			printf ("\nDisconnect Node");
#endif


			// remove redundant connection
			map<id_t, Addr> conn_list = _net->getconn ();
			map<id_t, Addr>::iterator conn_it;
			for (conn_it = conn_list.begin (); conn_it != conn_list.end (); conn_it++)
			{
				tmp_id = conn_it->first;
				// don't remove the connection which is in addition_list
				if (_voronoi->is_enclosing (tmp_id) == false &&
					is_inlist (tmp_id, _addition_conn) == false &&
					is_inlist (tmp_id, _addition_list) == false &&
					is_inlist (tmp_id, _addition_exchange) == false)
				{
					_net->disconnect (tmp_id);
#ifdef DEBUG_DETAIL
					printf ("  [%d]", (int)tmp_id);
#endif
				}
			}
			_addition_conn.clear ();

			// remove nonoverlapped nodes
			remove_nonoverlapped ();

#ifdef DEBUG_DETAIL
			printf (" nodes\n");
#endif
		}
		// not yet joined then wait for INILIST
		else
		{
			// Not joined, waiting for joined success
#ifdef DEBUG_DETAIL
			printf ("Node [%d] is not joined yet\n", (int)_self.id);
#endif
		}

		// send out all pending reliable messages
		_net->flush (true);
	}

	//
	// neighbor maintainence
	//

	// to add a node in self neighbor list
	// this function will be called when discover a new node which isn't in neighbor list
	bool vast_mc::insert_node (Node &node, const Addr &addr, bool refresh)
	{

		// check for redundency
		if (is_neighbor (node.id))
			return false;

		_voronoi->insert (node.id, node.pos);
		_id2node[node.id] = node;
		_id2addr[node.id] = addr;
		_neighbors.push_back (&_id2node[node.id]);
		_total_neighbor++;

		if (refresh) (void)TwoHopNbr_maintain ();
		
		// avoid self-connection
		// NOTE: connection may already exist with remote node, in which case
		//		 the insert_node process will continue (instead of aborting)
		if (node.id != _self.id && _voronoi->is_enclosing (node.id))
            // May not work, only fit to new connect model on the face
            //if ((addr.id != 0 && _net->connect (node.id, addr) == (-1)) || _net->connect (node.id) == (-1)))
            if (addr.id != 0 && _net->connect (node.id) == (-1))
				return false;
		
		return true;
	}

	bool vast_mc::delete_node (id_t id, bool disconnect)
	{
		if (is_neighbor (id) == false)
			return false;

		if (disconnect == true && _net->is_connected (id))
			_net->disconnect (id);

		_voronoi->remove (id);
		vector<Node *>::iterator it = _neighbors.begin ();
		for (; it != _neighbors.end (); it++)
		{
			if ((*it)->id == id)
			{
				_neighbors.erase (it);
				break;
			}
		}

		_id2node.erase (id);
		_total_neighbor--;

		return true;
	}

	bool vast_mc::update_node (Node &node)
	{
		if (is_neighbor (node.id) == false)
			return false;

		// only update the node if it's at a later time
		// it means the node information is new
		if (_id2node[node.id].time <= node.time)
		{
			// calculate displacement from last position
			_voronoi->update (node.id, node.pos);
			_id2node[node.id].update (node);
		}

		return true;
	}

	// maintain the 2 hop neighbor list
	vector<id_t> & vast_mc::TwoHopNbr_maintain (void)
	{
		vector<id_t> test_list;
		map<id_t, Node *> id2n;

		//_enc_neighbors.clear ();
		_2hop_neighbors.clear ();

		vector<id_t> enc_neighbors = _voronoi->get_en (_self.id, 1);
		_2hop_neighbors = _voronoi->get_en (_self.id, 1);
		int enc_size = enc_neighbors.size ();
		int test_size;
		int i,j;

		// starting addin 2 hop neighbors
		for (i = 0; i < enc_size; i++)
		{
			test_list = _voronoi->get_en (enc_neighbors[i], 1);
			test_size = test_list.size ();
			for (j = 0; j < test_size; j++)
			{
				if (is_inlist (test_list[j], _2hop_neighbors))
					continue;

				_2hop_neighbors.push_back (test_list[j]);
			}
		}

		return _2hop_neighbors;
	}

	vector<id_t> & vast_mc::select_next (Node &node)
	{
		Node root_node (node);
		_nexthop_list.clear ();
		int i;

		// First: get EN list
		vector<id_t> EN_list = _voronoi->get_en (_self.id, 1);

		vector<id_t> test_list;
		vector<id_t> candidate_list;
		map<id_t, double> id_dist;
		map<id_t, double>::iterator it;

		// Second: check which nodes in EN_list are in the source node's AOI
		int EN_size = EN_list.size ();
		for (i = 0; i < EN_size; i++)
		{
			test_list.clear ();
			candidate_list.clear ();
			id_dist.clear ();
			if (EN_list[i] == root_node.id)
				continue;

			if (_id2node[EN_list[i]].pos.dist (root_node.pos) <= (double)(root_node.aoi + _detection_buffer))
			{
				// Third: check who is the real parent
				test_list = _voronoi->get_en (EN_list[i], 1);

				int TEST_size = test_list.size ();
				int j = 0;
				double min_dist = _self.pos.dist (root_node.pos);
				id_t min_id = _self.id;

				// to find the min dist nodes between source node and EN in test_list
				// be candidate for real parent

				// calculate all distance in test_list
				for (j = 0; j < TEST_size; j++)
				{
					id_dist[test_list[j]] = _id2node[test_list[j]].pos.dist (root_node.pos);
				}

				// to find the min. distance from all distances
				for (it = id_dist.begin(), min_dist = it->second; it != id_dist.end(); it++)
				{
					if (it->second < min_dist)
						min_dist = it->second;
				}
				
				// to find out all the id with min. distance
				for (it = id_dist.begin(); it != id_dist.end(); it++)
					if (it->second == min_dist)
						candidate_list.push_back (it->first);

				// It may be have several min_dist, so we have to make the real next hop
				// be unique by comparing their ID
				if (candidate_list.size () == 0)
				{
					// if this situation occurs, it is an ERROR
					// it must have at least one
					printf ("Node [%d] ERROR: relay_node () didn't get candidate_list for Node [%d]\n", (int)_self.id, (int)EN_list[i]);
				}
				else if (candidate_list.size () == 1)
				{
					// it means the candidate one is equal the real one
					// then check this real one if it is equal _self.id
					if (candidate_list[0] == _self.id)
						_nexthop_list.push_back (EN_list[i]);
				}
				else if (candidate_list.size () >= 2)
				{
					// it means to choose a unique one from these candidates
					for (j = 0, min_id = candidate_list[0]; j < (int)candidate_list.size (); j++)
						if (candidate_list[j] < min_id)
							min_id = candidate_list[j];

					if (min_id == _self.id)
						_nexthop_list.push_back (EN_list[i]);
				}
			}
		}
		return _nexthop_list;
	}

	// send node infos to a particular node
	// 'list' is a list of indexes to _neighbors
	void vast_mc::send_nodes (id_t target, vector<id_t>&list, bool reliable, bool buffered)
	{
		int n = list.size ();
		if (n == 0) return;

#ifdef DEBUG_DETAIL
			printf ("	The INITLIST to [%d] is", (int)target);
#endif
		// TODO: sort the notify list by distance from target's center
        _buf[0] = (unsigned char)n;
        char *p = _buf + 1;

		Msg_Node msgnode;
		for (int i = 0; i < n; ++i, p += sizeof (Msg_Node))
		{
			msgnode.set (_id2node[list[i]]); //, _net->getaddr (list[i]));
			memcpy (p, (char *)&msgnode, sizeof (Msg_Node));
#ifdef DEBUG_DETAIL
			printf ("  [%d]", (int)msgnode.node.id);
#endif
		}
#ifdef DEBUG_DETAIL
			printf ("\n");
#endif

		// check whether to send the NODE via TCP or UDP
		_net->sendmsg (target, MC_INITLIST, _buf, 1 + n * sizeof (Msg_Node), reliable, true);
	}

	// to generate the next relay hop list
	void vast_mc::relay_nodes (vector<id_t>&nexthop_list, MC_Node relay_node, bool reliable, bool buffered)
	{
		// if the next hop list is empty, it means the node is 
		// the leaf of the spanning tree of the source node
		if (nexthop_list.empty () || relay_node.dest_hop == relay_node.hop_count)
		{
			//printf (" NULL");
			return;
		}

		// send to all nodes in the next hop list
		for (int i = 0; i < (int)nexthop_list.size (); i++)
		{
			if (_net->is_connected (nexthop_list[i]) == false)
                // May not work, only fit to new connect model on the face
				//_net->connect (nexthop_list[i], _id2addr[nexthop_list[i]]);
                _net->connect (_id2addr[nexthop_list[i]]);
			_net->sendmsg (nexthop_list[i], MC_RELAY, (char *)&relay_node, sizeof (MC_Node), true, true);
		}
	}

	// exchange neighbor lists
	void vast_mc::exchange_ENs (vector<id_t>&ENs_list)
	{
		int i;
		//ENs_list.push_back (_self.id);
		int n = ENs_list.size ();
		_ex_buf[0] = (unsigned char)n;
		char *p = _ex_buf + 1;

#ifdef DEBUG_DETAIL
		printf ("	The EXCHANGE content is");
#endif
		//Msg_Node newnode;
		Node newnode;

		for (i = 0; i < n; i++)
		{
			newnode = _id2node[ENs_list[i]];
			memcpy (p, (char *)&newnode, sizeof (Node));
			p += sizeof (Node);

#ifdef DEBUG_DETAIL
			printf (" [%d] ", (int)newnode.id);
#endif
		}

#ifdef DEBUG_DETAIL
		printf (" and send to");
#endif
		// avoid send to self
		for (i = 0; i < n; i++)
			if (ENs_list[i] != _self.id)
			{
				if (_net->is_connected (ENs_list[i]) == false)
				{
                    // May not work, only fit to new connect model on the face
					//_net->connect (ENs_list[i], _id2addr[ENs_list[i]]);
                    _net->connect (_id2addr[ENs_list[i]]);
				}
				_net->sendmsg (ENs_list[i], MC_EXCHANGE, _ex_buf, 1 + n * sizeof (Node), true, true);
#ifdef DEBUG_DETAIL
				printf ("  [%d]", (int)ENs_list[i]);
#endif
			}
	}

	// check for disconnectionfrom neighbors no longer in view
	// return numbers of neighbors removed
	int vast_mc::remove_nonoverlapped ()
	{
#ifdef DEBUG_DETAIL
		printf ("\nNode [%d] removed", (int)_self.id);
#endif
		// nodes considered for removing
		vector<id_t> remove_list;
		vector<id_t> twohop_list = TwoHopNbr_maintain ();
		id_t id;
		int i;


		// go over each neighbor and do an overlap check
		for (map<id_t, Node>::iterator it = _id2node.begin (); it != _id2node.end (); it++)
		{
			id = it->first;
			// check if a neighbor is relevant (= within AOI)
			// NOTE: we add 1 to the overlap test to avoid round-off error
			if (id == _self.id ||
				is_inlist (id, twohop_list) ||
				(_voronoi->overlaps (id, _self.pos, (aoi_t)(_self.aoi + _detection_buffer + 1)) == true &&
				_msg_count_down[id] < _max_hop))
			{
				// counting ...
				_count_drop[id] = 0;
				continue;
			}

			// record a list of non-overlapped neighbors
			if (++_count_drop[id] > MAX_DROP_COUNT)
				remove_list.push_back (id);

			for (map<id_t, int>::iterator rem_it = _msg_count_down.begin (); rem_it != _msg_count_down.end (); rem_it++)
			{
				id = rem_it->first;
				if (rem_it->second > _max_hop)
                    if (is_inlist (id, remove_list) == false)
						remove_list.push_back (id);
			}
		}

		int n = remove_list.size ();
		int n_deleted = 0;
		for (i = 0; i < n; i++)
		{
			id = remove_list[i];
			n_deleted++;
			delete_node (id);

#ifdef DEBUG_DETAIL
			printf ("  [%d]", (int)id);
#endif
		}
		return n_deleted;
	}
}	// end of namespace VAST

//#endif		// DEBUG_DETAIL
