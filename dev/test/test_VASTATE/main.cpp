
#include <iostream>
//#include "vastate.h"
#include "shared.h"

using namespace std;
using namespace VAST;

MValue MValue::unknown_value;

void object_info (MValue& m)
{
    cout << "Object information ---" << endl;
    cout << m.encodeToStr () << endl;
    cout << "----------------------" << endl;
}

int main ()
{
    cout << "Hello, World!" << endl;
    MBaseObject *mp = new MBaseObject ();
    MBaseObject &mpr = *mp;
    // assignment
    cout << "mpr.add (0, 20)" << endl;
    mpr.add (0, 20);
    mpr.add (5, 10);
    cout << endl;
    //mp->add (1, "test string");
    // implicit
    //mp[2] = "HP Unknown";
    cout << "mpr[5] = 77" << endl;
    //mpr[5] = 77;
//    mpr[5].assign (77);
    //mpr.getItem (5).assign (77);
    mpr[5] = 77;
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