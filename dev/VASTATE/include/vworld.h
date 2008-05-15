

/*
 * Virtual world model defination
 * vworld.h
 *
 * Ver 0.1 (2008/4/23)
 */

/* Csc 20080423
 *  All classes here has the prefix of "V" stands for object models 
        for Virtual World (Environment),
        and also distinguish from class/object in C++ 
    (e.g. if just named "object" or "value", can have meaning of object 
        instanted from any class or value of any variable)

 * with composite property between VValue, VSimpleValue, VContainer

        / VSimpleValue
VValue -
        \ VContainer - VObject - VEObject

VValueFactory - Factory class of whole object model

 */

#include "shared.h"

#ifndef _VASTATE_VWORLD_H
#define _VASTATE_VWORLD_H

//#define DEBUG_ENCODING_STDOUT 1

namespace VASTATE {

#define DECLARE_ACCESS_FUNCTION(t)      \
    public:                             \
    virtual int get (t& m)        {_not_impl ("get ()"); return -1;} \
    virtual int set (const t& m)  {_not_impl ("set ()"); return -1;}

// VValue - base class of a value item
///////////////////////////////////////
class VValue
{
public:
    const static objecttype_t T_ERROR         = 0x00;
    const static objecttype_t T_UNKNOWN       = 0x01;
    const static objecttype_t T_CONTAINER     = 0x02;
    const static objecttype_t T_OBJECT        = 0x03;
    const static objecttype_t T_VEOBJECT      = 0x04;
    const static objecttype_t T_PREFIX_SIMPLE = 0x10;
    const static objecttype_t TS_BOOL         = T_PREFIX_SIMPLE | 0x01;
    const static objecttype_t TS_INT          = T_PREFIX_SIMPLE | 0x02;
    const static objecttype_t TS_DOUBLE       = T_PREFIX_SIMPLE | 0x04;
    const static objecttype_t TS_STRING       = T_PREFIX_SIMPLE | 0x08;
    //const static std::string OBJECTTYPE_STR[] = {"T_UNKNOWN", "T_CONTAINER"};

    const static VValue null_value;
    //static VValue unknown_value;
    //static VValue error_value;

public:
    VValue (objecttype_t t = T_UNKNOWN)
        : _type (t) {}

    VValue (const VValue& m)
        : _type (m._type) {}

    virtual ~VValue ()
    {
    }

    inline
    objecttype_t get_type () const
    {
        return _type;
    }

    /*
        Value operators:
        get ()
        set ()
    */
    DECLARE_ACCESS_FUNCTION(bool)
    DECLARE_ACCESS_FUNCTION(int)
    DECLARE_ACCESS_FUNCTION(double)
    DECLARE_ACCESS_FUNCTION(std::string)

    /*
        Container Operators:
        add_attribute ()
        remove_attribute ()
        get_attribute ()
    */
    virtual int             add_attribute     (VASTATE::short_index_t index, const VValue& m) 
    {
        _not_impl ("add_attribute ()"); 
        return -1;
    }

    virtual int             remove_attribute  (VASTATE::short_index_t index)         
    {
        _not_impl ("remove_attribute ()"); 
        return -1;
    }

    virtual VValue&         get_attribute (VASTATE::short_index_t index)
    {
        _not_impl ("get_attribute ()"); 
        return *this;
    }

    virtual std::string     encodeToStr () const
    {
        std::ostringstream s;
        s << "[t" << (int) _type << "]";
        return s.str ();
    }

    virtual std::vector<uchar_t> encodeToVec () const
    {
        return vector<uchar_t> ();
    }

    virtual bool            decodeFromVec (const std::vector<uchar_t>& vec)
    {
        return false;
    }

protected:
    objecttype_t _type;    

    /* Debug functions */
protected:
    inline void _not_impl (const std::string& functionName)
    {
#ifdef DEBUG_DETAIL
        std::cout << "VValue: " << functionName << "not implements the function." << std::endl;
#endif
    }
};

// VValueFactory
///////////////////////////////////////
class EXPORT VValueFactory
{
private:
    /*
    const static int  _encodebufferlen = 10240;
    static char _encodebuffer[_encodebufferlen];
    static char _encodebuffer2[_encodebufferlen];
    */
public:
    static VValue * copy   (const VValue& value);
    static int      encode (const VValue *v, uchar_t *outbuffer, /* i/o parameber */ size_t& out_len);
    //static int      decode (const VValue *v, char * inbuffer, const size_t inlen);
    //static VValue * decode (const vector<unsigned char> & stru, const vector<vector<unsigned char> > & data);
    static VValue * decode (const uchar_t *buffer, const size_t & buffer_len);
    //static VValue * decode (const std::string& str);

    static void destroy (const VValue *m);
private:
    VValueFactory () {}
};

// Basic value define
///////////////////////////////////////
template<VASTATE::objecttype_t TYPE_ID,typename TYPE>
class VSimpleValue : public VValue
{
protected:
    TYPE _value;

public:
    VSimpleValue ()
        :VValue (TYPE_ID)
    {}

    VSimpleValue (TYPE init_value)
        : VValue (TYPE_ID), _value (init_value)
    {}

    VSimpleValue (const VSimpleValue<TYPE_ID, TYPE>& sv)
        : VValue (TYPE_ID), _value(sv._value)
    {
    }

    virtual ~VSimpleValue ()
    {
    }

    int get (TYPE& m)
    {
        m = _value;
        return 0;
    }

    int set (const TYPE &m)
    {
        _value = m;
        return 0;
    }

    int set (const VSimpleValue<TYPE_ID,TYPE>& m)
    {
        _value = m._value;
        return 0;
    }

    virtual string encodeToStr () const
    {
        std::ostringstream s;
        s << VValue::encodeToStr ();
        s << _value;
        return s.str ();
    }

    virtual std::vector<uchar_t> encodeToVec () const
    {
        uchar_t * ch = (uchar_t*) & _value;
        vector<uchar_t> out;
        for (size_t s = 0; s < sizeof(TYPE); ++s)
            out.push_back (ch [s]);
        return out;
    }

    virtual bool decodeFromVec (const std::vector<uchar_t>& vec)
    {
        if (vec.size () != sizeof(TYPE))
            return false;

        uchar_t * ch = (uchar_t*) & _value;
        for (size_t s = 0; s < sizeof(TYPE); ++s)
            ch[s] = vec[s];

        return true;
    }
};

// Simple value type defination
///////////////////////////////////////
typedef VSimpleValue<VValue::TS_BOOL, bool>             VSimpleValue_bool;
typedef VSimpleValue<VValue::TS_INT, int>               VSimpleValue_int;
typedef VSimpleValue<VValue::TS_DOUBLE, double>         VSimpleValue_double;
typedef VSimpleValue<VValue::TS_STRING, std::string>    VSimpleValue_string;
template<> vector<uchar_t> VSimpleValue<VValue::TS_STRING, std::string>::encodeToVec () const;
template<> bool VSimpleValue<VValue::TS_STRING, std::string>::decodeFromVec (const vector<uchar_t>& vec);

// Container
///////////////////////////////////////
class VContainer : public VValue
{
public:
    typedef std::map<VASTATE::short_index_t,VValue *> Box;

protected:
    Box _b;

public:
    VContainer ()
        : VValue (T_CONTAINER) {}

protected:
    VContainer (const objecttype_t type)
        : VValue (type) {}

    VContainer (const VContainer& c, const objecttype_t type)
        : VValue (type)
    {
        assign (c);
    }

public:
    VContainer (const VContainer& c)
        : VValue (T_CONTAINER)
    {
        assign (c);
    }

    ~VContainer ()
    {
        clean ();
    }

    int add_attribute (VASTATE::short_index_t index, const VValue& m) 
    {
        std::cout << "VContainer::add (" << (int) index << "," << m.encodeToStr () << ")" << endl;
        if (_b.find (index) != _b.end ())
            VValueFactory::destroy (_b[index]);

        VValue * c = VValueFactory::copy (m);
        _b[index] = c;
        return 0;
    }

    int remove_attribute (VASTATE::short_index_t index)         
    {
        if (_b.find (index) != _b.end ())
        {
            VValueFactory::destroy (_b[index]);
            _b.erase (index);
        }
        return -1;
    }

    VValue& get_attribute (VASTATE::short_index_t index)
    {
        std::cout << "VContainer::getItem ()" << endl;
        if (_b.find (index) == _b.end ())
            return *this;

        return *(_b[index]);
    }

    virtual std::string encodeToStr () const
    {
        std::ostringstream s;
        s << VValue::encodeToStr ();
        s << "{";
        s << "[s]" << (unsigned long) _b.size ();
        for (Box::const_iterator it = _b.begin (); it != _b.end (); it ++)
        {
            s << "[" << (int) it->first << "]" << it->second->encodeToStr ();
        }
        s << "}";
        return s.str ();
    }

protected:
    void assign (const VContainer& c)
    {
        clean ();
        Box::const_iterator it = c._b.begin ();
        for (; it != c._b.end (); it ++)
            add_attribute (it->first, *it->second);
    }

    void clean ()
    {
        Box::iterator it = _b.begin ();
        for (; it != _b.end (); it ++)
            delete it->second;
    }

    friend class VValueFactory;
};

class VObject : public VContainer
{
private:
    id_t    _id;

public:
    VObject (const id_t & id) 
        : VContainer(T_OBJECT), _id (id)  {}

protected:
    VObject (const VObject& o, const objecttype_t type)
        : VContainer (o, type), _id (o._id) {}

    VObject (const id_t id, const objecttype_t type)
        : VContainer (type), _id (id) {}

public:
    VObject (const VObject& o)
        : VContainer (o, T_OBJECT), _id (o._id) {}

public:
    ~VObject () {}

    inline 
    id_t get_id () const
    {   return _id;  }

    virtual std::string encodeToStr () const
    {
        std::ostringstream s;
        s << VValue::encodeToStr ();
        s << "[id]" << get_id ();
        s << "{";
        s << "[s]" << (unsigned long) _b.size ();
        for (Box::const_iterator it = _b.begin (); it != _b.end (); it ++)
        {
            s << "[" << (int) it->first << "]" << it->second->encodeToStr ();
        }
        s << "}";
        return s.str ();
    }
};

class VEObject : public VObject
{
public:
    Coord3D pos;

public:
    VEObject (const VASTATE::id_t& id, const Coord3D &init_pos)
        : VObject (id, T_VEOBJECT), pos (init_pos) {}

    VEObject (const VEObject & o)
        : VObject (o, T_VEOBJECT), pos (o.pos) {}

    ~VEObject () {}

    std::string encodeToStr () const
    {
        std::ostringstream s;
        s << VValue::encodeToStr ();
        s << "[pos]" << to_string (pos);
        s << "[id]" << get_id ();
        s << "{";
        s << "[s]" << (unsigned long) _b.size ();
        for (Box::const_iterator it = _b.begin (); it != _b.end (); it ++)
        {
            s << "[" << (int) it->first << "]" << it->second->encodeToStr ();
        }
        s << "}";
        return s.str ();
    }
};


class VEvent : public VObject
{
    VEvent (const id_t & id)
        : VObject (id)
    {}

    ~VEvent ()
    {}

    std::string encodeToStr () const
    {
        return VObject::encodeToStr ();
    }
};

} /* namespace VASTATE */

#endif /* _VASTATE_VWORLD_H */
