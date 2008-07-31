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

#include "precompile.h"
#include "shared.h"

using std::cout;
using std::endl;

namespace VAST {

// Static members for VValue
///////////////////////////////////////
    //MValue MValue::unknown_value (MValue::T_UNKNOWN);
    //MValue MValue::error_value   (MValue::T_ERROR);

// Coord3D output function
///////////////////////////////////////

    const std::string to_string (const Coord3D& p)
    {
        static string _s;
        std::ostringstream s;
        s.precision (6);
        s << fixed;
        s << "(" << p.x << "," << p.y << "," << p.z << ")";
        _s = s.str ();
        return _s;
    }

} /* end of namespace VAST */

///////////////////////////////////////////////////////////

namespace VAST
{   

    char VASTATE_MESSAGE[][20] = 
    {
        "VH_CONNECT", 
        "JOIN",
        "ENTER",
        "OBJECT",
        "STATE",
        "ARBITRATOR",
        "ARBITRATOR_LEAVE", 
        "EVENT",
        "TICK_EVENT",
        "OVERLOAD_M",
        "OVERLOAD_I",
        "UNDERLOAD", 
        "PROMOTE",
        "TRANSFER",
        "TRANSFER_ACK",
        "NEWOWNER",
        "S_QUERY",
        "S_REPLY", 
        "AGGREGATOR", 
        "MIGRATE", 
        "MIGRATE_ACK"
    };

    // store a new attribute into the list
    // returns the index within the list
    int attributes_impl::add (bool value)
    {
        _data += value;        
        _types.push_back (VASTATE_ATT_TYPE_BOOL);
        _length.push_back (sizeof(bool));
        _dirty.push_back (true);
        _size++;
        dirty = true;
        
        // printdata ();
        return 0;
    }

    int attributes_impl::add (int value)
    {        
        char *p = (char *)&value;
        for (int i=0; i < (int) sizeof(int); i++)
            _data += p[i];

        _types.push_back (VASTATE_ATT_TYPE_INT);
        _length.push_back (sizeof(int));
        _dirty.push_back (true);
        _size++;
        dirty = true;     
        
        // printdata ();
        return 0;
    }

    int attributes_impl::add (float  value)
    {
        char *p = (char *)&value;
        for (int i=0; i < (int) sizeof(int); i++)
            _data += p[i];        
        
        _types.push_back (VASTATE_ATT_TYPE_FLOAT);
        _length.push_back (sizeof(float));
        _dirty.push_back (true);
        _size++;
        dirty = true;        

        // printdata ();
        return 0;
    }

    int attributes_impl::add (string value)
    {
        _data += value;                
        
        /* equivalent effects
        const char *s = value.c_str ();
        for (int i=0; i<value.length (); i++)
            _data += s[i];  
        */

        _types.push_back (VASTATE_ATT_TYPE_STRING);
        _length.push_back (value.length ());
        _dirty.push_back (true);
        _size++;
        dirty = true;        

        // printdata ();
        return 0;
    }

    int attributes_impl::add (vec3_t value)
    {

        char *p = (char *)&value;
        for (int i=0; i < (int) sizeof(vec3_t); i++)
            _data += p[i];
        
        _types.push_back (VASTATE_ATT_TYPE_VEC3);
        _length.push_back (sizeof(vec3_t));
        _dirty.push_back (true);
        _size++;
        dirty = true;        

        // printdata();
        return 0;
    }
    
    // get the attribute value by index
    bool attributes_impl::get (int index, bool &value)
    {
        if (index <0 || index >= _size || _types[index] != VASTATE_ATT_TYPE_BOOL)
            return false;

        memcpy (&value, start_ptr (index), sizeof(bool));
        
        return true;
    }
    bool attributes_impl::get (int index, int &value)
    {
        if (index <0 || index >= _size || _types[index] != VASTATE_ATT_TYPE_INT)
            return false;
        
        memcpy (&value, start_ptr (index), sizeof(int));

        return true;
    }
    bool attributes_impl::get (int index, float  &value)
    {
        if (index <0 || index >= _size || _types[index] != VASTATE_ATT_TYPE_FLOAT)
            return false;
        
        memcpy (&value, start_ptr (index), sizeof(float));
        
        return true;
    }
    bool attributes_impl::get (int index, string &value)
    {
        /*
        if (index <0 || index >= _size || _types[index] != VASTATE_ATT_TYPE_STRING)
            return false;
        
        char *str = new char[_length[index]+1];
        memcpy (str, start_ptr (index), _length[index]);
        str[_length[index]] = 0;
        value = str;
        delete[] str;
        
        return true;
        */

        static string nstr;        
        if (index <0 || index >= _size || _types[index] != VASTATE_ATT_TYPE_STRING)            
            return false;                
        
        nstr = _data.substr(start_index (index), _length[index]);        
        value = nstr;
        
        return true;
    }

    bool attributes_impl::get (int index, vec3_t &value)
    {
        if (index <0 || index >= _size || _types[index] != VASTATE_ATT_TYPE_VEC3)
            return false;
        
        memcpy (&value, start_ptr(index), sizeof(vec3_t));

        return true;
    }
    
    // replace the existing value of an attribute by index (dirty flag will be set)
    bool attributes_impl::set (int index, bool value)
    {
        if (index <0 || index >= _size || _types[index] != VASTATE_ATT_TYPE_BOOL)
            return false;
                
        char *p = (char *)&value;
        int start = start_index (index);
        for (int i=0; i < (int) sizeof(bool); i++)
            _data[start+i] = p[i];

        _dirty[index] = true;
        dirty = true;        
        return true;
    }
    bool attributes_impl::set (int index, int value)
    {
        if (index <0 || index >= _size || _types[index] != VASTATE_ATT_TYPE_INT)
            return false;
        
        //printdata ();
        char *p = (char *)&value;
        int start = start_index (index);
        for (int i=0; i < (int) sizeof(int); i++)
            _data[start+i] = p[i];
        //printdata ();

        _dirty[index] = true;
        dirty = true;        
        return true;
    }
    bool attributes_impl::set (int index, float value)
    {
        if (index <0 || index >= _size || _types[index] != VASTATE_ATT_TYPE_FLOAT)
            return false;
        
        char *p = (char *)&value;
        int start = start_index (index);
        for (int i=0; i < (int) sizeof(float); i++)
            _data[start+i] = p[i];
        
        _dirty[index] = true;
        dirty = true;       
        return true;
    }
    
    bool attributes_impl::set (int index, string value)
    {
        if (index <0 || index >= _size || _types[index] != VASTATE_ATT_TYPE_STRING)
            return false;

        //printf ("original string -----\n");
        //printdata ();
        
        //char *p = (char *)&value;
        int start = start_index (index);
        _data.replace (start, _length[index], value.c_str ());
        _length[index] = value.length ();        

        //printf ("reset string -----\n");
        //printdata ();
                
        _dirty[index] = true;
        dirty = true;   
        return true;
    }
    bool attributes_impl::set (int index, vec3_t value)
    {
        if (index <0 || index >= _size || _types[index] != VASTATE_ATT_TYPE_VEC3)
            return false;
        
        char *p = (char *)&value;
        int start = start_index (index);
        for (int i=0; i < (int) sizeof(vec3_t); i++)
            _data[start+i] = p[i];

        _dirty[index] = true;
        dirty = true;
        return true;
    }
    
    // encode all current values into a byte string
    int attributes_impl::pack_all (char **buf)
    {
        int n = _types.size ();

        if (n == 0)
            return 0;

        char *p = *buf;
        Msg_ATTR_UPDATE record;
        
        // 1st byte stores # of attribute records, 2nd byte stores data length
        p[0] = n;
        p[1] = _data.size ();
        p += 2;

        // store info about each attribute
        for (int i=0; i<n; ++i)
        {
            record.index  = i;
            record.type   = _types[i];
            record.length = _length[i];

            memcpy (p, &record, sizeof (Msg_ATTR_UPDATE));
            p += sizeof (Msg_ATTR_UPDATE);
        }

        // copy actual data
        memcpy (p, _data.c_str (), _data.length ());
        
        //printf ("pack_all(): _data length: %d\n", _data.length());
        return 2 + sizeof (Msg_ATTR_UPDATE) * n + _data.length ();
    }

    
    // encode only those that have been updated (unset all dirty flag)
    int attributes_impl::pack_dirty (char **buf)
    {
        int n = _types.size ();
        
        if (n == 0)
            return 0;
        
        char *p = (*buf) + 2;
        Msg_ATTR_UPDATE record;
        
        int n_records = 0;
        int pack_size = 0;
        
        // store info about each attribute
        int i;
        for (i=0; i<n; ++i)
        {
            if (_dirty[i] == false)
                continue;

            record.index  = i;
            record.type   = _types[i];
            record.length = _length[i];
            
            memcpy (p, &record, sizeof (Msg_ATTR_UPDATE));
            
            p += sizeof (Msg_ATTR_UPDATE);
            n_records++;
        }

        if (n_records == 0)
            return 0;

        // copy actual data, 'p' points to our buffer, 'd' points to the bytestring of this class
        char *d = (char *)_data.c_str ();
        for (i=0; i<n; i++)
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
            pack_size += _length[i];
        }                

        // 1st byte stores # of attribute records, 2nd byte stores data length
        p = *buf;
        p[0] = n_records;
        p[1] = pack_size;
                       
        return 2 + sizeof (Msg_ATTR_UPDATE) * n_records + pack_size;
    }
        
    // unpack an encoded bytestring 
    bool attributes_impl::unpack (char *str)
    {
        // retrieve # of update records and total length of bytestring
        int n = str[0];
        //int size = str[1];

        // initialize pointers to attribute records and the bytestring
        char *p = str + 2;
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
                    return false;

                // insert new attribute                
                _types.push_back (update.type);
                _length.push_back (update.length);
                _dirty.push_back (true);
                
                for (int j=0; j<update.length; j++)
                    _data += d[j];

                _size++;
            }
            // existing attribute
            else if (_types[update.index] == update.type)
            {
                // update the attribute
                if (_types[update.index] == VASTATE_ATT_TYPE_STRING)
                {                    
                    char *s = new char[update.length + 1];
                    memcpy (s, d, update.length);       
                    s[update.length] = 0;
                    
                    _data.replace (start_index(update.index), _length[update.index], s);
                    delete[] s;                                                            

                    _length[update.index] = update.length;
                }
                else
                    memcpy ((void *)start_ptr(update.index), d, update.length);
                
                _dirty[update.index] = true;                
            }
            // assume corruption
            else
                return false;

            p += sizeof (Msg_ATTR_UPDATE);
            d += update.length;
        }

        return true;
    }

} // namespace VAST

