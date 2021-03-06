// Select_Reactor.cpp,v 4.46 2005/10/28 16:14:55 ossama Exp

#include "ace/Select_Reactor.h"

ACE_RCSID(ace, Select_Reactor, "Select_Reactor.cpp,v 4.46 2005/10/28 16:14:55 ossama Exp")

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

#if defined (ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION)
# if defined (ACE_MT_SAFE) && (ACE_MT_SAFE != 0)
template class ACE_Reactor_Token_T<ACE_Token>;
template class ACE_Select_Reactor_T< ACE_Reactor_Token_T<ACE_Token> >;
template class ACE_Lock_Adapter< ACE_Reactor_Token_T<ACE_Token> >;
template class ACE_Guard< ACE_Reactor_Token_T<ACE_Token> >;
# else
template class ACE_Reactor_Token_T<ACE_Noop_Token>;
template class ACE_Select_Reactor_T< ACE_Reactor_Token_T<ACE_Noop_Token> >;
template class ACE_Lock_Adapter< ACE_Reactor_Token_T<ACE_Noop_Token> >;
# endif /* ACE_MT_SAFE && ACE_MT_SAFE != 0 */
#elif defined (ACE_HAS_TEMPLATE_INSTANTIATION_PRAGMA)
# if defined (ACE_MT_SAFE) && (ACE_MT_SAFE != 0)
#   pragma instantiate ACE_Reactor_Token_T<ACE_Token>
#   pragma instantiate ACE_Select_Reactor_T< ACE_Reactor_Token_T<ACE_Token> >
#   pragma instantiate ACE_Lock_Adapter< ACE_Reactor_Token_T<ACE_Token> >
#   pragma instantiate ACE_Guard< ACE_Reactor_Token_T<ACE_Token> >
# else
#   pragma instantiate ACE_Reactor_Token_T<ACE_Noop_Token>
#   pragma instantiate ACE_Select_Reactor_T< ACE_Reactor_Token_T<ACE_Noop_Token> >
#   pragma instantiate ACE_Lock_Adapter< ACE_Reactor_Token_T<ACE_Noop_Token> >
# endif /* ACE_MT_SAFE && ACE_MT_SAFE != 0 */
#endif /* ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION */

ACE_END_VERSIONED_NAMESPACE_DECL
