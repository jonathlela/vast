// -*- C++ -*-
// OS_Errno.cpp,v 4.5 2005/10/28 16:14:54 ossama Exp

#include "ace/OS_Errno.h"

ACE_RCSID(ace, OS_Errno, "OS_Errno.cpp,v 4.5 2005/10/28 16:14:54 ossama Exp")

// Inlining this class on debug builds with gcc on Solaris can cause
// deadlocks during static initialization.
#if !defined (ACE_HAS_INLINED_OSCALLS) || \
    (defined (__GNUG__) && defined (__sun__) && !defined (ACE_NDEBUG))
# if defined (ACE_INLINE)
#  undef ACE_INLINE
# endif /* ACE_INLINE */
# define ACE_INLINE
# include "ace/OS_Errno.inl"
#endif /* ACE_HAS_INLINED_OS_CALLS */

#if defined (ACE_HAS_WINCE_BROKEN_ERRNO)

#include "ace/OS_Memory.h"

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

ACE_CE_Errno *ACE_CE_Errno::instance_ = 0;
DWORD ACE_CE_Errno::errno_key_ = 0xffffffff;

void
ACE_CE_Errno::init ()
{
  ACE_NEW (ACE_CE_Errno::instance_,
           ACE_CE_Errno ());
  ACE_CE_Errno::errno_key_ = TlsAlloc ();
}

void
ACE_CE_Errno::fini ()
{
  TlsFree (ACE_CE_Errno::errno_key_);
  delete ACE_CE_Errno::instance_;
  ACE_CE_Errno::instance_ = 0;
}

ACE_END_VERSIONED_NAMESPACE_DECL

#endif /* ACE_HAS_WINCE_BROKEN_ERRNO */
