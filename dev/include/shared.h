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

/*
 *  vastate_shared.h -- VASTATE - shared basic components
 *
 *  ver 0.2 (2008/03/16)
 *   
 */

#ifndef VASTATE_SHARED_H
#define VASTATE_SHARED_H

#ifdef WIN32
#pragma warning(disable: 4786)  // disable warning about "identifier exceeds'255' characters"
#endif

#include "precompile.h"
//#include "vastverse.h"
#include "typedef.h"
#include "vastutil.h"

namespace VAST {

typedef unsigned char objecttype_t;
typedef unsigned char short_index_t;

class Coord3D
{
public:
    coord_t x, y, z;

    Coord3D () 
        :x (0), y (0), z (0) {}

    Coord3D (const Coord3D &c)
        :x(c.x), y(c.y), z(c.z) {}

    Coord3D (int x_coord, int y_coord, int z_coord)
        :x((coord_t)x_coord), y((coord_t)y_coord), z((coord_t)z_coord) {}

    Coord3D (coord_t x_coord, coord_t y_coord, coord_t z_coord)
        :x(x_coord), y(y_coord), z(z_coord) {}

    Coord3D &operator=(const Coord3D& c)
    {
        x = c.x;
        y = c.y;
        z = c.z;
        return *this;
    }

    bool operator==(const Coord3D& c) const
    {
        return (this->x == c.x && this->y == c.y && this->z == c.z);
    }

    Coord3D &operator+=(const Coord3D& c)
    {
        x += c.x;
        y += c.y;
        z += c.z;
        return *this;
    }

    Coord3D &operator-=(const Coord3D& c)
    {
        x -= c.x;
        y -= c.y;
        z -= c.z;
        return *this;
    }

    Coord3D &operator*=(double value)
    {
        x *= value;
        y *= value;
        z *= value;
        return *this;
    }

    Coord3D &operator/=(double c)
    {
        x = (coord_t)((double)x / c);
        y = (coord_t)((double)y / c);
        z = (coord_t)((double)z / c);
        return *this;
    }

    inline Coord3D operator+(const Coord3D& c) const
    {
        return Coord3D (x + c.x, y + c.y, z + c.z);
    }

    inline Coord3D operator-(const Coord3D& c) const
    {
        return Coord3D (x - c.x, y - c.y, z - c.z);
    }

    inline double dist (const Coord3D &c) const
    {
        return sqrt (distsqr(c));
    }

    inline double distsqr (const Coord3D &c) const
    {
        return (pow (c.x - x, 2) + pow (c.y - y, 2) + pow (c.z - z, 2));
    }
};

//// Game object model defination
///////////////////////////////////////
/*
 * with composite property between MValue, MSimpleValue, MContainer

        / MSimpleValue
MValue -
        \ MContainer - MBaseObject - VEObject
*/
class MValue
{
public:
    const static objecttype_t T_ERROR         = 0x00;
    const static objecttype_t T_UNKNOWN       = 0x01;
    const static objecttype_t T_CONTAINER     = 0x02;
    const static objecttype_t T_PREFIX_SIMPLE = 0x10;
    const static objecttype_t TS_INT          = T_PREFIX_SIMPLE | 0x01;
    const static objecttype_t TS_DOUBLE       = T_PREFIX_SIMPLE | 0x02;
    const static objecttype_t TS_STRING       = T_PREFIX_SIMPLE | 0x04;
    //const static std::string OBJECTTYPE_STR[] = {"T_UNKNOWN", "T_CONTAINER"};

    const static MValue null_value;
    static MValue unknown_value;

public:
    MValue (objecttype_t t = T_UNKNOWN)
        : _type (t)
    {}

    virtual ~MValue ()
    {
        std::cout << "~MValue ()" << endl;
    }

    inline
    objecttype_t getType () const
    {
        return _type;
    }

    /*
        Value operators:
        getValue ()
    */
    virtual int& getValue_int ()    {static int i=0; _not_impl (); return i;}

    /*
        Container Operators:
        getItem ()
        add ()
        remove ()
    */
    virtual MValue& getItem (VAST::short_index_t index)     {std::cout << "MValue::getItem ()" << endl; return *this;}

    virtual void add (VAST::short_index_t index, int value) {std::cout << "MValue::add ()" << endl;    _not_impl ();}
    virtual void remove (VAST::short_index_t index)         {std::cout << "MValue::remove ()" << endl; _not_impl ();}

    /*
        common operators:
        assign ()
    */
    virtual MValue& assign (const int m)            {std::cout << "MValue::assign (int)" << endl; _not_impl (); return *this;}
    virtual MValue& assign (const VAST::MValue& m)  {std::cout << "MValue::assign (MValue)" << endl; _not_impl (); return *this;}

    // operators
    //virtual operator int ()     {return getValue_int ();}
    virtual MValue& operator[] (short_index_t index)    {return getItem (index);}

    virtual MValue& operator= (const int& m)            {return assign(m);}
    virtual MValue& operator= (const MValue& m)         {return assign(m);}

    virtual std::string encodeToStr () const
    {
        std::cout << "MValue::encodeToStr ()" << endl;
        std::ostringstream s;
        s << "[t" << (int) _type << "]";
        return s.str ();
    }

protected:
    objecttype_t _type;    

    /* Debug functions */
protected:
    inline void _not_impl ()
    {
#ifdef DEBUG_DETAIL
        printf ("MValue: not implements the function.\n");
#endif
    }
};

// Basic value define
///////////////////////////////////////
class MSimpleValue_int : public MValue
{
public:
    MSimpleValue_int (int init_value)
        : MValue (MValue::TS_INT), _value (init_value)
    {}

    MSimpleValue_int (const MSimpleValue_int& sv)
        : MValue (MValue::TS_INT), _value (sv._value)
    {}

    virtual ~MSimpleValue_int ()
    {}

    int& getValue_int ()
    {
        return _value;
    }

    MValue& assign (const int m)
    {
        std::cout << "MSimpleValue_int::assign (int)" << endl;
        _value = m;
        return *this;
    }

    string encodeToStr () const
    {
        std::cout << "MSimpleValue_int::encodeToStr ()" << endl;
        std::ostringstream s;
        s << MValue::encodeToStr ();
        s << _value;
        return s.str ();
    }

protected:
    void *_value;
};

// Container
///////////////////////////////////////
class MContainer : public MValue
{
public:
    typedef std::map<VAST::short_index_t,MValue *> Box;

protected:
    Box _b;

public:
    MContainer ()
        : MValue (T_CONTAINER)
    {}

    ~MContainer ()
    {
        std::cout << "~MContainer ()" << endl;
        Box::iterator it = _b.begin ();
        for (; it != _b.end (); it ++)
            delete it->second;
    }

    MValue& getItem (VAST::short_index_t index)
    {
        std::cout << "MContainer::getItem ()" << endl;
        if (_b.find (index) == _b.end ())
            return unknown_value;

        return *(_b[index]);
    }

    MValue& assign (VAST::short_index_t index, const int& m)
    {
        std::cout << "MContainer::assign () " << endl;
        *(_b[index]) = m;
        return *this;
    }

    void add (VAST::short_index_t index, int value)
    {
        std::cout << "MContainer::add ("<< (int) index << "," << value << ")" << endl;
        bool b_exist;
        if ((b_exist = _b.find (index) != _b.end ()) && _b[index]->getType() == MValue::TS_INT)
            _b[index]->assign (value);
        else
            if (b_exist)
                delete (_b[index]);
            _b[index] = new MSimpleValue_int (value);
    }

    void remove (VAST::short_index_t index)
    {
        if (_b.find (index) != _b.end ())
            _b.erase (index);
    }

    std::string encodeToStr () const
    {
        std::cout << "MContainer::encodeToStr ()" << endl;
        std::ostringstream s;
        s << MValue::encodeToStr ();
        s << "{";
        s << "[s" << (unsigned long) _b.size () << "]";
        for (Box::const_iterator it = _b.begin (); it != _b.end (); it ++)
        {
            s << "[" << (int) it->first << "]" << it->second->encodeToStr ();
        }
        s << "}";
        return s.str ();
    }
};

class MBaseObject : public MContainer
{
public:
    // any property here? 
    MBaseObject ()  {   }
    ~MBaseObject () {   }

    std::string encodeToStr () const
    {
        std::cout << "MBaseObject::encodeToStr ()" << endl;
        return MContainer::encodeToStr ();
    }
};

class VEObject : public MBaseObject
{
public:
    Coord3D pos;

private:
    id_t    _id;

public:
    VEObject (const id_t &id, const Coord3D &init_pos)
        : pos (init_pos), _id (id) {}
    ~VEObject () {}
    std::string encodeToStr ()
    {
        return MBaseObject::encodeToStr ();
    }

    inline 
    id_t get_id () const
    {   return _id;  }
};

} /* end of namespace VAST */

#endif // VASTATE_SHARED_H


/////////////////////////////////////////////////////////////////////////////////////////////

#if (0 == 1)


// TODO: reliable/unreliable flag for each attribute
//       hidden object for user position

#include "vastverse.h"


#define ARBITRATOR_REPULSION_VELOCITY (5)

//#define VASTATE_AOI     (100)
#define VASTATE_BUFSIZ  (10240)

#define THRESHOLD_ARBITRATOR      80      // on capacity scale 1 - 100 (max capacity)
#define THRESHOLD_OVERLOAD        70      // on load scale 1 - 100 (max load)
#define THRESHOLD_UNDERLOAD       30      // on load scale 1 - 100 (max load)
#define THRESHOLD_TIMEOUT         12      // timesteps ~ 20 sec
#define THRESHOLD_EVENTTICK       1       // longest time hasn't event send to my enclosing neighbor
#define THRESHOLD_EXPIRING_OBJECT 60      // steps to deleting an un-ownered object
#define THRESHOLD_LOAD_COUNTING   21     // # of steps between two overload/underload regard as different event

#define COUNTDOWN_TAKEOVER      5       // # of random steps to wait before ownership takeover
#define COUNTDOWN_REMOVE_AVATAR 10      // # of steps to delete disconnected avatar object
#define COUNTDOWN_PROMOTE       20      // # of steps can insert a new arbitrator in the same area after an arbitrator inserted

#define VASTATE_ATT_TYPE_BOOL   1
#define VASTATE_ATT_TYPE_INT    2
#define VASTATE_ATT_TYPE_FLOAT  3
#define VASTATE_ATT_TYPE_STRING 4
#define VASTATE_ATT_TYPE_VEC3   5

#define VASTATE_BUFFER_MULTIPLIER    (1.02)

namespace VAST 
{   

    typedef unsigned long  event_id_t;
    typedef unsigned long  obj_id_t;
    typedef unsigned long  query_id_t;      // unique id for referencing a given query
    typedef unsigned short version_t;       // sequential ver# for obj/attribute updates
    
    class system_parameter_t
    {
    public:
        system_parameter_t ()
            :width (0), height (0), aoi (0)
        {
        }

        system_parameter_t (const system_parameter_t & n)
            : width (n.width), height (n.height), aoi (n.aoi)
        {
        }

        ~system_parameter_t ()
        {
        }

        system_parameter_t & operator= (const system_parameter_t & n)
        {
            width = n.width;
            height = n.height;
            aoi = n.aoi;
        }

        unsigned int width;
        unsigned int height;
        int          aoi;
    };

    // Notice: change message definations must also change string mapping in shared.cpp
    typedef enum VASTATE_Message
    {
        JOIN = 100,         // peer joins the VSP network
        ENTER,              // peer enters a new region
        OBJECT,             // Notification of object creation, destruction, update
        STATE,              // Notification of attribute creation and update
        ARBITRATOR,         // Allow peer to have initial and spare arbitrators to connect
        ARBITRATOR_LEAVE,   // Notification of an arbitrator goes to off-line
        EVENT,              // Notification of a peer-generated event
        TICK_EVENT,         // Tick to others for arbitrators has no event sent this step
        OVERLOAD_M,         // Overloaded arbitrator's request for moving closer
        OVERLOAD_I,         // Overloaded arbitrator's request for inserting new arbitrator
        UNDERLOAD,          // Underloaded arbitrator's request for helping others
        PROMOTE,            // Info for the overloaded and the newly promoted arbitrator.
        TRANSFER,           // Ownership transfer from old to new owner (arbitrator)
        TRANSFER_ACK,       // Acknowledgement of ownership transfer
        NEWOWNER,           // Auto ownership assumption if arbitrators fail.
        S_QUERY,            // query storage
        S_REPLY             // response sent by storage
    };

    class ID_STR 
    {        
    public:
        
        
        static void tostring (unsigned long id, char *str)
        {            
            static char c[] = "01234567890ABCDEF";
            sprintf (str, "%c%c%c%c%c%c%c%c", c[(id >> 28) & 0x0F], 
                c[(id >> 24) & 0x0F], 
                c[(id >> 20) & 0x0F], 
                c[(id >> 16) & 0x0F], 
                c[(id >> 12) & 0x0F], 
                c[(id >> 8) & 0x0F], 
                c[(id >> 4) & 0x0F], 
                c[id & 0x0F]);
            str[8] = 0;
        }
    };

    //
    // message package to travel on the network
    //

    class Msg_OBJECT
    {
    public:
        Msg_OBJECT ()
            :is_request(false)
        {
        }

        Msg_OBJECT (char *p)
        {
            memcpy (this, p, sizeof (Msg_OBJECT));
        }

        obj_id_t    obj_id;
        Position    pos;        
        id_t        peer;               // TODO: see if we can eliminiate this, as it doesn't change all the time
        //version_t   version;
        version_t   pos_version;
        bool        is_request;
    };
    
    class Msg_STATE
    {
    public:
        Msg_STATE ()
            :is_request(false)
        {
        }
        
        Msg_STATE (char *p)
        {
            memcpy (this, p, sizeof (Msg_STATE));
        }
        
        obj_id_t    obj_id;
        version_t   version;
        int         size;
        bool        is_request;
    };
    
    // used for packing/unpacking attributes
    class Msg_ATTR_UPDATE
    {
    public:        
        byte_t index;
        byte_t type;
        word_t length;
    };
    
    // used during ENTER, for a peer to notify its arbitrator which objects it already knows
    class Msg_OBJ_UPDATEINFO
    {
    public:
        obj_id_t    obj_id;
        version_t   version;
    };
    
    // used for TRASNFER, signalling which object versions a node already knows
    class Msg_NODE_UPDATEINFO
    {
    public:
        id_t        node_id;
        version_t   version;
    };


    class Msg_EVENT
    {
    public:
        byte_t      type;           // type = 1 is a character creation event (?)        
        version_t   version;

        event_id_t  id;             
        id_t        sender;
        timestamp_t timestamp;
    };


    class Msg_TRANSFER
    {
    public:
        Msg_TRANSFER ()
        {
        }

        Msg_TRANSFER (char * buf)
        {
            memcpy (this, buf, sizeof (Msg_TRANSFER));
        }

        ~Msg_TRANSFER ()
        {
        }

        obj_id_t obj_id;
        id_t     new_owner;
        id_t     orig_owner;
    };

    class Msg_NODE
    {
    public:
        Msg_NODE () 
        {
        }
        
        Msg_NODE (char const *p)
        {
            memcpy (this, p, sizeof(Msg_NODE));
        }
        
        Msg_NODE (Node &n, Addr &a, int c)
            :node(n), addr(a), capacity(c)
        {
        }
        
        void set (Node const &node, Addr const &addr, int c)
        {
            this->node     = node;
            this->addr     = addr;
            this->capacity = c;
        }
        
        Node        node;
        Addr        addr;    
        int         capacity;
    };  


} // end namespace VAST

#endif /* if (0 == 1) */
