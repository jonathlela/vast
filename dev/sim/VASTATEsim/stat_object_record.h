
#ifndef _VASTATESIM_STAT_OBJECT_RECORD_H
#define _VASTATESIM_STAT_OBJECT_RECORD_H


class stat_object_record
{
public:
    stat_object_record ()
        : version(0), pos_version(0), pos(Position(0,0)), player_aoi(0)
    {
    }

    ~stat_object_record ()
    {
    }

public:
    timestamp_t pos_version;
	timestamp_t version;

	Position pos;
	int player_aoi;

    vector<id_t> k_peers;
	int seem;
};


#endif /* _VASTATESIM_STAT_OBJECT_RECORD_H */