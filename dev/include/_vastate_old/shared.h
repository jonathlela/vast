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
 *  ver 0.1 (2006/07/18)
 *   
 */

#ifndef VASTATE_SHARED_H
#define VASTATE_SHARED_H

#ifdef WIN32
#pragma warning(disable: 4786)  // disable warning about "identifier exceeds'255' characters"
#endif


#include <map>

// TODO: reliable/unreliable flag for each attribute
//       hidden object for user position

#include "vastverse.h"
#include <vector>

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


    // a list of attributes used by an object or an event
    class attributes
    {
    public:
        attributes ()
        {
            type = 0;
            dirty = false;
            version = 0;            
        }

        virtual ~attributes ()
        {
        }

        // return the size of the attribute list
        virtual int size () = 0;
        
        // store a new attribute into the list
        // returns the index within the list
        virtual int add (bool   value) = 0;
        virtual int add (int    value) = 0;
        virtual int add (float value) = 0;
        virtual int add (string value) = 0;
        virtual int add (vec3_t value) = 0;
        
        // get the attribute value by index
        virtual bool get (int index, bool   &value) = 0;
        virtual bool get (int index, int    &value) = 0;
        virtual bool get (int index, float &value) = 0;     
        virtual bool get (int index, string &value) = 0;
        virtual bool get (int index, vec3_t &value) = 0;
        
        // replace the existing value of an attribute by index (dirty flag will be set)
        virtual bool set (int index, bool   value) = 0;
        virtual bool set (int index, int    value) = 0;
        virtual bool set (int index, float value) = 0;        
        virtual bool set (int index, string value) = 0;
        virtual bool set (int index, vec3_t value) = 0; 

        // encode all current values into a byte string
        //virtual string &pack_all () = 0;
        virtual int pack_all (char **) = 0;
        
        // encode only those that have been updated (unset all dirty flag)
        virtual int pack_dirty (char **) = 0;
        
        // unpack an encoded bytestring
        virtual bool unpack (char *str) = 0;

        // returns the size of the attributes, if packed as a string
        //virtual bool packsize () = 0;
                
        byte_t      type;           // type = 1 is a character creation event (?)
        bool        dirty;          // right now any updated attribute will resend whole
        version_t   version;
    };
    
    // a list of attributes used by an object or an event
    class attributes_impl : public attributes
    {
    public:
        attributes_impl ()
        {
            _size = 0;
        }

        ~attributes_impl ()
        {
        }
        
        // return the size of the attribute list
        int size ()
        {
            return _size;
        }
        
        // store a new attribute into the list
        // returns the index within the list
        int add (bool   value);
        int add (int    value);
        int add (float  value);
        int add (string value);
        int add (vec3_t value);
        
        // get the attribute value by index
        bool get (int index, bool   &value);
        bool get (int index, int    &value);
        bool get (int index, float  &value);     
        bool get (int index, string &value);
        bool get (int index, vec3_t &value);
        
        // replace the existing value of an attribute by index (dirty flag will be set)
        bool set (int index, bool   value);
        bool set (int index, int    value);
        bool set (int index, float  value);        
        bool set (int index, string value);
        bool set (int index, vec3_t value); 
        
        // encode all current values into a byte string
        // returns 0 for error        
        //string &pack_all ();
        int pack_all (char **);
        
        // encode only those that have been updated (unset all dirty flag)
        int pack_dirty (char **);
        
        // unpack an encoded bytestring
        bool unpack (char *str);      
           
        bool is_dirty (int index)
        {
            return _dirty[index];
        }

        void reset_dirty ()
        {
            for (int i=0; i<(int)_dirty.size (); ++i)
                _dirty[i] = false;
            dirty = false;            
        }
        
         bool get (int index, void **ptr, int &length)
         {
             length = _length[index];
             *ptr = (void *)start_ptr (index);
             return true;
         }

    private:
        int             _size;                
        vector<byte_t>  _types;         // 16 bytes in memory
        string          _data;          // 16 bytes in memory
        vector<word_t>  _length;
        vector<bool>    _dirty;         // per attribute dirty flag

        inline int start_index (int index)
        {
            // get to the beginning of data
            int i, count = 0;
            for (i=0; i<index; ++i)
                count += _length[i];            
            return count;
        }

        inline const char *start_ptr (int index)
        {
            return _data.c_str() + start_index (index);
        }

        void printdata () 
        {
            for (int i=0; i<(int)_data.length (); i++)
                printf ("%3u ", (unsigned char)_data[i]);
            printf ("\n");
        }
    };
    
    class event : public attributes_impl
    {
    public:
        event () 
        {
        }

        event (id_t sender, event_id_t event_id, timestamp_t occur_time)
            :_sender(sender), _id(event_id), _timestamp (occur_time)
        {
        }

        event_id_t get_id ()
        {
            return _id;
        }

        id_t get_sender ()
        {
            return _sender;
        }

        timestamp_t get_timestamp ()
        {
            return _timestamp;
        }

        // encode an event to a buffer, return the number of bytes encoded
        int encode (char *buf)
        {
            Msg_EVENT info;

            // encode this event
            info.type       = type;
            info.version    = version;
            info.id         = _id;
            info.sender     = _sender;
            info.timestamp  = _timestamp;

            // copy data in event class
            memcpy (buf, (void *)&info, sizeof (Msg_EVENT));        
        
            // copy entire bytestring of the attribute values
            char *p = buf + sizeof (Msg_EVENT);
        
            return (sizeof (Msg_EVENT) + this->pack_all (&p));
        }

        // decode an serialized event back to this class
        bool decode (char *buf)
        {
            Msg_EVENT info;

            memcpy (&info, buf, sizeof (Msg_EVENT));
            type        = info.type;
            version     = info.version;
            _id         = info.id;
            _sender     = info.sender;
            _timestamp  = info.timestamp;

            return this->unpack (buf + sizeof (Msg_EVENT));
        }       

    private:
        event_id_t  _id;
        id_t        _sender;
        timestamp_t _timestamp;
    };
        
    class object : public attributes_impl
    {
    public:
        object (obj_id_t id)
            :_id(id), peer (0), pos_dirty(false), visible(false), pos_version (0), _alive(true)
        {
            dirty = false;
        }

        ~object ()
        {
        }

        obj_id_t get_id ()
        {
            return _id;
        }
        
        void set_pos (Position &pos)
        {
            _pos = pos;
            pos_dirty = true;
        } 

        Position &get_pos ()
        {
            return _pos;
        }
                
        void mark_deleted ()
        {
            _alive = false;
        }

		bool is_alive ()
		{
			return _alive;
		}

        bool is_AOI_object (Node &n, bool add_buffer = false)
        {
            if (add_buffer)
                return n.pos.dist (_pos) <= ((double)n.aoi * VASTATE_BUFFER_MULTIPLIER);
            else
                return n.pos.dist (_pos) <= n.aoi;
        }

        char *tostring ()
        {
            static char str[9];
            ID_STR::tostring (_id, str);
            return str;
        }
            
        // encode an object header, returns the # of bytes encoded
        int encode_pos (char *buf, bool dirty_only, bool is_request = false)
        {
            if (dirty_only == true && pos_dirty == false)
                return 0;
            
            Msg_OBJECT info;
            info.obj_id      = _id;
            info.pos         = _pos;
            info.pos_version = pos_version;
            //info.version     = version;
            info.peer        = peer;
            info.is_request  = is_request;
            
            // store the object part        
            memcpy (buf, &info, sizeof(Msg_OBJECT));
            
            return sizeof (Msg_OBJECT);
        }   

        bool decode_pos (char *buf)
        {
            Msg_OBJECT info;
            memcpy (&info, buf, sizeof (Msg_OBJECT));

            if (_id != info.obj_id)
                return false;

            // if the update is not newer then we don't apply
            if (info.pos_version < pos_version)
                return false;

            // decode_pos for a mark_deleted object, make it alive
            _alive = true;

            _pos        = info.pos;
            peer        = info.peer;     // TODO: don't do this every time, redundent
            //version     = info.version;
            //pos_version = info.pos_version;
            pos_dirty   = true;

            return true;
        }

        // encode an object states
        // returns the # of bytes encoded
        // packing order: 
        //      1.    Msg_STATE
        //      2.    encoded bytestring (variable length)        
        int encode_states (char *buf, bool dirty_only, bool is_request = false)
        {        
            // store the attribute part        
            char *p = buf + sizeof (Msg_STATE);
            
            Msg_STATE info;  
            info.obj_id     = _id;
            info.version    = version;
            info.size       = (dirty_only ? this->pack_dirty (&p) : this->pack_all (&p));
            info.is_request = is_request;
            
            if (info.size == 0)
                return 0;
            
            memcpy (buf, &info, sizeof (Msg_STATE));
            
            return sizeof (Msg_STATE) + info.size;
        }
        
        bool decode_states (char *buf)
        {
            Msg_STATE info;
            memcpy (&info, buf, sizeof (Msg_STATE));

            // if the update is not newer then we don't apply
            if (info.version < version)
                return false;

            if (this->unpack (buf + sizeof (Msg_STATE)) == true)
            {
                this->dirty = true;
                return true;
            }
            return false;
        }
                    
        id_t        peer;
        bool        pos_dirty;    // new position has been given		
        bool		visible;
		version_t   pos_version;

    private:
        obj_id_t    _id;
        Position    _pos;        
        
        bool        _alive;      
    };

    //
    // message for transport over network
    //

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

#endif // VASTATE_SHARED_H