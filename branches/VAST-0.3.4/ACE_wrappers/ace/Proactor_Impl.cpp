// Proactor_Impl.cpp,v 4.3 2005/10/28 16:14:54 ossama Exp

#include "ace/Proactor_Impl.h"

ACE_RCSID (ace,
           Proactor_Impl,
           "Proactor_Impl.cpp,v 4.3 2005/10/28 16:14:54 ossama Exp")

#if ((defined (ACE_WIN32) && !defined (ACE_HAS_WINCE)) || (defined (ACE_HAS_AIO_CALLS)))
// This only works on Win32 platforms and on Unix platforms supporting
// aio calls.

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

ACE_Proactor_Impl::~ACE_Proactor_Impl (void)
{
}

ACE_END_VERSIONED_NAMESPACE_DECL

#endif
