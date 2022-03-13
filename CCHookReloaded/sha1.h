#ifndef SHA1_H
#define SHA1_H

/*
   SHA-1 in C
   By Steve Reid <steve@edmweb.com>
   100% Public Domain
 */

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t state[5];
    uint32_t count[2];
    uint8_t buffer[64];
} SHA1_CTX;

void SHA1Transform(
    uint32_t state[5],
    const uint8_t buffer[64]
    );

void SHA1Init(
    SHA1_CTX * context
    );

void SHA1Update(
    SHA1_CTX * context,
    const uint8_t *data,
    uint32_t len
    );

void SHA1Final(
    uint8_t digest[20],
    SHA1_CTX * context
    );

void SHA1(
    char *hash_out,
    const char *str,
    uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* SHA1_H */