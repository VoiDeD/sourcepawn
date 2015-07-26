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
#include "native-registry.h"
#include "plugin-runtime.h"
#include "environment.h"
#include "code-stubs.h"

using namespace sp;
using namespace ke;
using namespace SourcePawn;

NativeRegistry::NativeRegistry(Environment* env, const char* name)
 : env_(env),
   name_(name)
{
  table_.init(128);
}

PassRef<INativeGroup>
NativeRegistry::NewNativeGroup(const char* name, INativeGroup::Delegate* delegate)
{
  return new NativeGroup(this, delegate, name);
}

void
NativeRegistry::AddNativeVector(INativeGroup* aGroup, const NativeDef* defs, size_t count)
{
  NativeGroup* group = aGroup ? aGroup->ToNativeGroup() : nullptr;
  assert(!group || group->registry() == this);

  for (size_t i = 0; i < count; i++)
    addNative(group, &defs[i]);

  if (group)
    group->addNatives(NativeGroup::NativeVector(defs, count));
}

void
NativeRegistry::AddNativeList(INativeGroup* aGroup, const NativeDef* defs)
{
  NativeGroup* group = aGroup ? aGroup->ToNativeGroup() : nullptr;
  assert(!group || group->registry() == this);

  size_t count = 0;
  for (const NativeDef* iter = defs; iter->name; iter++, count++)
    addNative(group, iter);

  if (group)
    group->addNatives(NativeGroup::NativeVector(defs, count));
}

void
NativeRegistry::addNative(NativeGroup* group, const NativeDef* def)
{
  NativeTable::Insert i = table_.findForAdd(def->name);
  if (i.found()) {
    assert(false);
    return;
  }
  if (!table_.add(i))
    return;

  i->def = def;
  i->group = group;
}

void
NativeRegistry::RemoveNatives(const Ref<INativeGroup>& group)
{
  for (NativeTable::iterator iter(&table_); !iter.empty(); iter.next()) {
    if (iter->group != group.get())
      continue;
    iter.erase();
  }

  // :TODO: weak refs
}

bool
NativeRegistry::AddRoutedNative(INativeGroup* aGroup,
                                const char* name,
                                SPVM_FAKENATIVE_FUNC callback,
                                void* data)
{
  NativeGroup* group = aGroup ? aGroup->ToNativeGroup() : nullptr;

  AutoPtr<RoutedNative> rn(new RoutedNative);
  rn->chunk = env_->stubs()->CreateFakeNativeStub(callback, data);
  if (!rn->chunk.address())
    return false;

  rn->name = new char[strlen(name)+1];
  strcpy(rn->name, name);

  rn->def = new NativeDef;
  rn->def->method = rn->chunk.address();
  rn->def->name = rn->name;
  rn->def->sig = nullptr;

  CharsAndLength key(rn->name);
  NativeTable::Insert i = table_.findForAdd(key);
  if (i.found()) {
    assert(false);
    return false;
  }

  if (!table_.add(i))
    return false;

  i->def = rn->def;
  i->group = group;
  i->rn = rn.take();

  if (group)
    group->addNatives(NativeGroup::NativeVector(rn->def, 1));
  return true;
}

void
NativeRegistry::Bind(PluginRuntime* rt, NativeEntry* entry)
{
  if (entry->status == SP_NATIVE_BOUND && !(entry->flags & SP_NTVFLAG_DYNAMIC))
    return;

  CharsAndLength name(entry->name);
  NativeTable::Result r = table_.find(name);
  if (!r.found())
    return;

  entry->status = SP_NATIVE_BOUND;
  entry->binding = r->def;
  entry->group = r->group;
}

NativeGroup::NativeGroup(NativeRegistry* registry, Delegate* delegate, const char* name)
 : registry_(registry),
   delegate_(delegate),
   name_(name)
{
}
