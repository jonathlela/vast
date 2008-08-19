// -*- C++ -*-
// Event.inl,v 4.2 2005/10/28 16:14:52 ossama Exp

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

ACE_INLINE ACE_event_t
ACE_Event::handle (void) const
{
  return this->handle_;
}

ACE_INLINE void
ACE_Event::handle (ACE_event_t new_handle)
{
  this->handle_ = new_handle;
}

ACE_END_VERSIONED_NAMESPACE_DECL
