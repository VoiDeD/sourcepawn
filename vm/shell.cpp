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
#include <sp_vm_api.h>
#include <stdlib.h>
#include <stdarg.h>
#include <am-cxx.h>
#include "dll_exports.h"
#include "environment.h"
#include "stack-frames.h"

using namespace ke;
using namespace sp;
using namespace SourcePawn;

Environment *sEnv;

static void
DumpStack(IFrameIterator &iter)
{
  int index = 0;
  for (; !iter.Done(); iter.Next(), index++) {
    if (iter.IsInternalFrame())
      continue;

    const char *name = iter.FunctionName();
    if (!name) {
      fprintf(stdout, "  [%d] <unknown>\n", index);
      continue;
    }

    if (iter.IsScriptedFrame()) {
      const char *file = iter.FilePath();
      if (!file)
        file = "<unknown>";
      fprintf(stdout, "  [%d] %s::%s, line %d\n", index, file, name, iter.LineNumber());
    } else {
      fprintf(stdout, "  [%d] %s()\n", index, name);
    }
  }
}

class ShellDebugListener : public IDebugListener
{
public:
  void ReportError(const IErrorReport &report, IFrameIterator &iter) KE_OVERRIDE {
    fprintf(stdout, "Exception thrown: %s\n", report.Message());
    DumpStack(iter);
  }

  void OnDebugSpew(const char *msg, ...) {
#if !defined(NDEBUG) && defined(DEBUG)
    va_list ap;
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
#endif
  }
};

static cell_t Print(IPluginContext *cx, const cell_t *params)
{
  char *p;
  cx->LocalToString(params[1], &p);

  return printf("%s", p);
}

static cell_t PrintNum(IPluginContext *cx, const cell_t *params)
{
  return printf("%d\n", params[1]);
}

static cell_t PrintNums(IPluginContext *cx, const cell_t *params)
{
  for (size_t i = 1; i <= size_t(params[0]); i++) {
    int err;
    cell_t *addr;
    if ((err = cx->LocalToPhysAddr(params[i], &addr)) != SP_ERROR_NONE)
      return cx->ThrowNativeErrorEx(err, "Could not read argument");
    fprintf(stdout, "%d", *addr);
    if (i != size_t(params[0]))
      fprintf(stdout, ", ");
  }
  fprintf(stdout, "\n");
  return 1;
}

static cell_t DoNothing(IPluginContext *cx, const cell_t *params)
{
  return 1;
}

static cell_t PrintFloat(IPluginContext *cx, const cell_t *params, void* data)
{
  assert(data == reinterpret_cast<void*>(data));
  return printf("%f\n", sp_ctof(params[1]));
}

static cell_t DoExecute(IPluginContext *cx, const cell_t *params)
{
  int32_t ok = 0;
  for (size_t i = 0; i < size_t(params[2]); i++) {
    if (IPluginFunction *fn = cx->GetFunctionById(params[1])) {
      if (fn->Execute(nullptr) != SP_ERROR_NONE)
        continue;
      ok++;
    }
  }
  return ok;
}

static cell_t DoInvoke(IPluginContext *cx, const cell_t *params)
{
  for (size_t i = 0; i < size_t(params[2]); i++) {
    if (IPluginFunction *fn = cx->GetFunctionById(params[1])) {
      if (!fn->Invoke())
        return 0;
    }
  }
  return 1;
}

static cell_t DumpStackTrace(IPluginContext *cx, const cell_t *params)
{
  FrameIterator iter;
  DumpStack(iter);
  return 0;
}

static cell_t ReportError(IPluginContext* cx, const cell_t *params)
{
  cx->ReportError("What the crab?!");
  return 0;
}

static int Execute(Ref<INativeRegistry> registry, const char *file)
{
  char error[255];
  AutoPtr<IPluginRuntime> rt(sEnv->APIv2()->LoadBinaryFromFile(file, error, sizeof(error)));
  if (!rt) {
    fprintf(stderr, "Could not load plugin: %s\n", error);
    return 1;
  }

  rt->BindNatives(registry);

  IPluginFunction *fun = rt->GetFunctionByName("main");
  if (!fun)
    return 0;

  IPluginContext *cx = rt->GetDefaultContext();

  int result;
  {
    ExceptionHandler eh(cx);
    if (!fun->Invoke(&result)) {
      fprintf(stderr, "Error executing main: %s\n", eh.Message());
      return 1;
    }
  }

  return result;
}

SourcePawn::NativeDef sNatives[] = {
  { "print",            Print },
  { "printnum",         PrintNum },
  { "printnums",        PrintNums },
  { "donothing",        DoNothing },
  { "execute",          DoExecute },
  { "invoke",           DoInvoke },
  { "dump_stack_trace", DumpStackTrace },
  { "report_error",     ReportError },
  { nullptr,            nullptr },
};

int main(int argc, char **argv)
{
  if (argc != 2) {
    fprintf(stderr, "Usage: <file>\n");
    return 1;
  }

  if ((sEnv = Environment::New()) == nullptr) {
    fprintf(stderr, "Could not initialize ISourcePawnEngine2\n");
    return 1;
  }

  if (getenv("DISABLE_JIT"))
    sEnv->SetJitEnabled(false);

  ShellDebugListener debug;
  sEnv->SetDebugger(&debug);
  sEnv->InstallWatchdogTimer(5000);

  Ref<INativeRegistry> registry = sEnv->NewNativeRegistry("shell");
  registry->AddNativeList(nullptr, sNatives);
  registry->AddRoutedNative(nullptr, "printfloat", PrintFloat, reinterpret_cast<void*>(0x1234));

  int errcode = Execute(registry, argv[1]);

  sEnv->SetDebugger(NULL);
  sEnv->Shutdown();
  delete sEnv;

  return errcode;
}
