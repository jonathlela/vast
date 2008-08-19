// -*- C++ -*-

//=============================================================================
/**
 *  @file    UPIPE_Addr.h
 *
 *  UPIPE_Addr.h,v 4.10 2005/10/28 23:55:10 ossama Exp
 *
 *  @author Doug Schmidt
 */
//=============================================================================


#ifndef ACE_UPIPE_ADDR_H
#define ACE_UPIPE_ADDR_H

#include /**/ "ace/pre.h"

#include "ace/SPIPE_Addr.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
# pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

typedef ACE_SPIPE_Addr ACE_UPIPE_Addr;

ACE_END_VERSIONED_NAMESPACE_DECL

#include /**/ "ace/post.h"

#endif /* ACE_UPIPE_ADDR_H */
