#ifndef	YISR_VERSION_INCLUDED
#define YISR_VERSION_INCLUDED

#define MAKE_STR_HELPER(a_str) #a_str
#define MAKE_STR(a_str) MAKE_STR_HELPER(a_str)

#define YISR_VERSION_MAJOR	1
#define YISR_VERSION_MINOR	2
#define YISR_VERSION_PATCH	0
#define YISR_VERSION_BETA	0
#define YISR_VERSION_VERSTRING	MAKE_STR(YISR_VERSION_MAJOR) "." MAKE_STR(YISR_VERSION_MINOR) "." MAKE_STR(YISR_VERSION_PATCH) "." MAKE_STR(YISR_VERSION_BETA)

#endif
