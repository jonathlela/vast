// Obstack.cpp,v 4.11 2005/11/22 09:23:55 ossama Exp

#include "ace/Obstack.h"

ACE_RCSID(ace, Obstack, "Obstack.cpp,v 4.11 2005/11/22 09:23:55 ossama Exp")

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

#if defined (ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION)
template class ACE_Obstack_T<char>;
#elif defined (ACE_HAS_TEMPLATE_INSTANTIATION_PRAGMA)
#pragma instantiate ACE_Obstack_T<char>
#endif /* ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION */

ACE_END_VERSIONED_NAMESPACE_DECL
