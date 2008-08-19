// -*- C++ -*-
//
// Dynamic_Service.inl,v 4.2 2005/10/28 16:14:52 ossama Exp

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

#if defined (ACE_USES_WCHAR)
template <class TYPE> ACE_INLINE TYPE *
ACE_Dynamic_Service<TYPE>::instance (const ACE_ANTI_TCHAR *name)
{
  return instance (ACE_TEXT_CHAR_TO_TCHAR (name));
}
#endif  // ACE_USES_WCHAR

ACE_END_VERSIONED_NAMESPACE_DECL
