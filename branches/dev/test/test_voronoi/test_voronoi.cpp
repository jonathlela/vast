

#include "vastverse.h"
#include <vector>

using namespace std;
using namespace VAST;

#define DIM_X       1000
#define DIM_Y       1000

#define N_SET     3

int main (int argc, char *argv[])
{
    if (argc < 2)
    {
        printf ("Usage: %s [input file] [optional interest radius]\n", argv[0]);
        exit (0);
    }
    
    // create the Voronoi class    
    vastverse world (VAST_MODEL_DIRECT, VAST_NET_EMULATED, 0);
    voronoi *v = world.create_voronoi ();

    // read the input data set
    FILE *fp;
    if ((fp = fopen (argv[1], "rt")) == NULL)
    {
        printf ("cannot read input file '%s'\n", argv[1]);
        exit (0);
    }

	// get interest radius
	double interest_radius = (argc > 2 ? atof (argv[2]) : 0);
    
    double x, y;
    int n = 0;
    while (fscanf (fp, "%lf %lf\n", &x, &y) != EOF)
    {
        Position pt (x, y);
        printf ("reading site [%d] at (%lf, %lf)\n", ++n, pt.x, pt.y);

        // insert the points and build the voronoi diagram
        v->insert  (n, pt);
    }

    // create output files
    FILE *out_fp[N_SET];
    char filename[80];
    int k;
    for (k=0; k<N_SET; ++k)
    {
        sprintf (filename, "output-%d.txt", k+1);
        out_fp[k] = fopen (filename, "wt");
    }
    
    // print out the 1st, 2nd, and 3rd set of nearest neighbor of all the nodes
    int i, j;
	vector<int>		node_list;		// list of node ID
	vector<double>	dist_list;	// list of distances

    for (i=1; i<=n; ++i)
    {
        Position NN;
        Position current = v->get (i);

        // loop through the jth set of nearest neighbor
        for (j=0; j<N_SET; ++j)
        {                              
            // get enclosing neighbors with a given level
            // note that level starts with 1
            vector<id_t> &en_list = v->get_en (i, j+1);
			node_list.clear ();
			dist_list.clear ();
            
            // go through each NN and find the distance
            for (k=0; k<(int)en_list.size (); ++k)
            {           
                NN = v->get (en_list[k]);
				double dist = NN.dist (current);				

				if (interest_radius != 0 && dist > interest_radius)
					continue;

				node_list.push_back (en_list[k]);
				dist_list.push_back (dist);                
            }

            // print ID & total # of NN
            fprintf (out_fp[j], "%d %d ", i, node_list.size ());
			
            // print out NN id & dist
			for (k=0; k<(int)node_list.size (); ++k)
                fprintf (out_fp[j], "[%d] %lf ", node_list[k], dist_list[k]);

            fprintf (out_fp[j], "\n");            
        }               
    }    

    // close output files
    for (k=0; k<N_SET; ++k)
    {
        fclose (out_fp[k]);
    }

    world.destroy_voronoi (v);

    return 0;
}


