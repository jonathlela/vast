// Dirent_Selector.cpp,v 4.10 2006/02/10 10:05:58 jwillemsen Exp

#include "ace/Dirent_Selector.h"

#if !defined (__ACE_INLINE__)
#include "ace/Dirent_Selector.inl"
#endif /* __ACE_INLINE__ */

#include "ace/OS_NS_dirent.h"
#include "ace/OS_NS_stdlib.h"

ACE_RCSID (ace,
           Dirent_Selector,
           "Dirent_Selector.cpp,v 4.10 2006/02/10 10:05:58 jwillemsen Exp")

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

// Construction/Destruction

ACE_Dirent_Selector::ACE_Dirent_Selector (void)
  : namelist_ (0),
    n_ (0)
{
}

ACE_Dirent_Selector::~ACE_Dirent_Selector (void)
{
  // Free up any allocated resources.
  this->close();
}

int
ACE_Dirent_Selector::open (const ACE_TCHAR *dir,
                           int (*sel)(const ACE_DIRENT *d),
                           int (*cmp) (const ACE_DIRENT **d1,
                                       const ACE_DIRENT **d2))
{
  n_ = ACE_OS::scandir (dir, &this->namelist_, sel, cmp);
  return n_;
}

int
ACE_Dirent_Selector::close (void)
{
  for (--n_; n_ >= 0; --n_)
    {
#if defined (ACE_LACKS_STRUCT_DIR)
      // Only the lacking-struct-dir emulation allocates this. Native
      // scandir includes d_name in the dirent struct itself.
      ACE_OS::free (this->namelist_[n_]->d_name);
#endif
      ACE_OS::free (this->namelist_[n_]);
    }

  ACE_OS::free (this->namelist_);
  this->namelist_ = 0;
  return 0;
}

ACE_END_VERSIONED_NAMESPACE_DECL
