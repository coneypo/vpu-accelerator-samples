#ifndef __MACROS_H__
#define __MACROS_H__

#ifdef __cplusplus
extern "C" {
#endif


#ifndef ALIGN_POW2
#define ALIGN_POW2(a, b) ((a + (b - 1)) & ~(b - 1))
#endif

#ifndef ALIGN_32
#define ALIGN_32(a) ALIGN_POW2(a, 32)
#endif

#ifndef ALIGN_128
#define ALIGN_128(a) ALIGN_POW2(a, 128)
#endif

#ifndef RETURN_IF_FAIL
#define RETURN_IF_FAIL(condition) \
    do{ \
        if (!(condition)) \
            return;  \
    }while(0)
#endif

#ifndef RETURN_VAL_IF_FAIL
#define RETURN_VAL_IF_FAIL(condition, value) \
    do{ \
        if (!(condition)) \
            return (value);  \
    }while(0)
#endif


#ifdef __cplusplus
}
#endif


#endif
