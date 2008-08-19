// -*- C++ -*-
//
// Timeprobe.inl,v 4.2 2005/10/28 23:55:10 ossama Exp

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

ACE_INLINE bool
ACE_Event_Descriptions::operator== (const ACE_Event_Descriptions &rhs) const
{
  return this->minimum_id_ == rhs.minimum_id_ &&
    this->descriptions_ == rhs.descriptions_;
}

ACE_END_VERSIONED_NAMESPACE_DECL
