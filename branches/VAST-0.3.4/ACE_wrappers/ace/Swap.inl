// -*- C++ -*-

/**
 * @file Swap.inl
 *
 * Swap.inl,v 1.2 2005/10/28 16:14:56 ossama Exp
 *
 * @author Carlos O'Ryan <coryan@uci.edu>
 */

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

template<class T> ACE_INLINE void
ACE_Swap<T>::swap (T &lhs, T& rhs)
{
  T tmp = lhs;
  lhs = rhs;
  rhs = tmp;
}

ACE_END_VERSIONED_NAMESPACE_DECL
