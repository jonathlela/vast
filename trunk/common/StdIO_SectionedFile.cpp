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


#include "VASTUtil.h"

namespace Vast
{  

bool StdIO_SectionedFile::open  (const string & filename, SFOpenMode mode)
{
    if (_fp != NULL)
    {
        error ("StdIO_SectionedFile: open (): already opened.\n");
        return false;
    }

    string s_mode;
    switch (mode)
    {
    case SFMode_Read:
        s_mode = "rb";
        break;
    case SFMode_Write:
        s_mode = "wb";
        break;
    default:
        s_mode = "";
    }

    if ((_fp = fopen (filename.c_str (), s_mode.c_str ())) == NULL)
    {
        error ("StdIO_SectionedFile: open (): can't open file.");

        string s = "StdIO_SectionedFile: open (): filename: " + filename;
        error (s.c_str ());
        return false;
    }
    else
        _mode = mode;

    if (mode == SFMode_Read)
    {
        unsigned int section_count;
        vector<unsigned int> section_id;
        vector<unsigned int> section_size;
        if (fread (&section_count, sizeof (unsigned int), 1, _fp) != 1)
        {
            error ("StdIO_SectionedFile: open (): reading section count error.\n");
            return false;
        }

        for (unsigned int sci = 0; sci < section_count; sci ++)
        {
            unsigned int i[2];
            if (fread (&i, sizeof (unsigned int), 2, _fp) != 2)
            {
                error ("StdIO_SectionedFile: open (): reading section id&size error.\n");
                return false;
            }

            section_id.push_back (i[0]);
            section_size.push_back (i[1]);
        }

        for (unsigned int scii = 0; scii < section_count; scii ++)
        {
            section[section_id[scii]].insert (section[section_id[scii]].end (), section_size[scii], 0);
            if (fread (&(section[section_id[scii]][0]), section_size[scii], 1, _fp) != 1)
            {
                error ("StdIO_SectionedFile: open (): reading section content error.\n");
                section.clear ();
                return false;
            }
        }
    }

    return true;
}

int  StdIO_SectionedFile::read  (unsigned int section_id, void * buffer, int record_size, int record_count)
{
    if (_mode != SFMode_Read)
        return 0;

    unsigned int actually_readed = 0;
    int record_readed = 0;

    while ((section[section_id].size () >= static_cast<size_t>(record_size)) && (record_readed < record_count))
    {
        unsigned char * buffer_c = static_cast<unsigned char *>(buffer);
        memcpy (&buffer_c[actually_readed], &(section[section_id][actually_readed]), (size_t) record_size);
        record_readed ++;
        actually_readed += record_size;
    }

    section[section_id].erase (section[section_id].begin (), section[section_id].begin () + actually_readed);
    return record_readed;
}

int  StdIO_SectionedFile::write (unsigned int section_id, void * buffer, int record_size, int record_count)
{
    if (_mode != SFMode_Write)
        return 0;
    section[section_id].insert (section[section_id].end (), static_cast<unsigned char *>(buffer), static_cast<unsigned char *>(buffer) + (record_size * record_count));
    return record_count;
}

bool StdIO_SectionedFile::close ()
{
    if (_fp != NULL)
    {
        refresh ();
        fclose (_fp);
        _fp = NULL;
        
    }
    return true;
}

bool StdIO_SectionedFile::refresh ()
{
    if ((_fp != NULL) && (_mode == SFMode_Write))
    {
        unsigned int record_count = static_cast<unsigned int>(section.size ());
        vector<unsigned char> buffer;
        buffer.reserve (record_count * sizeof (unsigned int) * 2);

        map<unsigned int,vector<unsigned char> >::iterator sit = section.begin ();
        for (; sit != section.end (); sit ++)
        {
            unsigned int id = sit->first;
            unsigned int size = (unsigned int) sit->second.size ();
            buffer.insert (buffer.end (), reinterpret_cast<unsigned char *>(&id), reinterpret_cast<unsigned char *>(&id + 1));
            buffer.insert (buffer.end (), reinterpret_cast<unsigned char *>(&size), reinterpret_cast<unsigned char *>(&size + 1));
        }

        if (fwrite (&record_count, sizeof(unsigned int), 1, _fp) != 1)
        {
            error ("StdIO_SectionedFile: refresh (): writing record count to file error.\n");
            return false;
        }
        if (fwrite (&buffer[0], buffer.size (), 1, _fp) != 1)
        {
            error ("StdIO_SectionedFile: refresh (): writing section record to file error.\n");
            return false;
        }

        map<unsigned int,vector<unsigned char > >::iterator sbit = section.begin ();
        for (; sbit != section.end (); sbit ++)
        {
            if (fwrite (&(sbit->second[0]), sbit->second.size (), 1, _fp) != 1)
            {
                error ("StdIO_SectionedFile: refresh (): writing section content to file error.\n");
                return false;
            }
        }
    }
    return true;
}

void StdIO_SectionedFile::error (const char * msg)
{
#ifdef _ERROROUTPUT_H
    errout eo;
    eo.output (msg);
#else
    puts (msg);
#endif
}

} // namespace Vast