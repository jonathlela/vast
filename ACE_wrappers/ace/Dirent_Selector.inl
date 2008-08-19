// -*- C++ -*-
//
// Dirent_Selector.inl,v 4.5 2006/02/10 10:05:58 jwillemsen Exp

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

ACE_INLINE int
ACE_Dirent_Selector::length (void) const
{
  return n_;
}

ACE_INLINE ACE_DIRENT *
ACE_Dirent_Selector::operator[] (const int n) const
{
  return this->namelist_[n];
}

ACE_END_VERSIONED_NAMESPACE_DECL
