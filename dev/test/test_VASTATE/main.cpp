
#include <iostream>
//#include "vastate.h"
#include "shared.h"

using namespace std;

void object_info (const MValue & m)
{
    cout << "Object information ---" << endl;
    cout << m.encodeToStr () << endl;
    cout << "----------------------" << endl;
}

int main ()
{
    cout << "Hello, World!" << endl;

    MBaseObject *mp = new MBaseObject ();
    // assignment
    mp.add (0, 20);
    mp.add (1, "test string");
    // implicit
    mp[2] = "HP Unknown";
    mp[5] = 77.88;

    if (mp[5] >= 10)
        mp[2] = "HP Good!";

    MBaseObject *pack = new MBaseObject ();
    pack.add (1, "Red pack");

    mp[24] = pack;

    cout << "Object information ---" << endl;
    cout << mp->encodeToStr () << endl;
    cout << "----------------------" << endl;

    delete mp;
    return 0;
}