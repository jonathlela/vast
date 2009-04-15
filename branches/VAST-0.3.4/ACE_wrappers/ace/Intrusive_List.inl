// -*- C++ -*-
//
// Intrusive_List.inl,v 4.2 2005/10/28 16:14:52 ossama Exp

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

template<class T> ACE_INLINE int
ACE_Intrusive_List<T>::empty (void) const
{
  return this->head_ == 0;
}

template<class T> ACE_INLINE T *
ACE_Intrusive_List<T>::head (void) const
{
  return this->head_;
}

template<class T> ACE_INLINE T *
ACE_Intrusive_List<T>::tail (void) const
{
  return this->tail_;
}

ACE_END_VERSIONED_NAMESPACE_DECL
