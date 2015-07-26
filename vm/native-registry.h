// vim: set sts=2 ts=8 sw=2 tw=99 et:
// 
// Copyright (C) 2006-2015 AlliedModders LLC
// 
// This file is part of SourcePawn. SourcePawn is free software: you can
// redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.
//
// You should have received a copy of the GNU General Public License along with
// SourcePawn. If not, see http://www.gnu.org/licenses/.
//
#ifndef _include_sourcepawn_vm_native_cache_h_
#define _include_sourcepawn_vm_native_cache_h_

#include <sp_vm_api.h>
#include <am-hashtable.h>
#include <am-refcounting.h>
#include <am-vector.h>
#include <string.h>
#include "code-allocator.h"

namespace sp {

using namespace SourcePawn;
using namespace ke;

class PluginRuntime;
struct NativeEntry;

class NativeGroup :
  public SourcePawn::INativeGroup,
  public ke::Refcounted<NativeGroup>
{
 public:
  NativeGroup(NativeRegistry* cache, Delegate* delegate, const char* name);

  KE_IMPL_REFCOUNTING(NativeGroup);

  virtual NativeGroup* ToNativeGroup() override {
    return this;
  }
  NativeRegistry* registry() const {
    return registry_;
  }

 public:
  struct NativeVector {
    NativeVector()
     : defs(nullptr),
       length(0)
    {}
    NativeVector(const NativeDef* defs, size_t count)
     : defs(defs),
       length(count)
    {}
    const NativeDef* defs;
    size_t length;
  };

  void addNatives(const NativeVector& natives) {
    natives_.append(natives);
  }

 private:
  Ref<NativeRegistry> registry_;
  Delegate* delegate_;
  const char* name_;
  Vector<NativeVector> natives_;
};

class NativeRegistry :
  public SourcePawn::INativeRegistry,
  public ke::Refcounted<NativeRegistry>
{
 public:
  NativeRegistry(Environment* env, const char* name);

  KE_IMPL_REFCOUNTING(NativeRegistry);

  NativeRegistry* ToNativeRegistry() override {
    return this;
  }
  PassRef<INativeGroup> NewNativeGroup(const char* name, INativeGroup::Delegate* delegate) override;
  void AddNativeVector(INativeGroup* group, const NativeDef* defs, size_t count) override;
  void AddNativeList(INativeGroup* group, const NativeDef* defs) override;
  bool AddRoutedNative(INativeGroup* group,
                       const char* name,
                       SPVM_FAKENATIVE_FUNC callback,
                       void* data) override;
  void RemoveNatives(const Ref<INativeGroup>& group) override;

  void Bind(PluginRuntime* rt, NativeEntry* entry);

 private:
  void addNative(NativeGroup* group, const NativeDef* def);

 private:
  Environment* env_;
  const char* name_;

 private:
  struct CharsAndLength
  {
    CharsAndLength(const char* ptr)
     : ptr(ptr),
       length(strlen(ptr))
    {}
    const char* ptr;
    size_t length;
  };

  struct RoutedNative {
    CodeChunk chunk;
    ke::AutoPtr<NativeDef> def;
    ke::AutoArray<char> name;
  };

  struct Record {
    const NativeDef* def;
    Ref<NativeGroup> group;
    AutoPtr<RoutedNative> rn;
  };

  struct Policy {
    typedef Record Payload;
    static uint32_t hash(const CharsAndLength& key) {
      return ke::FastHashCharSequence(key.ptr, key.length);
    }
    static bool matches(const CharsAndLength& key, const Payload& other) {
      return strcmp(key.ptr, other.def->name) == 0;
    }
  };

  typedef ke::HashTable<Policy> NativeTable;
  NativeTable table_;
};

}

#endif // _include_sourcepawn_vm_native_cache_h_