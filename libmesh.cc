// Copyright 2016 University of Massachusetts, Amherst

#include <stdio.h>   // for sprintf
#include <stdlib.h>  // for abort
#include <unistd.h>  // for write
#include <cstddef>   // for size_t
#include <new>       // for operator new

// never allocate exeecutable heap
#define HL_MMAP_PROTECTION_MASK (PROT_READ | PROT_WRITE)

#define MALLOC_TRACE 0

#include "heaplayers.h"

#include "file-backed-mmap.h"

#include "wrappers/gnuwrapper.cpp"

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

using namespace HL;

// allocator for mesh-internal data structures, like heap metadata
typedef ExactlyOneHeap<LockedHeap<PosixLockType, FreelistHeap<BumpAlloc<16384, PrivateFileBackedMmapHeap>>>>
    InternalAlloc;

class TopHeap : public ExactlyOneHeap<FileBackedMmapHeap<InternalAlloc>> {
public:
  typedef ExactlyOneHeap<FileBackedMmapHeap<InternalAlloc>> Super;
};

template<typename SuperHeap, typename InternalAlloc>
class MiniHeap : public SuperHeap {
public:
  MiniHeap(size_t object_size) {

  }

  inline void *malloc(size_t sz) {
    // random probe into bitmap, if free, use that

    return nullptr;
  }

  inline void free(void *ptr) {
  }

  inline size_t getSize(void *ptr) {
    return 0;
  }

  bool isFull() {
    // FIXME: num_allocated > threshold
    return false;
  }

  // rng
  // bitmap
};

template <typename SuperHeap, typename InternalAlloc>
class MeshingHeap {
public:
  enum { Alignment = 16 };  // FIXME

  MeshingHeap() : _current(nullptr), _alloc() {
  }

  inline void *malloc(size_t sz) {
    if (unlikely(_current == nullptr)) {
      void *buf = _alloc.malloc(sizeof(MiniHeap<SuperHeap, InternalAlloc>));
      if (!buf)
        abort();
      _current = new (buf) MiniHeap<SuperHeap, InternalAlloc>(sz);
    }

    void *ptr = _current->malloc(sz);
    if (_current->isFull())
      _current = nullptr;

    return ptr;
  }

  inline void free(void *ptr) {
    // FIXME: check if ptr is in current, if so free there, otherwise check all other miniheaps
    // this needs to take into account threads + locks, maybe
  }

  inline size_t getSize(void *ptr) {
    return 0;
  }

private:
  MiniHeap<SuperHeap, InternalAlloc> *_current;
  InternalAlloc                       _alloc;
  // TODO: btree of address-ranges to miniheaps, for free
};

// the mesh heap doesn't coalesce and doesn't have free lists
class CustomHeap : public ANSIWrapper<KingsleyHeap<MeshingHeap<TopHeap, InternalAlloc>, TopHeap>> {};

inline static CustomHeap *getCustomHeap(void) {
  static char buf[sizeof(CustomHeap)];
  static CustomHeap *heap = new (buf) CustomHeap();
  return heap;
}

extern "C" {
void *xxmalloc(size_t sz) {
  return getCustomHeap()->malloc(sz);
}

void xxfree(void *ptr) {
  getCustomHeap()->free(ptr);
}

size_t xxmalloc_usable_size(void *ptr) {
  return getCustomHeap()->getSize(ptr);
}

void xxmalloc_lock() {
}

void xxmalloc_unlock() {
}
}
