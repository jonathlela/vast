#include "ace/Method_Request.h"

ACE_RCSID (ace,
           Method_Request,
           "Method_Request.cpp,v 4.6 2005/10/28 16:14:53 ossama Exp")

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

ACE_Method_Request::ACE_Method_Request (unsigned long prio)
  : priority_ (prio)
{
}

ACE_Method_Request::~ACE_Method_Request (void)
{
}

unsigned long
ACE_Method_Request::priority (void) const
{
  return this->priority_;
}

void
ACE_Method_Request::priority (unsigned long prio)
{
  this->priority_ = prio;
}

ACE_END_VERSIONED_NAMESPACE_DECL
