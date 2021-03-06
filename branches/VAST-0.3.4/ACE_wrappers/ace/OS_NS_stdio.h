// -*- C++ -*-

//=============================================================================
/**
 *  @file   OS_NS_stdio.h
 *
 *  OS_NS_stdio.h,v 1.19 2006/01/23 15:50:44 jwillemsen Exp
 *
 *  @author Douglas C. Schmidt <schmidt@cs.wustl.edu>
 *  @author Jesper S. M|ller<stophph@diku.dk>
 *  @author and a cast of thousands...
 *
 *  Originally in OS.h.
 */
//=============================================================================

#ifndef ACE_OS_NS_STDIO_H
# define ACE_OS_NS_STDIO_H

# include /**/ "ace/pre.h"

# include "ace/config-all.h"

# if !defined (ACE_LACKS_PRAGMA_ONCE)
#  pragma once
# endif /* ACE_LACKS_PRAGMA_ONCE */

#include "ace/os_include/os_stdio.h"
#include "ace/os_include/os_fcntl.h"
#include "ace/ACE_export.h"

#if defined (CYGWIN32)
#  include "ace/os_include/os_unistd.h"
#endif /* CYGWIN32 */

#if defined (ACE_EXPORT_MACRO)
#  undef ACE_EXPORT_MACRO
#endif
#define ACE_EXPORT_MACRO ACE_Export

/**
 * On some platforms clearerr is a macro. Defining ACE_OS::clearerr()
 * becomes really hard, as there is no way to save the macro
 * definition using the pre-processor.
 */
# if !defined (ACE_LACKS_CLEARERR)
#   if defined (clearerr)
#     define __ace_clearerr_hack
inline void __ace_clearerr(FILE *stream)
{
  clearerr(stream);
}
#     undef clearerr
#   endif /* defined (clearerr) */
# endif /* !ACE_LACKS_CLEARERR */

#if !defined (ACE_LACKS_CUSERID) && !defined(ACE_HAS_ALT_CUSERID) \
    && !defined(ACE_WIN32) && !defined (VXWORKS)
/// Helper for the ACE_OS::cuserid() function
/**
 * On some platforms cuserid is a macro.  Defining ACE_OS::cuserid()
 * becomes really hard, as there is no way to save the macro
 * definition using the pre-processor.
 * This inline function achieves the same effect, without namespace
 * pollution or performance penalties.
 *
 * @todo We maybe should move a lot of the code in ACE_OS::cuserid here so
 *       it is treated the same as the above ace_difftime and ace_timezone.
 *       But since there is a good deal more code in ACE_OS::cuserid, we
 *       probably need to move some of it off into some sort of emulation
 *       function.
 */
inline char *ace_cuserid(char *user)
{
  return cuserid(user);
}
#endif /* !ACE_LACKS_CUSERID && !ACE_HAS_ALT_CUSERID && ... */

# if defined (ACE_LACKS_FILELOCKS)
#   if ! defined (VXWORKS) && ! defined (ACE_PSOS) && ! defined (__rtems__) && !defined (INTEGRITY)
// VxWorks defines struct flock in sys/fcntlcom.h.  But it doesn't
// appear to support flock ().  RTEMS defines struct flock but
// currently does not support locking.
struct flock
{
  short l_type;
  short l_whence;
  off_t l_start;
  off_t l_len;          /* len == 0 means until end of file */
  long  l_sysid;
  pid_t l_pid;
  long  l_pad[4];               /* reserve area */
};
#   endif /* ! VXWORKS */
# endif /* ACE_LACKS_FILELOCKS */

ACE_BEGIN_VERSIONED_NAMESPACE_DECL

namespace ACE_OS {

  /**
   * @class ace_flock_t
   *
   * @brief OS file locking structure.
   */
  class ACE_Export ace_flock_t
  {
  public:
  /// Dump state of the object.
    void dump (void) const;

# if defined (ACE_WIN32)
    ACE_OVERLAPPED overlapped_;
# else
    struct flock lock_;
# endif /* ACE_WIN32 */

    /// Name of this filelock.
    const ACE_TCHAR *lockname_;

    /// Handle to the underlying file.
    ACE_HANDLE handle_;

# if defined (CHORUS)
    /// This is the mutex that's stored in shared memory.  It can only
    /// be destroyed by the actor that initialized it.
    ACE_mutex_t *process_lock_;
# endif /* CHORUS */
  };

# if defined (ACE_USES_WCHAR)
  // If fp points to the Unicode format file, the file pointer will be moved right next
  // to the Unicode header (2 types).  Otherwise, file pointer will be at the beginning.
  extern ACE_Export
  void checkUnicodeFormat (FILE* fp);
# endif  // ACE_USES_WCHAR

# if !defined (ACE_LACKS_CLEARERR)
  ACE_NAMESPACE_INLINE_FUNCTION
  void clearerr (FILE* fp);
# endif /* !ACE_LACKS_CLEARERR */

  //@{ @name Wrappers to obtain the current user id
  // Legacy as per SUSV3
# if !defined (ACE_LACKS_CUSERID)
#   if defined(cuserid)
#     undef cuserid
#   endif /* cuserid */

  ACE_NAMESPACE_INLINE_FUNCTION
  char *cuserid (char *user,
                 size_t maxlen = ACE_MAX_USERID);

#   if defined (ACE_HAS_WCHAR)
  ACE_NAMESPACE_INLINE_FUNCTION
  wchar_t *cuserid (wchar_t *user,
                    size_t maxlen = ACE_MAX_USERID);
#   endif /* ACE_HAS_WCHAR */
# endif /* ACE_LACKS_CUSERID */
  //@}

  ACE_NAMESPACE_INLINE_FUNCTION
  int fclose (FILE *fp);

# if defined (fdopen)
#   undef fdopen
# endif /* fdopen */

  ACE_NAMESPACE_INLINE_FUNCTION
  FILE *fdopen (ACE_HANDLE handle, const ACE_TCHAR *mode);

  ACE_NAMESPACE_INLINE_FUNCTION
  int fflush (FILE *fp);

  ACE_NAMESPACE_INLINE_FUNCTION
  int fgetc (FILE* fp);

  ACE_NAMESPACE_INLINE_FUNCTION
  int fgetpos (FILE* fp, fpos_t* pos);

  ACE_NAMESPACE_INLINE_FUNCTION
  char *fgets (char *buf, int size, FILE *fp);

# if defined (ACE_HAS_WCHAR) && !defined(ACE_LACKS_FGETWS)
  ACE_NAMESPACE_INLINE_FUNCTION
  wchar_t *fgets (wchar_t *buf, int size, FILE *fp);
# endif /* ACE_HAS_WCHAR && !ACE_LACKS_FGETWS */

  //@{ @name A set of wrappers for file locks.

# if defined (ACE_WIN32)
  ACE_NAMESPACE_INLINE_FUNCTION
  void flock_adjust_params (ace_flock_t *lock,
                            short whence,
                            off_t &start,
                            off_t &len);
# endif /* ACE_WIN32 */

  ACE_NAMESPACE_INLINE_FUNCTION
  int flock_init (ace_flock_t *lock,
                  int flags = 0,
                  const ACE_TCHAR *name = 0,
                  mode_t perms = 0);

  ACE_NAMESPACE_INLINE_FUNCTION
  int flock_destroy (ace_flock_t *lock,
                     int unlink_file = 1);

  ACE_NAMESPACE_INLINE_FUNCTION
  int flock_rdlock (ace_flock_t *lock,
                    short whence = 0,
                    off_t start = 0,
                    off_t len = 0);

  ACE_NAMESPACE_INLINE_FUNCTION
  int flock_tryrdlock (ace_flock_t *lock,
                       short whence = 0,
                       off_t start = 0,
                       off_t len = 0);

  ACE_NAMESPACE_INLINE_FUNCTION
  int flock_trywrlock (ace_flock_t *lock,
                       short whence = 0,
                       off_t start = 0,
                       off_t len = 0);

  ACE_NAMESPACE_INLINE_FUNCTION
  int flock_unlock (ace_flock_t *lock,
                    short whence = 0,
                    off_t start = 0,
                    off_t len = 0);

  ACE_NAMESPACE_INLINE_FUNCTION
  int flock_wrlock (ace_flock_t *lock,
                    short whence = 0,
                    off_t start = 0,
                    off_t len = 0);

  //@}

#if defined (ACE_WIN32) && !defined (ACE_HAS_WINCE)
  extern ACE_Export
#else
  ACE_NAMESPACE_INLINE_FUNCTION
#endif /* ACE_WIN32 */
  FILE *fopen (const char *filename, const ACE_TCHAR *mode);

#if defined (ACE_HAS_WCHAR)
#if defined (ACE_WIN32) && !defined (ACE_HAS_WINCE)
  extern ACE_Export
#else
  ACE_NAMESPACE_INLINE_FUNCTION
#endif /* ACE_WIN32 */
  FILE *fopen (const wchar_t *filename, const ACE_TCHAR *mode);
#endif /* ACE_HAS_WCHAR */

#if defined (ACE_WIN32)
  /// Default Win32 Security Attributes definition.
  ACE_NAMESPACE_INLINE_FUNCTION
  LPSECURITY_ATTRIBUTES default_win32_security_attributes (LPSECURITY_ATTRIBUTES);
  ACE_NAMESPACE_INLINE_FUNCTION
  LPSECURITY_ATTRIBUTES default_win32_security_attributes_r (LPSECURITY_ATTRIBUTES,
                                             LPSECURITY_ATTRIBUTES,
                                             SECURITY_DESCRIPTOR*);

  // = Win32 OS version determination function.
  /// Return the win32 OSVERSIONINFO structure.
  ACE_NAMESPACE_INLINE_FUNCTION
  const OSVERSIONINFO &get_win32_versioninfo (void);

  // = A pair of functions for modifying ACE's Win32 resource usage.
  /// Return the handle of the module containing ACE's resources. By
  /// default, for a DLL build of ACE this is a handle to the ACE DLL
  /// itself, and for a static build it is a handle to the executable.
  ACE_NAMESPACE_INLINE_FUNCTION
  HINSTANCE get_win32_resource_module (void);

  /// Allow an application to modify which module contains ACE's
  /// resources. This is mainly useful for a static build of ACE where
  /// the required resources reside somewhere other than the executable.
  ACE_NAMESPACE_INLINE_FUNCTION
  void set_win32_resource_module (HINSTANCE);

  /// Translate fopen's mode char to open's mode.  This helper function
  /// is here to avoid maintaining several pieces of identical code.
  ACE_NAMESPACE_INLINE_FUNCTION
  void fopen_mode_to_open_mode_converter (ACE_TCHAR x, int &hmode);

  extern ACE_Export OSVERSIONINFO win32_versioninfo_;

  extern ACE_Export HINSTANCE win32_resource_module_;

#endif /* ACE_WIN32 */

  extern ACE_Export
  int fprintf (FILE *fp, const char *format, ...);

# if defined (ACE_HAS_WCHAR)
  extern ACE_Export
  int fprintf (FILE *fp, const wchar_t *format, ...);
# endif /* ACE_HAS_WCHAR */

  ACE_NAMESPACE_INLINE_FUNCTION
  int fputs (const char *s,
             FILE *stream);

# if defined (ACE_HAS_WCHAR) && !defined(ACE_LACKS_FPUTWS)
  ACE_NAMESPACE_INLINE_FUNCTION
  int fputs (const wchar_t *s,
             FILE *stream);
# endif /* ACE_HAS_WCHAR && !ACE_LACKS_FPUTWS */

  ACE_NAMESPACE_INLINE_FUNCTION
  size_t fread (void *ptr,
                size_t size,
                size_t nelems,
                FILE *fp);

  ACE_NAMESPACE_INLINE_FUNCTION
  FILE *freopen (const ACE_TCHAR *filename,
                 const ACE_TCHAR *mode,
                 FILE* stream);

  ACE_NAMESPACE_INLINE_FUNCTION
  int fseek (FILE *fp,
             long offset,
             int ptrname);

  ACE_NAMESPACE_INLINE_FUNCTION
  int  fsetpos (FILE* fp, fpos_t* pos);

  ACE_NAMESPACE_INLINE_FUNCTION
  long ftell (FILE* fp);

  ACE_NAMESPACE_INLINE_FUNCTION
  size_t fwrite (const void *ptr,
                 size_t size,
                 size_t nitems,
                 FILE *fp);

  ACE_NAMESPACE_INLINE_FUNCTION
  void perror (const char *s);

#if defined (ACE_HAS_WCHAR)
  ACE_NAMESPACE_INLINE_FUNCTION
  void perror (const wchar_t *s);
#endif /* ACE_HAS_WCHAR */

  extern ACE_Export
  int printf (const char *format, ...);

  ACE_NAMESPACE_INLINE_FUNCTION
  int puts (const char *s);

#if defined (ACE_HAS_WCHAR)
  ACE_NAMESPACE_INLINE_FUNCTION
  int puts (const wchar_t *s);
#endif /* ACE_HAS_WCHAR */

  ACE_NAMESPACE_INLINE_FUNCTION
  int rename (const char *old_name,
              const char *new_name,
              int flags = -1);

#if defined (ACE_HAS_WCHAR)
  ACE_NAMESPACE_INLINE_FUNCTION
  int rename (const wchar_t *old_name,
              const wchar_t *new_name,
              int flags = -1);
#endif /* ACE_HAS_WCHAR */

  ACE_NAMESPACE_INLINE_FUNCTION
  void rewind (FILE *fp);

  extern ACE_Export
  int snprintf (char *buf, size_t maxlen, const char *format, ...);

# if defined (ACE_HAS_WCHAR)
  extern ACE_Export
  int snprintf (wchar_t *buf, size_t maxlen, const wchar_t *format,...);
# endif /* ACE_HAS_WCHAR */

  extern ACE_Export
  int sprintf (char *buf, const char *format, ...);

# if defined (ACE_HAS_WCHAR)
  extern ACE_Export
  int sprintf (wchar_t *buf, const wchar_t *format, ...);
# endif /* ACE_HAS_WCHAR */

  ACE_NAMESPACE_INLINE_FUNCTION
  char *tempnam (const char *dir = 0,
                 const char *pfx = 0);

#if defined (ACE_HAS_WCHAR)
  ACE_NAMESPACE_INLINE_FUNCTION
  wchar_t *tempnam (const wchar_t *dir,
                    const wchar_t *pfx = 0);
#endif /* ACE_HAS_WCHAR */

  ACE_NAMESPACE_INLINE_FUNCTION
  int vsprintf (char *buffer, const char *format, va_list argptr);

  ACE_NAMESPACE_INLINE_FUNCTION
  int vsnprintf (char *buffer, size_t maxlen, const char *format, va_list argptr);

# if defined (ACE_HAS_WCHAR)
  ACE_NAMESPACE_INLINE_FUNCTION
  int vsprintf (wchar_t *buffer, const wchar_t *format, va_list argptr);

  ACE_NAMESPACE_INLINE_FUNCTION
  int vsnprintf (wchar_t *buffer, size_t maxlen, const wchar_t *format, va_list argptr);
# endif /* ACE_HAS_WCHAR */

} /* namespace ACE_OS */

ACE_END_VERSIONED_NAMESPACE_DECL

# if defined (ACE_HAS_INLINED_OSCALLS)
#   if defined (ACE_INLINE)
#     undef ACE_INLINE
#   endif /* ACE_INLINE */
#   define ACE_INLINE inline
#   include "ace/OS_NS_stdio.inl"
# endif /* ACE_HAS_INLINED_OSCALLS */

# include /**/ "ace/post.h"
#endif /* ACE_OS_NS_STDIO_H */
