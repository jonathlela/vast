/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007 Shao-Chen Chang (cscxcs at gmail.com)
 *                    Shun-Yun Hu     (syhu at yahoo.com)
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
    Utility classes for VAST: 
    
    NOTE: these classes should be self-sufficent, and should not rely on 
          any other VAST-related classes or type definitions except C++ standard includes
*/

#ifndef _VAST_UTIL_H
#define _VAST_UTIL_H

#include "Config.h"
#include "VASTTypes.h"

#include <string>
#include <map>
#include <vector>
#include <math.h>


using namespace std;

// forward declaration

//class EXPORT errout;

// 
// errout: Error output utility
//

namespace Vast {

class EXPORT errout
{
public:
	errout ();
	virtual ~errout ();
	void setout (errout * out);
	void output (const char * str);
	virtual void outputHappened (const char * str);

    static char textbuf [10240];
private:
	static errout * _actout;
};

//
//  SectionedFile: store/load data onto file in sections
//

enum SFOpenMode
{
    SFMode_NULL,
    SFMode_Read,
    SFMode_Write
};

class EXPORT SectionedFile
{
public:
    SectionedFile ()
    {
    }

    virtual ~SectionedFile ()
    {
    }

    virtual bool open    (const std::string & file_name, SFOpenMode mode) = 0;
    virtual int  read    (unsigned int section_id, void * buffer, int record_size, int record_count) = 0;
    virtual int  write   (unsigned int section_id, void * buffer, int record_size, int record_count) = 0;
    virtual bool close   () = 0;
    virtual bool refresh () = 0;
};

class EXPORT FileClassFactory
{
public:
    FileClassFactory ()
    {
    }

    ~FileClassFactory ()
    {
    }

    SectionedFile * CreateFileClass (int type);
    bool            DestroyFileClass (SectionedFile * filec);
};


//
//  StdIO_SectionedFile: store/load data onto StdIO
//

class StdIO_SectionedFile : public SectionedFile
{
public:
    StdIO_SectionedFile ()
        : _fp (NULL), _mode (SFMode_NULL)
    {
    }

    ~StdIO_SectionedFile ()
    {
        close ();
    }

    bool open  (const string & filename, SFOpenMode mode);
    int  read  (unsigned int section_id, void * buffer, int record_size, int record_count);
    int  write (unsigned int section_id, void * buffer, int record_size, int record_count);
    bool close ();
    bool refresh ();
    void error (const char * msg);

private:
    FILE * _fp;
    map<unsigned int,vector<unsigned char> > section;
    SFOpenMode _mode;
};

class EXPORT Compressor
{
public:
    size_t compress (unsigned char *source, unsigned char *dest, size_t size);
};

} // end namespace Vast

#endif /* _VAST_UTIL_H */

