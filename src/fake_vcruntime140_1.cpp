#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

 extern "C" EXCEPTION_DISPOSITION __declspec(dllexport) __CxxFrameHandler4(void*, void*, void*, void*) {
        //todo: forward this to the real one?
         MessageBoxA(NULL, __func__, "Betterconsole Crashed!", 0);
         std::abort();
}
