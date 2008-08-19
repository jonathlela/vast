// -*- C++ -*-
//
// Process_Manager.inl,v 4.2 2005/10/28 16:14:54 ossama Exp

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

ACE_INLINE size_t
ACE_Process_Manager::managed (void) const
{
  return current_count_;
}

ACE_END_VERSIONED_NAMESPACE_DECL
