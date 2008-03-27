
#include <iostream>
//#include "vastate.h"
#include "shared.h"

using namespace std;
using namespace VAST;

void object_info (MValue* m)
{
    cout << "Object information ---" << endl;
    cout << m->encodeToStr () << endl;
    cout << "----------------------" << endl;
}

int main ()
{
    cout << "Hello, World!" << endl;

    MBaseObject *mp = new MBaseObject ();
    MBaseObject &mpr = *mp;
    // assignment
    mp->add (0, 20);
    //mp->add (1, "test string");
    // implicit
    //mp[2] = "HP Unknown";
    //mp[5] = 77;
    //mp->operator [] (5).add (77);
    mpr[5].add (77);
    

//    if (mp[5] >= 10)
//        mp[2] = "HP Good!";

    MBaseObject *pack = new MBaseObject ();
    //pack->add (1, "Red pack");
    pack->add (2, 10);
    mpr[24] = *pack;

    object_info (mp);
    delete mp;
    return 0;
}