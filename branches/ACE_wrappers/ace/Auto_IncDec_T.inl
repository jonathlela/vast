// -*- C++ -*-
//
// Auto_IncDec_T.inl,v 4.2 2005/10/28 16:14:51 ossama Exp

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

// Implicitly and automatically increment the counter.

template <class ACE_SAFELY_INCREMENTABLE_DECREMENTABLE> ACE_INLINE
ACE_Auto_IncDec<ACE_SAFELY_INCREMENTABLE_DECREMENTABLE>::ACE_Auto_IncDec
  (ACE_SAFELY_INCREMENTABLE_DECREMENTABLE &counter)
  : counter_ (counter)
{
  ++this->counter_;
}

// Implicitly and automatically decrement the counter.

template <class ACE_SAFELY_INCREMENTABLE_DECREMENTABLE> ACE_INLINE
ACE_Auto_IncDec<ACE_SAFELY_INCREMENTABLE_DECREMENTABLE>::~ACE_Auto_IncDec (void)
{
  --this->counter_;
}

ACE_END_VERSIONED_NAMESPACE_DECL
