#include "../oslib/oslib.h"
#include "../imgread/common.h"

#define _WIN32_WINNT 0x0500 
#include <windows.h>

bool VramLockedWrite(u8* address);
bool ngen_Rewrite(unat& addr,unat retadr,unat acc);
bool BM_LockedWrite(u8* address);


LONG ExeptionHandler(EXCEPTION_POINTERS *ExceptionInfo)
{
   EXCEPTION_POINTERS* ep = ExceptionInfo;

   u32 dwCode = ep->ExceptionRecord->ExceptionCode;

   EXCEPTION_RECORD* pExceptionRecord=ep->ExceptionRecord;

   if (dwCode != EXCEPTION_ACCESS_VIOLATION)
      return EXCEPTION_CONTINUE_SEARCH;

   u8* address=(u8*)pExceptionRecord->ExceptionInformation[1];

   //printf("[EXC] During access to : 0x%X\n", address);

   if (VramLockedWrite(address))
   {
      return EXCEPTION_CONTINUE_EXECUTION;
   }
#ifndef TARGET_NO_NVMEM
   else if (BM_LockedWrite(address))
   {
      return EXCEPTION_CONTINUE_EXECUTION;
   }
#endif
#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86
   else if ( ngen_Rewrite((unat&)ep->ContextRecord->Eip,*(unat*)ep->ContextRecord->Esp,ep->ContextRecord->Eax) )
   {
      //remove the call from call stack
      ep->ContextRecord->Esp+=4;
      //restore the addr from eax to ecx so its valid again
      ep->ContextRecord->Ecx=ep->ContextRecord->Eax;
      return EXCEPTION_CONTINUE_EXECUTION;
   }
#endif
   else
   {
      printf("[GPF]Unhandled access to : 0x%X\n",address);
   }

   return EXCEPTION_CONTINUE_SEARCH;
}

void os_MakeExecutable(void* ptr, u32 sz)
{
   DWORD old;
   VirtualProtect(ptr,sizeof(sz),PAGE_EXECUTE_READWRITE,&old);
}

#ifdef _WIN64
#include "hw/sh4/dyna/ngen.h"

typedef union _UNWIND_CODE {
   struct {
      u8 CodeOffset;
      u8 UnwindOp : 4;
      u8 OpInfo : 4;
   };
   USHORT FrameOffset;
} UNWIND_CODE, *PUNWIND_CODE;

typedef struct _UNWIND_INFO {
   u8 Version : 3;
   u8 Flags : 5;
   u8 SizeOfProlog;
   u8 CountOfCodes;
   u8 FrameRegister : 4;
   u8 FrameOffset : 4;
   //ULONG ExceptionHandler;
   UNWIND_CODE UnwindCode[1];
   /*  UNWIND_CODE MoreUnwindCode[((CountOfCodes + 1) & ~1) - 1];
    *   union {
    *       OPTIONAL ULONG ExceptionHandler;
    *       OPTIONAL ULONG FunctionEntry;
    *   };
    *   OPTIONAL ULONG ExceptionData[]; */
} UNWIND_INFO, *PUNWIND_INFO;

static RUNTIME_FUNCTION Table[1];
static _UNWIND_INFO unwind_info[1];

   EXCEPTION_DISPOSITION
__gnat_SEH_error_handler(struct _EXCEPTION_RECORD* ExceptionRecord,
      void *EstablisherFrame,
      struct _CONTEXT* ContextRecord,
      void *DispatcherContext)
{
   EXCEPTION_POINTERS ep;
   ep.ContextRecord = ContextRecord;
   ep.ExceptionRecord = ExceptionRecord;

   return (EXCEPTION_DISPOSITION)ExeptionHandler(&ep);
}

PRUNTIME_FUNCTION
seh_callback(
      _In_ DWORD64 ControlPc,
      _In_opt_ PVOID Context
      ) {
   unwind_info[0].Version = 1;
   unwind_info[0].Flags = UNW_FLAG_UHANDLER;
   /* We don't use the unwinding info so fill the structure with 0 values.  */
   unwind_info[0].SizeOfProlog = 0;
   unwind_info[0].CountOfCodes = 0;
   unwind_info[0].FrameOffset = 0;
   unwind_info[0].FrameRegister = 0;
   /* Add the exception handler.  */

   //		unwind_info[0].ExceptionHandler =
   //	(DWORD)((u8 *)__gnat_SEH_error_handler - CodeCache);
   /* Set its scope to the entire program.  */
   Table[0].BeginAddress = 0;// (CodeCache - (u8*)__ImageBase);
   Table[0].EndAddress = /*(CodeCache - (u8*)__ImageBase) +*/ CODE_SIZE;
   Table[0].UnwindData = (DWORD)((u8 *)unwind_info - CodeCache);
   printf("TABLE CALLBACK\n");
   //for (;;);
   return Table;
}
void setup_seh() {
#if 1
   /* Get the base of the module.  */
   //u8* __ImageBase = (u8*)GetModuleHandle(NULL);
   /* Current version is always 1 and we are registering an
      exception handler.  */
   unwind_info[0].Version = 1;
   unwind_info[0].Flags = UNW_FLAG_NHANDLER;
   /* We don't use the unwinding info so fill the structure with 0 values.  */
   unwind_info[0].SizeOfProlog = 0;
   unwind_info[0].CountOfCodes = 1;
   unwind_info[0].FrameOffset = 0;
   unwind_info[0].FrameRegister = 0;
   /* Add the exception handler.  */

   unwind_info[0].UnwindCode[0].CodeOffset = 0;
   unwind_info[0].UnwindCode[0].UnwindOp = 2;// UWOP_ALLOC_SMALL;
   unwind_info[0].UnwindCode[0].OpInfo = 0x20 / 8;

   //unwind_info[0].ExceptionHandler =
   //(DWORD)((u8 *)__gnat_SEH_error_handler - CodeCache);
   /* Set its scope to the entire program.  */
   Table[0].BeginAddress = 0;// (CodeCache - (u8*)__ImageBase);
   Table[0].EndAddress = /*(CodeCache - (u8*)__ImageBase) +*/ CODE_SIZE;
   Table[0].UnwindData = (DWORD)((u8 *)unwind_info - CodeCache);
   /* Register the unwind information.  */
   RtlAddFunctionTable(Table, 1, (DWORD64)CodeCache);
#endif

   //verify(RtlInstallFunctionTableCallback((unat)CodeCache | 0x3, (DWORD64)CodeCache, CODE_SIZE, seh_callback, 0, 0));
}
#endif

int CALLBACK WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShowCmd)
{
   ReserveBottomMemory();

   //SetUnhandledExceptionFilter(&ExeptionHandler);
   __try
   {
      int dc_init(int argc,wchar* argv[]);
      void dc_run();
      void dc_term();
      dc_init(argc,argv);

#ifdef _WIN64
      setup_seh();
#endif

      dc_run();
      dc_term();
   }
   __except( ExeptionHandler(GetExceptionInformation()) )
   {
      printf("Unhandled exception - Emulation thread halted...\n");
   }
   SetUnhandledExceptionFilter(0);

   return 0;
}
