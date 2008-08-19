// -*- C++ -*-
//
// Lock_Adapter_T.inl,v 4.3 2005/10/28 16:14:53 ossama Exp

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

template <class ACE_LOCKING_MECHANISM>
ACE_INLINE
ACE_Lock_Adapter<ACE_LOCKING_MECHANISM>::ACE_Lock_Adapter (
  ACE_LOCKING_MECHANISM &lock)
  : lock_ (&lock),
    delete_lock_ (0)
{
}

ACE_END_VERSIONED_NAMESPACE_DECL
