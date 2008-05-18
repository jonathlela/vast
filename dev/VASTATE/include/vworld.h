

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
#include "rawdata.h"

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
class VValue : public encodable
{
public:
    const static objecttype_t T_ERROR         = 0x00;
    const static objecttype_t T_UNKNOWN       = 0x01;
    const static objecttype_t T_CONTAINER     = 0x02;
    const static objecttype_t T_OBJECT        = 0x03;
    const static objecttype_t T_VEOBJECT      = 0x04;
    const static objecttype_t T_EVENT         = 0x08;
    const static objecttype_t T_PREFIX_SIMPLE = 0x10;
    const static objecttype_t TS_BOOL         = T_PREFIX_SIMPLE | 0x01;
    const static objecttype_t TS_INT          = T_PREFIX_SIMPLE | 0x02;
    const static objecttype_t TS_DOUBLE       = T_PREFIX_SIMPLE | 0x04;
    const static objecttype_t TS_STRING       = T_PREFIX_SIMPLE | 0x08;

    const static objecttype_t T_DELETE        = 0x20;
    //const static std::string OBJECTTYPE_STR[] = {"T_UNKNOWN", "T_CONTAINER"};

    const static VValue null_value;

public:
    VValue (objecttype_t t = T_UNKNOWN)
        : _type (t), _modified (false) {}

    VValue (const VValue& m)
        : _type (m._type), _modified (false) {}

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

    virtual bool            is_modified () const
    {
        return _modified;
    }

    virtual void            clear_edit ()
    {
        _modified = false;
    }

    virtual std::string     encodeToStr () const
    {
        std::ostringstream s;
        s << "[t" << (int) _type << "]";
        return s.str ();
    }

    virtual RawData         encodeToRaw   (bool only_updated = false) const
    {
        RawData r;
        r.push_back ((uchar_t) _type);
        return r;
    }

    virtual bool            decodeFromRaw (RawData& raw)
    {
        if (raw.size () < sizeof (objecttype_t))
            return false;
        if (raw.front () != _type)
            return false;
        raw.pop_front (sizeof(objecttype_t));
        return true;
    }

protected:
    objecttype_t _type;
    bool         _modified;

    /* Debug functions */
protected:
    inline void _not_impl (const std::string& functionName)
    {
#ifdef DEBUG_DETAIL
        std::cout << "VValue: " << functionName << "not implements the function." << std::endl;
#endif
    }
};


#define DECLARE_FAC_ACCESS_FUNCTION(t)  \
    public:                             \
    static int get (VValue *v, t& m)        {return v->get(m);} \
    static int set (VValue *v, const t& m)  {return v->set(m);}

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
    // TODO: refactor encode/decode into a OO style, by implementing them as encodeToStr
    //       decoding by supporting a function new_imstance_by_type and decodeFromVec
    //       Also, implements operator operations (like + as apply updates, - as difference)
    static RawData encodeToRaw    (const VValue* value, bool only_updated = false);
    static VValue* decodeFromRaw  (RawData& raw, VValue *baseObj = NULL);

private:
    static VValue* objecttypeToObj (const objecttype_t type);

public:
    static void destroy (const VValue *m);

    DECLARE_FAC_ACCESS_FUNCTION(int)
    DECLARE_FAC_ACCESS_FUNCTION(bool)
    DECLARE_FAC_ACCESS_FUNCTION(double)
    DECLARE_FAC_ACCESS_FUNCTION(std::string)

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
        _modified = true;
        return 0;
    }

    int set (const VSimpleValue<TYPE_ID,TYPE>& m)
    {
        _value = m._value;
        _modified = true;
        return 0;
    }

    virtual string encodeToStr () const
    {
        std::ostringstream s;
        s << VValue::encodeToStr ();
        s << _value;
        return s.str ();
    }

    virtual RawData         encodeToRaw   (bool only_updated = false) const
    {
        if (!only_updated || is_modified ())
        {
            RawData r = VValue::encodeToRaw (only_updated);
            r.push_array ((uchar_t *) & _value, sizeof(TYPE));
            return r;
        }
        return RawData ();
    }

    virtual bool            decodeFromRaw (RawData& raw)
    {
        if (!VValue::decodeFromRaw (raw))
            return false;

        // get value
        return raw.pop_array ((uchar_t *) & _value, sizeof(TYPE));
    }
};

// Simple value type defination
///////////////////////////////////////
typedef VSimpleValue<VValue::TS_BOOL, bool>             VSimpleValue_bool;
typedef VSimpleValue<VValue::TS_INT, int>               VSimpleValue_int;
typedef VSimpleValue<VValue::TS_DOUBLE, double>         VSimpleValue_double;
typedef VSimpleValue<VValue::TS_STRING, std::string>    VSimpleValue_string;
template<> RawData VSimpleValue<VValue::TS_STRING, std::string>::encodeToRaw (bool only_updated) const;
template<> bool VSimpleValue<VValue::TS_STRING, std::string>::decodeFromRaw (RawData& raw);

// Container
///////////////////////////////////////
class VContainer : public VValue
{
public:
    typedef std::map<VASTATE::short_index_t,VValue *> Box;
    typedef std::map<VASTATE::short_index_t,bool>     BoxFlag;

protected:
    Box _b;
    BoxFlag _bmodified;


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
        _bmodified[index] = true;
        return 0;
    }

    int remove_attribute (VASTATE::short_index_t index)         
    {
        if (_b.find (index) != _b.end ())
        {
            VValueFactory::destroy (_b[index]);
            _b.erase (index);
            _bmodified [index] = true;
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

    virtual bool is_modified () const
    {
        // container object is modified on only any of its attributes modified
        if (_modified || _bmodified.size () > 0)
            return true;

        for (Box::const_iterator it = _b.begin (); it != _b.end (); it ++)
            if (it->second->is_modified ())
                return true;

        return false;
    }

    virtual void clear_edit ()
    {
        _modified = false;
        _bmodified.clear ();
        for (Box::const_iterator it = _b.begin (); it != _b.end (); it ++)
            it->second->clear_edit ();
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

    virtual RawData encodeToRaw   (bool only_updated = false) const
    {
        if (!only_updated || is_modified ())
        {
            RawData r;
            // check type for ensure I'm first level caller and must check for type consistent
            if (get_type () == VValue::T_CONTAINER)
                r = VValue::encodeToRaw (only_updated);

            // decide number of items
            short_index_t siz = 0;
            if (only_updated)
            {
                siz += (short_index_t) _bmodified.size ();
                for (Box::const_iterator it = _b.begin (); it != _b.end (); it ++)
                    if (it->second->is_modified ())
                        siz ++;
            }
            else
                siz = (short_index_t) _b.size ();

            r.push_array ((uchar_t *) & siz, sizeof (short_index_t));

            // push delta arrays
            if (only_updated)
            {
                for (BoxFlag::const_iterator it = _bmodified.begin (); it != _bmodified.end (); it ++)
                {
                    r.push_array (rawdata_p (it->first), sizeof (short_index_t));
                    r.push_array (rawdata_p (VValue::T_DELETE), sizeof (objecttype_t));
                }
            }

            // push datas
            for (Box::const_iterator it = _b.begin (); it != _b.end (); it ++)
            {
                if (!only_updated || it->second->is_modified ())
                {
                    r.push_array (rawdata_p(it->first), sizeof (short_index_t));
                    r.push_raw (it->second->encodeToRaw (only_updated));
                }
            }

            return r;
        }

        return RawData ();
    }

    virtual bool    decodeFromRaw (RawData& raw)
    {
        // check type for ensure I'm first level caller and must check for type consistent
        if (get_type () == VValue::T_CONTAINER
            && !VValue::decodeFromRaw (raw))
            return false;

        short_index_t siz;
        raw.pop_array ((uchar_t *) & siz, sizeof(short_index_t));
        for (short_index_t s = 0; s < siz; ++ s)
        {
            short_index_t idx;
            raw.pop_array ((uchar_t *) & idx, sizeof(short_index_t));

            VValue *value = VValueFactory::decodeFromRaw (raw, 
                            (_b.find (idx) != _b.end ()) ? _b[idx] : NULL);
            if (value == NULL)
                return false;
            _b[idx] = value;
        }

        return true;
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
        _b.clear ();
    }

    friend class VValueFactory;
};

class VObject : public VContainer
{
private:
    id_t    _id;

public:
    VObject ()
        : VContainer (T_OBJECT) {}
    VObject (const id_t & id) 
        : VContainer(T_OBJECT), _id (id)  {}
    VObject (const VObject& o)
        : VContainer (o, T_OBJECT), _id (o._id) {}

protected:
    VObject (const objecttype_t type)
        : VContainer (type) {}
    VObject (const id_t id, const objecttype_t type)
        : VContainer (type), _id (id) {}
    VObject (const VObject& o, const objecttype_t type)
        : VContainer (o, type), _id (o._id) {}

public:
    ~VObject () {}

    inline 
    id_t get_id () const
    {   return _id;  }

    virtual bool is_modified () const
    {
        return _modified || VContainer::is_modified ();
    }

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

    virtual RawData encodeToRaw   (bool only_updated = false) const
    {
        RawData r;

        // check type for ensure I'm first level caller and must check for type consistent
        if (get_type () == VValue::T_OBJECT)
            r = VValue::encodeToRaw (only_updated);

        r.push_array ((uchar_t *) & _id, sizeof (id_t));
        r.push_raw (VContainer::encodeToRaw (only_updated));

        return r;
    }

    virtual bool    decodeFromRaw (RawData& raw)
    {
        // check type for ensure I'm first level caller and must check for type consistent
        if (get_type () == VValue::T_OBJECT
            && !VValue::decodeFromRaw (raw))
            return false;

        if (!raw.pop_array ((uchar_t *) & _id, sizeof(id_t)))
            return false;

        return VContainer::decodeFromRaw (raw);
    }
};

class VEObject : public VObject
{
public:
    Coord3D pos;

public:
    VEObject () 
        : VObject (T_VEOBJECT) {}
    ~VEObject () {}

    VEObject (const VASTATE::id_t& id, const Coord3D &init_pos)
        : VObject (id, T_VEOBJECT), pos (init_pos) {}

    VEObject (const VEObject & o)
        : VObject (o, T_VEOBJECT), pos (o.pos) {}

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

    virtual RawData encodeToRaw   (bool only_updated = false) const
    {
        RawData r;

        // check type for ensure I'm first level caller and must check for type consistent
        if (get_type () == VValue::T_VEOBJECT)
            r = VValue::encodeToRaw (only_updated);

        r.push_array ((uchar_t *) & pos, sizeof (Coord3D));
        r.push_raw (VObject::encodeToRaw (only_updated));

        return r;
    }

    virtual bool    decodeFromRaw (RawData& raw)
    {
        // check type for ensure I'm first level caller and must check for type consistent
        if (get_type () == VValue::T_VEOBJECT
            && !VValue::decodeFromRaw (raw))
            return false;

        if (!raw.pop_array ((uchar_t *) & pos, sizeof(Coord3D)))
            return false;

        return VObject::decodeFromRaw (raw);
    }
};


class VEvent : public VObject
{
    VEvent (const id_t & id)
        : VObject (id, VValue::T_EVENT)
    {}

    ~VEvent ()
    {}

    std::string encodeToStr () const
    {
        return VObject::encodeToStr ();
    }

    virtual RawData encodeToRaw   (bool only_updated = false) const
    {
        RawData r;

        // check type for ensure I'm first level caller and must check for type consistent
        if (get_type () == VValue::T_EVENT)
            r = VValue::encodeToRaw (only_updated);

        r.push_raw (VObject::encodeToRaw (only_updated));
        return r;
    }

    virtual bool    decodeFromRaw (RawData& raw)
    {
        // check type for ensure I'm first level caller and must check for type consistent
        if (get_type () == VValue::T_EVENT
            && !VValue::decodeFromRaw (raw))
            return false;

        return VObject::decodeFromRaw (raw);
    }
};

} /* namespace VASTATE */

#endif /* _VASTATE_VWORLD_H */
