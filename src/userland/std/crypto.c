#include "../include/crypto.h"


int CRT_sha256_init(sha256_ctx* ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;

    ctx->count[0] = 0;
    ctx->count[1] = 0;

    return 1;
}

int CRT_sha256_transform(sha256_ctx* ctx, const uint8_t data[]) {
    uint32_t a, b, c, d, e, f, g, h, i, t1, t2;
    uint32_t m[64];

    for (i = 0; i < 16; i++) {
        m[i] = (data[i * 4] << 24) | (data[i * 4 + 1] << 16) | (data[i * 4 + 2] << 8) | (data[i * 4 + 3]);
    }
    for (i = 16; i < 64; i++) {
        m[i] = sigma1(m[i - 2]) + m[i - 7] + sigma0(m[i - 15]) + m[i - 16];
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0; i < 64; i++) {
        t1 = h + SIGMA1(e) + CH(e, f, g) + k[i] + m[i];
        t2 = sigma0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;

    return 1;
}

int CRT_sha256_update(sha256_ctx* ctx, const uint8_t* data, size_t len) {
    size_t j;

    j = (ctx->count[0] >> 3) & 63;
    if ((ctx->count[0] += (len << 3)) < (len << 3)) {
        ctx->count[1]++;
    }

    ctx->count[1] += (len >> 29);
    while (len--) {
        ctx->buffer[j++] = *data++;
        if (j == 64) {
            CRT_sha256_transform(ctx, ctx->buffer);
            j = 0;
        }
    }

    return 1;
}

int CRT_sha256_final(sha256_ctx* ctx, uint8_t hash[SHA256_BLOCK_SIZE]) {
    uint32_t i;

    i = (ctx->count[0] >> 3) & 63;
    ctx->buffer[i++] = 0x80;
    if (i > 56) {
        while (i < 64) ctx->buffer[i++] = 0;
        CRT_sha256_transform(ctx, ctx->buffer);
        i = 0;
    }

    while (i < 56) {
        ctx->buffer[i++] = 0;
    }

    ctx->buffer[56] = (ctx->count[1] >> 24) & 0xff;
    ctx->buffer[57] = (ctx->count[1] >> 16) & 0xff;
    ctx->buffer[58] = (ctx->count[1] >> 8) & 0xff;
    ctx->buffer[59] = (ctx->count[1]) & 0xff;
    ctx->buffer[60] = (ctx->count[0] >> 24) & 0xff;
    ctx->buffer[61] = (ctx->count[0] >> 16) & 0xff;
    ctx->buffer[62] = (ctx->count[0] >> 8) & 0xff;
    ctx->buffer[63] = (ctx->count[0]) & 0xff;

    CRT_sha256_transform(ctx, ctx->buffer);
    for (i = 0; i < 8; i++) {
        hash[i * 4] = (ctx->state[i] >> 24) & 0xff;
        hash[i * 4 + 1] = (ctx->state[i] >> 16) & 0xff;
        hash[i * 4 + 2] = (ctx->state[i] >> 8) & 0xff;
        hash[i * 4 + 3] = (ctx->state[i]) & 0xff;
    }

    return 1;
}

int CRT_compare_hash(const char* input, const uint8_t expected_hash[SHA256_BLOCK_SIZE]) {
    uint8_t hash[SHA256_BLOCK_SIZE];

    sha256_ctx ctx;
    CRT_sha256_init(&ctx);
    CRT_sha256_update(&ctx, (uint8_t*)input, strlen(input));
    CRT_sha256_final(&ctx, hash);

    if (memcmp(hash, expected_hash, SHA256_BLOCK_SIZE) == 0) return 1;
    else return 0;
}
