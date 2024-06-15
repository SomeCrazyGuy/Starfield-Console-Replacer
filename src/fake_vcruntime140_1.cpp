#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

 extern "C" EXCEPTION_DISPOSITION __declspec(dllexport) __CxxFrameHandler4(void* A, void* B, void* C, void* D) {
         auto hmodule = LoadLibraryExW(L"vcruntime140_1.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
         if (!hmodule) {
                 MessageBoxA(NULL, __func__, "Betterconsole Crashed!", 0);
                 std::abort();
         }
         auto p = (decltype(&__CxxFrameHandler4))GetProcAddress(hmodule, "__CxxFrameHandler4");
         return p(A, B, C, D);
}
