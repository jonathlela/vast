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
 *
 *
 */

#ifndef _COMMON_TOOLS_SECTIONED_FILE_H
#define _COMMON_TOOLS_SECTIONED_FILE_H

#include "config.h"
#include <string>


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

class FileClassFactory
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

#endif /* _COMMON_TOOLS_SECTIONED_FILE_H */

