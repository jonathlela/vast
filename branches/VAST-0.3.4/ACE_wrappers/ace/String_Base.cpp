// String_Base.cpp,v 4.21 2006/02/22 18:31:55 shuston Exp

#ifndef ACE_STRING_BASE_CPP
#define ACE_STRING_BASE_CPP

#include "ace/ACE.h"
#include "ace/Malloc_Base.h"
#include "ace/String_Base.h"
#include "ace/Auto_Ptr.h"
#include "ace/OS_NS_string.h"

#if !defined (__ACE_INLINE__)
#include "ace/String_Base.inl"
#endif /* __ACE_INLINE__ */

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

ACE_ALLOC_HOOK_DEFINE(ACE_String_Base)

template <class CHAR>
  CHAR ACE_String_Base<CHAR>::NULL_String_ = 0;

// Default constructor.

template <class CHAR>
ACE_String_Base<CHAR>::ACE_String_Base (ACE_Allocator *the_allocator)
  : allocator_ (the_allocator ? the_allocator : ACE_Allocator::instance ()),
    len_ (0),
    buf_len_ (0),
    rep_ (&ACE_String_Base<CHAR>::NULL_String_),
    release_ (0)
{
  ACE_TRACE ("ACE_String_Base<CHAR>::ACE_String_Base");
}

// Constructor that actually copies memory.

template <class CHAR>
ACE_String_Base<CHAR>::ACE_String_Base (const CHAR *s,
                                        ACE_Allocator *the_allocator,
                                        int release)
  : allocator_ (the_allocator ? the_allocator : ACE_Allocator::instance ()),
    len_ (0),
    buf_len_ (0),
    rep_ (0),
    release_ (0)
{
  ACE_TRACE ("ACE_String_Base<CHAR>::ACE_String_Base");
  this->set (s, release);
}

template <class CHAR>
ACE_String_Base<CHAR>::ACE_String_Base (CHAR c,
                                        ACE_Allocator *the_allocator)
  : allocator_ (the_allocator ? the_allocator : ACE_Allocator::instance ()),
    len_ (0),
    buf_len_ (0),
    rep_ (0),
    release_ (0)
{
  ACE_TRACE ("ACE_String_Base<CHAR>::ACE_String_Base");

  this->set (&c, 1, 1);
}

// Constructor that actually copies memory.

template <class CHAR>
ACE_String_Base<CHAR>::ACE_String_Base (const CHAR *s,
                                        size_t len,
                                        ACE_Allocator *the_allocator,
                                        int release)
  : allocator_ (the_allocator ? the_allocator : ACE_Allocator::instance ()),
    len_ (0),
    buf_len_ (0),
    rep_ (0),
    release_ (0)
{
  ACE_TRACE ("ACE_String_Base<CHAR>::ACE_String_Base");

  this->set (s, len, release);
}

// Copy constructor.

template <class CHAR>
ACE_String_Base<CHAR>::ACE_String_Base (const ACE_String_Base<CHAR> &s)
  : allocator_ (s.allocator_ ? s.allocator_ : ACE_Allocator::instance ()),
    len_ (0),
    buf_len_ (0),
    rep_ (0),
    release_ (0)
{
  ACE_TRACE ("ACE_String_Base<CHAR>::ACE_String_Base");

  this->set (s.rep_, s.len_, 1);
}

template <class CHAR>
ACE_String_Base<CHAR>::ACE_String_Base (size_t len, CHAR c, ACE_Allocator *the_allocator)
  : allocator_ (the_allocator ? the_allocator : ACE_Allocator::instance ()),
    len_ (0),
    buf_len_ (0),
    rep_ (0),
    release_ (0)
{
  ACE_TRACE ("ACE_String_Base<CHAR>::ACE_String_Base");

  this->resize (len, c);
}

template <class CHAR>
ACE_String_Base<CHAR>::~ACE_String_Base (void)
{
  ACE_TRACE ("ACE_String_Base<CHAR>::~ACE_String_Base");

  if (this->buf_len_ != 0 && this->release_ != 0)
      this->allocator_->free (this->rep_);
}

// this method might benefit from a little restructuring.
template <class CHAR> void
ACE_String_Base<CHAR>::set (const CHAR *s, size_t len, int release)
{
  // Case 1. Going from memory to more memory
  size_t new_buf_len = len + 1;
  if (s != 0 && len != 0 && release && this->buf_len_ < new_buf_len)
    {
      CHAR *temp;
      ACE_ALLOCATOR (temp,
                     (CHAR *) this->allocator_->malloc (new_buf_len * sizeof (CHAR)));

    if (this->buf_len_ != 0 && this->release_ != 0)
        this->allocator_->free (this->rep_);

      this->rep_ = temp;
      this->buf_len_ = new_buf_len;
      this->release_ = 1;
      this->len_ = len;
      ACE_OS::memcpy (this->rep_, s, len * sizeof (CHAR));
    this->rep_[len] = 0;
    }
  else // Case 2. No memory allocation is necessary.
    {
      // Free memory if necessary and figure out future ownership
    if (release == 0 || s == 0 || len == 0)
        {
      if (this->buf_len_ != 0 && this->release_ != 0)
            {
              this->allocator_->free (this->rep_);
              this->release_ = 0;
            }
        }
      // Populate data.
      if (s == 0 || len == 0)
        {
          this->buf_len_ = 0;
          this->len_ = 0;
          this->rep_ = &ACE_String_Base<CHAR>::NULL_String_;
      this->release_ = 0;
        }
    else if (release == 0) // Note: No guarantee that rep_ is null terminated.
        {
          this->buf_len_ = len;
          this->len_ = len;
          this->rep_ = const_cast <CHAR *> (s);
      this->release_ = 0;
        }
      else
        {
          ACE_OS::memcpy (this->rep_, s, len * sizeof (CHAR));
          this->rep_[len] = 0;
          this->len_ = len;
        }
    }
}

// Return substring.
template <class CHAR> ACE_String_Base<CHAR>
ACE_String_Base<CHAR>::substring (size_t offset, ssize_t length) const
{
  ACE_String_Base<CHAR> nill;
  size_t count = length;

  // case 1. empty string
  if (this->len_ == 0)
    return nill;

  // case 2. start pos past our end
  if (offset >= this->len_)
    return nill;
  // No length == empty string.
  else if (length == 0)
    return nill;
  // Get all remaining bytes.
  else if (length == -1 || count > (this->len_ - offset))
    count = this->len_ - offset;

  return ACE_String_Base<CHAR> (&this->rep_[offset], count, this->allocator_);
}

template <class CHAR> ACE_String_Base<CHAR> &
ACE_String_Base<CHAR>::append (const CHAR* s, size_t slen)
{
  ACE_TRACE ("ACE_String_Base<CHAR>::append(const CHAR*, size_t)");
  if (slen > 0)
  {
    // case 1. No memory allocation needed.
    if (this->buf_len_ >= this->len_ + slen + 1)
    {
      // Copy in data from new string.
      ACE_OS::memcpy (this->rep_ + this->len_, s, slen * sizeof (CHAR));
    }
    else // case 2. Memory reallocation is needed
    {
      const size_t new_buf_len =
        ace_max(this->len_ + slen + 1, this->buf_len_ + this->buf_len_ / 2);

      CHAR *t = 0;

      ACE_ALLOCATOR_RETURN (t,
        (CHAR *) this->allocator_->malloc (new_buf_len * sizeof (CHAR)), *this);

      // Copy memory from old string into new string.
      ACE_OS::memcpy (t, this->rep_, this->len_ * sizeof (CHAR));

      ACE_OS::memcpy (t + this->len_, s, slen * sizeof (CHAR));

      if (this->buf_len_ != 0 && this->release_ != 0)
        this->allocator_->free (this->rep_);

      this->release_ = 1;
      this->rep_ = t;
      this->buf_len_ = new_buf_len;
    }

    this->len_ += slen;
    this->rep_[this->len_] = 0;
  }

  return *this;
}

template <class CHAR> u_long
ACE_String_Base<CHAR>::hash (void) const
{
  return
    ACE::hash_pjw (reinterpret_cast<char *> (
                      const_cast<CHAR *> (this->rep_)),
                   this->len_ * sizeof (CHAR));
}

template <class CHAR> void
ACE_String_Base<CHAR>::resize (size_t len, CHAR c)
{
  ACE_TRACE ("ACE_String_Base<CHAR>::resize");

  // Only reallocate if we don't have enough space...
  if (this->buf_len_ <= len)
    {
    if (this->buf_len_ != 0 && this->release_ != 0)
        this->allocator_->free (this->rep_);

    this->rep_ = static_cast<CHAR*>(
      this->allocator_->malloc ((len + 1) * sizeof (CHAR)));
      this->buf_len_ = len + 1;
      this->release_ = 1;
    }
  this->len_ = 0;
  ACE_OS::memset (this->rep_, c, this->buf_len_ * sizeof (CHAR));
}

template <class CHAR> void
ACE_String_Base<CHAR>::clear (int release)
{
  // This can't use set(), because that would free memory if release=0
  if (release != 0)
  {
    if (this->buf_len_ != 0 && this->release_ != 0)
      this->allocator_->free (this->rep_);

    this->rep_ = &ACE_String_Base<CHAR>::NULL_String_;
  this->len_ = 0;
    this->buf_len_ = 0;
    this->release_ = 0;
}
  else
  {
    this->fast_clear ();
  }
}

// Assignment operator (does copy memory).
template <class CHAR> ACE_String_Base<CHAR> &
ACE_String_Base<CHAR>::operator= (const CHAR *s)
{
  ACE_TRACE ("ACE_String_Base<CHAR>::operator=");
  if (s != 0)
    this->set (s, 1);
  return *this;
}

// Assignment operator (does copy memory).
template <class CHAR> ACE_String_Base<CHAR> &
ACE_String_Base<CHAR>::operator= (const ACE_String_Base<CHAR> &s)
{
  ACE_TRACE ("ACE_String_Base<CHAR>::operator=");

  // Check for identify.
  if (this != &s)
    {
      this->set (s.rep_, s.len_, 1);
    }

  return *this;
}

template <class CHAR> void
ACE_String_Base<CHAR>::set (const CHAR *s, int release)
{
  size_t length = 0;
  if (s != 0)
    length = ACE_OS::strlen (s);

  this->set (s, length, release);
}

template <class CHAR> void
ACE_String_Base<CHAR>::fast_clear (void)
{
  this->len_ = 0;
  if (this->release_ != 0)
    {
      // String retains the original buffer.
      if (this->rep_ != &ACE_String_Base<CHAR>::NULL_String_)
        this->rep_[0] = 0;
    }
  else
    {
      // External buffer: string relinquishes control of it.
      this->buf_len_ = 0;
      this->rep_ = &ACE_String_Base<CHAR>::NULL_String_;
    }
}

// Get a copy of the underlying representation.

template <class CHAR> CHAR *
ACE_String_Base<CHAR>::rep (void) const
{
  ACE_TRACE ("ACE_String_Base<CHAR>::rep");

  CHAR *new_string;
  ACE_NEW_RETURN (new_string, CHAR[this->len_ + 1], 0);
  ACE_OS::strsncpy (new_string, this->rep_, this->len_+1);

  return new_string;
}

template <class CHAR> int
ACE_String_Base<CHAR>::compare (const ACE_String_Base<CHAR> &s) const
{
  ACE_TRACE ("ACE_String_Base<CHAR>::compare");

  if (this->rep_ == s.rep_)
    return 0;

  // Pick smaller of the two lengths and perform the comparison.
  size_t smaller_length = ace_min (this->len_, s.len_);

  int result = ACE_OS::memcmp (this->rep_,
                               s.rep_,
                               smaller_length * sizeof (CHAR));

  if (!result)
    result = static_cast<int> (this->len_ - s.len_);
  return result;
}

// Comparison operator.

template <class CHAR> bool
ACE_String_Base<CHAR>::operator== (const ACE_String_Base<CHAR> &s) const
{
  ACE_TRACE ("ACE_String_Base<CHAR>::operator==");
  if (this->len_ != s.len_)
    return false;
  return compare (s) == 0;
}

template <class CHAR> ssize_t
ACE_String_Base<CHAR>::find (const CHAR *s, size_t pos) const
{
  CHAR *substr = this->rep_ + pos;
  size_t len = ACE_OS::strlen (s);
  CHAR *pointer = ACE_OS::strnstr (substr, s, len);
  if (pointer == 0)
    return ACE_String_Base<CHAR>::npos;
  else
    return pointer - this->rep_;
}

template <class CHAR> ssize_t
ACE_String_Base<CHAR>::find (CHAR c, size_t pos) const
{
  CHAR *substr = this->rep_ + pos;
  CHAR *pointer = ACE_OS::strnchr (substr, c, this->len_ - pos);
  if (pointer == 0)
    return ACE_String_Base<CHAR>::npos;
  else
    return pointer - this->rep_;
}

template <class CHAR> ssize_t
ACE_String_Base<CHAR>::rfind (CHAR c, ssize_t pos) const
{
  if (pos == npos || pos > static_cast<ssize_t> (this->len_))
    pos = static_cast<ssize_t> (this->len_);

  for (ssize_t i = pos - 1; i >= 0; i--)
    if (this->rep_[i] == c)
      return i;

  return ACE_String_Base<CHAR>::npos;
}

template <class CHAR> ACE_String_Base<CHAR>
operator+ (const ACE_String_Base<CHAR> &s, const ACE_String_Base<CHAR> &t)
{
  ACE_String_Base<CHAR> temp (s.length() + t.length());
  temp += s;
  temp += t;
  return temp;
}

template <class CHAR> ACE_String_Base<CHAR>
operator+ (const CHAR *s, const ACE_String_Base<CHAR> &t)
{
  size_t slen = 0;
  if (s != 0)
    slen = ACE_OS::strlen (s);
  ACE_String_Base<CHAR> temp (slen + t.length());
  if (slen > 0)
    temp.append(s, slen);
  temp += t;
  return temp;
}

template <class CHAR> ACE_String_Base<CHAR>
operator+ (const ACE_String_Base<CHAR> &s, const CHAR *t)
{
  size_t tlen = 0;
  if (t != 0)
    tlen = ACE_OS::strlen (t);
  ACE_String_Base<CHAR> temp (s.length() + tlen);
  temp += s;
  if (tlen > 0)
    temp.append(t, tlen);
  return temp;
}

template <class CHAR>
ACE_String_Base<CHAR> operator + (const ACE_String_Base<CHAR> &t,
                                  const CHAR c)
{
  ACE_String_Base<CHAR> temp (t.length() + 1);
  temp += t;
  temp += c;
  return temp;
}

template <class CHAR>
ACE_String_Base<CHAR> operator + (const CHAR c,
                                  const ACE_String_Base<CHAR> &t)
{
  ACE_String_Base<CHAR> temp (t.length() + 1);
  temp += c;
  temp += t;
  return temp;
}

template <class CHAR> ACE_String_Base<CHAR> &
ACE_String_Base<CHAR>::operator+= (const CHAR* s)
{
  size_t slen = 0;
  if (s != 0)
    slen = ACE_OS::strlen (s);
  return this->append (s, slen);
}

template <class CHAR> ACE_String_Base<CHAR> &
ACE_String_Base<CHAR>::operator+= (const ACE_String_Base<CHAR> &s)
{
  ACE_TRACE ("ACE_String_Base<CHAR>::operator+=(const ACE_String_Base<CHAR> &)");
  return this->append(s.rep_, s.len_);
}

template <class CHAR> ACE_String_Base<CHAR> &
ACE_String_Base<CHAR>::operator+= (const CHAR c)
{
  ACE_TRACE ("ACE_String_Base<CHAR>::operator+=(const CHAR)");
  const size_t slen = 1;
  return this->append(&c, slen);
}

ACE_END_VERSIONED_NAMESPACE_DECL

#endif  /* ACE_STRING_BASE_CPP */
