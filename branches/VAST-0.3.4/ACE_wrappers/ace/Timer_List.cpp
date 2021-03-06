// Timer_List.cpp,v 4.34 2005/11/24 09:48:55 ossama Exp

#include "ace/Timer_List.h"
#include "ace/Synch_Traits.h"
#include "ace/Recursive_Thread_Mutex.h"

ACE_RCSID(ace, Timer_List, "Timer_List.cpp,v 4.34 2005/11/24 09:48:55 ossama Exp")

#if defined (ACE_HAS_BROKEN_HPUX_TEMPLATES)
#include "ace/Timer_Hash.h"
#include "ace/Timer_List_T.cpp"

#if defined (ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION)

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

template class
    ACE_Timer_List_T<
        ACE_Event_Handler*,
        ACE_Timer_Hash_Upcall<
        ACE_Event_Handler*,
        ACE_Event_Handler_Handle_Timeout_Upcall<ACE_Null_Mutex>,
        ACE_Null_Mutex>,
    ACE_Null_Mutex>;

template class
ACE_Timer_List_Iterator_T<
    ACE_Event_Handler*,
    ACE_Timer_Hash_Upcall<
        ACE_Event_Handler*,
        ACE_Event_Handler_Handle_Timeout_Upcall<ACE_Null_Mutex>,
        ACE_Null_Mutex>,
    ACE_Null_Mutex>;

ACE_END_VERSIONED_NAMESPACE_DECL

#endif /* ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION */
#endif /* ACE_HAS_BROKEN_HPUX_TEMPLATES */

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

#if defined (ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION)
template class ACE_Timer_List_T<ACE_Event_Handler *, ACE_Event_Handler_Handle_Timeout_Upcall<ACE_SYNCH_RECURSIVE_MUTEX>, ACE_SYNCH_RECURSIVE_MUTEX>;
template class ACE_Timer_List_Iterator_T<ACE_Event_Handler *, ACE_Event_Handler_Handle_Timeout_Upcall<ACE_SYNCH_RECURSIVE_MUTEX>, ACE_SYNCH_RECURSIVE_MUTEX>;
#elif defined (ACE_HAS_TEMPLATE_INSTANTIATION_PRAGMA)
#pragma instantiate ACE_Timer_List_T<ACE_Event_Handler *, ACE_Event_Handler_Handle_Timeout_Upcall<ACE_SYNCH_RECURSIVE_MUTEX>, ACE_SYNCH_RECURSIVE_MUTEX>
#pragma instantiate ACE_Timer_List_Iterator_T<ACE_Event_Handler *, ACE_Event_Handler_Handle_Timeout_Upcall<ACE_SYNCH_RECURSIVE_MUTEX>, ACE_SYNCH_RECURSIVE_MUTEX>
#endif /* ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION */

ACE_END_VERSIONED_NAMESPACE_DECL
