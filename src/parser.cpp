#include "main.h"

#include <ctype.h>
#include <math.h>

#include "parser.h"

static inline double powerd(double x, int64_t y) {
        double temp;
        
        if (y == 0) {
                return 1;
        }

        temp = powerd(x, y / 2);
        
        if ((y % 2) == 0) {
                return temp * temp;
        }
        else {
                if (y > 0) {
                        return x * temp * temp;
                }
                else {
                        return (temp * temp) / x;
                }
        }
}

enum NumberParseFlags : unsigned {
        NumberParseFlags_None = 0,
        NumberParseFlags_Float = 1 << 0,
        NumberParseFlags_Unsigned = 1 << 1,
        NumberParseFlags_Hex = 1 << 2
};

static bool ParseNumber(const char* str, void* out_val, unsigned flags) {
        if (!str) return false;
        if (!out_val) return false;

        while (*str <= ' ') {
                if (
                        (*str == ' ') ||
                        (*str == '\t') ||
                        (*str == '\r') ||
                        (*str == '\n') ||
                        (*str == '\v')
                ) {
                        ++str;
                        continue;
                }

                return false;
        }

        const bool is_unsigned = flags & NumberParseFlags_Unsigned;
        const bool is_float = flags & NumberParseFlags_Float;
        const bool is_negative = (*str == '-');
        const char* const start = str;
        uint32_t radix = (flags & NumberParseFlags_Hex) ? 16 : 10;

        if (*str == '0') {
                ++str;
                if (*str == 'x' || *str == 'X') {
                        ++str;
                        radix = 16;
                }
        }

        const bool is_hex = radix == 16;

        if  (is_negative || (*str == '+')){
                ++str;
        } 
        
        if (is_negative && is_unsigned) {
                DEBUG("Cannot have negative unsigned number");
                return false;
        }

        if (is_float && is_hex) {
                DEBUG("Only decimal floats are supported");
                return false;
        }

        enum ComponentType : unsigned {
                CT_Integer,
                CT_Exponent,
                CT_Fraction,
                CT_Count
        };

        uint64_t num[CT_Count] = {};
        uint8_t frac_len = 0;
        uint8_t num_type = CT_Integer;
        bool exponent_neg = false;
        unsigned c = 0;

        while (*str) {
                c = (unsigned char) *str++;

                if (c < '0') {
                        if (is_float && c == '.' && num_type != CT_Fraction) {
                                num_type = CT_Fraction;
                                frac_len = 0;
                                continue;
                        }
                        else {
                                goto INVALID_CHARACTER;
                        }
                }

                c |= unsigned{32};

                if (c > 'f') {
                        goto INVALID_CHARACTER;
                }

                if (is_float) {
                        if (c == 'e' && num_type != CT_Exponent) {
                                if (*str == '-') {
                                        exponent_neg = true;
                                        ++str;
                                }
                                else if (*str == '+') {
                                        ++str;
                                }
                                num_type = CT_Exponent;
                                continue;
                        }
                }

                if (num_type == CT_Fraction) {
                        ++frac_len;
                }

                if (c <= '9') {
                        num[num_type] *= radix;
                        num[num_type] += c - '0';
                        continue;
                }

                if (c >= 'a') {
                        if (!is_hex) {
                                goto INVALID_CHARACTER;
                        }
                        num[num_type] *= radix;
                        num[num_type] += c - 'a' + 10;
                        continue;
                }

                goto INVALID_CHARACTER;
        }

        if (is_unsigned) {
                *(uint64_t*)out_val = num[CT_Integer];
                DEBUG("Parse '%s' as U64: %llu", start, num[CT_Integer]);
        }
        else if (is_float) {
                double dval = (double)num[CT_Fraction];
                int64_t exponent = num[CT_Exponent];
                exponent *= (exponent_neg) ? -1 : 1;
                dval /= powerd(10, frac_len);
                dval += num[CT_Integer];
                dval *= powerd(10, exponent);
                *(double*)out_val = dval;
                DEBUG("Parse '%s' as Double: %f", start, dval);
        }
        else {
                *(int64_t*)out_val = num[CT_Integer] * (is_negative) ? -1 : 1;
                DEBUG("Parse '%s' as I64: %lld", start, num[CT_Integer]);
        }
        return true;

INVALID_CHARACTER:
        DEBUG("Invalid character: %u", c);
        DEBUG("Start: '%s', Rest: '%s', Int: %llu, Exp: %llu, Frac: %llu, Frac Len: %u", start, str, num[CT_Integer], num[CT_Exponent], num[CT_Fraction], frac_len);
        return false;
}


static bool ParseU64(const char* str, uint64_t* out_val, bool as_hex) {
        const auto flags = NumberParseFlags_Unsigned | ((as_hex)? NumberParseFlags_Hex : NumberParseFlags_None);
        const auto ret = ParseNumber(str, out_val, flags);

        return ret;
}

static bool ParseU32(const char* str, uint32_t* out_val, bool as_hex) {
        uint64_t val = 0;
        const auto ret = ParseU64(str, &val, as_hex);
        if (val > UINT32_MAX) {
                val = UINT32_MAX;
        }
        *out_val = (uint32_t) val;
        return ret;
}


static bool ParseU16(const char* str, uint16_t* out_val, bool as_hex) {
        uint64_t val = 0;
        const auto ret = ParseU64(str, &val, as_hex);
        if (val > UINT16_MAX) {
                val = UINT16_MAX;
        }
        *out_val = (uint16_t) val;
        return ret;
}


static bool ParseU8(const char* str, uint8_t* out_val, bool as_hex) {
        uint64_t val = 0;
        const auto ret = ParseU64(str, &val, as_hex);
        if (val > UINT8_MAX) {
                val = UINT8_MAX;
        }
        *out_val = (uint8_t) val;
        return ret;
}


static bool ParseS64(const char* str, int64_t* out_val, bool as_hex) {
        const auto flags = (as_hex)? NumberParseFlags_Hex : NumberParseFlags_None;
        const auto ret = ParseNumber(str, out_val, flags);

        return ret;
}


static bool ParseS32(const char* str, int32_t* out_val, bool as_hex) {
        int64_t val = 0;
        const auto ret = ParseS64(str, &val, as_hex);
        if (val > INT32_MAX) {
                val = INT32_MAX;
        }
        else if (val < INT32_MIN) {
                val = INT32_MIN;
        }
        *out_val = (int32_t) val;
        return ret;
}


static bool ParseS16(const char* str, int16_t* out_val, bool as_hex) {
        int64_t val = 0;
        const auto ret = ParseS64(str, &val, as_hex);
        if (val > INT16_MAX) {
                val = INT16_MAX;
        } else if (val < INT16_MIN) {
                val = INT16_MIN;
        }
        *out_val = (int16_t) val;
        return ret;
}


static bool ParseS8(const char* str, int8_t* out_val, bool as_hex) {
        int64_t val = 0;
        const auto ret = ParseS64(str, &val, as_hex);
        if (val > INT8_MAX) {
                val = INT8_MAX;
        }
        else if (val < INT8_MIN) {
                val = INT8_MIN;
        }
        *out_val = (int8_t) val;
        return ret;
}


static bool ParseDouble(const char* str, double* out_val) {
        return ParseNumber(str, out_val, NumberParseFlags_Float);
}

static bool ParseFloat(const char* str, float* out_val) {
        double dval;
        const auto ret = ParseNumber(str, &dval, NumberParseFlags_Float);
        if(dval > FLT_MAX) {
                dval = FLT_MAX;
        } else if (dval < FLT_MIN) {
                dval = FLT_MIN;
        }
        *out_val = (float) dval;
        return ret;
}


extern const struct parse_api_t* GetParserAPI() {
        static const struct parse_api_t api = {
                ParseU64,
                ParseU32,
                ParseU16,
                ParseU8,
                ParseS64,
                ParseS32,
                ParseS16,
                ParseS8,
                ParseDouble,
                ParseFloat,
        };
        return &api;
}