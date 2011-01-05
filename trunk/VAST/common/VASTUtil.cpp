/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2010   Shun-Yun Hu     (syhu@ieee.org)
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
    Implementation of VASTUtil

*/

#include "VASTUtil.h"

#include "ace/ACE.h"        // ACE_OS::gettimeofday () for TimeMonitor
//#include "ace/OS.h"
#include "ace/Task.h"               // for ACE_Thread_Mutex

#include "stdarg.h"         // taking variable arguments in writeLogFile

namespace Vast
{  

// Global static pointer used to ensure a single instance of the class.
LogManager* LogManager::_instance = NULL; 

// mutex for log file (to avoid contention)
ACE_Thread_Mutex g_log_mutex;

LogManager* LogManager::instance()
{
   if (!_instance)   // Only allow one instance of class to be generated.
      _instance = new LogManager;
 
   return _instance;
}

void 
LogManager::terminateInstance ()
{
    if (_instance)
        delete _instance;
    _instance = NULL;
}
 
bool LogManager::setLogFile (FILE *fp)
{
    if (_logfile || fp == NULL)
        return false;

    _logfile = fp;
    return true;
}


// TODO: make it thread-safe
// example from:
// http://www.ozzu.com/cpp-tutorials/tutorial-writing-custom-printf-wrapper-function-t89166.html
bool LogManager::writeLogFile (const char *format, ...)
{
    g_log_mutex.acquire ();

    // print time
	time_t rawtime;
	time (&rawtime);
	tm *timeinfo = gmtime (&rawtime);

    if (_logfile)
    {
        // print formatted date/time in string
        //fprintf (_logfile, "%s: ", asctime (timeinfo)); 
        // time only
        fprintf (_logfile, "[GMT %04d/%02d/%02d %02d:%02d:%02d] ", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    }

    if (_logfile)
    {
        va_list args;
        va_start (args, format);
        vfprintf (_logfile, format, args);
        va_end (args);
    }

    // print to stdout    
    va_list args;
    va_start (args, format);
    vfprintf (stdout, format, args);
    va_end (args);

    if (_logfile)
    {
        fprintf (_logfile, "\n");
        fflush (_logfile);
    }
    fprintf (stdout, "\n");

    g_log_mutex.release ();

    return true;
}


bool LogManager::unsetLogFile ()
{
    if (_logfile == NULL)
        return false;

    _logfile = NULL;
    return true;
}


TimeMonitor* TimeMonitor::_instance = NULL;

TimeMonitor::TimeMonitor ()
{
    ACE::init ();
    _budget = 0;

    ACE_Time_Value time = ACE_OS::gettimeofday ();
    _start = (unsigned long long)time.sec () * MICROSECOND_PERSEC + time.usec ();
}


// set how much time is left (in microsecond)
void 
TimeMonitor::setBudget (int time_budget)
{
    if (time_budget > MICROSECOND_PERSEC)
    {
        printf ("TimeMonitor::setBudget () budget exceeds 1000 ms, may want to check if this is correct\n");
    }

    _budget = time_budget;

    ACE_Time_Value time = ACE_OS::gettimeofday ();
    _start = (unsigned long long)time.sec () * MICROSECOND_PERSEC + time.usec ();
    _first = true;
}

// still time available?
int
TimeMonitor::available ()
{
    // unlimited budget returns (-1)
    if (_budget == 0)
        return (-1);
    // budget already used up, return immediately
    else if (_budget < 0)
        return 0;

    // check how much time has elapsed since setBudget was called
    ACE_Time_Value time = ACE_OS::gettimeofday();

    unsigned long long current = (unsigned long long)time.sec () * MICROSECOND_PERSEC + time.usec ();
    long long elapsed = (long long)(current - _start);

    //if (elapsed > 0)
    //    printf ("available (): elapsed %ld budget: %ld\n", (long)elapsed, (long)_budget);

    int time_left = (int)(_budget - elapsed);

    if (time_left < 0)
    {
        printf ("available (): time_left: %d elapsed %ld budget: %ld\n", time_left, (long)elapsed, (long)_budget);
        time_left = 0;
    }

    // we make sure that a positive response is returned at least once per cycle
    if (_first == true)
    {
        _first = false;
     
        if (time_left == 0)
            time_left = 1;
    }

    return time_left;
}

// return a global instance of TimeMonitor
TimeMonitor *
TimeMonitor::instance ()
{
   if (!_instance)   // Only allow one instance of class to be generated.
      _instance = new TimeMonitor;
 
   return _instance;
}

// end the TimeMonitor and release memory
void 
TimeMonitor::terminateInstance ()
{
    if (_instance)
        delete _instance;

    _instance = NULL;
}


} // namespace Vast

