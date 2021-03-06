// -*- C++ -*-
//
// Token_Manager.inl,v 4.2 2005/10/28 23:55:10 ossama Exp

#if defined (ACE_HAS_TOKENS_LIBRARY)

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

ACE_INLINE ACE_TOKEN_CONST::MUTEX &
ACE_Token_Manager::mutex (void)
{
  ACE_TRACE ("ACE_Token_Manager::mutex");
  return lock_;
}

ACE_INLINE void
ACE_Token_Manager::debug (int d)
{
  ACE_TRACE ("ACE_Token_Manager::debug");
  debug_ = d;
}

ACE_END_VERSIONED_NAMESPACE_DECL

#endif /* ACE_HAS_TOKENS_LIBRARY */
