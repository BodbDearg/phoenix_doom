/*======================================================================================================================
 * CPU features 
 *====================================================================================================================*/

/* Define if we have SSE CPU extensions */
#define HAVE_SSE
#define HAVE_SSE2
#define HAVE_SSE3
#define HAVE_SSE4_1

/*======================================================================================================================
 * Compiler features
 *====================================================================================================================*/
#define HAVE_C99_BOOL               /* Define if we have C99 _Bool support */

/* Because MSVC doesn't do plain C well... (sigh) */
#if defined(_MSC_VER)
    #ifndef restrict
        #define restrict __restrict
    #endif
#endif

/*======================================================================================================================
 * Library features
 *====================================================================================================================*/
#define HAVE_CBRTF                  /* Define if we have the cbrtf function */
#define HAVE_COPYSIGNF              /* Define if we have the copysignf function */
#define HAVE_CPUID_H                /* Define if we have cpuid.h */
#define HAVE_FENV_H                 /* Define if we have fenv.h */
#define HAVE_FLOAT_H                /* Define if we have float.h */
#define HAVE_INTRIN_H               /* Define if we have intrin.h */
#define HAVE_LOG2F                  /* Define if we have the log2f function */
#define HAVE_LRINTF                 /* Define if we have the lrintf function */
#define HAVE_MALLOC_H               /* Define if we have malloc.h */
#define HAVE_MODFF                  /* Define if we have the modff function */
#define HAVE_STDBOOL_H              /* Define if we have stdbool.h */
#define HAVE_STDINT_H               /* Define if we have stdint.h */
#define HAVE_STRNLEN                /* Define if we have the strnlen function */
#define HAVE_STRTOF                 /* Define if we have the strtof function */
#define HAVE_STRUCT_TIMESPEC        /* Define if we have the 'timespec' struct */

/* Define if we have windows.h */
#ifdef _WIN32
    #define HAVE_WINDOWS_H
#endif

#if defined(_MSC_VER)
    /* Define if we have _controlfp() */
    #define HAVE__CONTROLFP

    /* MSVC doesn't have <strings.h> so these have to be used instead */
    #ifdef _MSC_VER 
        #define strncasecmp _strnicmp
        #define strcasecmp _stricmp
    #endif
#endif

/*======================================================================================================================
 * Misc
 *====================================================================================================================*/
#define AL_ALEXT_PROTOTYPES                         /* Making available AL extension functions */
#define AL_API                                      /* API declaration export attribute */
#define AL_LIBTYPE_STATIC                           /* Always build a static library */
#define ALC_API                                     /* API declaration export attribute */
#define SIZEOF_LONG         sizeof(long)            /* Define to the size of a long int type */
#define SIZEOF_LONG_LONG    sizeof(long long)       /* Define to the size of a long long int type */

/* Define any available alignment declaration */
#if defined(_MSC_VER)
    #define ALIGN(x) __declspec(align(x))
#elif defined(__GNUC__)
    #define ALIGN(x) __attribute__ ((aligned(x)))
#else
    #error Need to define 'ALIGN' macro!
#endif

/* Define a built-in call indicating an aligned data pointer */
#ifdef __GNUC__
    #define ASSUME_ALIGNED(x, y) __builtin_assume_aligned(x, y)
#else
    #define ASSUME_ALIGNED(x, y)
#endif

/*======================================================================================================================
 * Backends
 *====================================================================================================================*/

/* Define if we have the DSound backend */
#ifdef _WIN32
    #define HAVE_DSOUND
#endif

/* Define if we have the CoreAudio backend */
#ifdef __APPLE__
    #define HAVE_COREAUDIO
#endif

/*======================================================================================================================
 * Not defining these ever!
 *====================================================================================================================*/
#if 0
    /*==================================================================================================================
     * CPU features 
     *================================================================================================================*/
    #define HAVE_NEON                   /* Define if we have ARM Neon CPU extensions */

    /*==================================================================================================================
     * Compiler features
     *================================================================================================================*/
    #define HAVE___INT64                /* Define if we have the __int64 type */
    #define HAVE_C11_ALIGNAS            /* Define if we have C11 _Alignas support */
    #define HAVE_C11_ATOMIC             /* Define if we have C11 _Atomic support */
    #define HAVE_C11_STATIC_ASSERT      /* Define if we have C11 _Static_assert support */
    #define HAVE_CPUID_INTRINSIC        /* Define if we have the __cpuid() intrinsic */
    #define HAVE_GCC_DESTRUCTOR         /* Define if we have GCC's destructor attribute */
    #define HAVE_GCC_FORMAT             /* Define if we have GCC's format attribute */
    #define HAVE_GCC_GET_CPUID          /* Define if we have GCC's __get_cpuid() */
    
    /*==================================================================================================================
     * Library features
     *================================================================================================================*/    
    #define HAVE___CONTROL87_2                      /* Define if we have __control87_2() */
    #define HAVE__ALIGNED_MALLOC                    /* Define if we have the _aligned_malloc function */
    #define HAVE_ALIGNED_ALLOC                      /* Define if we have the C11 aligned_alloc function */
    #define HAVE_BITSCANFORWARD64_INTRINSIC         /* Define if we have the _BitScanForward64() intrinsic */
    #define HAVE_BITSCANFORWARD_INTRINSIC           /* Define if we have the _BitScanForward() intrinsic */
    #define HAVE_DIRENT_H                           /* Define if we have dirent.h */
    #define HAVE_DLFCN_H                            /* Define if we have dlfcn.h */
    #define HAVE_GETOPT                             /* Define if we have the getopt function */
    #define HAVE_GUIDDEF_H                          /* Define if we have guiddef.h */
    #define HAVE_IEEEFP_H                           /* Define if we have ieeefp.h */    
    #define HAVE_INITGUID_H                         /* Define if we have initguid.h */
    #define HAVE_POSIX_MEMALIGN                     /* Define if we have the posix_memalign function */
    #define HAVE_PROC_PIDPATH                       /* Define if we have the proc_pidpath function */
    #define HAVE_PTHREAD_MUTEX_TIMEDLOCK            /* Define if we have pthread_mutex_timedlock() */
    #define HAVE_PTHREAD_MUTEXATTR_SETKIND_NP       /* Define if we have pthread_mutexattr_setkind_np() */
    #define HAVE_PTHREAD_NP_H                       /* Define if we have pthread_np.h */
    #define HAVE_PTHREAD_SET_NAME_NP                /* Define if we have pthread_set_name_np() */
    #define HAVE_PTHREAD_SETNAME_NP                 /* Define if we have pthread_setname_np() */
    #define HAVE_PTHREAD_SETSCHEDPARAM              /* Define if we have pthread_setschedparam() */
    #define HAVE_STAT                               /* Define if we have the stat function */
    #define HAVE_STDALIGN_H                         /* Define if we have stdalign.h */
    #define HAVE_STRINGS_H                          /* Define if we have strings.h */
    #define HAVE_SYS_SYSCONF_H                      /* Define if we have sys/sysconf.h */
    #define HAVE_SYSCONF                            /* Define if we have the sysconf function */
    #define PTHREAD_SETNAME_NP_ONE_PARAM            /* Define if pthread_setname_np() only accepts one parameter */
    #define PTHREAD_SETNAME_NP_THREE_PARAMS         /* Define if pthread_setname_np() accepts three parameters */
    
    /*==================================================================================================================
     * Misc
     *================================================================================================================*/
    #define ALSOFT_EMBED_HRTF_DATA      /* Define if HRTF data is embedded in the library */

    /*==================================================================================================================
     * Backends
     *================================================================================================================*/
    #define HAVE_ALSA               /* Define if we have the ALSA backend */    
    #define HAVE_JACK               /* Define if we have the JACK backend */
    #define HAVE_OPENSL             /* Define if we have the OpenSL backend */
    #define HAVE_OSS                /* Define if we have the OSS backend */
    #define HAVE_PORTAUDIO          /* Define if we have the PortAudio backend */
    #define HAVE_PULSEAUDIO         /* Define if we have the PulseAudio backend */
    #define HAVE_QSA                /* Define if we have the QSA backend */
    #define HAVE_SDL2               /* Define if we have the SDL2 backend */
    #define HAVE_SNDIO              /* Define if we have the SndIO backend */
    #define HAVE_SOLARIS            /* Define if we have the Solaris backend */
    #define HAVE_WASAPI             /* Define if we have the WASAPI backend */
    #define HAVE_WAVE               /* Define if we have the Wave Writer backend */
    #define HAVE_WINMM              /* Define if we have the Windows Multimedia backend */
#endif
