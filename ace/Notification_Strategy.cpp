#include "ace/Notification_Strategy.h"

#if !defined (__ACE_INLINE__)
#include "ace/Notification_Strategy.inl"
#endif /* __ACE_INLINE __ */

ACE_RCSID(ace, Strategies, "Notification_Strategy.cpp,v 4.2 2005/10/28 16:14:53 ossama Exp")

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

ACE_Notification_Strategy::ACE_Notification_Strategy (ACE_Event_Handler *eh,
                                                      ACE_Reactor_Mask mask)
  : eh_ (eh),
    mask_ (mask)
{
}

ACE_Notification_Strategy::~ACE_Notification_Strategy (void)
{
}

ACE_END_VERSIONED_NAMESPACE_DECL
