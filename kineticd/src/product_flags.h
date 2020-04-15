#ifndef KINETIC_PRODUCT_FLAGS_H_
#define KINETIC_PRODUCT_FLAGS_H_
// For managing multiple product configs
// Product specific settings and dependencies are configured here

#if BUILD_FOR_ARM == 1

// LAMARR Product Defines
#ifdef PRODUCT_LAMARRKV
#define SED_SUPPORTED
#define FIRMWARE_SIGNING_ENABLED
#define VECTOR_PORZ
#define LLDP_ENABLED
// #define QOS_ENABLED
// #define DEFAULT_QOS 2
// #define NO_SPACE_REPORTING
#endif

// Lamarr Interposer build for espresso bin
#ifdef PRODUCT_LAMARRKV_ARMADALP
//#define SED_SUPPORTED
#define FIRMWARE_SIGNING_ENABLED
// #define VECTOR_PORZ
#define LLDP_ENABLED
// #define QOS_ENABLED
// #define DEFAULT_QOS 2
// #define NO_SPACE_REPORTING
#endif

#else // BUILD_FOR_ARM

#ifdef PRODUCT_X86
#undef SED_SUPPORTED
#define FIRMWARE_SIGNING_ENABLED
#undef VECTOR_PORZ
// #define QOS_ENABLED
// #define DEFAULT_QOS 2
#endif

#endif // BUILD_FOR_ARM

#endif  // KINETIC_PRODUCT_FLAGS_H_
