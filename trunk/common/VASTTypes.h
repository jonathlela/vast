/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2009 Shun-Yun Hu (syhu@yahoo.com)
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


#ifndef _VAST_Types_H
#define _VAST_Types_H

#include "Config.h"
#include <string.h>     // memcpy
#include <math.h>

#include <string>
#include <sstream>
#include <vector>


namespace Vast
{

typedef unsigned long   id_t;           // node ID (per IP/port pair)
typedef unsigned long   timestamp_t;    // short: 0 - 65535  long: 0 - 4294967296
typedef unsigned char   byte_t;
typedef unsigned short  word_t;

typedef double          coord_t;        // type of coordinates
typedef unsigned long   length_t;       // type for a length, for radius or rectangle length

typedef unsigned short  msgtype_t;      // the types of messages (0 - 255)
typedef unsigned char   layer_t;        // the number of layers in the overlay (0 - 255)

typedef unsigned char   listsize_t;     // size of a list sent over network

// right now each handler_no is 4 bits, so only go to 15
//#define MAX_HANDLER_NO 15

// create a combined msgtype from groupID, toHandlerNo, fromHandleNo, Msgtype
//#define COMBINE_MSGTYPE(g, tH, fH, t) ((g << 24) | (tH << 20) | (fH << 16) | (0x0FFFF & t))

// obtain the handler ID from the combined msgtype (consisting of MsgGroupID + HandlerNo)
//#define EXTRACT_HANDLERID(m)        (m & 0xFFF00000)

// obtain the (to) handler_no from the combined msgtype
//#define EXTRACT_TO_HANDLERNO(m)     ((m & 0x00F00000) >> 20)

// obtain the (from) handler_no from the combined msgtype
//#define EXTRACT_FROM_HANDLERNO(m)   ((m & 0x000F0000) >> 16)

// obtain the msgtype from the combined msgtype
//#define EXTRACT_MSGTYPE(m)          (m & 0x0000FFFF)

// make handlerID from groupID, handlerNo
//#define MAKE_HANDLERID(g, h)        ((id_t)g << 24 | (id_t)h << 20)

// make a unique ID consisting of network ID and handlerNo
//#define COMBINE_ID(handler, id)     ((handler << 28) | id)

// obtain the handler no from the combined ID
//#define EXTRACT_HANDLER_NO(id)      ((id & 0xF0000000) >> 28)

// obtain the unique node (network) ID from the combined ID
//#define EXTRACT_NODE_ID(id)         (id & 0x0FFFFFFF)


// node / message handler can all have these states
typedef enum NodeState
{
    ABSENT  = 0,
    JOINING,
    JOINED,
};

// bandwidth type define
typedef enum bandwidth_t
{
    BW_UNKNOWN = 0,
    BW_UPLOAD, 
    BW_DOWNLOAD
};

// virtual interface that a class should implement to become
// serializable into a bytestring sendable over network
class EXPORT Serializable 
{
public:
    Serializable () {}
    ~Serializable () {}

    // store into a buffer (assuming the buffer has enough space)
    // buffer can be NULL (simply to query the total size)
    // returns the total size of the packed class
    virtual size_t serialize (char *buffer) = 0;

    // restores a buffer into the object
    // returns number of bytes restored (should be > 0 if correct)
    virtual size_t deserialize (char *buffer) = 0;
};

//
// a spot location in the virtual world
//
class EXPORT Position 
{

public:
    Position () 
        : x(0), y(0)
    {
    }

    Position (coord_t x_coord, coord_t y_coord)
        : x(x_coord), y(y_coord)
    {
    }

    Position (char const *p) 
    {
        memcpy (this, p, sizeof(Position));
    }
            
    coord_t distance (Position const &p) const
    {        
        coord_t dx = ((coord_t)p.x - (coord_t)x);
        coord_t dy = ((coord_t)p.y - (coord_t)y);
        return sqrt (pow (dx, 2) + pow (dy, 2));
    }

    coord_t distanceSquare (Position const &p) const
    {        
        coord_t dx = (p.x - x);
        coord_t dy = (p.y - y);
        return (dx*dx + dy*dy);
    }

    bool operator== (Position const &p)
    {
        return (x == p.x && y == p.y);
    }

    bool operator!= (Position const &p)
    {
        return (x != p.x || y != p.y);
    }

    coord_t x;
    coord_t y;
    //coord_t z;
};

class EXPORT Area
{
public:
    Area ()
        :center (0, 0), radius (0), width (0), height (0)
    {
    }

    Area (Position &c, length_t r)
        :center (c), radius (r)
    {
    }

    ~Area ()
    {
    };

    Position    center;
    length_t    radius;
    length_t    width;
    length_t    height;
};

class EXPORT Message : public Serializable
{
public:
    Message (id_t sender, msgtype_t type) 
    {
        // allocate the default space for message
        from    = sender;
        msgtype = type;
        _alloc  = true;
        _curr   = data = new char[VAST_MSGSIZ];                
        size    = 0;
        _free   = VAST_MSGSIZ;        
    }

    // create a message to store a bytestring
    // NOTE: optional "no allocation" is possible, if it is expected that the string being
    //       extracted will not need to be manipulated or changed
    Message (msgtype_t type, const char *msg, size_t msize, bool alloc = true)
    {
        msgtype = type;
        _alloc  = alloc;

        // check if we need to allocate new space, or could simply use the pointer given
        if (alloc == true)
        {
            _curr = data = new char[msize];
            memcpy (data, msg, msize);
        }
        else
            _curr = data = (char *)msg;

        size = msize;
        _free = 0;
    }

    ~Message ()
    {
        // we only release memory if we had previously allocated
        if (_alloc == true)
            delete[] data;
    }

    // store an item to this message, record_size indicates whether this is
    // a fixed-size item or variable-size item (needs to records its size)
    bool store (const char *p, size_t item_size, bool record_size = false)
    {
        // if no internal allocation, then cannot store new data
        if (_alloc == false)
            return false;

        // check available space
        expand ((record_size ? item_size + sizeof(size_t) : item_size));

        if (record_size)
        {
            memcpy (_curr, &item_size, sizeof (size_t));
            _curr   += sizeof (size_t);
            _free   -= sizeof (size_t);
            size    += sizeof (size_t);
        }

        // copy data and adjust pointer & size
        memcpy (_curr, p, item_size);
        _curr += item_size;
        _free -= item_size;
        size  += item_size;

        return true;
    }

    // extract an item from this message
    // item_size == 0 indicates a variable size item, item_size > 0 means fixed-size item
    // NOTE: the destination pointer is assumed to have enough space
    size_t extract (char *p, size_t item_size)
    {
        // extract "item size" first, if not specified
        if (item_size == 0 && ((size_t)((_curr + sizeof(size_t)) - data) <= size))
        {
            memcpy (&item_size, _curr, sizeof (size_t));
            _curr += sizeof (size_t);
        }

        // see if we still have something to extract
        if ((size_t)((_curr + item_size) - data) > size)
            return 0;

        // copy the item out
        memcpy (p, _curr, item_size);
        _curr += item_size;

        return item_size;
    }

    // clear existing content of the message, reset all counters
    void clear (id_t sender, msgtype_t type)
    {
        from    = sender;
        targets.clear ();

        msgtype = type;
        _alloc  = true;
        _curr   = data;
        _free   = _free + size;
        size    = 0;
    }

    // serialize this object to a buffer, returns total size in bytes
    // NOTE: assumes enough space is pre-allocated to buffer, 
    //       use NULL for buffer to get total size
    size_t serialize (char *buffer)
    {
        size_t total_size = sizeof (listsize_t) + sizeof (id_t) * targets.size () + 
                            sizeof (msgtype_t) + sizeof (size_t) + size;

        if (buffer != NULL)
        {
            char *p = buffer;
            
            // copy sender & receiver NodeIDs
            memcpy (p, &from, sizeof (id_t));
            p += sizeof (id_t);

            listsize_t num_targets = (listsize_t)targets.size ();
            memcpy (p, &num_targets, sizeof (listsize_t));
            p += sizeof (listsize_t);
            for (size_t i = 0; i < targets.size (); i++)                
            {
                memcpy (p, &targets[i], sizeof (id_t));
                p += sizeof (id_t);
            }

            // copy message type, message size, message
            memcpy (p, &msgtype, sizeof (msgtype_t));
            p += sizeof (msgtype_t);
            memcpy (p, &size, sizeof (size_t));
            p += sizeof (size_t);
            memcpy (p, data, size);
        }
        return total_size;
    }

    // restores a buffer into the Message object
    // returns number of bytes restored (should be > 0 if correct)
    size_t deserialize (char *buffer)
    {
        size_t size_restored = 0;
        char *p = buffer;
        
        if (buffer != NULL)
        {
            clear (0, 0);

            // restore sender & receiver NodeIDs
            memcpy (&from, p, sizeof (id_t));
            p += sizeof (id_t);

            listsize_t num_targets;
            memcpy (&num_targets, p, sizeof (listsize_t));
            p += sizeof (listsize_t);

            id_t target;
            for (size_t i=0; i < (size_t)num_targets; i++)
            {
                memcpy (&target, p, sizeof (id_t));
                p += sizeof (id_t);
                targets.push_back (target);
            }

            // restore message type, message & size
            memcpy (&msgtype, p, sizeof (msgtype_t));            
            p += sizeof (msgtype_t);            
            
            memcpy (&size_restored, p, sizeof (size_t));
            p += sizeof (size_t);
            if (store (p, size_restored) == false)
                size_restored = 0;
        }
        return size_restored;
    }

    char       *data;           // pointer to the message buffer
    size_t      size;           // size of the message
    msgtype_t   msgtype;        // type of message    
    id_t        from;           // nodeID of the sender    
    std::vector<id_t> targets;  // NodeIDs this message targets

private:
    size_t      _free;          // # of bytes left in buffer
    char       *_curr;          // current position in buffer
    bool        _alloc;         // whether internal allocation was used

    void expand (size_t len)
    {
        // don't expand if we have enough
        if (len <= _free)
            return;

        // estimate new size
        size_t newsize = (((size + len) / VAST_MSGSIZ) + 1) * VAST_MSGSIZ;
        
        char *temp = new char[newsize];

        // copy old data to new buffer & adjust pointer 
        memcpy (temp, data, size);        
        _curr = temp + (_curr - data);
        _free = newsize - size;

        // remove old buffer
        delete[] data;
        data = temp;
    }
};

class EXPORT IPaddr
{
public:
    IPaddr ()
        : host(0), port(0)
    {
    }
    
    IPaddr (unsigned long i, unsigned short p)
    {
        host = i;
        port = p;
    }
    
    IPaddr (const char *ip_string, unsigned short p)
    {
        int j=0;
        char c;
        unsigned long i = 0;
        unsigned long t = 0;
        while ((c = ip_string[j++]) != '\0')
        {
            if (c >= '0' && c <= '9')
                t = t*10 + (c - '0');
            else if (c == '.')
            {
                i = (i<<8) + t;
                t = 0;
            }
        }
        i = (i<<8) + t;
        
        host = i;
        port = p;
    }
    
    ~IPaddr ()
    {
    }   
        
    void operator= (IPaddr const &a)
    {
        host = a.host;
        port = a.port;
    }

    void getString (char *p)
    {
        sprintf (p, "%d.%d.%d.%d", (int)((host>>24) & 0xff), (int)((host>>16) & 0xff), (int)((host>>8) & 0xff), (int)(host & 0xff));
    }

    void parseIP (const std::string & instr)
    {
        IPaddr::parseIP (*this, instr);
    }

    static int parseIP (IPaddr & addr, const std::string & instr)
    {
        if ((signed) instr.find (":") == -1)
            return -1;

        size_t port_part = instr.find (":");
        int port = atoi (instr.substr (port_part + 1).c_str ());
        if (port <= 0 || port >= 65536)
            return -1;

        addr = IPaddr (instr.substr (0, port_part).c_str (), port);
        return 0;
    }

    inline bool operator== (const IPaddr & adr) const
    {
        return (adr.host == host && adr.port == port);
    }

    unsigned long       host;
    unsigned short      port;   
    unsigned short      pad;
};

/*
 *  Addr class
 *  raw string format ( [] means ignorable )
 *    PublicIP:port[|Private:port]
 *  for ex bbs site "PTT" should be 
 *    140.112.172.11:23
 *  someone behind NAT should be
 *    140.112.172.11:23|192.168.5.8:22018
 */
class EXPORT Addr
{
    
public:
    // should this correct? define an id in Addr and in typedef.h
    //Vast::id_t      id;
    IPaddr          publicIP;
    IPaddr          privateIP;
    timestamp_t     lastAccessed;

    Addr ()
    {
        //id = 0; // = NET_ID_UNASSIGNED;
    }

    /*
    Addr (id_t i)
    {
        id = i;
    }
    */

    ~Addr ()
    {
    }

    void zero ()
    {
        memset (this, 0, sizeof (Addr));
    }

    void setPublic (unsigned long i, unsigned short p)
    {
        publicIP.host = i;
        publicIP.port = p;
    }
    
    void setPrivate (unsigned long i, unsigned short p)
    {
        privateIP.host = i;
        privateIP.port = p;
    }
    
    const std::string & toString ()
    {
        static std::string str;

        // temp variables to help output
        std::ostringstream oss;
        char tmpstr[4*4];   // max 3 digit + 1 dot and 4 fields totally

        str.empty ();

        publicIP.getString (tmpstr);
        oss << tmpstr << ":" << publicIP.port;

        if (privateIP.port != 0)
        {
            str.append ("|");
            privateIP.getString (tmpstr);
            oss << tmpstr << ":" << privateIP.port;
        }

        str = oss.str ();

        return str;
    }

    // return 0 on success
    int fromString (const std::string & str)
    {
        zero ();

        size_t spt_pos = str.find ("|");
        // find no "|" mark
        if ((signed) spt_pos == -1)
        {
            if (IPaddr::parseIP (publicIP, str))
                return -1;
        }
        else
        {
            std::string pub = str.substr (0, spt_pos);
            std::string priv = str.substr (spt_pos + 1);

            if (IPaddr::parseIP (publicIP, pub) || IPaddr::parseIP (privateIP, priv))
                return -1;
        }

        return 0;
    }
    
    inline bool operator== (const Addr & adr) const
    {
        return (publicIP == adr.publicIP && ((privateIP.port == adr.privateIP.port == 0) || (privateIP == adr.privateIP)));
    }

    inline bool operator!= (const Addr & adr) const
    {
        return !(*this == adr);
    }
};

class EXPORT Node
{
public:
    Node ()
        : id (0), //pos (0,0), 
          aoi (), time (0), addr ()
    {
    }

    Node (id_t i, //Position &p, 
          Area &a, timestamp_t t, Addr &ar)
        : id (i), //pos(p), 
          aoi (a), time (t), addr (ar)
    {
    }

    Node (const Node & n)
        : id (n.id), //pos (n.pos), 
          aoi (n.aoi), time (n.time), addr (n.addr)
    {
    }

    ~Node ()
    {
    }

    void update (Node const &rhs)
    {
        id      = rhs.id;
        //pos     = rhs.pos;
        aoi     = rhs.aoi;
        time    = rhs.time;
        addr    = rhs.addr;
    }

    Node& operator= (const Node& n)
    {        
        id      = n.id;
        //pos     = n.pos;
        aoi     = n.aoi;
        time    = n.time;
        addr    = n.addr;
        return *this;
    }

    id_t            id;         // unique ID for the node   
    //Position        pos;        // current position of the node
    Area            aoi;        // the area of interest
    timestamp_t     time;       // last update time
    Addr            addr;       // IP address for this node
};

// a simple class to keep count & calculate statistics
class EXPORT Ratio
{
public:
    Ratio ()
        :normal (0), total (0)
    {
    }

    // calculates between ratio of two numbers
    double ratio ()
    {
        return (double)normal / (double)total;
    }

    size_t  normal;
    size_t  total;    
};


//
//  Following are used by Voronoi class
//

class EXPORT point2d
{
public:

	double  x;
	double  y;

	point2d (double a = 0 , double b = 0)
	    :x(a), y(b)
	{
	}

    point2d (const point2d& a)
        : x(a.x), y(a.y)
    {
    }

	void operator= (const point2d& a)
	{
            x = a.x;
            y = a.y;
	}

	bool operator< (const point2d& a)
	{
        return (y < a.y ? true : false);
	}

    double distance (const point2d& p) 
    {        
        double dx = p.x - x;
        double dy = p.y - y;
        return sqrt (pow (dx, 2) + pow (dy, 2));
    }

    double dist_sqr (const point2d& p) 
    {        
        double dx = p.x - x;
        double dy = p.y - y;
        return pow (dx, 2) + pow (dy, 2);
    }
};


class EXPORT segment
{
public:
	point2d p1;
	point2d p2;
	
    segment (double x1, double y1, double x2, double y2)
    {
        p1.x = x1;
        p1.y = y1;
        p2.x = x2;
        p2.y = y2;
    }

    segment (point2d& a, point2d& b)
    {
        p1.x = a.x;	p1.y = a.y;
        p2.x = b.x;	p2.y = b.y;
    }
    
    void operator= (const segment& s)
    {
        p1 = s.p1; p2 = s.p2;
    }
    
	bool is_inside (point2d& p)
	{
		double xmax, xmin, ymax, ymin;

		if (p1.x > p2.x) 
	    {	
            xmax = p1.x; 	xmin =  p2.x;	
        }
		else
		{	
            xmax = p2.x;	xmin =  p1.x;	
        }
		if (p1.y > p2.y)     
		{	
            ymax = p1.y; 	ymin =  p2.y;	
        }
		else
		{	
            ymax = p2.y; 	ymin =  p1.y;	
        }
		if (xmin <= p.x && p.x <= xmax && ymin <= p.y && p.y <= ymax) 
			return true;
		else
			return false;	
	}

    bool intersects (const point2d& p3, int radius)
    {
        double u;

        //ACE_DEBUG( (LM_DEBUG, "check if (%d, %d) intersects with p1: (%d, %d) p2: (%d, %d)\n", (int)p3.x, (int)p3.y, (int)p1.x, (int)p1.y, (int)p2.x, (int)p2.y));

        // we should re-order p1 and p2's position such that p2 > p1
        double x1, x2, y1, y2;
        if (p2.x > p1.x)
        {
            x1 = p1.x;  y1 = p1.y;
            x2 = p2.x;  y2 = p2.y;
        }
        else
        {
            x1 = p2.x;  y1 = p2.y;
            x2 = p1.x;  y2 = p1.y;         
        }

        // formula from http://astronomy.swin.edu.au/~pbourke/geometry/sphereline/
        u = ((p3.x - x1) * (x2 - x1) + (p3.y - y1) * (y2 - y1)) /
            ((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
        
        if (u >= 0 && u <= 1)
        {
            double x = x1 + (x2 - x1) * u;
            double y = y1 + (y2 - y1) * u;
            
            bool result = ((point2d (x, y).distance (p3) <= (double)radius) ? true : false);
            
            /*
            if (result == true)
                ACE_DEBUG ((LM_DEBUG, "(%d, %d) intersects with p1: (%d, %d) p2: (%d, %d) at (%d, %d)\n", (int)p3.x, (int)p3.y, (int)x1, (int)y1, (int)x2, (int)y2, (int)x, (int)y));
            */
            return result; 
        }
        else
            return false;
    }
};

class EXPORT line2d
{
public:

    double  a,b,c;
    int     bisecting[2];
    int     vertexIndex[2];
    segment seg;
    
	line2d (double x1, double y1, double x2 , double y2)
        : seg(x1, y1, x2, y2)
	{
		if (y1 == y2) 
		{	
			a = 0; b = 1; c = y1;
		}
		else if (x1 == x2) 
		{
			a = 1; b = 0; c = x1;
		}
		else
		{
			double dx = x1 - x2; 
			double dy = y1 - y2;
			double m = dx / dy;
			a = -1 * m;
			b = 1;
			c = a*x1 + b*y1;
		}
	}

	line2d (double A=0, double B=0, double C=0)
		:a (A), b (B), c (C), seg(0, 0, 0, 0)
	{
		vertexIndex[0] = -1;
		vertexIndex[1] = -1;
	}

	bool intersection (line2d& l , point2d& p)
	{
		//The polynomial judgement
		double D = (a * l.b) - (b * l.a);
		if (D == 0) 
        {
			p.x = 0;
			p.y = 0;
			return false;
		}
		else
		{
			p.x=(c*l.b-b*l.c)/D/1.0;
			p.y=(a*l.c-c*l.a)/D/1.0;
            return true;
		}	
	}

    double distance (const point2d& p) 
    {        
        /*
        double u;

        // u = ((x3-x1)(x2-x1) + (y3-y1)(y2-y1)) / (p2.dist(p1)^2)
        u = ((p.x - seg.p1.x) * (seg.p2.x - seg.p1.x) + 
             (p.y - seg.p1.y) * (seg.p2.y - seg.p1.y)) /
             (seg.p1.distance (seg.p2))

        double x = seg.p1.x + u * (seg.p2.x - seg.p1.x);
        double y = seg.p1.y + u * (seg.p2.y - seg.p1.y);

        return p.distance (point2d (x, y));
        */

        return fabs (a * p.x + b * p.y + c) / sqrt (pow (a, 2) + pow (b, 2));
    }
};

/*
class rect
{
public:
	point2d vertex[4];
	line2d  lines[4];

	rect (point2d& c, unsigned int w, unsigned int h)
		:center (c), width (w), height(h)
	{
		calculateVertex();
	}

	bool is_inside (point2d& p)
	{
	    return vertex[1].x >= p.x && vertex[3].x <= p.x &&
               vertex[1].y >= p.y && vertex[3].y <= p.y;
	}

	void setCenter (point2d& np)
	{
		center.x = np.x;
		center.y = np.y;
		calculateVertex();
	}
	
	point2d getCenter ()
	{
		return center;
	}

	unsigned int getWidth()
	{
		return width;
	}
	
	unsigned int getHeight()
	{
		return height;
	}

	void setWidth (unsigned int nw)
	{
		width = nw;
		calculateVertex();
	}

	void setHeight (unsigned int nh)
	{
		height = nh;
		calculateVertex();
	}

protected:

    point2d center;
    int     width, height;

	void calculateVertex()
	{
		vertex[0].x = center.x - width/2;
		vertex[0].y = center.y + height/2;
		vertex[1].x = center.x + width/2;
		vertex[1].y = center.y + height/2;
		vertex[2].x = center.x + width/2;
		vertex[2].y = center.y - height/2;
		vertex[3].x = center.x - width/2;
		vertex[3].y = center.y - height/2;
		
		lines[0].a = 0 ; lines[0].b = 1 ; lines[0].c = vertex[1].y;
		lines[1].a = 1 ; lines[1].b = 0 ; lines[1].c = vertex[1].x;	
		lines[2].a = 0 ; lines[2].b = 1 ; lines[2].c = vertex[3].y;
		lines[3].a = 1 ; lines[3].b = 0 ; lines[3].c = vertex[3].x;
	}

private:

};
*/


} // end namespace Vast

#endif // TYPEDEF_H

