
#include "precompile.h"
#include "shared.h"

//using namespace std;
using std::cout;
using std::endl;
using namespace VAST;

// Static members for MValue
///////////////////////////////////////
MValue MValue::unknown_value (MValue::T_UNKNOWN);
MValue MValue::error_value   (MValue::T_ERROR);


// Coord3D output function
///////////////////////////////////////
std::string VAST::to_string (const Coord3D& p)
{
    std::ostringstream s;
    s.precision (6);
    s << fixed;
    s << "(" << p.x << "," << p.y << "," << p.z << ")";
    return s.str ();
}


// MValueFactory Function implementation 
///////////////////////////////////////
MValue* MValueFactory::copy (const MValue& value)
{
    switch (value.get_type ())
    {
    case MValue::TS_BOOL:
        return new MSimpleValue_bool (*(MSimpleValue_bool*) & value);

    case MValue::TS_INT:
        return new MSimpleValue_int  (*(MSimpleValue_int*) & value);

    case MValue::TS_DOUBLE:
        return new MSimpleValue_double (*(MSimpleValue_double*) & value);

    case MValue::TS_STRING:
        return new MSimpleValue_string (*(MSimpleValue_string*) & value);

    case MValue::T_CONTAINER:
        {
            MContainer *oldc = (MContainer *) & value;
            MContainer *newc = new MContainer ();
            MContainer::Box::iterator it = oldc->_b.begin ();
            for (; it != oldc->_b.end (); it ++)
                newc->add_attribute (it->first, *it->second);
            return newc;
        }

    default:
        cout << "MValueFactory: copy (): Copy unknown type(" << value.get_type () << ") of Mvalue" << endl;
    }

    return NULL;
}

void MValueFactory::destroy (const MValue *m)
{
    delete m;
}
