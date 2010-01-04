
#include "ReadFunc.h"

bool ReadLatency (int &lat_size, vector<vector<float>> &tg_lat_table)
{
	// read a given latency table for peers
	lat_size = ReadTopGenLatency (tg_lat_table);
	//lat_table_size = ReadPairPingLatency (pp_lat_table);

	// error message
	if (lat_size == -1)
	{
		printf ("Fail to reading latency table ...\n");
		return false;
	}
	else if (lat_size == -2)
	{
		printf ("File open error ...\n");
		return false;
	}

	return true;
}


int ReadTopGenLatency (vector<vector<float>> &tg_lat_table)
{
	FILE *fp;
	if ((fp = fopen ("latency1000peers.dat", "rt")) == NULL)
		return -2;

	int size;
	float f;
	char buff[20480];

	while (fgets (buff, 20480, fp) != NULL)
	{
		// skip any comments or empty lines
		if (buff[0] == '#' || buff[0] == ' ' || buff[0] == '\t' || buff[0] == '\n')
			continue;

		// read the next valid parameter
		char *p = buff;
		int length = 0;

		if (sscanf (p, "%d%n", &size, &length) == 1)
		{
			vector<float> inner;

			// for parsing character
			p += length;
			for ( ; sscanf (p, "%f%n", &f, &length) == 1 ; p+=length )
				inner.push_back (f);

			tg_lat_table.push_back (inner);
		}
		else
			return -1;
	}

	size++;
	return size;
}

int ReadPairPingLatency (vector<NodeList> &pp_lat_table)
{
	FILE *fp;
	if ((fp = fopen ("pairping.dat", "rt")) == NULL)
		return -2;

	int length = 0;
	int size;
	int ip_mask1, ip_mask2, ip_mask3, ip_mask4;
	char buff[20480];
	char *p = buff;
	PairPing node;

	// parsing number of nodes
	while (fgets (buff, 20480, fp) != NULL)
	{
		if (buff[0] == '#' || buff[0] == ' ' || buff[0] == '\n')
			continue;

		while (sscanf (p, "%d.%d.%d.%d%n", &ip_mask1, &ip_mask2, &ip_mask3, &ip_mask4, &length ) == 4)
		{
			NodeList inner;
			sprintf (inner.SOUR_IP, "%d.%d.%d.%d\0", ip_mask1, ip_mask2, ip_mask3, ip_mask4);
			pp_lat_table.push_back (inner);
			p += length + 1;
		}

		break;
	}

	size = pp_lat_table.size ();

	// parsing pairs-ping for nodes
	for (int i = 0; fgets (buff, 20480, fp) != NULL && i < size; i++)
	{
		p = buff;

		for (int j = 0; j < size; ++j)
		{
			// parsing pattern 1: min_ping / avg_ping / max_ping
			if (sscanf (p, "%f/%f/%f%n", &node.MIN_PING, &node.AVG_PING, &node.MAX_PING, &length) == 3)
			{
				sprintf (node.DEST_IP, "%s", pp_lat_table[j].SOUR_IP);
				pp_lat_table[i].DEST_NODE.push_back (node);
				//printf ("node[%d][%d]: min_ping = %f, avg_ping = %f, max_ping = %f\n", i, j, node.MIN_PING, node.AVG_PING, node.MAX_PING);
			}

			// parsing pattern 2: ***no_traceroute***
			else if (strncmp(p, "***no_traceroute***", 19) == 0)
			{
				node.MIN_PING = -1.;
				node.AVG_PING = -1.;
				node.MAX_PING = -1.;

				//pp_lat_table[i].push_back (node);

				//printf ("node[%d][%d]: ***no_traceroute***\n", i, j);
				length = 19;
			}

			// parsing pattern 3:  ***last_ip_address***
			else if (sscanf (p, "***%d%*[.]%d%*[.]%d%*[.]%d***%n", &ip_mask1, &ip_mask2, &ip_mask3, &ip_mask4, &length) == 4)
			{
				node.MIN_PING = 10000;
				node.AVG_PING = 10000;
				node.MAX_PING = 10000;
				//pp_lat_table[i].push_back (node);
				//printf ("node[%d][%d]: %d.%d.%d.%d\n", i, j, ip_mask1, ip_mask2, ip_mask3, ip_mask4);
			}

			// parsing pattern 4: *** no data received for node (IP) ***
			else if (sscanf (p, "*** no data received for %*[0-9].%*[0-9].%*[0-9].%*[0-9] ***%n", &length) == 0)
			{
				/*
				do
				{
				node.MIN_PING = -1.;
				node.AVG_PING = -1.;
				node.MAX_PING = -1.;

				pp_lat_table[i].push_back (node);

				j++;
				}
				while (j < g_pp_lat_table.size ());
				*/

				//printf ("node[%d][%d]: *** no data received for IP... ***", i, j);
				//pp_lat_table.erase (g_pp_lat_table.end () - 1);
				i--;
				break;
			}

			// exception catch
			else
			{
				printf ("node[%d][%d]: Something Error!!", i, j);
				return -1;
			}

			p += length + 1;
		}
	}

	return size;
}

bool ReadBandwidth (vector<SimBand> &band)
{
	FILE *fp;
	if ((fp = fopen ("bandwidth.dat", "rt")) == NULL)
		return false;

	// check whether the sum of percentage read from file equals to 100% or not
	int upload,download = upload = 0;

	char buff[255];
	while (fgets (buff, 255, fp) != NULL)
	{
		// skip any comments or empty lines
		if (buff[0] == '#' || buff[0] == ' ' || buff[0] == '\n')
			continue;

		// read bandwidth from input file
		SimBand data;
		if (sscanf (buff, "%d%d%d", &download, &upload, &data.RANGEBASE ) != 3)
			return false;
		data.DOWNLINK = download;
		data.UPLINK = upload;
		band.push_back (data);
	}

	return true;
}
/*
void SetBandwidth (vector<SimBand> &band)
{
	// set different bandwidth limitation for each node
	int i, seed = 0;
	seed = (int)( 100 * ( rand() / (float)(RAND_MAX+1) ) );

	for (i = band.size () - 1; i >= 0 ; --i)
	{
		if (seed >= band[i].RANGEBASE)
		{
			para.DOWNLOAD_LIMIT = (band[i].DOWNLINK << 7) / para.STEPS_PERSEC;
			para.UPLOAD_LIMIT = (band[i].UPLINK << 7) / para.STEPS_PERSEC;
			break;
		}
	}
}
*/