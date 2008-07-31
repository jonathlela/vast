/*
 * VAST, a scalable peer-to-peer network for virtual environments
 * Copyright (C) 2005-2007 Shun-Yun Hu (syhu@yahoo.com)
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

#ifndef VAST_CONFIG_H
#define VAST_CONFIG_H

#ifdef WIN32
// disable warning about "identifier exceeds'255' characters"
#pragma warning(disable: 4786)
// disable warning about "unreferenced formal parameter"
#pragma warning(disable: 4100)
// disable warning about LNK4089 "all references to "WS2_32.dll" discarded by /OPT:REF"
//#pragma warning (disable : 4089)
// disable warning about C4996: 'sprintf' was declared deprecated
#pragma warning(disable: 4996)

#endif

#ifdef _WINDOWS
#define EXPORT __declspec (dllexport)
#else
#define EXPORT /* nothing */
#endif

#include <stdio.h>      // for printing out debug messages

#define VAST_BUFSIZ     32000       // generic message buffer size, note that send/recv cannot exceed this size
#define VAST_MSG_SIZE   20          // maximum number of internal messages supported by VAST

#define DEBUG_MSG_ID(x) {printf ("[%3d] ", (int)_self.id); printf ((x));}
#define ERROR_MSG_ID(x) {printf ("[%3d] error: ", (int)_self.id); printf ((x));}

#define DEBUG_MSG(x) {printf ((x));}
#define ERROR_MSG(x) {printf ((x));}

#ifdef _DEBUG
#define DEBUG_DETAIL  // show detail debug messages
#endif

// force to enable debug message
//#define DEBUG_DETAIL

#define ACE_DISABLED
#define RECORD_INCONSISTENT_NODES_

// Send all messages by bandwidth limitation
// /* don't define anything */
// Send only PAYLOAD by bandwidth limitation
//#define VAST_NET_EMULATED_BL_TYPE_SENDDIRECT(msgtype) (msgtype!=PAYLOAD)
// Comment below line to disable a limitation of size of send queue in NET_EMU_BL
#define VAST_NET_EMULATED_BL_SIZED_QUEUE

#endif // VAST_CONFIG_H

