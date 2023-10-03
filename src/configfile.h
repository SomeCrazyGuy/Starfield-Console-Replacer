#pragma once

#include "main.h"

// quick hack for structname.membername or struct->member passed to BIND_INT
inline constexpr const char* member_name_only(const char* in) {
        while (*in) ++in;
        --in;
        while (
                ((*in >= 'a') && (*in <= 'z')) ||
                ((*in >= '0') && (*in <= '9')) ||
                ((*in >= 'A') && (*in <= 'Z'))
              ) --in;
        ++in;
        return in;
}

#define BIND_INT(X) member_name_only(#X), &X

extern const struct config_api_t* GetConfigAPI();