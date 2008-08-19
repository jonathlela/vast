// -*- C++ -*-
//
// Reactor_Notification_Strategy.inl,v 4.4 2005/10/28 16:14:55 ossama Exp

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

ACE_INLINE ACE_Reactor *
ACE_Reactor_Notification_Strategy::reactor (void)
{
  return this->reactor_;
}

ACE_INLINE void
ACE_Reactor_Notification_Strategy::reactor (ACE_Reactor *r)
{
  this->reactor_ = r;
}

ACE_END_VERSIONED_NAMESPACE_DECL
