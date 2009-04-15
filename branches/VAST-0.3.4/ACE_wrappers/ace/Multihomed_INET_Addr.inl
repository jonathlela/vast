// -*- C++ -*-
//
// Multihomed_INET_Addr.inl,v 4.2 2005/10/28 16:14:53 ossama Exp

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

// Accessor implementations
ACE_INLINE
size_t
ACE_Multihomed_INET_Addr::get_num_secondary_addresses() const
{
  return this->secondaries_.size();
}

ACE_END_VERSIONED_NAMESPACE_DECL
