
/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007-2008 Shao-Chen Chang (cscxcs at gmail.com)
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

/*
 *  VASTATEsim_console.cpp (Console Interface of VASTATEsim)
 *
 *      ver. 0.01  (2007/01/30)
 */

#include "vastatesim.h"
#include "vastate.h"
#include "attributebuilder.h"

SimPara para;

void print_command_line ()
{
    printf (
        "Usage:\n"
        "    vastatesim_console [ParaFile] [MODE] [FoodFile] [ActionFile] \n"
        "\n"
        "       ParaFile   : simulation parameter file\n"
        "       MODE       : (empty) | rec | play\n"
        "       FoodFile/\n"
        "       ActionFile : Record file for food or actions\n"
        "\n"
        "       Note: When MODE is play or rec, FoodFile and ActionFile \n"
        "             must be specified.\n"
        "\n"
        );
}

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
        print_command_line ();
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

