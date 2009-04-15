// -*- C++ -*-
//
// UPIPE_Acceptor.inl,v 4.2 2005/10/28 23:55:10 ossama Exp

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

ACE_INLINE int
ACE_UPIPE_Acceptor::remove (void)
{
  ACE_TRACE ("ACE_UPIPE_Acceptor::remove");
  return this->ACE_SPIPE_Acceptor::remove ();
}

ACE_END_VERSIONED_NAMESPACE_DECL
