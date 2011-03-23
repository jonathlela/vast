/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2011 Shun-Yun Hu (syhu@ieee.org)
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

// NOTE: extern "C" is to make sure the exported function symbols are "C-style" 
//       without attachments so it could be recognized
#ifdef WIN32
#define VASTC_EXPORT extern "C" __declspec (dllexport)
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
VASTC_EXPORT bool     VAST_CALL    InitVAST (bool is_gateway, const char *gateway);  // initialize the VAST library
VASTC_EXPORT bool     VAST_CALL    ShutVAST ();                                // close down the VAST library

// unique layer
VASTC_EXPORT void     VAST_CALL    VASTReserveLayer (uint32_t layer = 0);          // obtain a unique & unused layer (preferred layer # as input)
VASTC_EXPORT uint32_t VAST_CALL    VASTGetLayer ();                                // get the currently reserved layer, 0 for not yet reserved
VASTC_EXPORT bool     VAST_CALL    VASTReleaseLayer ();                            // release back the layer

// main join / move / publish functions
VASTC_EXPORT bool     VAST_CALL     VASTJoin (uint16_t world_id, float x, float y, uint16_t radius);   // join at location on a partcular layer
VASTC_EXPORT bool     VAST_CALL     VASTLeave ();                                   // leave the overlay
VASTC_EXPORT bool     VAST_CALL     VASTMove (float x, float y);                    // move to a new position
VASTC_EXPORT int      VAST_CALL     VASTTick (int time_budget = 0);              // do routine processsing, each  in millisecond)
VASTC_EXPORT bool     VAST_CALL     VASTPublish (const char *msg, size_t size, uint16_t radius = 0);     // publish a message to current layer at current location, with optional radius
VASTC_EXPORT const char* VAST_CALL  VASTReceive (uint64_t *from, size_t *size);                                 // receive any message received

// socket messaging
VASTC_EXPORT uint64_t VAST_CALL     VASTOpenSocket (const char *ip_port, bool is_secure = false);                           // open a new TCP socket
VASTC_EXPORT bool     VAST_CALL     VASTCloseSocket (uint64_t socket);                              // close a TCP socket
VASTC_EXPORT bool     VAST_CALL     VASTSendSocket (uint64_t socket, const char *msg, size_t size); // send a message to a socket
VASTC_EXPORT const char * VAST_CALL VASTReceiveSocket (uint64_t *from, size_t *size);               // receive a message from socket, if any

// helpers
VASTC_EXPORT bool     VAST_CALL    isVASTInit ();                                  // is initialized done (ready to join)
VASTC_EXPORT bool     VAST_CALL    isVASTJoined ();                                // whether the join is successful
VASTC_EXPORT uint64_t VAST_CALL    VASTGetSelfID ();                               // obtain an ID of self


#endif // VAST_WRAPPER_C_H


