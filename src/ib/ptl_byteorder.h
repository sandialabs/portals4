#ifndef PTL_BYTEORDER_H
#define PTL_BYTEORDER_H

/* use these for network byte order */
typedef uint16_t __be16;
typedef uint32_t __be32;
typedef uint64_t __be64;
typedef uint16_t __le16;
typedef uint32_t __le32;
typedef uint64_t __le64;

static inline __be16 cpu_to_be16(uint16_t x)
{
    return htons(x);
}

static inline uint16_t be16_to_cpu(__be16 x)
{
    return htons(x);
}

static inline __be32 cpu_to_be32(uint32_t x)
{
    return htonl(x);
}

static inline uint32_t be32_to_cpu(__be32 x)
{
    return htonl(x);
}

static inline __be64 cpu_to_be64(uint64_t x)
{
    uint64_t y = htonl((uint32_t) x);
    return (y << 32) | htonl((uint32_t) (x >> 32));
}

static inline uint64_t be64_to_cpu(__be64 x)
{
    return cpu_to_be64(x);
}

#if __BYTE_ORDER==__LITTLE_ENDIAN
static inline __le16 cpu_to_le16(uint16_t x)
{
    return x;
}

static inline uint16_t le16_to_cpu(__le16 x)
{
    return x;
}

static inline __le32 cpu_to_le32(uint32_t x)
{
    return x;
}

static inline uint32_t le32_to_cpu(__le32 x)
{
    return x;
}

static inline __le64 cpu_to_le64(uint64_t x)
{
    return x;
}

static inline uint64_t le64_to_cpu(__le64 x)
{
    return x;
}
#elif __BYTE_ORDER==__BIG_ENDIAN
static inline __le16 cpu_to_le16(uint16_t x)
{
    return htole16(x);
}

static inline uint16_t le16_to_cpu(__le16 x)
{
    return le16toh(x);
}

static inline __le32 cpu_to_le32(uint32_t x)
{
    return htole32(x);
}

static inline uint32_t le32_to_cpu(__le32 x)
{
    return le32toh(x);
}

static inline __le64 cpu_to_le64(uint64_t x)
{
    return htole64(x);
}

static inline uint64_t le64_to_cpu(__le64 x)
{
    return le64toh(x);
}
#else
#error Not defined yet.
#endif

#endif
