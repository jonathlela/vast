/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2007 Shao-Chen Chang (cscxcs at gmail.com)
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
*  Sectioned File (Section-based File I/O class)
*      : Standard I/O Implementation
*
*/

#ifndef _COMMON_TOOLS_STDIO_SECTIONED_FILE_H
#define _COMMON_TOOLS_STDIO_SECTIONED_FILE_H

#include <string>
#include <map>
#include <vector>
using namespace std;
#include <stdio.h>

#include "SectionedFile.h"

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



#endif /* _COMMON_TOOLS_STDIO_SECTIONED_FILE_H */
