
/*
 *  VASTATEsim_console.cpp (Console version Interface of VASTATEsim)
 *
 *      ver. 0.01  (2007/01/30)
 */


#include "vastatesim.h"
#include "vastate.h"
#include "attributebuilder.h"

SimPara para;

int main (int argc, char *argv[])
{
    //srand (time (NULL));

    int simulation_mode = VS_NORMAL_MODE;
    std::string filename ("VASTATEsim.ini");
    std::string foodfile, actionfile;

    if (argc >= 2)
        filename = argv[1];

    if (argc >= 5)
    {
        if (!strcmp (argv[2], "play"))
            simulation_mode = VS_PLAY_MODE;
        else if (!strcmp (argv[2], "rec"))
            simulation_mode = VS_RECORD_MODE;

        foodfile = argv[3];
        actionfile = argv[4];
    }

	if (!ReadPara (para, filename))
	{
        printf ("VASTATESIM_console: read parameter file failed.\n");
        return -1;
	}

    InitVSSim (para, simulation_mode, foodfile.c_str (), actionfile.c_str ());
	
	int n;
	for (n=0; n<para.NODE_SIZE; n++)
		CreateNode (85);

	/*
	for (n=0; n<5; n++)
	    ProcessMsg ();
    */

    for (n = 0; n < para.TIME_STEPS; n ++)
    {
        printf ("Step %d ---\n", n);
        NextStep ();
	    ProcessMsg ();
    }

    refreshStatistics ();

    //getchar ();
    return 0;
}