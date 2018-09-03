/**
 * \file map_file.c
 *
 * \brief Routines for working with memory mapped files.
 */

#ifdef WIN32
#include <Windows.h>
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <sys/mman.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "map_file.h"
#include "mymalloc.h"
#include "log.h"

/** Memory map a file.
 *
 * \param filename The name of the file to map
 * \param writable True if it should be a read/write map.
 * \return A pointer to a mapped file structure
 */
MAPPED_FILE *
map_file(const char *filename, bool writable)
{
  MAPPED_FILE *f = NULL;
#ifdef WIN32
  HANDLE fh, mh;
  LARGE_INTEGER size;
  DWORD flags, mflags, vflags;

  if (writable) {
    flags = GENERIC_READ | GENERIC_WRITE;
    mflags = PAGE_READWRITE;
    vflags = FILE_MAP_READ | FILE_MAP_WRITE;
  } else {
    flags = GENERIC_READ;
    mflags = PAGE_READONLY;
    vflags = FILE_MAP_READ;
  }

  fh = CreateFile(filename, flags, 0, NULL, OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL, NULL);
  if (fh == INVALID_HANDLE_VALUE) {
    do_rawlog(LT_ERR, "map_file: unable to open file '%s': error %ld", filename,
              GetLastError());
    return NULL;
  }

  if (!GetFileSizeEx(fh, &size)) {
    do_rawlog(LT_ERR, "map_file: unable to get size of file '%s': error %ld",
              filename, GetLastError());
    CloseHandle(fh);
    return NULL;
  }

  mh = CreateFileMappingA(fh, NULL, mflags, size.HighPart, size.LowPart, NULL);
  if (mh == INVALID_HANDLE_VALUE) {
    do_rawlog(LT_ERR,
              "map_file: unable to create file mapping for '%s': error %ld",
              filename, GetLastError());
    CloseHandle(fh);
    return NULL;
  }

  f = mush_malloc(sizeof *f, "mapped_file");
  if (!f) {
    CloseHandle(mh);
    CloseHandle(fh);
    return NULL;
  }

  f->fh = fh;
  f->mh = mh;
  f->len = size.QuadPart;
  f->data = MapViewOfFile(mh, vflags, 0, 0, 0);
  if (!f->data) {
    do_rawlog(LT_ERR, "Unable to map file '%s': error %ld", filename,
              GetLastError());
    CloseHandle(mh);
    CloseHandle(fh);
    mush_free(f, "mapped_file");
    return NULL;
  }

#else

  /* POSIX version */
  int fd;
  struct stat s;
  int flags, prot;

  if (writable) {
    flags = O_RDWR;
    prot = PROT_READ | PROT_WRITE;
  } else {
    flags = O_RDONLY;
    prot = PROT_READ;
  }

  if ((fd = open(filename, flags)) < 0) {
    do_rawlog(LT_ERR, "map_file: unable to open file '%s': %s", filename,
              strerror(errno));
    return NULL;
  }

  if (fstat(fd, &s) < 0) {
    close(fd);
    do_rawlog(LT_ERR, "map_file: unable to stat file '%s': %s", filename,
              strerror(errno));
    return NULL;
  }

  f = mush_malloc(sizeof *f, "mapped_file");
  if (!f) {
    close(fd);
    return NULL;
  }

  f->len = s.st_size;
  f->data = mmap(NULL, s.st_size, prot, MAP_SHARED, fd, 0);

  if (!f->data) {
    do_rawlog(LT_ERR, "map_file: unable to mmap file '%s': %s", filename,
              strerror(errno));
    close(fd);
    mush_free(f, "mapped_file");
    return NULL;
  }
  close(fd);
#endif
  return f;
}

/** Delete a file mapping.
 *
 * \param mapped the mapped file to unmap.
 */
void
unmap_file(MAPPED_FILE *mapped)
{
#ifdef WIN32
  if (!UnmapViewOfFile(mapped->data)) {
    do_rawlog(LT_ERR, "unmap_file: error code %ld", GetLastError());
  }
  CloseHandle(mapped->mh);
  CloseHandle(mapped->fh);
#else
  if (munmap(mapped->data, mapped->len) < 0) {
    do_rawlog(LT_ERR, "unmap_file: %s", strerror(errno));
  }
#endif
  mush_free(mapped, "mapped_file");
}
