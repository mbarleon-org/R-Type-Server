#pragma once

#if defined(_WIN32)
    #if defined(R_TYPE_SRV_BUILD)
        #define RTYPE_SRV_API __declspec(dllexport)
    #else
        #define RTYPE_SRV_API __declspec(dllimport)
    #endif
#else
    #define RTYPE_SRV_API __attribute__((visibility("default")))
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define RTYPE_SRV_HIDE_FROM_ABI __attribute__((visibility("hidden")))
#else
    #define RTYPE_SRV_HIDE_FROM_ABI
#endif
