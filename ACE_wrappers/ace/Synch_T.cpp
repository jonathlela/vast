// Synch_T.cpp,v 4.63 2005/10/28 16:14:56 ossama Exp

#ifndef ACE_SYNCH_T_CPP
#define ACE_SYNCH_T_CPP

#include "ace/Thread.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
# pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

// FUZZ: disable check_for_synch_include
#include "ace/Synch_T.h"
#include "ace/Log_Msg.h"

#include "ace/Lock_Adapter_T.cpp"
#include "ace/Reverse_Lock_T.cpp"
#include "ace/Guard_T.cpp"
#include "ace/TSS_T.cpp"
#include "ace/Condition_T.cpp"

#endif /* ACE_SYNCH_T_CPP */
