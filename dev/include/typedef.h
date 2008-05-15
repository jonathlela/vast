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


#ifndef TYPEDEF_H
#define TYPEDEF_H

#include <string.h> // memcpy
#include <math.h>

#include <string>
#include <sstream>
#include "config.h"

namespace VAST
{

//typedef unsigned short int id_t;
//typedef unsigned short int aoi_t;
//typedef long          aoi_t;          // NOTE: do not use unsigned, as AOI could get negative in dAOI
//typedef unsigned long timestamp_t;    // short: 0 - 65535  

typedef unsigned long  id_t;
typedef short          aoi_t;          // NOTE: do not use unsigned, as AOI could get negative in dAOI
typedef unsigned short timestamp_t;    // short: 0 - 65535  
typedef unsigned char  byte_t;
typedef unsigned short word_t;

typedef unsigned char msgtype_t;

typedef struct {
    float x, y, z;
} vec3_t;

// common message types
typedef enum VAST_Message
{
    DISCONNECT = 0,         // disconnection without action: leaving overlay or no longer overlapped
    ID,                     // id for the host
    IPQUERY,                // query IP of a specified node
    PAYLOAD,
};

// bandwidth type define
typedef enum bandwidth_type_t
{
    BW_UNKNOWN = 0,
    BW_UPLOAD, 
    BW_DOWNLOAD
};

class EXPORT Position 
{

public:
    Position () 
        : x(0), y(0)
    {
    }

    Position (double x_coord, double y_coord)
        : x(x_coord), y(y_coord)
    {
    }

    Position (char const *p) 
    {
        memcpy (this, p, sizeof(Position));
    }
            
    double dist (Position const &p) const
    {        
        double dx = ((double)p.x - (double)x);
        double dy = ((double)p.y - (double)y);
        return sqrt (pow (dx, 2) + pow (dy, 2));
    }

    bool operator== (Position const &p)
    {
        return (x == p.x && y == p.y);
    }

    bool operator!= (Position const &p)
    {
        return (x != p.x || y != p.y);
    }

    double x;
    double y;
    //double z;
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

    void get_string (char *p)
    {
        sprintf (p, "%d.%d.%d.%d", (int)((host>>24) & 0xff), (int)((host>>16) & 0xff), (int)((host>>8) & 0xff), (int)(host & 0xff));
    }

    void parseIP (const std::string & instr)
    {
        IPaddr::parseIP (*this, instr);
    }

    static int parseIP (IPaddr & addr, const std::string & instr)
    {
        if (instr.find (":") == -1)
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
 *  someone is behind of NAT should be
 *    140.112.172.11:23|192.168.5.8:22018
 */
class EXPORT Addr
{
    
public:
    // should this correct? define an id in Addr and in typedef.h
    id_t   id;
    IPaddr publicIP;
    IPaddr privateIP;

    Addr ()
    {
        //zero ();
        id = 0; // = NET_ID_UNASSIGNED;
    }

    ~Addr ()
    {
    }

    void zero ()
    {
        memset (this, 0, sizeof (Addr));
    }

    void set_public (unsigned long i, unsigned short p)
    {
        publicIP.host = i;
        publicIP.port = p;
    }
    
    void set_private (unsigned long i, unsigned short p)
    {
        privateIP.host = i;
        privateIP.port = p;
    }
    
    const std::string & to_string ()
    {
        static std::string str;

        // temp variables to help output
        std::ostringstream oss;
        char tmpstr[4*4];   // max 3 digit + 1 dot and 4 fields totally

        str.empty ();

        publicIP.get_string (tmpstr);
        oss << tmpstr << ":" << publicIP.port;

        if (privateIP.port != 0)
        {
            str.append ("|");
            privateIP.get_string (tmpstr);
            oss << tmpstr << ":" << privateIP.port;
        }

        str = oss.str ();

        return str;
    }

    // return 0 on success
    int from_string (const std::string & str)
    {
        zero ();

        size_t spt_pos = str.find ("|");
        // find no "|" mark
        if (spt_pos == -1)
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
        : pos(0,0), id(0), aoi(0),time(0)
    {
    }

    Node (id_t i, aoi_t a, Position &p, timestamp_t t = 0)
        : pos(p), id(i), aoi(a), time(t)
    {
    }

    Node (const Node & n)
        : pos(n.pos), id (n.id), aoi(n.aoi), time(n.time)
    {
    }

    ~Node ()
    {
    }

    void update (Node const &rhs)
    {
        id = rhs.id;
        aoi = rhs.aoi;
        pos = rhs.pos;
        time = rhs.time;
    }

    Node& operator= (const Node& n)
    {
        id = n.id;
        pos = n.pos;
        aoi = n.aoi;
        time = n.time;
        return *this;
    }

    /*
    bool contains (Position p)
    {
        return pos.dist (p) <= (double)aoi;
    }
    */

    Position        pos;        // current position of the node
    id_t            id;         // unique id for the node    
    aoi_t           aoi;        // current AOI-radius of the node
    timestamp_t     time;       // valid time for current position
    //Addr            addr;       // IP address for this node (TODO: to be removed?)
};


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

    double dist (const point2d& p) 
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
	
    /*
    segment () 
    {
        p1.x = p1.y = p2.x = p2.y = 0;
    }
    */
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
            
            bool result = ((point2d (x, y).dist (p3) <= (double)radius) ? true : false);
            
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

    double dist (const point2d& p) 
    {        
        /*
        double u;

        // u = ((x3-x1)(x2-x1) + (y3-y1)(y2-y1)) / (p2.dist(p1)^2)
        u = ((p.x - seg.p1.x) * (seg.p2.x - seg.p1.x) + 
             (p.y - seg.p1.y) * (seg.p2.y - seg.p1.y)) /
             (seg.p1.dist (seg.p2))

        double x = seg.p1.x + u * (seg.p2.x - seg.p1.x);
        double y = seg.p1.y + u * (seg.p2.y - seg.p1.y);

        return p.dist (point2d (x, y));
        */

        return fabs (a * p.x + b * p.y + c) / sqrt (pow (a, 2) + pow (b, 2));
    }
};

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



} // end namespace VAST

#endif // TYPEDEF_H

