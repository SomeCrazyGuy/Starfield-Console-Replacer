#include "main.h"

#include <stdio.h>


static constexpr struct std_api_t api {
        &malloc,
        &free,
        &snprintf,
        &memcpy,
        &memset
};

extern constexpr const struct std_api_t* GetStdAPI() {
        return &api;
}