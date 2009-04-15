
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

#ifndef _VASTATESIM_RECORD_FILE_MANAGER_H
#define _VASTATESIM_RECORD_FILE_MANAGER_H

#include <stdio.h>
#include "vastutil.h"
#include "vastatesim.h"

struct b_target
{
    VAST::id_t object_id;
	Position center;
	int radius;
    char last_action [2];
};

struct food_creation_reg
{
	Position pos;
	int count;
};

struct RecordFileHeader
{
	int NODE_SIZE;
	int WORLD_WIDTH;
	int WORLD_HEIGHT;
	int TIME_STEPS;
};

struct StepHeader
{
    int timestamp;
};

struct NodeInfo
{
    VAST::id_t     id;
    obj_id_t obj_id;
    int      type;
    Position pos;
};

class RecordFileManager
{
// constant defines //
public:
	static const int MODE_READ   = 1;
	static const int MODE_WRITE  = 2;
//////////////////////

public:
	RecordFileManager ()
		: _rfp (NULL), _mode (0), _initialized (false)
	{
	}

	RecordFileManager (int mode, const char * filename)
		: _rfp (NULL), _mode (0), _initialized (false)
	{
		initRecordFile (mode, filename);
	}

	~RecordFileManager ()
	{
        _closefp ();
	}

    inline void _closefp ()
    {
    	if (_rfp != NULL)
		{
			fclose (_rfp);
            _rfp = NULL;
            _initialized = false;
		}
    }

	inline bool IsInitialized ()
	{
		return _initialized;
	}

	bool initRecordFile (int mode, const char * filename)
	{
        if (IsInitialized ())
            return false;

		if ((filename == NULL) || (filename[0] == '\0'))
        {
            _initialized = false;
			return false;
        }

		switch (mode)
		{
		case MODE_READ:
			_rfp = fopen (filename, "rb");
			break;
		case MODE_WRITE:
			_rfp = fopen (filename, "wb");
			break;
		default:
			_e.output ("VASTATEsim: Open file failed." NEWLINE);
		}

		if (_rfp != NULL)
		{
			_initialized = true;
			_mode = mode;
            return true;
		}
        return false;
	}

	bool writeFileHeader (SimPara *para)
	{
        if (!IsInitialized () || (_mode != MODE_WRITE))
            return false;

		RecordFileHeader rfh;
		memset (&rfh, 0, sizeof (RecordFileHeader));
		rfh.NODE_SIZE    = para->NODE_SIZE;
		rfh.TIME_STEPS   = para->TIME_STEPS;
		rfh.WORLD_HEIGHT = rfh.WORLD_HEIGHT;
		rfh.WORLD_WIDTH  = rfh.WORLD_WIDTH;

        if (fwrite (&rfh, sizeof(RecordFileHeader), 1, _rfp) != 1)
            return false;
        return true;
	}

    bool readFileHeader (RecordFileHeader & rfh)
    {
        if (!IsInitialized () || (_mode != MODE_READ))
            return false;

        return (1 == fread (&rfh, sizeof (RecordFileHeader), 1, _rfp));
    }

    bool compareFileHeader (SimPara *para)
    {
        if (!IsInitialized () || (_mode != MODE_READ))
            return false;

        RecordFileHeader rfh, curr_rfh;
        memset (&rfh, 0, sizeof (RecordFileHeader));
        memset (&curr_rfh, 0, sizeof (RecordFileHeader));
		curr_rfh.NODE_SIZE    = para->NODE_SIZE;
		curr_rfh.TIME_STEPS   = para->TIME_STEPS;
		curr_rfh.WORLD_HEIGHT = rfh.WORLD_HEIGHT;
        curr_rfh.WORLD_WIDTH  = rfh.WORLD_WIDTH;        

        if (fread (&rfh, sizeof(RecordFileHeader), 1, _rfp) != 1)
            return false;
        else if (memcmp (&rfh, &curr_rfh, sizeof (RecordFileHeader)) == 0)
            return true;

        return false;
    }

	bool writeFood (SimPara *para, vector<int>& food_start_index, vector<food_creation_reg>& food_store, bool BacktoStart = false)
	{
        if (!IsInitialized () || (_mode != MODE_WRITE))
            return false;

        if (BacktoStart)
            if (fseek (_rfp, 0, SEEK_SET) != 0)
                return false;

        int f;
        for (f=0; f <= para->TIME_STEPS; f++)
            if (fwrite (&food_start_index[f], sizeof(int), 1, _rfp) != 1)
                return false;

        for (f=0; f < food_start_index[para->TIME_STEPS]; f++)
            if (fwrite (&food_store[f], sizeof(food_creation_reg), 1, _rfp) != 1)
                return false;

        return true;
	}

    bool readFood (SimPara *para, vector<int> & fsi, vector<food_creation_reg> & fs)
    {
        if (!IsInitialized () || (_mode != MODE_READ))
            return false;

        int f;
        fsi.reserve (para->TIME_STEPS + 1);
        for (f=0; f <= para->TIME_STEPS; f++)
            if (fread (&fsi[f], sizeof(int), 1, _rfp) != 1)
                return false;

        fs.reserve (fsi[para->TIME_STEPS]);
        for (f=0; f < fsi[para->TIME_STEPS]; f++)
            if (fread (&fs[f], sizeof(food_creation_reg), 1, _rfp) != 1)
                return false;

        return true;
    }

    bool writeAction (int & action, b_target & target)
    {
        if (!IsInitialized () || (_mode != MODE_WRITE))
            return false;

        bool a,b;
        a = (1 == fwrite (&action, sizeof (int), 1, _rfp));
        b = (1 == fwrite (&target, sizeof (b_target), 1, _rfp));
        return (a && b);
    }

    bool readAction (int & action, b_target & target)
    {
        if (!IsInitialized () || (_mode != MODE_READ))
            return false;

        bool a,b;
        a = (1 == fread (&action, sizeof(int), 1, _rfp));
	    b = (1 == fread (&target, sizeof(b_target), 1, _rfp));
        return (a && b);
    }

    bool writeNode (const NodeInfo & n)
    {
        if (!IsInitialized () || (_mode != MODE_WRITE))
            return false;

        return (1 == fwrite (&n, sizeof (NodeInfo), 1, _rfp));
    }

    bool readNode (NodeInfo & n)
    {
        if (!IsInitialized () || (_mode != MODE_READ))
            return false;

        return (1 == fread (&n, sizeof (NodeInfo), 1, _rfp));
    }

    bool writeStepHeader (const StepHeader & h)
    {
        if (!IsInitialized () || (_mode != MODE_WRITE))
            return false;

        return (1 == fwrite (&h, sizeof (StepHeader), 1, _rfp));
    }

    bool readStepHeader (StepHeader & header)
    {
        if (!IsInitialized () || (_mode != MODE_READ))
            return false;

        return (1 == fread (&header, sizeof (StepHeader), 1, _rfp));
    }

    bool refresh ()
    {
        if (!IsInitialized () || _mode != MODE_WRITE)
            return false;

        fflush (_rfp);
        return true;
    }

private:
	FILE * _rfp;
	int _mode;
	bool _initialized;
	errout _e;
};


#endif /* _VASTATESIM_RECORD_FILE_MANAGER_H */

