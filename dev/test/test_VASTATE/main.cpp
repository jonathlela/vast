
#include "precompile.h"
//#include "vastate.h"
#include "shared.h"

using namespace std;
using namespace VAST;

void object_info (MValue& m)
{
    cout << "Object information ---" << endl;
    cout << m.encodeToStr () << endl;
    cout << "----------------------" << endl;
}

int main ()
{
    cout << "Hello, World!" << endl;
    VEObject *mp = new VEObject (1, Coord3D (318.52, 224.3, 900.0));
    VEObject &mpr = *mp;
    // assignment
    cout << "mpr.add (0, 20)" << endl;
    mpr.add_attribute (0, MSimpleValue_string ("IlMgcer"));
    mpr.add_attribute (1, MSimpleValue_int (100));
    mpr.add_attribute (2, MSimpleValue_int (220));
    mpr.add_attribute (3, MSimpleValue_double (2.5));
    mpr.add_attribute (10, MContainer ());
    cout << endl;

    // sword attributes
    cout << "New object: sword" << endl;
    MBaseObject sword (2);
    sword.add_attribute (0, MSimpleValue_string("¶Ã¤C¤KÁV¼C"));
    sword.add_attribute (5, MSimpleValue_int(10));
    sword.add_attribute (6, MSimpleValue_double(2.2));
    sword.add_attribute (7, MSimpleValue_int(100));

    if (mpr.get_attribute (10).get_type () != MValue::T_CONTAINER)
        cout << "error containor type" << endl;

    // find a place to put in
    MContainer & res = *(MContainer*) &mpr.get_attribute (10);
    short_index_t free_index = 0;
    for (short_index_t i = 0; i < (short_index_t)0 - (short_index_t)1; i ++)
    {
        if (res.get_attribute (i).get_type () == MValue::T_ERROR)
        {
            free_index = i;
            break;
        }
    }

    res.add_attribute (free_index, sword);

    // demage
    cout << "mpr[5] = 77" << endl;
    int hp_diff = -5;
    //int hp = mpr.get_attribute (1);
    int hp;
    mpr.get_attribute (1).get ((int) hp);
    hp += hp_diff;
    mpr.get_attribute (1).set ((int) hp);
    cout << endl;

//    if (mp[5] >= 10)
//        mp[2] = "HP Good!";

    //MBaseObject *pack = new MBaseObject ();
    //pack->add (1, "Red pack");
    //pack->add (2, 10);
    //mpr[24] = *pack;

    object_info (mpr);
    cout << "delete mp" << endl;
    delete mp;
    cout << endl;

    getchar ();
    return 0;
}