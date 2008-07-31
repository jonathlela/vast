/*
 *
 *
 *
 */

#include "vast.h"
#include "net_msg.h"
#include "msghandler.h"

#include <map>

namespace VAST
{

#define MAX_DROP_COUNT			4	// # of time-steps to disconnect a non-overlapped neighbor
#define MAX_HOP_COUNT_BUF		2

	typedef enum VAST_MC_Message
    {
        MC_QUERY = 30,			// find out host to contact for initial neighbor list
		MC_INITLIST,			// get initial neighbor list
		MC_REQNBR,				// when join success, we notify EN our existance
		MC_ACKNBR,				// send back to request node a neighbor list
		MC_REQADDR,
		MC_ACKADDR,
        MC_HELLO,				// initial connection request
		MC_EXCHANGE,			// exchange enclosing neighbor lists
        MC_MOVE,				// position update to normal peers
		MC_RELAY,				// help for relay other's position update
        MC_UNKNOWN
    };

	typedef struct msgcontent
	{
		char content[VAST_BUFSIZ];
	}msgcontent;

	typedef struct id_addr
	{
		id_t id;
		Addr addr;
	}id_addr;

	class MC_Node
	{
	public:
		MC_Node ()
		{
		}

		// using this constructor will create a exist node copy
		MC_Node (char const *p)
		{
			// void *memcpy( void *dest, const void *src, size_t count );
			memcpy (this, p, sizeof(MC_Node));
		}

		// using this constructor will create a Msg_Node with initial value
		MC_Node (Node &n, int d_hop, int hopcount)
			:node(n), dest_hop(d_hop), hop_count(hopcount)
		{			
		}

		// setting a Msg_Node to point the given node and address
		void set (Node const &node, int const &d_hop, int const &hopcount)
		{
			this->node = node;
			this->dest_hop = d_hop;
			this->hop_count = hopcount;
		}

		Node	node;
		int		dest_hop;
		int		hop_count;
	};

	class Msg_Node
	{
	public:
		Msg_Node ()
		{
		}

		// using this constructor will create a exist node copy
		Msg_Node (char const *p)
		{
			// void *memcpy( void *dest, const void *src, size_t count );
			memcpy (this, p, sizeof(Msg_Node));
		}

		// using this constructor will create a Msg_Node with initial value
		Msg_Node (Node &n)//, Addr &a)
			:node(n) //, addr(a)
		{			
		}

		// setting a Msg_Node to point the given node and address
		void set (Node const &node) //Addr const &addr
		{
			this->node = node;
			//this->addr = addr;
		}

		Node	node;
		//Addr	addr;
	};

    class Msg_Query
    {
    public:
        Msg_Query () 
        {
        }

        Msg_Query (const char *p)
        {
            memcpy (this, p, sizeof (Msg_Query));
        }

        Msg_Query (Node &n, Addr &a)
            : node (n), addr (a)
        {
        }

        Msg_Query& operator= (const Msg_Query& m)
        {
            node = m.node;
            addr = m.addr;
            return *this;
        }

        Node node;
        Addr addr;
    };
    

	// Considering... if create a new type of Position and Neighbor Exchange

	class msgbuf
	{
	public:
		msgbuf ()
		{
		}

		msgbuf (char const *p)
		{
			memcpy (this, p, sizeof (msgbuf));
		}
		msgbuf (msgtype_t &msgtype, int &msgsize, msgbuf *next = NULL)
			:type(msgtype), size(msgsize)
		{
		}

		msgtype_t type;
		int size;
		msgcontent *msgc;
		msgbuf *next;
	};

	class vast_mc : public vast
	{
	public:
		
		// construct network model
		vast_mc (network *netlayer, aoi_t detect_buffer/*, int conn_limit = 0*/);
		~vast_mc ();

		// join VON to obtain an initial set of AOI neighbors
		// NOTE: this only initiates the process but does not guarantee a successful join
		bool join (id_t id, aoi_t AOI, Position &pos, Addr &gateway);

		// quit VON
		void leave ();

		// AOI related functions
		void setAOI (aoi_t radius);
		aoi_t getAOI ();

        // move to a new position, returns actual position
		Position & setpos (Position &pos);

		char * getstat (bool clear = false);

	private:

		// returns whether the message has been handled successfully
		bool handlemsg (id_t from_id, msgtype_t msgtype, timestamp_t recvtime, char *msg, int size);

		// do neighbor discovery check for AOI neighbors
		void post_processmsg ();

		// neighbor maintainence
		inline bool insert_node (Node &node, const Addr &addr = Addr(), bool refresh = false);
		inline bool delete_node (id_t id, bool disconnect = true);
		inline bool update_node (Node &node);
		inline vector<id_t> & select_next (Node &root_node);
		inline vector<id_t> & TwoHopNbr_maintain (void);

		// helper methods
		inline bool is_neighbor (id_t id)
		{
			return (_id2node.find (id) != _id2node.end ());
		}

		inline bool is_inlist (id_t test_id, vector<id_t> &list)
		{
			int i, size;
			size = list.size ();
			for (i = 0; i < size; i++)
				if (test_id == list[i])
					return true;

			return false;
		}

		// to check if a node's position is in given quadrangle region
		inline bool is_contain (Position left_bottom, Position right_bottom, Position left_top, Position right_top)
		{
                    // TODO: syhu: needs implementation
                    return true;
		}

		// send node infos to a particular node
		void send_nodes (id_t target, vector<id_t>&list, bool reliable = true, bool buffered = true);

		// relay others' message
		void relay_nodes (vector<id_t>&nexthop_list, MC_Node relay_node, bool reliable = true, bool buffered = true);

		// exchange neighbor lists
		void exchange_ENs (vector<id_t>&ENs_list);

        // check for disconnection from neighbors no longer in view
        // returns number of neighbors removed
        int  remove_nonoverlapped ();

		// internal data structures
		int		_addr_index;
		int		_max_hop;
		int		_min_hop;
		int		_now_hop;
		int		_f_count;
		int		_last_f_count;
		int		_exponent;
		aoi_t	_detection_buffer;			// buffer area for neighbor discovery detection
		bool	_nbr_change;				// to count the number of neighbor change in the network
		vector<id_t> _2hop_neighbors;		// keep the 2 hop neighbors to do the child decision
		//vector<id_t> _enc_neighbors;		// keep the enclosing neighbor list
		vector<id_t> _nexthop_list;			// for select the next hop list
		vector<id_t> _addition_list;
		vector<id_t> _addition_exchange;
		vector<id_t> _addition_conn;

		// counters
		map<id_t, int>   _count_drop;        // counter for disconnecting a node

		// mapping from id to node
		map<id_t, Node> _id2node;

		// mapping from id to Addr
		map<id_t, Addr> _id2addr;

		// msg recv count down
		map<id_t, int> _msg_count_down;

		// store tempertory message (from_id, msg)
		map<id_t, msgbuf *> _msgstore;

		// store tempertory message content
		vector<msgcontent> _contentstore;

		// nodes worth considering to connect
		map<id_t, vector<id_t> *> _notified_nodes;

		//
		map<id_t, vector<id_t> *> _now_en_status;

		// stat for message size breakdown / count
		// to count for the receiving message number
		vector<int> _msg_stat;

		// record link state
		vector<vector<id_t> *> _linkstate;

        // generic send buffer
        char _buf[VAST_BUFSIZ];
		char _ex_buf[VAST_BUFSIZ];
		char _reqnbr_buf[VAST_BUFSIZ];
		char _acknbr_buf[VAST_BUFSIZ];
		char _reqaddr_buf[VAST_BUFSIZ];
		char _ackaddr_buf[VAST_BUFSIZ];

		int _total_neighbor;
		int _NM_total;
		int _NM_known;
	};
}

