// -*- C++ -*-
//
// Token_Collection.inl,v 4.2 2005/10/28 23:55:10 ossama Exp

#if defined (ACE_HAS_TOKENS_LIBRARY)

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

ACE_INLINE const ACE_TCHAR *
ACE_Token_Collection::name (void) const
{
  return name_;
}

ACE_END_VERSIONED_NAMESPACE_DECL

#endif /* ACE_HAS_TOKENS_LIBRARY */
