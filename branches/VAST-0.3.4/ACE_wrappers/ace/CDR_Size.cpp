#include "ace/CDR_Size.h"
#include "ace/SString.h"

#if !defined (__ACE_INLINE__)
# include "ace/CDR_Size.inl"
#endif /* ! __ACE_INLINE__ */

ACE_RCSID (ace,
           CDR_Size,
           "CDR_Size.cpp,v 4.4 2005/10/28 16:14:51 ossama Exp")

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

ACE_CDR::Boolean
ACE_SizeCDR::write_wchar (ACE_CDR::WChar x)
{
  // Note: translator framework is not supported.
  //
  if (ACE_OutputCDR::wchar_maxbytes () == 0)
    {
      errno = EACCES;
      return (this->good_bit_ = false);
    }
  if (static_cast<ACE_CDR::Short> (major_version_) == 1
          && static_cast<ACE_CDR::Short> (minor_version_) == 2)
    {
      ACE_CDR::Octet len =
        static_cast<ACE_CDR::Octet> (ACE_OutputCDR::wchar_maxbytes ());
      if (this->write_1 (&len))
        {
          if (ACE_OutputCDR::wchar_maxbytes () == sizeof(ACE_CDR::WChar))
            return
              this->write_octet_array (
                reinterpret_cast<const ACE_CDR::Octet*> (&x),
                static_cast<ACE_CDR::ULong> (len));
          else
            if (ACE_OutputCDR::wchar_maxbytes () == 2)
              {
                ACE_CDR::Short sx = static_cast<ACE_CDR::Short> (x);
                return
                  this->write_octet_array (
                    reinterpret_cast<const ACE_CDR::Octet*> (&sx),
                    static_cast<ACE_CDR::ULong> (len));
              }
            else
              {
                ACE_CDR::Octet ox = static_cast<ACE_CDR::Octet> (x);
                return
                  this->write_octet_array (
                    reinterpret_cast<const ACE_CDR::Octet*> (&ox),
                    static_cast<ACE_CDR::ULong> (len));
              }
        }
    }
  else if (static_cast<ACE_CDR::Short> (minor_version_) == 0)
    { // wchar is not allowed with GIOP 1.0.
      errno = EINVAL;
      return (this->good_bit_ = false);
    }
  if (ACE_OutputCDR::wchar_maxbytes () == sizeof (ACE_CDR::WChar))
    return this->write_4 (reinterpret_cast<const ACE_CDR::ULong *> (&x));
  else if (ACE_OutputCDR::wchar_maxbytes () == 2)
    {
      ACE_CDR::Short sx = static_cast<ACE_CDR::Short> (x);
      return this->write_2 (reinterpret_cast<const ACE_CDR::UShort *> (&sx));
    }
  ACE_CDR::Octet ox = static_cast<ACE_CDR::Octet> (x);
  return this->write_1 (reinterpret_cast<const ACE_CDR::Octet *> (&ox));
}

ACE_CDR::Boolean
ACE_SizeCDR::write_string (ACE_CDR::ULong len,
                             const ACE_CDR::Char *x)
{
  // Note: translator framework is not supported.
  //
  if (len != 0)
    {
      if (this->write_ulong (len + 1))
        return this->write_char_array (x, len + 1);
    }
  else
    {
      // Be nice to programmers: treat nulls as empty strings not
      // errors. (OMG-IDL supports languages that don't use the C/C++
      // notion of null v. empty strings; nulls aren't part of the OMG-IDL
      // string model.)
      if (this->write_ulong (1))
        return this->write_char (0);
    }

  return (this->good_bit_ = false);
}

ACE_CDR::Boolean
ACE_SizeCDR::write_string (const ACE_CString &x)
{
  // @@ Leave this method in here, not the `.i' file so that we don't
  //    have to unnecessarily pull in the `ace/SString.h' header.
  return this->write_string (static_cast<ACE_CDR::ULong> (x.length ()),
                             x.c_str());
}

ACE_CDR::Boolean
ACE_SizeCDR::write_wstring (ACE_CDR::ULong len,
                              const ACE_CDR::WChar *x)
{
  // Note: translator framework is not supported.
  //
  if (ACE_OutputCDR::wchar_maxbytes () == 0)
    {
      errno = EACCES;
      return (this->good_bit_ = false);
    }

  if (static_cast<ACE_CDR::Short> (this->major_version_) == 1
      && static_cast<ACE_CDR::Short> (this->minor_version_) == 2)
    {
      if (x != 0)
        {
          //In GIOP 1.2 the length field contains the number of bytes
          //the wstring occupies rather than number of wchars
          //Taking sizeof might not be a good way! This is a temporary fix.
          if (this->write_ulong (ACE_OutputCDR::wchar_maxbytes () * len))
            return this->write_wchar_array (x, len);
        }
      else
        //In GIOP 1.2 zero length wstrings are legal
        return this->write_ulong (0);
    }

  else
    if (x != 0)
      {
        if (this->write_ulong (len + 1))
          return this->write_wchar_array (x, len + 1);
      }
    else if (this->write_ulong (1))
      return this->write_wchar (0);
   return (this->good_bit_ = false);
}

ACE_CDR::Boolean
ACE_SizeCDR::write_1 (const ACE_CDR::Octet *x)
{
  ACE_UNUSED_ARG (x);
  this->adjust (1);
  return true;
}

ACE_CDR::Boolean
ACE_SizeCDR::write_2 (const ACE_CDR::UShort *x)
{
  ACE_UNUSED_ARG (x);
  this->adjust (ACE_CDR::SHORT_SIZE);
  return true;
}

ACE_CDR::Boolean
ACE_SizeCDR::write_4 (const ACE_CDR::ULong *x)
{
  ACE_UNUSED_ARG (x);
  this->adjust (ACE_CDR::LONG_SIZE);
  return true;
}

ACE_CDR::Boolean
ACE_SizeCDR::write_8 (const ACE_CDR::ULongLong *x)
{
  ACE_UNUSED_ARG (x);
  this->adjust (ACE_CDR::LONGLONG_SIZE);
  return true;
}

ACE_CDR::Boolean
ACE_SizeCDR::write_16 (const ACE_CDR::LongDouble *x)
{
  ACE_UNUSED_ARG (x);
  this->adjust (ACE_CDR::LONGDOUBLE_SIZE,
                ACE_CDR::LONGDOUBLE_ALIGN);
  return true;
}

ACE_CDR::Boolean
ACE_SizeCDR::write_wchar_array_i (const ACE_CDR::WChar *x,
                                    ACE_CDR::ULong length)
{
  ACE_UNUSED_ARG (x);

  if (length == 0)
    return true;

  const size_t align = (ACE_OutputCDR::wchar_maxbytes () == 2) ?
    ACE_CDR::SHORT_ALIGN :
    ACE_CDR::OCTET_ALIGN;

  this->adjust (ACE_OutputCDR::wchar_maxbytes () * length, align);
  return true;
}


ACE_CDR::Boolean
ACE_SizeCDR::write_array (const void *x,
                          size_t size,
                          size_t align,
                          ACE_CDR::ULong length)
{
  ACE_UNUSED_ARG (x);

  if (length == 0)
    return true;

  this->adjust (size * length, align);
  return true;
}

ACE_CDR::Boolean
ACE_SizeCDR::write_boolean_array (const ACE_CDR::Boolean* x,
                                  ACE_CDR::ULong length)
{
  ACE_UNUSED_ARG (x);
  this->adjust (length, 1);
  return true;
}

void
ACE_SizeCDR::adjust (size_t size)
{
  adjust (size, size);
}

void
ACE_SizeCDR::adjust (size_t size,
                     size_t align)
{
  size_ = align * (size_ / align + (size_ % align ? 1 : 0));
  size_ += size;
}

ACE_CDR::Boolean
operator<< (ACE_SizeCDR &ss, const ACE_CString &x)
{
  ss.write_string (x);
  return ss.good_bit ();
}

ACE_END_VERSIONED_NAMESPACE_DECL
