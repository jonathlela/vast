// -*- C++ -*-
//
// Read_Buffer.inl,v 4.2 2005/10/28 16:14:55 ossama Exp

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

// Accessor to the number of bytes in the buffer.

ACE_INLINE size_t
ACE_Read_Buffer::size (void) const
{
  ACE_TRACE ("ACE_Read_Buffer::size");
  return this->size_;
}

// The total number of characters replaced.

ACE_INLINE size_t
ACE_Read_Buffer::replaced (void) const
{
  ACE_TRACE ("ACE_Read_Buffer::replaced");
  return this->occurrences_;
}

ACE_INLINE ACE_Allocator *
ACE_Read_Buffer::alloc (void) const
{
  ACE_TRACE ("ACE_Read_Buffer::alloc");
  return this->allocator_;
}

ACE_END_VERSIONED_NAMESPACE_DECL
