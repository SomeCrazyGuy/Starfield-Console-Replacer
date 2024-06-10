#include "main.h"

#include <stdio.h>


extern const struct std_api_t* GetStdAPI() {
        static const struct std_api_t api {
                &malloc,
                &free,
                &snprintf,
                &memcpy,
                &memset
        };
        return &api;
}