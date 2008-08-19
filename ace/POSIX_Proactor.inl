// -*- C++ -*-
//
// POSIX_Proactor.inl,v 4.3 2005/10/28 16:14:54 ossama Exp

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

ACE_INLINE
ACE_Asynch_Pseudo_Task& ACE_POSIX_Proactor::get_asynch_pseudo_task (void)
{
  return this->pseudo_task_;
}

ACE_END_VERSIONED_NAMESPACE_DECL
