// vim: set sts=2 ts=8 sw=2 tw=99 et:
//
// Copyright (C) 2012-2014 David Anderson
//
// This file is part of SourcePawn.
//
// SourcePawn is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
// 
// SourcePawn is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// SourcePawn. If not, see http://www.gnu.org/licenses/.
#ifndef _include_jitcraft_pool_allocator_h_
#define _include_jitcraft_pool_allocator_h_

#include <new>
#include <stddef.h>
#include <stdio.h>
#include <limits.h>
#include <am-vector.h>
#include <am-fixedarray.h>

namespace sp {

using namespace ke;

// Allocates memory in chunks that are not freed until the entire allocator
// is freed. This is intended for use with large, temporary data structures.
class PoolAllocator
{
  friend class PoolAllocationScope;

  struct Pool {
    char *base;
    char *ptr;
    char *end;
    Pool *prev;

    size_t size() const {
      return size_t(end - base);
    }
  };

  static const size_t kDefaultPoolSize = 8 * 1024;
  static const size_t kMaxReserveSize = 64 * 1024;

 private:
  Pool *reserved_;
  Pool *last_;

 private:
  void unwind(char *pos);
  void *slowAllocate(size_t actualBytes);

 public:
  PoolAllocator();
  ~PoolAllocator();

  void memoryUsage(size_t *allocated, size_t *reserved, size_t *bookkeeping) const {
    *allocated = 0;
    *reserved = 0;
    *bookkeeping = 0;
    for (Pool *cursor = last_; cursor; cursor = cursor->prev) {
      *allocated += size_t(cursor->ptr - cursor->base);
      *reserved += size_t(cursor->end - cursor->base);
      *bookkeeping += sizeof(Pool);
    }
    if (reserved_) {
      *reserved += size_t(reserved_->end - reserved_->base);
      *bookkeeping += sizeof(Pool);
    }
  }

  void *rawAllocate(size_t bytes) {
    // Guarantee malloc alignment.
    size_t actualBytes = Align(bytes, kMallocAlignment);
    if (!last_ || (size_t(last_->end - last_->ptr) < actualBytes))
      return slowAllocate(actualBytes);
    char *ptr = last_->ptr;
    last_->ptr += actualBytes;
    return ptr;
  }

  template <typename T>
  T *alloc(size_t count = 1) {
    if (!IsUintPtrMultiplySafe(count, sizeof(T))) {
      fprintf(stderr, "allocation overflow\n");
      return NULL;
    }
    void *ptr = rawAllocate(count * sizeof(T));
    if (!ptr)
      return NULL;

    return reinterpret_cast<T *>(ptr);
  }

  char *enter();
  void leave(char *position);
};

class PoolScope
{
  PoolAllocator *pool_;
  char *position_;

 public:
  PoolScope()
   : pool_(nullptr),
     position_(nullptr)
  {
  }
  PoolScope(PoolAllocator &allocator)
   : pool_(&allocator),
     position_(allocator.enter())
  {
  }
  ~PoolScope()
  {
    if (pool_)
      pool_->leave(position_);
  }

  void enter(PoolAllocator &pool)
  {
    assert(!pool_);
    pool_ = &pool;
    position_ = pool.enter();
  }

 private:
  PoolScope(const PoolScope &other) KE_DELETE;
  PoolScope &operator =(const PoolScope &other) KE_DELETE;
};

class PoolObject
{
 public:
  void *operator new(size_t size, PoolAllocator &pool) {
    return pool.rawAllocate(size);
  }
  void *operator new [](size_t size, PoolAllocator &pool) {
    return pool.rawAllocate(size);
  }
  
  // Using delete on pool-allocated objects is illegal.
  void operator delete(void *ptr, PoolAllocator &pool) {
    assert(false);
  }
  void operator delete [](void *ptr, PoolAllocator &pool) {
    assert(false);
  }
};

class PoolAllocationPolicy
{
 protected:
  void reportAllocationOverflow();
  void reportOutOfMemory();

 public:
  void *malloc(size_t bytes);
  void free(void *ptr);
};

template <typename T>
class PoolList
 : public Vector<T, PoolAllocationPolicy>,
   public PoolObject
{
 public:
  PoolList()
    : Vector<T, PoolAllocationPolicy>(PoolAllocationPolicy())
  {
  }
};

template <typename T>
class FixedPoolList
 : public FixedArray<T, PoolAllocationPolicy>,
   public PoolObject
{
 public:
  FixedPoolList(size_t length)
    : FixedArray<T, PoolAllocationPolicy>(length, PoolAllocationPolicy())
  {
  }
};

} // namespace ke

#endif // _include_jitcraft_pool_allocator_h_
