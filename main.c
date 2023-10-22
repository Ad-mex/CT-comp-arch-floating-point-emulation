#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define ui unsigned int
#define ull unsigned long long
#define us unsigned short

#ifdef __GNUC__
#define clz(x) __builtin_clz(x)
#define clzll(x) __builtin_clzll(x)
#else
int clz(ui x) {
    for (int shift = 31; shift >= 0; shift--) {
        if (x >> shift & 1)
            return 31 - shift;
    }
    return 32;
}
int clzll(ull x) {
    for (int shift = 63; shift >= 0; shift--) {
        if (x >> shift & 1)
            return 63 - shift;
    }
    return 64;
}
#endif

#define clzs(x) (clz((ui)x) - 16)

/*
 * format parse and errors
 */

void _format_parse_ab(char *arg, ui *a, ui *b) {
    char *dot;
    *a = strtoll(arg, &dot, 10);
    dot++;
    *b = strtoll(dot, &dot, 10);
}

void _format_error_message(const char *message) {
    fprintf(stderr, "error, bad format\n");
    fprintf(stderr, "%s", message);
    exit(1);
}

void _format_error_len(int argc) {
    if (argc != 4 && argc != 6) {
        _format_error_message("invalid number of arguments");
    }
}

void _format_error_ab(char *arg, ui *a, ui *b) {
    int len = strlen(arg);

    if (len > 5) {
        _format_error_message("invalid A.B format");
    }

    ui cnt_dot = 0;
    for (int i = 0; i < len; i++) {
        if (arg[i] == '.') {
            cnt_dot++;
        } else if (arg[i] < '0' || arg[i] > '9') {
            _format_error_message("invalid format A.B");
        }
    }

    if (cnt_dot != 1 || arg[0] == '.' || arg[len - 1] == '.') {
        _format_error_message("invalid format A.B");
    }

    _format_parse_ab(arg, a, b);
    if (*a + *b > 32 || *a == 0) {
        _format_error_message("invalid format A.B");
    }
}

void _format_error_round_type(char *arg) {
    if (strcmp(arg, "0") != 0 && strcmp(arg, "1") != 0 &&
        strcmp(arg, "2") != 0 && strcmp(arg, "3") != 0) {
        _format_error_message("invalid round argument");
    }
}

void _format_error_hex_arg(char *arg) {
    int len = strlen(arg);
    if (len < 3 || strncmp(arg, "0x", 2) != 0) {
        _format_error_message("invalid hex argument");
    }
    for (int i = 2; i < len; i++) {
        if ((arg[i] < 'a' || arg[i] > 'f') && (arg[i] < 'A' || arg[i] > 'F') &&
            (arg[i] < '0' || arg[i] > '9')) {
            _format_error_message("invalid hex argument");
        }
    }
}

ui _format_parse_hex(char *arg) {
    int len = strlen(arg);
    if (len > 10) {
        char *useless;
        ui ans = strtoll(arg + len - 8, &useless, 16);
        return ans;
    } else {
        char *useless;
        ui ans = strtoll(arg + 2, &useless, 16);
        return ans;
    }
}

void _format_error_operation(char *arg) {
    if (strcmp(arg, "*") != 0 && strcmp(arg, "+") != 0 &&
        strcmp(arg, "-") != 0 && strcmp(arg, "/") != 0) {
        _format_error_message("invalid operation type");
    }
}

/*
 * fixed point
 */

bool _fixed_has_minus(ui num, ui a, ui b) {
    return num & (1ull << (a + b - 1));
}

ui _fixed_normalize(ui num, ui a, ui b) {
    return num & ((1ull << (a + b)) - 1);
}

ui _fixed_minus(ui num, ui a, ui b) { return _fixed_normalize(~num + 1, a, b); }

void _fixed_out(ui num, ui a, ui b) {
    bool minus_flag = 0;
    if (_fixed_has_minus(num, a, b)) { // => minus
        minus_flag = 1;
        num = _fixed_minus(num, a, b);
    }
    ull frac = num & ((1u << b) - 1);
    ui cel = num >> b;
    ui drob;
    if (b >= 3)
        drob = (ui)((frac * 125) >> (b - 3));
    else
        drob = (ui)((frac * 125) << (3 - b));
    if (minus_flag && (cel != 0 || drob != 0)) {
        printf("-");
    }
    printf("%u.", cel);
    printf("%03d", drob);
}

ui _fixed_add(ui num1, ui num2, ui a, ui b) {
    return _fixed_normalize(num1 + num2, a, b);
}

ui _fixed_sub(ui num1, ui num2, ui a, ui b) {
    return _fixed_normalize(num1 - num2, a, b);
}

ui _fixed_mul(ui num1, ui num2, ui a, ui b) {
    bool minus_flag =
        _fixed_has_minus(num1, a, b) ^ _fixed_has_minus(num2, a, b);
    if (_fixed_has_minus(num1, a, b))
        num1 = _fixed_minus(num1, a, b);
    if (_fixed_has_minus(num2, a, b))
        num2 = _fixed_minus(num2, a, b);
    ull resx2_16 = ((ull)num1 * num2);
    ui ans = _fixed_normalize(resx2_16 >> b, a, b);
    if (minus_flag)
        ans = _fixed_minus(ans, a, b);

    return _fixed_normalize(ans, a, b);
}

ui _fixed_div(ui num1, ui num2, ui a, ui b) {

    if (num2 == 0) {
        printf("error");
        exit(0);
    }

    bool minus_flag = _fixed_has_minus(num1, a, b) ^
                      _fixed_has_minus(num2, a, b); // => div has minus

    if (_fixed_has_minus(num1, a, b))
        num1 = _fixed_minus(num1, a, b);
    if (_fixed_has_minus(num2, a, b))
        num2 = _fixed_minus(num2, a, b);

    ull ext_num1 = (ull)num1 << b;
    ull dv = _fixed_normalize(ext_num1 / num2, a, b);

    if (minus_flag) {
        dv = _fixed_minus(dv, a, b);
    }

    return _fixed_normalize(dv, a, b);
}

/*
 * single-precision
 */

#define SINGLE_NAN 0x7ff00000u
#define SINGLE_PLUS_INF 0x7f800000u
#define SINGLE_MINUS_INF 0xff800000u
#define SINGLE_NULL 0u
#define SINGLE_MINUS_NULL 0x80000000u

ui _single_minus(ui x) { return x ^ (1u << 31); }

bool _single_has_minus(ui x) { return x >> 31 & 1; }

ui _single_abs(ui x) { return (x & ~(1u << 31)); }

bool _single_is_plus_null(ui x) { return x == SINGLE_NULL; }

bool _single_is_minus_null(ui x) { return x == SINGLE_MINUS_NULL; }

bool _single_is_plus_inf(ui x) { return x == SINGLE_PLUS_INF; }

bool _single_is_minus_inf(ui x) { return x == SINGLE_MINUS_INF; }

bool _single_is_null(ui x) {
    return x == SINGLE_NULL || x == SINGLE_MINUS_NULL;
}

bool _single_is_nan(ui x) {
    ui ux = _single_abs(x);
    return ux != SINGLE_PLUS_INF && (ux ^ SINGLE_PLUS_INF) <= 0x7fffffu;
}

bool _single_is_denormalized(ui x) {
    ui ux = _single_abs(x);
    return ux <= 0x7fffffu;
}

int _single_get_exp(ui x) {
    return ((x & SINGLE_PLUS_INF) >> 23) == 0
               ? -126
               : (int)((x & SINGLE_PLUS_INF) >> 23) - 127;
}

ui _single_get_mant(ui x) { return x & 0x7fffff; }

ui _single_align_mant_to_hex(ui mant) { return mant << 1; }

bool _single_less(ui a, ui b) { // without infs, nans
    if (_single_is_null(a) && _single_is_null(b))
        return 0;
    if (_single_has_minus(a) && !_single_has_minus(b))
        return 1;
    if (_single_has_minus(b) && !_single_has_minus(a))
        return 0;
    bool flag_invert = 0;
    if (_single_has_minus(a) && _single_has_minus(b)) {
        flag_invert = 1;
    }
    if (a == b)
        return 0;
    if (_single_is_denormalized(a) && !_single_is_denormalized(b))
        return 1 ^ flag_invert;
    if (!_single_is_denormalized(a) && _single_is_denormalized(b)) {
        return 0 ^ flag_invert;
    }
    if (_single_get_exp(a) < _single_get_exp(b)) {
        return 1 ^ flag_invert;
    }
    if (_single_get_exp(a) > _single_get_exp(b)) {
        return 0 ^ flag_invert;
    }

    return (_single_get_mant(a) < _single_get_mant(b)) ^ flag_invert;
}

void _single_out(ui x) {
    if (_single_is_minus_inf(x)) {
        printf("-inf");
    } else if (_single_is_plus_inf(x)) {
        printf("inf");
    } else if (_single_is_nan(x)) {
        printf("nan");
    } else if (_single_is_plus_null(x)) {
        printf("0x0.000000p+0");
    } else if (_single_is_minus_null(x)) {
        printf("-0x0.000000p+0");
    } else {
        if (_single_has_minus(x)) {
            printf("-");
        }
        if (_single_is_denormalized(x)) {
            ui mant = _single_get_mant(x);
            int shift = clz(mant) - 8;
            mant <<= shift;
            mant &= ~(1 << 23);
            printf("0x1.%06xp%d", _single_align_mant_to_hex(mant),
                   -126 - shift);
        } else {
            int exp = _single_get_exp(x);
            ui mant = _single_get_mant(x);
            printf("0x1.%06xp", _single_align_mant_to_hex(mant));
            if (exp < 0) {
                printf("%d", exp);
            } else {
                printf("+%d", exp);
            }
        }
    }
}

ui _single_construct(int exp, ull mask) {
    if (mask == 0)
        return SINGLE_NULL;

    int shift = clzll(mask) - 8 - 32;
    if (shift < 0) {
        mask >>= -shift;
        exp -= shift;
        shift = 0;
    }
    if (exp - shift >= -126) {
        mask <<= shift;
        exp -= shift;
        mask ^= 1 << 23;
        if (exp >= 128)
            return SINGLE_PLUS_INF;
        return (((ui)(exp + 127)) << 23) | mask;
    }

    if (exp < -126) {
        if (-126 - exp >= 32)
            return SINGLE_NULL;
        mask >>= -126 - exp;
    } else
        mask <<= (exp + 126);
    return mask;
}

ui _single_add(ui, ui);

ui _single_sub(ui, ui);

ui _single_add(ui a, ui b) {
    if (_single_is_nan(a) || _single_is_nan(b)) {
        return SINGLE_NAN;
    }
    if (_single_is_plus_inf(a) && _single_is_minus_inf(b) ||
        _single_is_minus_inf(a) && _single_is_plus_inf(b)) {
        return SINGLE_NAN;
    }
    if (_single_is_plus_inf(a) && _single_is_plus_inf(b)) {
        return SINGLE_PLUS_INF;
    }
    if (_single_is_minus_inf(a) && _single_is_plus_inf(b)) {
        return SINGLE_MINUS_INF;
    }
    if (_single_has_minus(a) && _single_has_minus(b)) {
        return _single_minus(_single_add(_single_minus(a), _single_minus(b)));
    }
    if (_single_has_minus(a) && !_single_has_minus(b)) {
        return _single_sub(b, _single_minus(a));
    }
    if (!_single_has_minus(a) && _single_has_minus(b)) {
        return _single_sub(a, _single_minus(b));
    }

    // that a >= 0, b >= 0

    if (_single_is_denormalized(a) && _single_is_denormalized(b)) {
        return a + b;
    }

    int expa = _single_get_exp(a);
    int expb = _single_get_exp(b);

    if (expa < expb) {
        ui tmp = a;
        a = b;
        b = tmp;
        expa = _single_get_exp(a);
        expb = _single_get_exp(b);
    }
    int r = expa - expb;
    ui manta = _single_get_mant(a);
    ui mantb = _single_get_mant(b);

    if (!_single_is_denormalized(a))
        manta |= (1 << 23);
    if (!_single_is_denormalized(b))
        mantb |= (1 << 23);

    if (r >= 32) {
        return a;
    }

    mantb >>= r;
    manta += mantb;

    return _single_construct(expa, manta);
}

ui _single_sub(ui a, ui b) {
    if (_single_is_nan(a) || _single_is_nan(b)) {
        return SINGLE_NAN;
    }
    if (_single_is_plus_inf(a) && _single_is_plus_inf(b) ||
        _single_is_minus_inf(a) && _single_is_minus_inf(b)) {
        return SINGLE_NAN;
    }
    if (_single_is_plus_inf(a) && _single_is_minus_inf(b)) {
        return SINGLE_PLUS_INF;
    }
    if (_single_is_minus_inf(a) && _single_is_plus_inf(b)) {
        return SINGLE_MINUS_INF;
    }
    if (_single_has_minus(a) && !_single_has_minus(b)) {
        return _single_minus(_single_add(b, _single_minus(a)));
    }
    if (!_single_has_minus(a) && _single_has_minus(b)) {
        return _single_add(a, _single_minus(b));
    }
    if (_single_has_minus(a) && _single_has_minus(b)) {
        return _single_sub(_single_minus(b), _single_minus(a));
    }

    // that a >= 0, b >= 0

    bool flag_minus = 0;
    if (_single_less(a, b)) {
        flag_minus = 1;
        // swap
        ui tmp = a;
        a = b;
        b = tmp;
    }

    if (_single_is_denormalized(a) && _single_is_denormalized(b)) {
        return flag_minus ? _single_minus(a - b) : a - b;
    }

    int expa = _single_get_exp(a);
    int expb = _single_get_exp(b);
    int r = expa - expb;
    ui manta = _single_get_mant(a);
    ui mantb = _single_get_mant(b);

    if (!_single_is_denormalized(a))
        manta |= (1 << 23);
    if (!_single_is_denormalized(b))
        mantb |= (1 << 23);

    if (r >= 32) {
        return flag_minus ? _single_minus(a) : a;
    }

    mantb >>= r;
    manta -= mantb;

    ui ans = _single_construct(expa, manta);

    return flag_minus ? _single_minus(ans) : ans;
}

ui _single_mul(ui a, ui b) {
    if (_single_is_nan(a) || _single_is_nan(b))
        return SINGLE_NAN;
    if (_single_is_minus_inf(a) && _single_is_null(b)) {
        return SINGLE_NAN;
    }
    if (_single_is_plus_inf(a) && _single_is_null(b)) {
        return SINGLE_NAN;
    }
    if (_single_is_minus_inf(b) && _single_is_null(a)) {
        return SINGLE_NAN;
    }
    if (_single_is_plus_inf(b) && _single_is_null(a)) {
        return SINGLE_NAN;
    }

    bool flag_minus = _single_has_minus(a) ^ _single_has_minus(b);
    a = _single_abs(a);
    b = _single_abs(b);

    if (_single_is_null(a) || _single_is_null(b)) {
        return flag_minus ? SINGLE_MINUS_NULL : SINGLE_NULL;
    }
    if (_single_is_plus_inf(a) || _single_is_plus_inf(b)) {
        return flag_minus ? SINGLE_MINUS_INF : SINGLE_PLUS_INF;
    }

    int expa = _single_get_exp(a);
    int expb = _single_get_exp(b);
    ui manta = _single_get_mant(a);
    ui mantb = _single_get_mant(b);

    if (!_single_is_denormalized(a))
        manta |= 1 << 23;
    if (!_single_is_denormalized(b))
        mantb |= 1 << 23;

    ull mul = (ull)manta * mantb;
    int resexp = expa + expb;

    ui ans = _single_construct(resexp - 23, mul);
    return flag_minus ? _single_minus(ans) : ans;
}

ui _single_div(ui a, ui b) {
    if (_single_is_nan(a) || _single_is_nan(b)) {
        return SINGLE_NAN;
    }
    if(_single_is_null(a) && _single_is_null(b)){
        return SINGLE_NAN;
    }

    bool flag_minus = _single_has_minus(a) ^ _single_has_minus(b);
    a = _single_abs(a);
    b = _single_abs(b);
    
    if(_single_is_null(a)){
        return flag_minus ? SINGLE_MINUS_NULL : SINGLE_NULL;
    }
    if(_single_is_null(b)){
        return flag_minus ? SINGLE_MINUS_INF : SINGLE_PLUS_INF;
    }

    int expa = _single_get_exp(a);
    int expb = _single_get_exp(b);
    ui manta = _single_get_mant(a);
    ui mantb = _single_get_mant(b);

    if (!_single_is_denormalized(a))
        manta |= 1 << 23;
    if (!_single_is_denormalized(b))
        mantb |= 1 << 23;

    ull ext_a = (ull)manta << 23;
    ull dv = ext_a / mantb;
    int resexp = expa - expb;

    return flag_minus ? _single_minus(_single_construct(resexp, dv))
                      : _single_construct(resexp, dv);
}

/*
 * half-precision
 */

#define HALF_NAN 0x7fffu
#define HALF_PLUS_INF 0x7c00u
#define HALF_MINUS_INF 0xfc00u
#define HALF_NULL 0u
#define HALF_MINUS_NULL 0x8000u

us _half_minus(us x) { return x ^ (1u << 15); }

bool _half_has_minus(us x) { return x >> 15 & 1; }

us _half_abs(us x) { return (x & ~(1u << 15)); }

bool _half_is_plus_null(us x) { return x == HALF_NULL; }

bool _half_is_minus_null(us x) { return x == HALF_MINUS_NULL; }

bool _half_is_plus_inf(us x) { return x == HALF_PLUS_INF; }

bool _half_is_minus_inf(us x) { return x == HALF_MINUS_INF; }

bool _half_is_null(us x) { return x == HALF_NULL || x == HALF_MINUS_NULL; }

bool _half_is_nan(us x) {
    us ux = _half_abs(x);
    return ux != HALF_PLUS_INF && (ux ^ HALF_PLUS_INF) <= 0x3ffu;
}

bool _half_is_denormalized(us x) {
    us ux = _half_abs(x);
    return ux <= 0x3ff;
}

int _half_get_exp(us x) {
    int exp = (x & HALF_PLUS_INF) >> 10;
    return exp == 0 ? -14 : exp - 15;
}

ui _half_get_mant(ui x) { return x & 0x3ff; }

ui _half_align_mant_to_hex(ui mant) { return mant << 2; }

bool _half_less(ui a, ui b) { // without infs, nans
    if (_half_is_null(a) && _half_is_null(b))
        return 0;
    if (_half_has_minus(a) && !_half_has_minus(b))
        return 1;
    if (_half_has_minus(b) && !_half_has_minus(a))
        return 0;
    bool flag_invert = 0;
    if (_half_has_minus(a) && _half_has_minus(b)) {
        flag_invert = 1;
    }
    if (a == b)
        return 0;
    if (_half_is_denormalized(a) && !_half_is_denormalized(b))
        return 1 ^ flag_invert;
    if (!_half_is_denormalized(a) && _half_is_denormalized(b)) {
        return 0 ^ flag_invert;
    }
    if (_half_get_exp(a) < _half_get_exp(b)) {
        return 1 ^ flag_invert;
    }
    if (_half_get_exp(a) > _half_get_exp(b)) {
        return 0 ^ flag_invert;
    }

    return (_half_get_mant(a) < _half_get_mant(b)) ^ flag_invert;
}

void _half_out(us x) {
    if (_half_is_minus_inf(x)) {
        printf("-inf");
    } else if (_half_is_plus_inf(x)) {
        printf("inf");
    } else if (_half_is_nan(x)) {
        printf("nan");
    } else if (_half_is_plus_null(x)) {
        printf("0x0.000p+0");
    } else if (_half_is_minus_null(x)) {
        printf("-0x0.000p+0");
    } else {
        if (_half_has_minus(x)) {
            printf("-");
        }
        if (_half_is_denormalized(x)) {
            us mant = _half_get_mant(x);
            int shift = clzs(mant) - 5;
            mant <<= shift;
            mant &= ~(1 << 10);
            printf("0x1.%03xp%d", _half_align_mant_to_hex(mant), -14 - shift);
        } else {
            int exp = _half_get_exp(x);
            us mant = _half_get_mant(x);
            printf("0x1.%03xp", _half_align_mant_to_hex(mant));
            if (exp < 0) {
                printf("%d", exp);
            } else {
                printf("+%d", exp);
            }
        }
    }
}

us _half_construct(int exp, ui mask) {
    if (mask == 0)
        return SINGLE_NULL;

    int shift = clz(mask) - 5 - 16;
    if (shift < 0) {
        mask >>= -shift;
        exp -= shift;
        shift = 0;
    }
    if (exp - shift >= -14) {
        mask <<= shift;
        exp -= shift;
        if (exp >= 16)
            return HALF_PLUS_INF;
        mask ^= 1 << 10;
        return (((us)(exp + 15)) << 10) | mask;
    }

    if (exp < -14) {
        mask >>= -14 - exp;
        if (-14 - exp >= 16)
            return SINGLE_NULL;
    } else
        mask <<= (exp + 14);
    return mask;
}

us _half_add(us, us);

us _half_sub(us, us);

us _half_add(us a, us b) {
    if (_half_is_nan(a) || _half_is_nan(b)) {
        return HALF_NAN;
    }
    if (_half_is_plus_inf(a) && _half_is_minus_inf(b) ||
        _half_is_minus_inf(a) && _half_is_plus_inf(b)) {
        return HALF_NAN;
    }
    if (_half_is_plus_inf(a) && _half_is_plus_inf(b)) {
        return HALF_PLUS_INF;
    }
    if (_half_is_minus_inf(a) && _half_is_plus_inf(b)) {
        return HALF_MINUS_INF;
    }
    if (_half_has_minus(a) && _half_has_minus(b)) {
        return _half_minus(_half_add(_half_minus(a), _half_minus(b)));
    }
    if (_half_has_minus(a) && !_half_has_minus(b)) {
        return _half_sub(b, _half_minus(a));
    }
    if (!_half_has_minus(a) && _half_has_minus(b)) {
        return _half_sub(a, _half_minus(b));
    }

    // that a >= 0, b >= 0

    if (_half_is_denormalized(a) && _half_is_denormalized(b)) {
        return a + b;
    }

    int expa = _half_get_exp(a);
    int expb = _half_get_exp(b);

    if (expa < expb) {
        us tmp = a;
        a = b;
        b = tmp;
        expa = _half_get_exp(a);
        expb = _half_get_exp(b);
    }
    int r = expa - expb;
    us manta = _half_get_mant(a);
    us mantb = _half_get_mant(b);

    if (!_half_is_denormalized(a))
        manta |= (1 << 10);
    if (!_half_is_denormalized(b))
        mantb |= (1 << 10);

    if (r >= 16) {
        return a;
    }

    mantb >>= r;
    manta += mantb;

    return _half_construct(expa, manta);
}

us _half_sub(us a, us b) {
    if (_half_is_nan(a) || _half_is_nan(b)) {
        return HALF_NAN;
    }
    if (_half_is_plus_inf(a) && _half_is_plus_inf(b) ||
        _half_is_minus_inf(a) && _half_is_minus_inf(b)) {
        return HALF_NAN;
    }
    if (_half_is_plus_inf(a) && _half_is_minus_inf(b)) {
        return HALF_PLUS_INF;
    }
    if (_half_is_minus_inf(a) && _half_is_plus_inf(b)) {
        return HALF_MINUS_INF;
    }
    if (_half_has_minus(a) && !_half_has_minus(b)) {
        return _half_minus(_half_add(b, _half_minus(a)));
    }
    if (!_half_has_minus(a) && _half_has_minus(b)) {
        return _half_add(a, _half_minus(b));
    }
    if (_half_has_minus(a) && _half_has_minus(b)) {
        return _half_sub(_half_minus(b), _half_minus(a));
    }

    // that a >= 0, b >= 0

    bool flag_minus = 0;
    if (_half_less(a, b)) {
        flag_minus = 1;
        // swap
        us tmp = a;
        a = b;
        b = tmp;
    }

    if (_half_is_denormalized(a) && _half_is_denormalized(b)) {
        return flag_minus ? _half_minus(a - b) : a - b;
    }

    int expa = _half_get_exp(a);
    int expb = _half_get_exp(b);
    int r = expa - expb;
    us manta = _half_get_mant(a);
    us mantb = _half_get_mant(b);

    if (!_half_is_denormalized(a))
        manta |= (1 << 10);
    if (!_half_is_denormalized(b))
        mantb |= (1 << 10);

    if (r >= 16) {
        return flag_minus ? _half_minus(a) : a;
    }

    mantb >>= r;
    manta -= mantb;

    return flag_minus ? _half_minus(_half_construct(expa, manta))
                      : _half_construct(expa, manta);
}

us _half_mul(us a, us b) {
    if (_half_is_nan(a) || _half_is_nan(b))
        return HALF_NAN;
    if (_half_is_minus_inf(a) && _half_is_null(b)) {
        return HALF_NAN;
    }
    if (_half_is_plus_inf(a) && _half_is_null(b)) {
        return HALF_NAN;
    }
    if (_half_is_minus_inf(b) && _half_is_null(a)) {
        return HALF_NAN;
    }
    if (_half_is_plus_inf(b) && _half_is_null(a)) {
        return HALF_NAN;
    }

    bool flag_minus = _half_has_minus(a) ^ _half_has_minus(b);
    a = _half_abs(a);
    b = _half_abs(b);

    if (_half_is_null(a) || _half_is_null(b)) {
        return flag_minus ? HALF_MINUS_NULL : HALF_NULL;
    }
    if (_half_is_plus_inf(a) || _half_is_plus_inf(b)) {
        return flag_minus ? HALF_MINUS_INF : HALF_PLUS_INF;
    }

    int expa = _half_get_exp(a);
    int expb = _half_get_exp(b);
    us manta = _half_get_mant(_half_abs(a));
    us mantb = _half_get_mant(_half_abs(b));

    if (!_half_is_denormalized(a))
        manta |= 1 << 10;
    if (!_half_is_denormalized(b))
        mantb |= 1 << 10;

    ui mul = (ui)manta * mantb;
    int resexp = expa + expb;

    us ans = _half_construct(resexp - 10, mul);
    return flag_minus ? _half_minus(ans) : ans;
}

us _half_div(us a, us b) {
    if (_half_is_nan(a) || _half_is_nan(b)) {
        return HALF_NAN;
    }
    if(_half_is_null(a) && _half_is_null(b)){
        return HALF_NAN;
    }

    bool flag_minus = _half_has_minus(a) ^ _half_has_minus(b);
    a = _half_abs(a);
    b = _half_abs(b);
    
    if(_half_is_null(a)){
        return flag_minus ? HALF_MINUS_NULL : HALF_NULL;
    }
    if(_half_is_null(b)){
        return flag_minus ? HALF_MINUS_INF : HALF_PLUS_INF;
    }

    int expa = _half_get_exp(a);
    int expb = _half_get_exp(b);
    us manta = _half_get_mant(_half_abs(a));
    us mantb = _half_get_mant(_half_abs(b));

    if (!_half_is_denormalized(a))
        manta |= 1 << 10;
    if (!_half_is_denormalized(b))
        mantb |= 1 << 10;

    ui ext_a = (ui)manta << 10;
    ui dv = ext_a / mantb;
    int resexp = expa - expb;

    return flag_minus ? _half_minus(_half_construct(resexp, dv))
                      : _half_construct(resexp, dv);
}

/*
 * main parser
 */

int main(int argc, char **argv) {

    _format_error_len(argc);

    char *format_str = argv[1];
    char *round_str = argv[2];
    char format;
    char round;

    if (strcmp(format_str, "h") == 0) {
        format = 2;
    } else if (strcmp(format_str, "f") == 0) {
        format = 3;
    } else {
        format = 1;
    }

    round = round_str[0] - '0';

    if (round == 0) {

        if (format == 1) {

            ui a, b;
            _format_error_ab(argv[1], &a, &b);

            if (argc == 4) { // one number
                _format_error_hex_arg(argv[3]);

                ui num = _fixed_normalize(_format_parse_hex(argv[3]), a, b);
                _fixed_out(num, a, b);
            } else {
                _format_error_hex_arg(argv[3]);
                _format_error_hex_arg(argv[5]);

                ui num1 = _format_parse_hex(argv[3]);
                ui num2 = _format_parse_hex(argv[5]);

                _format_error_operation(argv[4]);

                char operation = argv[4][0];

                num1 = _fixed_normalize(num1, a, b);
                num2 = _fixed_normalize(num2, a, b);

                if (operation == '+') {
                    _fixed_out(_fixed_add(num1, num2, a, b), a, b);
                } else if (operation == '-') {
                    _fixed_out(_fixed_sub(num1, num2, a, b), a, b);
                } else if (operation == '*') {
                    _fixed_out(_fixed_mul(num1, num2, a, b), a, b);
                } else if (operation == '/') {
                    _fixed_out(_fixed_div(num1, num2, a, b), a, b);
                }
            }
        } else if (format == 3) {
            if (argc == 4) { // one number
                _format_error_hex_arg(argv[3]);

                ui num = _format_parse_hex(argv[3]);
                _single_out(num);
            } else {
                _format_error_hex_arg(argv[3]);
                _format_error_hex_arg(argv[5]);

                ui num1 = _format_parse_hex(argv[3]);
                ui num2 = _format_parse_hex(argv[5]);

                _format_error_operation(argv[4]);

                char operation = argv[4][0];

                if (operation == '+') {
                    _single_out(_single_add(num1, num2));
                } else if (operation == '-') {
                    _single_out(_single_sub(num1, num2));
                } else if (operation == '*') {
                    _single_out(_single_mul(num1, num2));
                } else if (operation == '/') {
                    _single_out(_single_div(num1, num2));
                }
            }
        } else {
            if (argc == 4) { // one number
                _format_error_hex_arg(argv[3]);

                us num = _format_parse_hex(argv[3]);
                _half_out(num);
            } else {
                _format_error_hex_arg(argv[3]);
                _format_error_hex_arg(argv[5]);

                us num1 = _format_parse_hex(argv[3]);
                us num2 = _format_parse_hex(argv[5]);

                _format_error_operation(argv[4]);

                char operation = argv[4][0];

                if (operation == '+') {
                    _half_out(_half_add(num1, num2));
                } else if (operation == '-') {
                    _half_out(_half_sub(num1, num2));
                } else if (operation == '*') {
                    _half_out(_half_mul(num1, num2));
                } else if (operation == '/') {
                    _half_out(_half_div(num1, num2));
                }
            }
        }
    } else {
        exit(1);
    }
}