

/*
 * Virtual world model defination
 * vworld.cpp
 *
 * ver 0.1 (2008/04/23)
 */

#include "precompile.h"
#include "vworld.h"

// VValueFactory Function implementation 
///////////////////////////////////////

namespace VASTATE {

    // for VSimpleValue_string
    ///////////////////////////////////////
    template<> 
    RawData VSimpleValue<VValue::TS_STRING, std::string>::encodeToRaw (bool only_updated) const
    {
        RawData r;
        if (!only_updated || is_modified ())
        {
            r = VValue::encodeToRaw ();
            r.push_sized_array ((uchar_t *) _value.c_str (), _value.size ());
        }

        return r;
    }

    template<> 
    bool VSimpleValue<VValue::TS_STRING, std::string>::decodeFromRaw (RawData& raw)
    {
        if (!VValue::decodeFromRaw (raw))
            return false;

        // get data size
        size_t s;
        if (!raw.pop_array ((uchar_t *) & s, sizeof (size_t)) || raw.size () < s)
            return false;

        // get value
        _value.clear ();
        _value.append ((char *) &(raw[0]), s);
        raw.pop_front (s);
        return true;
    }

    VValue* VValueFactory::copy (const VValue& value)
    {
        using namespace std;

        VContainer *newc = NULL;

        switch (value.get_type ())
        {
        case VValue::TS_BOOL:
            return new VSimpleValue_bool (*(VSimpleValue_bool*) & value);

        case VValue::TS_INT:
            return new VSimpleValue_int  (*(VSimpleValue_int*) & value);

        case VValue::TS_DOUBLE:
            return new VSimpleValue_double (*(VSimpleValue_double*) & value);

        case VValue::TS_STRING:
            return new VSimpleValue_string (*(VSimpleValue_string*) & value);

        case VValue::T_CONTAINER:
            return new VContainer (* (VContainer *) & value);

        case VValue::T_OBJECT:
            return new VObject (* (VObject *) & value);

        case VValue::T_VEOBJECT:
            return new VEObject (* (VEObject *) & value);
        
            /*
        case VValue::T_VEOBJECT:
            {
                newc = new VEObject (
            }

        case VValue::T_OBJECT:
            {
                VObject * oldo = (VObject) &value;
                if (newc == NULL)
                    newc = new VObject (oldo->get_id ());
            }

        case VValue::T_CONTAINER:
            {
                VContainer *oldc = (VContainer *) & value;
                VContainer *newc = new VContainer ();
                VContainer::Box::iterator it = oldc->_b.begin ();
                for (; it != oldc->_b.end (); it ++)
                    newc->add_attribute (it->first, *it->second);
                return newc;
            }*/

        default:
            cout << "VValueFactory: copy (): Copy unknown type(" << (int) value.get_type () << ") of VValue" << endl;
        }

        return NULL;
    }

    RawData  VValueFactory::encodeToRaw   (const VValue* value, bool only_updated)
    {
        return value->encodeToRaw (only_updated);
    }

    VValue* VValueFactory::decodeFromRaw  (RawData& raw, VValue * baseObj)
    {
        objecttype_t type = raw.front ();
        VValue * value = NULL;

        // if baseObj at same type is provided, use that to decode, or creates new object
        if (baseObj != NULL && baseObj->get_type () == type)
            value = baseObj;
        else
            value = objecttypeToObj (type);

        // decodeing
        if (value != NULL && !value->decodeFromRaw (raw))
        {
            if (baseObj == NULL)
                delete value;

            return baseObj;
        }

        if (baseObj != NULL && baseObj->get_type () != type)
            delete baseObj;

        return value;
    }

    VValue* VValueFactory::objecttypeToObj (const objecttype_t type)
    {
        VValue * value = NULL;
        switch (type)
        {
            case VValue::TS_BOOL:       value = new VSimpleValue_bool (); break;
            case VValue::TS_INT:        value = new VSimpleValue_int  (); break;
            case VValue::TS_DOUBLE:     value = new VSimpleValue_double (); break;
            case VValue::TS_STRING:     value = new VSimpleValue_string (); break;
            case VValue::T_CONTAINER:   value = new VContainer (); break;
            case VValue::T_OBJECT:      value = new VObject (); break;
            case VValue::T_VEOBJECT:    value = new VEObject (); break;
        }
        return value;
    }

    void VValueFactory::destroy (const VValue *m)
    {
        delete m;
    }

} /* namespace VASTATE */
