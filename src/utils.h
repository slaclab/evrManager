#ifndef evr_manager_utils_h 
#define evr_manager_utils_h

#include <endian.h>

#if BYTE_ORDER == BIG_ENDIAN
# define be16_to_cpu(x) (x)
# define be32_to_cpu(x) (x)
#elif BYTE_ORDER == LITTLE_ENDIAN
# define be16_to_cpu(x) (((x>>8) & 0x00ff) |\
						((x<<8) & 0xff00))
# define be32_to_cpu(x) (((x>>24) & 0x000000ff) |\
						((x>>8)  & 0x0000ff00) |\
						((x<<8)  & 0x00ff0000) |\
						((x<<24) & 0xff000000))
#else
# error "Oops: unknown byte order"
#endif

#define cpu_to_be16(x) be16_to_cpu(x)
#define cpu_to_be32(x) be32_to_cpu(x)


#define ADBG(FORMAT, ...) printf("DBG: " FORMAT "\n", ## __VA_ARGS__)
#define AINFO(FORMAT, ...) printf("INFO: " FORMAT "\n", ## __VA_ARGS__)
#define AERR(FORMAT, ...) printf("ERROR: " FORMAT "\n", ## __VA_ARGS__)



#endif // evr_manager_utils_h


