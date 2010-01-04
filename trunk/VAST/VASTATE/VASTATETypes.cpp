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

#include "VASTATETypes.h"
#include <iostream>
#include <ctime>

using namespace std;

namespace Vast
{   

    char VASTATE_MESSAGE[][20] = 
    {
        "none",
        "LOGIN",
        "LOGOUT",
        "JOIN",
        "LEAVE",
        "EVENT",
        "MOVEMENT",
        "OBJECT_C",
        "OBJECT_D",
        "OBJECT_R",
        "POSITION",
        "STATE",
        "AGENT_MSG",
        "GATEWAY_MSG",
        "OWNER",
        "REJOIN",
        "NEWOWNER",
        "TRANSFER",
        "TRANSFER_ACK",
        "ARBITRATOR",
        "CAPACITY",
        "PROMOTE",
        "ARBITRATOR_C",
        "ARBITRATOR_J",
        "OVERLOAD_M",
        "OVERLOAD_I",
        "UNDERLOAD",   
        "SWITCH",
        "STAT",
    };


    //
    // UID implementation
    //
    UID::UID ()
    {
        setZero ();
    }
    
    UID::UID ( const UID& r )
    {
        B32* a = (B32*)_data;
        B32* b = (B32*)r._data;
        a[0] = b[0];
        a[1] = b[1];
        a[2] = b[2];
        a[3] = b[3];
    }
    
    UID::UID ( const B08* b )
    {
        fromBytes (b);
    }
    
    
    UID::UID ( const char* s )
    {
        if ( !fromString (s) )
            setZero ();
    }

    UID::UID (B32 id)
    {
        setZero ();
        memcpy (_data, (char *)&id, sizeof (B32));
    }
    
    UID::~UID ()
    { 
    }
    
    void UID::setZero ()
    {
        B32* p = (B32*)_data;
        p[0] = 0;
        p[1] = 0;
        p[2] = 0;
        p[3] = 0;
    }
    
    UID& UID::operator= ( const UID& r )
    {
        B32* a = (B32*)_data;
        B32* b = (B32*)r._data;
        a[0] = b[0];
        a[1] = b[1];
        a[2] = b[2];
        a[3] = b[3];

        //cout << "UID assign R=" << b[0] << b[1] << b[2] << b[3] <<endl;
        //cout << "            L=" << a[0] << a[1] << a[2] << a[3] <<endl;
        return *this;
    }
    
    bool UID::operator== ( const UID& r ) const
    {
        B32* a = (B32*)_data;
        B32* b = (B32*)r._data;
        //cout << "UID == R=" << b[0] << b[1] << b[2] << b[3] <<endl;
        //cout << "        L=" << a[0] << a[1] << a[2] << a[3] <<endl;
        return ( (a[0]==b[0]) & (a[1]==b[1]) & (a[2]==b[2]) & (a[3]==b[3]) );
    }
    
    bool UID::operator!= ( const UID& r ) const
    {
        B32* a = (B32*)_data;
        B32* b = (B32*)r._data;
        //cout << "UID != R=" << b[0] << b[1] << b[2] << b[3] <<endl;
        //cout << "        L=" << a[0] << a[1] << a[2] << a[3] <<endl;
        return ( (a[0]!=b[0]) | (a[1]!=b[1]) | (a[2]!=b[2]) | (a[3]!=b[3]) );
    }
    
    bool UID::operator< ( const UID &r ) const
    {
        B32 end = UID_BYTE_LEN - 1;
        for (B32 i = 0; i < end; ++i)
            if ( _data[i] != r._data[i] )
                return ( _data[i] < r._data[i] );
        return ( _data[end] < r._data[end] );
    }
    
    bool UID::operator> ( const UID &r ) const
    {
        B32 end = UID_BYTE_LEN - 1;
        for (B32 i = 0; i < end; ++i)
            if ( _data[i] != r._data[i] )
                return ( _data[i] > r._data[i] );
        return ( _data[end] > r._data[end] );
    }
    
    static const char *sym = "0123456789abcdef";
    static const int  counts[] = { 4, 2, 2, 2, 6 };
    inline int H2B (char h)
    {
        if (h >= '0' && h <= '9') return h - '0';
        if (h >= 'A' && h <= 'F') return h - 'A' + 10;
        if (h >= 'a' && h <= 'f') return h - 'a' + 10;
        return -1;
    }

    inline int HB2B (const char *hex)
    {
        int hi = H2B(*hex);
        if (hi == -1) return -1;
        ++hex;

        int low = H2B(*hex);
        if (low == -1) return -1;
        return (hi << 4) | low;
    }
   
    bool UID::fromString (const char *src)
    {        
        int offset = 0;
        int cnt = 0;

        for (int i = 0; i < 5; ++i)
        {
            for (int j = 0; j < counts[i]; ++j)
            {
                int r = HB2B (src+cnt);
                if (r == -1)
                    return false;

                _data[offset++] = static_cast<unsigned char> (r);
                cnt += 2;
            }

            if (cnt<36 && src[cnt] != '-')
                return false;
            cnt += 1;
        }

        return true;
    }
    
    void UID::fromBytes ( const B08* bytes )
    {
        memcpy (_data, bytes, UID_BYTE_LEN);
    }
    
    char* UID::toBytes () const
    {
        return (char*)_data;
    }

    std::string _UIDstring;
    const string& UID::toString() const /*/< convert to UID string */
    {        
        _UIDstring.clear ();
        for (int i = 0; i < 16; ++i)
        {
            _UIDstring.append (1, sym[_data[i] >> 4] );
            _UIDstring.append (1, sym[_data[i] & 0x0f]);
            if (i==3 || i==5 || i==7 || i==9)
                _UIDstring.append( 1, '-' );
        }

        return _UIDstring;
    }

    // encode an to a buffer, return the number of bytes encoded
    // buffer can be NULL (simply to query the total size)
    // returns the total size of the packed class
    size_t UID::serialize (char *buf) const
    {
        if (buf != NULL)
            memcpy (buf, _data, UID_BYTE_LEN);

        return UID_BYTE_LEN;
    }

    // decode an serialized Event back to this class
    // returns number of bytes restored (should be > 0 if correct)
    size_t UID::deserialize (const char *buf)
    {
        fromBytes ((B08*)buf);
        return UID_BYTE_LEN;
    }

    // size of this class, must be implemented
    size_t UID::sizeOf () const
    {
        return UID_BYTE_LEN;
    }

    UID UID::rand4 () 
    {
        static char buf[36];
        static bool seed = false;
        if (!seed)
        {
            seed = true;
            srand ( (unsigned int)time (NULL) );
        }
    
        for (int i = 0; i < 36; ++i)
        {
            switch (i)
            {
            case 8:
            case 13:
            case 18:
            case 23:
                buf[i] = '-';
                break;
            case 14:
            case 19:
                break;
            default:
                buf[i] = sym[rand () & 15];
                break;
            }
        }

        buf[14] = '4'; // version
        buf[19] = sym[8 | (rand () & 3)];        
        return UID (buf);
    }

    // 
    // AttributeImpl
    //

    AttributesImpl::AttributesImpl ()
    {
        _size = 0;
    }

    // copy constructor
    AttributesImpl::AttributesImpl (AttributesImpl const &a)
    {
        _size   = a._size;
        _types  = a._types;
        _data   = a._data;
        _length = a._length;
        _dirty  = a._dirty;

        type    = a.type;
        dirty   = a.dirty;
        version = a.version;
    }

    AttributesImpl::~AttributesImpl ()
    {
        _types.clear ();
        _length.clear ();
        _dirty.clear ();
        _data.clear ();
    }


    // return the size of the attribute list
    byte_t AttributesImpl::size ()
    {
        return _size;
    }

    // store a new attribute into the list
    // returns the index within the list
    int AttributesImpl::add (bool value)
    {
        _data += value;   
        _types.push_back (VASTATE_ATTRIBUTE_TYPE_BOOL);
        _length.push_back (sizeof(bool));
        _dirty.push_back (true);
        _size++;
        dirty = true;
        
        // printData ();
        return 0;
    }

    int AttributesImpl::add (int value)
    {        
        char *p = (char *)&value;
        for (size_t i=0; i < sizeof (int); i++)
            _data += p[i];

        _types.push_back (VASTATE_ATTRIBUTE_TYPE_INT);
        _length.push_back (sizeof(int));
        _dirty.push_back (true);
        _size++;
        dirty = true;     
        
        // printData ();
        return 0;
    }

    int AttributesImpl::add (float value)
    {
        char *p = (char *)&value;
        for (size_t i=0; i < sizeof (float); i++)
            _data += p[i];        
        
        _types.push_back (VASTATE_ATTRIBUTE_TYPE_FLOAT);
        _length.push_back (sizeof (float));
        _dirty.push_back (true);
        _size++;
        dirty = true;        

        // printData ();
        return 0;
    }

    int AttributesImpl::add (const string &value)
    {
        // check size not exceeding a word
        if (value.length () >= 65536)
            return (-1);

        _data += value;                
        
        /* equivalent effects
        const char *s = value.c_str ();
        for (int i=0; i<value.length (); i++)
            _data += s[i];  
        */

        _types.push_back (VASTATE_ATTRIBUTE_TYPE_STRING);
        _length.push_back ((word_t)value.length ());        
        _dirty.push_back (true);
        _size++;
        dirty = true;        

        // printData ();
        return 0;
    }

    int AttributesImpl::add (const Position &value)
    {
        char *p = (char *)&value;
        for (size_t i=0; i<sizeof(Position); i++)
            _data += p[i];
        
        _types.push_back (VASTATE_ATTRIBUTE_TYPE_VEC3);
        _length.push_back (sizeof(Position));
        _dirty.push_back (true);
        _size++;
        dirty = true;        

        // printData();
        return 0;
    }
    
    // get the attribute value by index
    bool AttributesImpl::get (int index, bool &value)
    {
        if (index <0 || index >= _size || _types[index] != VASTATE_ATTRIBUTE_TYPE_BOOL)
            return false;

        memcpy (&value, startPointer (index), sizeof (bool));
        
        return true;
    }
    bool AttributesImpl::get (int index, int &value)
    {
        if (index <0 || index >= _size || _types[index] != VASTATE_ATTRIBUTE_TYPE_INT)
            return false;
        
        memcpy (&value, startPointer (index), sizeof (int));

        return true;
    }
    bool AttributesImpl::get (int index, float &value)
    {
        if (index <0 || index >= _size || _types[index] != VASTATE_ATTRIBUTE_TYPE_FLOAT)
            return false;
        
        memcpy (&value, startPointer (index), sizeof (float));
        
        return true;
    }
    bool AttributesImpl::get (int index, string &value)
    {
        //static string nstr;        
        if (index <0 || index >= _size || _types[index] != VASTATE_ATTRIBUTE_TYPE_STRING)            
            return false;                
        
        //nstr = _data.substr (startIndex (index), _length[index]);        
        //value = nstr;

        value = _data.substr (startIndex (index), _length[index]);
        
        return true;
    }

    bool AttributesImpl::get (int index, Position &value)
    {
        if (index <0 || index >= _size || _types[index] != VASTATE_ATTRIBUTE_TYPE_VEC3)
            return false;
        
        memcpy (&value, startPointer(index), sizeof(Position));

        return true;
    }

    bool AttributesImpl::get (int index, void **ptr, word_t &length)
    {
        length = _length[index];
        *ptr = (void *)startPointer (index);
        return true;
    }
    
    // replace the existing value of an attribute by index (dirty flag will be set)
    bool AttributesImpl::set (int index, bool value)
    {
        if (index <0 || index >= _size || _types[index] != VASTATE_ATTRIBUTE_TYPE_BOOL)
            return false;
                
        char *p = (char *)&value;
        int start = startIndex (index);
        for (size_t i=0; i<sizeof(bool); i++)
            _data[start+i] = p[i];

        _dirty[index] = true;
        dirty = true;        
        return true;
    }

    bool AttributesImpl::set (int index, int value)
    {
        if (index <0 || index >= _size || _types[index] != VASTATE_ATTRIBUTE_TYPE_INT)
            return false;
        
        //printData ();
        char *p = (char *)&value;
        int start = startIndex (index);
        for (size_t i=0; i<sizeof(int); i++)
            _data[start+i] = p[i];
        //printData ();

        _dirty[index] = true;
        dirty = true;        
        return true;
    }

    bool AttributesImpl::set (int index, float value)
    {
        if (index <0 || index >= _size || _types[index] != VASTATE_ATTRIBUTE_TYPE_FLOAT)
            return false;
        
        char *p = (char *)&value;
        int start = startIndex (index);
        for (size_t i=0; i<sizeof(float); i++)
            _data[start+i] = p[i];
        
        _dirty[index] = true;
        dirty = true;       
        return true;
    }
    
    bool AttributesImpl::set (int index, const string& value)
    {
        if (index < 0 || index >= _size || 
            _types[index] != VASTATE_ATTRIBUTE_TYPE_STRING ||
            value.length () >= 65536)
            return false;

        //printf ("original string -----\n");
        //printData ();
        
        int start = startIndex (index);
        _data.replace (start, _length[index], value.c_str ());
        _length[index] = (word_t)value.length ();        

        //printf ("reset string -----\n");
        //printData ();
                
        _dirty[index] = true;
        dirty = true;   
        return true;
    }
    bool AttributesImpl::set (int index, const Position &value)
    {
        if (index <0 || index >= _size || _types[index] != VASTATE_ATTRIBUTE_TYPE_VEC3)
            return false;
        
        char *p = (char *)&value;
        int start = startIndex (index);
        for (size_t i=0; i<sizeof(Position); i++)
            _data[start+i] = p[i];

        _dirty[index] = true;
        dirty = true;
        return true;
    }
        
    // encode attributes with optional 'dirty only' copy (which will unset all dirty flag)
    size_t AttributesImpl::pack (char **buf, bool dirty_only) const
    {
        int n = _types.size ();

        if (n == 0)
            return 0;

        // reserve 1+4 bytes to store # of entries and total pack_size
        char *p = (buf == NULL ? NULL : ((*buf) + 1 + sizeof (size_t)));

        // NOTE that certain states belonging to Attribute (version, pos_version, type, etc.) 
        // are not encoded here, as they will be stored & sent via Msg_EVENT or Msg_OBJECT

        Msg_ATTR_UPDATE record;
        
        int     n_records = 0;      // number of attributes stored
        size_t  pack_size = 0;      // total size of packed data
        
        // store info about each attribute
        for (byte_t i=0; i<n; ++i)
        {
            if (dirty_only && _dirty[i] == false)
                continue;

            record.index  = i;
            record.type   = _types[i];
            record.length = _length[i];
            
            if (p != NULL)
            {
                memcpy (p, &record, sizeof (Msg_ATTR_UPDATE));           
                p += sizeof (Msg_ATTR_UPDATE);
            }
            n_records++;
            pack_size += _length[i];
        }

        // if nothing to pack
        if (n_records == 0)
            return 0;

        //
        // copy actual data if buffer is provided
        //
        if (p != NULL)
        {
            // check to perform full copy
            if (n == n_records)
                memcpy (p, _data.c_str (), _data.length ());
    
            // otherwise selectively copy dirty fields
            else
            {
                // 'p' points to our buffer, 'd' points to the bytestring of this class
                char *d = (char *)_data.c_str ();
                for (int i=0; i<n; i++)
                {
                    if (_dirty[i] == false)
                    {
                        // NOTE: do not place 'd' advancement into the 3rd argument of for loop 
                        //       as it will execute for at least once before the loop begins
                        d += _length[i];
                        continue;
                    }
                                
                    memcpy (p, d, _length[i]);
                    
                    p += _length[i];            
                    d += _length[i];                
                }
            }
    
            // 1st byte stores # of attribute records, 2nd byte stores data length
            p = *buf;
            p[0] = (unsigned char)n_records;
            memcpy (p+1, &pack_size, sizeof (size_t));            
        }
              
        // return the packed data size
        return 1 + sizeof (size_t) + sizeof (Msg_ATTR_UPDATE) * n_records + pack_size;
    }
        
    // unpack an encoded bytestring 
    size_t AttributesImpl::unpack (char *str)
    {
        // retrieve # of update records and total length of bytestring
        int n    = str[0];
        size_t pack_size;
        memcpy (&pack_size, str+1, sizeof (size_t));

        // initialize pointers to attribute records and the bytestring
        char *p = str + 1 + sizeof (size_t);
        char *d = p + sizeof (Msg_ATTR_UPDATE) * n;

        Msg_ATTR_UPDATE update;

        // loop through each attribute record
        for (int i=0; i<n; ++i)
        {
            // extract update header
            memcpy (&update, p, sizeof (Msg_ATTR_UPDATE));

            // new attribute
            if (update.index >= _types.size ())
            {
                // this update has an index larger than existing record, considered corrupt
                if (update.index != _types.size ())
                    return 0;

                // insert new attribute                
                _types.push_back (update.type);
                _length.push_back (update.length);
                _dirty.push_back (true);
                
                _data.append (d, update.length);
                
                //for (int j=0; j<update.length; j++)
                //    _data += d[j];

                _size++;
            }
            // existing attribute
            else if (_types[update.index] == update.type)
            {
                // update the attribute
                if (_types[update.index] == VASTATE_ATTRIBUTE_TYPE_STRING)
                {                    
                    char *s = new char[update.length + 1];
                    memcpy (s, d, update.length);       
                    s[update.length] = 0;
                    
                    _data.replace (startIndex(update.index), _length[update.index], s);
                    delete[] s;                                                            

                    _length[update.index] = update.length;
                }
                else
                    memcpy ((void *)startPointer(update.index), d, update.length);
                
                _dirty[update.index] = true;                
            }
            // assume corruption
            else
                return 0;

            p += sizeof (Msg_ATTR_UPDATE);
            d += update.length;
        }

        // return total size unpacked / processed
        return d-str;
    }

    bool AttributesImpl::isDirty (int index)
    {
        return _dirty[index];
    }

    void AttributesImpl::resetDirty ()
    {
        for (size_t i=0; i < _dirty.size (); ++i)
            _dirty[i] = false;
        dirty = false;            
    }

    inline int AttributesImpl::startIndex (int index)
    {
        // get to the beginning of data
        int i, count = 0;
        for (i=0; i<index; ++i)
            count += _length[i];            
        return count;
    }

    inline const char *AttributesImpl::startPointer (int index)
    {
        return _data.c_str() + startIndex (index);
    }

    void AttributesImpl::printData () 
    {
        for (size_t i=0; i < _data.length (); i++)
            printf ("%3u ", (unsigned char)_data[i]);
        printf ("\n");
    }

    //
    // Event
    //

    id_t Event::getSender ()
    {
        return _sender;
    }

    timestamp_t Event::getTimestamp ()
    {
        return _timestamp;
    }

    // encode an Event to a buffer, return the number of bytes encoded
    size_t Event::serialize (char *buf) const
    {
        // returns expected encoded size only
        if (buf == NULL)
            return (sizeof (Msg_EVENT) + this->pack (NULL));

        Msg_EVENT info;

        // encode this event
        info.type       = type;
        info.version    = version;
        info.sender     = _sender;
        info.timestamp  = _timestamp;

        // copy data in Event class
        memcpy (buf, (void *)&info, sizeof (Msg_EVENT));        
    
        // copy entire bytestring of the attribute values
        char *p = buf + sizeof (Msg_EVENT);
    
        return (sizeof (Msg_EVENT) + this->pack (&p));
    }

    // decode an serialized Event back to this class
    size_t Event::deserialize (const char *buf)
    {
        Msg_EVENT info;

        memcpy (&info, buf, sizeof (Msg_EVENT));
        type        = info.type;
        version     = info.version;
        _sender     = info.sender;
        _timestamp  = info.timestamp;

        return this->unpack ((char *)buf + sizeof (Msg_EVENT));
    }

    // size of this class, must be implemented
    size_t 
    Event::sizeOf () const
    {
        return serialize (NULL);
    }

    //
    // Object
    //

    const obj_id_t &Object::getID () const
    {
        return _id;
    }
    
    void Object::setPosition (const Position &pos)
    {
        _pos = pos;
        pos_dirty = true;
    } 

    const Position & Object::getPosition () const
    {
        return _pos;
    }
            
    void Object::markDeleted ()
    {
        _alive = false;
    }

	bool Object::isAlive ()
	{
		return _alive;
	}

    bool Object::isAOIObject (Node &n, bool add_buffer)
    {
        if (add_buffer)
            return n.aoi.center.distance (_pos) <= ((double)n.aoi.radius * (1.0 + VASTATE_BUFFER_RATIO));
        else
            return n.aoi.center.distance (_pos) <= n.aoi.radius;
    }

    const char *Object::toString () const
    {

#ifdef UID_FOR_OBJECT_ID
        
        /*
        static char str[100];
        string s = _id.toString ();
        strcpy (str, s.c_str ());         
        */
        
        return _id.toString ().c_str ();
#else
        static char str[9];
        ID_STR::toString (_id, str);
        return str;
#endif
        

    }
        
    // encode an Object header, returns the # of bytes encoded
    size_t Object::encodePosition (Message &msg, bool dirty_only)
    {
        if (dirty_only == true && pos_dirty == false)
            return 0;
        
        Msg_OBJECT info;
        info.obj_id      = _id;
        info.pos         = _pos;
        info.pos_version = pos_version;          
        
        // store the Object part        
        msg.store ((char *)&info, sizeof(Msg_OBJECT));
        
        return sizeof (Msg_OBJECT);
    }   

    bool Object::decodePosition (Message &msg)
    {
        Msg_OBJECT info;
        msg.extract ((char *)&info, sizeof (Msg_OBJECT));

        if (_id != info.obj_id)
            return false;

        // if the update is not newer then we don't apply
        if (info.pos_version <= pos_version)
            return false;

        _pos        = info.pos;
        pos_version = info.pos_version;
        
        // decodePosition for a markDeleted object, make it alive
        //_alive      = true;
        pos_dirty   = true;

        return true;
    }

    // encode an Object states
    // returns the # of bytes encoded
    // packing order: 
    //      1.    Msg_STATE
    //      2.    encoded bytestring (variable length)  
    //
    size_t Object::encodeStates (Message &msg, bool dirty_only)
    {        
        // copy the header
        Msg_STATE info;
        info.obj_id     = _id;
        info.version    = version;
        info.size       = this->pack (NULL, dirty_only);

        if (info.size == 0)
            return 0;

        size_t total_size = sizeof (Msg_STATE) + info.size;

        // reserve size & specify new 'size' for Message
        msg.expand (total_size);

        // obtain current valid pointer within msg
        char *p = msg.addSize (0);

        memcpy (p, &info, sizeof (Msg_STATE));
        p += sizeof (Msg_STATE);

        // store the attribute part
        this->pack (&p, dirty_only);

        // adjust new size for msg
        msg.addSize (total_size);

        return total_size;
    }
    
    bool Object::decodeStates (Message &msg)
    {
        Msg_STATE info;
        msg.extract ((char *)&info, sizeof (Msg_STATE));
        
        // if the update is not newer then we don't apply
        if (info.obj_id != _id || info.version <= this->version)
            return false;

        // restore state data while checking if size matches
        if (this->unpack (msg.addSize (0)) != info.size)
            return false;

        // restore basic object info
        this->version = info.version;
        this->dirty = true;
        return true;
    }

    Object::Object () 
        :agent (0), pos_dirty (false), visible (false), pos_version (0), _id ((unsigned int)0), _alive (true)
    {        
        dirty = false;
    }

    Object::Object (obj_id_t &id)
        :agent (0), pos_dirty (false), visible (false), pos_version (0), _id (id), _alive (true)
    {
        dirty = false;
    }

    Object::Object (Object const &obj)
        :AttributesImpl (obj)
    {
        agent       = obj.agent;
        pos_dirty   = obj.pos_dirty;
        visible     = obj.visible;
        pos_version = obj.pos_version;

        _id         = obj._id;
        _pos        = obj._pos;
        _alive      = obj._alive;
    }

    Object::~Object ()
    {

    }
} // namespace Vast
