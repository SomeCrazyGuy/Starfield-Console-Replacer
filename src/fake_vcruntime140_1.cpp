#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>


[[noreturn]] static inline void fatal_error(const char* const message) noexcept {
        MessageBoxA(NULL, message, "Fatal Error", 0);
        std::abort();
        _STL_UNREACHABLE;
}


 extern "C" EXCEPTION_DISPOSITION __declspec(dllexport) __CxxFrameHandler4(void*, void*, void*, void*) {
        //todo, forward this to the real one?
        fatal_error(__func__);
}
