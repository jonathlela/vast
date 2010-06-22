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

// NOTE: extern "C" is to make sure the exported function symbols are "C-style" without attachments
//       so it could be recognized
#ifdef WIN32
#define EXPORT extern "C" __declspec (dllexport)
#define VAST_CALL __stdcall
#else
#define EXPORT      /* nothing */
#define VAST_CALL   /* nothing */
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
EXPORT int      VAST_CALL    InitVAST (bool is_gateway, const char *gateway);  // initialize the VAST library
EXPORT int      VAST_CALL    ShutVAST ();                                // close down the VAST library

// unique layer
EXPORT void     VAST_CALL    VASTReserveLayer (uint32_t layer = 0);          // obtain a unique & unused layer (preferred layer # as input)
EXPORT uint32_t VAST_CALL    VASTGetLayer ();                                // get the currently reserved layer, 0 for not yet reserved
EXPORT bool     VAST_CALL    VASTReleaseLayer ();                            // release back the layer

// main join / move / publish functions
EXPORT bool     VAST_CALL    VASTJoin (float x, float y, uint16_t radius);   // join at location on a partcular layer
EXPORT bool     VAST_CALL    VASTLeave ();                                   // leave the overlay
EXPORT bool     VAST_CALL    VASTMove (float x, float y);                    // move to a new position
EXPORT size_t   VAST_CALL    VASTTick (size_t time_budget = 0);              // do routine processsing, each  in millisecond)
EXPORT bool     VAST_CALL    VASTPublish (const char *msg, size_t size, uint16_t radius = 0);     // publish a message to current layer at current location, with optional radius
//EXPORT VAST_C_Msg * VAST_CALL VASTReceive ();                                 // receive any message received
//EXPORT bool     VAST_CALL    VASTReceive (char **msg, size_t *size, uint64_t *from);                                 // receive any message received
EXPORT const char* VAST_CALL VASTReceive (size_t *size, uint64_t *from);                                 // receive any message received

// helpers
EXPORT bool     VAST_CALL    isVASTInit ();                                  // is initialized done (ready to join)
EXPORT bool     VAST_CALL    isVASTJoined ();                                // whether the join is successful
EXPORT uint64_t VAST_CALL    VASTGetSelfID ();                               // obtain an ID of self


#endif // VAST_WRAPPER_C_H


