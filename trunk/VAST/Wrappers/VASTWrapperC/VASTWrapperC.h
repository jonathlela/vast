/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2010 Shun-Yun Hu (syhu@yahoo.com)
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
 *  VASTWrapperC -- header for a C-interface wrapper of the VAST C++ class
 *
 *  History:
 *      2010/06/17      first version
 *   
 */

#ifndef VAST_WRAPPER_C_H
#define VAST_WRAPPER_C_H

#ifdef _WINDOWS
#define EXPORT __declspec (dllexport)
#else
#define EXPORT /* nothing */
#endif

// define uintxx_t types
#include "../../common/standard/stdint.h"

#ifdef WIN32
// disable warning about "identifier exceeds'255' characters"
#pragma warning(disable: 4786)
#endif

typedef struct 
{
    const char  *msg;       // content of message
    size_t      size;       // size of message
    uint64_t    from;       // sender of message
} VAST_C_Msg;

// basic init of VAST
EXPORT int          InitVAST (bool is_gateway, uint16_t port);  // initialize the VAST library
EXPORT int          ShutVAST ();                                // close down the VAST library

// unique layer
EXPORT void         VASTReserveLayer (uint32_t layer = 0);          // obtain a unique & unused layer (preferred layer # as input)
EXPORT uint32_t     VASTGetLayer ();                                // get the currently reserved layer, 0 for not yet reserved
EXPORT bool         VASTReleaseLayer ();                            // release back the layer

// main join / move / publish functions
EXPORT bool         VASTJoin (float x, float y, uint16_t radius);   // join at location on a partcular layer
EXPORT bool         VASTLeave ();                                   // leave the overlay
EXPORT bool         VASTMove (float x, float y);                    // move to a new position
EXPORT size_t       VASTTick (size_t time_budget = 0);              // do routine processsing, each  in millisecond)
EXPORT bool         VASTPublish (const char *msg, size_t size, uint16_t radius = 0);     // publish a message to current layer at current location, with optional radius
EXPORT VAST_C_Msg * VASTReceive ();                                 // receive any message received

// helpers
EXPORT bool         isVASTInit ();                                  // is initialized done (ready to join)
EXPORT bool         isVASTJoined ();                                // whether the join is successful
EXPORT uint64_t     GetSelfID ();                               // obtain an ID of self


#endif // VAST_WRAPPER_C_H


