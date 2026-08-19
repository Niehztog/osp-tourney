// ngmark.c -- <INVENTED FILENAME>. The ngWorldStats verification mark: the
// running MD5 over every log line, the per-player identifier and the
// transmitted mark.

#include "g_local.h"

MD5_CTX context;

// gamex86.dll: 1004DCE0..1004DCF2
// gamei386.so: 0006EF58..0006EF78
void ngLog_initMark(void)
{
    MD5Init(&context);
}

/*
==============
ngLog_inputLine

Fold one line into the running checksum, then scramble it in place.
==============
*/
// gamex86.dll: 1004DCF2..1004DD52
// gamei386.so: 0006EF78..0006EFC9
void ngLog_inputLine(char *line)
{
    int     len;
    int     i;

    len = strlen(line);
    MD5Update(&context, line, len);
    for (i = 0; i < len; i++)
        line[i] = line[i] ^ 0xa7;
}

/*
==============
ngLog_giveMark

Close out the running checksum and write it as 32 hex digits.
==============
*/
// gamex86.dll: 1004DD52..1004DE1E
// gamei386.so: 0006EFCC..0006F086
void ngLog_giveMark(char *out)
{
    unsigned char   mark[16];
    char            salt[2048];
    char            hexchars[1024];
    unsigned int    i;

    ngLog_transMark(salt, (int *)&i);
    MD5Update(&context, salt, i);
    MD5Final(mark, &context);

    salt[0] = '\0';
    for (i = 0; i < 16; i++) {
        sprintf(hexchars, "%02x", mark[i]);
        strcat(salt, hexchars);
    }
    strcpy(out, salt);
}

/*
==============
ngLog_playerIdentifier

An MD5 of two strings plus the salt, as hex. Its own context, not the running
one, so it does not disturb the log mark.
==============
*/
// gamex86.dll: 1004DE1E..1004DEFF
// gamei386.so: 0006F088..0006F1AB
char *ngLog_playerIdentifier(char *a, char *b)
{
    // 2048, not 256: real's .bss puts the next TU's first static exactly 0x800
    // bytes further on.  Nothing in either audit can see a static buffer's
    // SIZE -- the ELF masks the [ebx+-disp] that reaches it.
    static char     idbuf[2048];
    MD5_CTX         md5ctx;
    unsigned char   digest[16];
    char            hexchars[1024];
    int             i;

    MD5Init(&md5ctx);
    MD5Update(&md5ctx, a, strlen(a));
    MD5Update(&md5ctx, b, strlen(b));
    ngLog_transMark(idbuf, &i);
    MD5Update(&md5ctx, idbuf, i);
    MD5Final(digest, &md5ctx);

    idbuf[0] = '\0';
    for (i = 0; i < 16; i++) {
        sprintf(hexchars, "%02x", digest[i]);
        strcat(idbuf, hexchars);
    }
    return idbuf;
}

/*
==============
ngLog_transMark

Decode the 0x21-int salt table out of .rodata, one byte at a time, and wipe the
plaintext copy afterwards so it is never resident.
==============
*/
// gamex86.dll: 1004DEFF..1004E113
// gamei386.so: 0006F1AC..0006F257
void ngLog_transMark(char *out, int *count)
{
    char        scratch[16];
    char        buf[128];
    // "ngUS@ ngL0G Kw@ke2 1mplem3ntati0n" ^ 0xa9, 33 ints.
    int         salttab[33] = {
        0xc7, 0xce, 0xfc, 0xfa, 0xe9, 0x89, 0xc7, 0xce, 0xe5, 0x99, 0xee,
        0x89, 0xe2, 0xde, 0xe9, 0xc2, 0xcc, 0x9b, 0x89, 0x98, 0xc4, 0xd9,
        0xc5, 0xcc, 0xc4, 0x9a, 0xc7, 0xdd, 0xc8, 0xdd, 0xc0, 0x99, 0xc7
    };
    int         i;

    buf[0] = '\0';
    *count = 33;

    for (i = 0; i < *count; i++) {
        sprintf(scratch, "%c", salttab[i] ^ 0xa9);
        strcat(buf, scratch);
    }
    strcpy(out, buf);

    for (i = 0; i < *count; i++)
        buf[i] = '\0';
}

//=============================================================================
// RFC 1321 Appendix A's md5c.c, verbatim apart from its two #includes, which
// g_local.h already carries.  It is part of THIS object, not a TU of its own:
// real's DLL starts MD5Init at an unaligned 1004E113 with no int3 before it.
//=============================================================================

/* MD5C.C - RSA Data Security, Inc., MD5 message-digest algorithm
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
 */


/* Constants for MD5Transform routine.
 */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

static void MD5Transform PROTO_LIST ((UINT4 [4], unsigned char [64]));
static void Encode PROTO_LIST
  ((unsigned char *, UINT4 *, unsigned int));
static void Decode PROTO_LIST
  ((UINT4 *, unsigned char *, unsigned int));
static void MD5_memcpy PROTO_LIST ((POINTER, POINTER, unsigned int));
static void MD5_memset PROTO_LIST ((POINTER, int, unsigned int));

static unsigned char PADDING[64] = {
  0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* F, G, H and I are basic MD5 functions.
 */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits.
 */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
Rotation is separate from addition to prevent recomputation.
 */
#define FF(a, b, c, d, x, s, ac) { \
 (a) += F ((b), (c), (d)) + (x) + (UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) { \
 (a) += G ((b), (c), (d)) + (x) + (UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) { \
 (a) += H ((b), (c), (d)) + (x) + (UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) { \
 (a) += I ((b), (c), (d)) + (x) + (UINT4)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }

/* MD5 initialization. Begins an MD5 operation, writing a new context.
 */
// gamex86.dll: 1004E113..1004E153
// gamei386.so: 0006F258..0006F289
void MD5Init (context)
MD5_CTX *context;                                        /* context */
{
  context->count[0] = context->count[1] = 0;
  /* Load magic initialization constants.
*/
  context->state[0] = 0x67452301;
  context->state[1] = 0xefcdab89;
  context->state[2] = 0x98badcfe;
  context->state[3] = 0x10325476;
}

/* MD5 block update operation. Continues an MD5 message-digest
  operation, processing another message block, and updating the
  context.
 */
// gamex86.dll: 1004E153..1004E252
// gamei386.so: 0006F28C..0006F338
void MD5Update (context, input, inputLen)
MD5_CTX *context;                                        /* context */
unsigned char *input;                                /* input block */
unsigned int inputLen;                     /* length of input block */
{
  unsigned int i, index, partLen;

  /* Compute number of bytes mod 64 */
  index = (unsigned int)((context->count[0] >> 3) & 0x3F);

  /* Update number of bits */
  if ((context->count[0] += ((UINT4)inputLen << 3))
   < ((UINT4)inputLen << 3))
 context->count[1]++;
  context->count[1] += ((UINT4)inputLen >> 29);

  partLen = 64 - index;

  /* Transform as many times as possible.
*/
  if (inputLen >= partLen) {
 MD5_memcpy
   ((POINTER)&context->buffer[index], (POINTER)input, partLen);
 MD5Transform (context->state, context->buffer);

 for (i = partLen; i + 63 < inputLen; i += 64)
   MD5Transform (context->state, &input[i]);

 index = 0;
  }
  else
 i = 0;

  /* Buffer remaining input */
  MD5_memcpy
 ((POINTER)&context->buffer[index], (POINTER)&input[i],
  inputLen-i);
}

/* MD5 finalization. Ends an MD5 message-digest operation, writing the
  the message digest and zeroizing the context.
 */
// gamex86.dll: 1004E252..1004E2ED
// gamei386.so: 0006F338..0006F4FA
void MD5Final (digest, context)
unsigned char digest[16];                         /* message digest */
MD5_CTX *context;                                       /* context */
{
  unsigned char bits[8];
  unsigned int index, padLen;

  /* Save number of bits */
  Encode (bits, context->count, 8);

  /* Pad out to 56 mod 64.
*/
  index = (unsigned int)((context->count[0] >> 3) & 0x3f);
  padLen = (index < 56) ? (56 - index) : (120 - index);
  MD5Update (context, PADDING, padLen);

  /* Append length (before padding) */
  MD5Update (context, bits, 8);
  /* Store state in digest */
  Encode (digest, context->state, 16);

  /* Zeroize sensitive information.
*/
  MD5_memset ((POINTER)context, 0, sizeof (*context));
}

/* MD5 basic transformation. Transforms state based on block.
 */
// gamex86.dll: 1004E2ED..1004F196
// gamei386.so: 0006F4FA..0006FE0B
static void MD5Transform (state, block)
UINT4 state[4];
unsigned char block[64];
{
  UINT4 a = state[0], b = state[1], c = state[2], d = state[3], x[16];

  Decode (x, block, 64);

  /* Round 1 */
  FF (a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
  FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
  FF (c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
  FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
  FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
  FF (d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
  FF (c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
  FF (b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
  FF (a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
  FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
  FF (c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
  FF (b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
  FF (a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
  FF (d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
  FF (c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
  FF (b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

 /* Round 2 */
  GG (a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
  GG (d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
  GG (c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
  GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
  GG (a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
  GG (d, a, b, c, x[10], S22,  0x2441453); /* 22 */
  GG (c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
  GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
  GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
  GG (d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
  GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
  GG (b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
  GG (a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
  GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
  GG (c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
  GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

  /* Round 3 */
  HH (a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
  HH (d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
  HH (c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
  HH (b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
  HH (a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
  HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
  HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
  HH (b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
  HH (a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
  HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
  HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
  HH (b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
  HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
  HH (d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
  HH (c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
  HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

  /* Round 4 */
  II (a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
  II (d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
  II (c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
  II (b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
  II (a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
  II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
  II (c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
  II (b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
  II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
  II (d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
  II (c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
  II (b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
  II (a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
  II (d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
  II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
  II (b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;

  /* Zeroize sensitive information.
*/
  MD5_memset ((POINTER)x, 0, sizeof (x));
}

/* Encodes input (UINT4) into output (unsigned char). Assumes len is
  a multiple of 4.
 */
// gamex86.dll: 1004F196..1004F236
// gamei386.so: 0006FE0B..0006FE4E
static void Encode (output, input, len)
unsigned char *output;
UINT4 *input;
unsigned int len;
{
  unsigned int i, j;

  for (i = 0, j = 0; j < len; i++, j += 4) {
 output[j] = (unsigned char)(input[i] & 0xff);
 output[j+1] = (unsigned char)((input[i] >> 8) & 0xff);
 output[j+2] = (unsigned char)((input[i] >> 16) & 0xff);
 output[j+3] = (unsigned char)((input[i] >> 24) & 0xff);
  }
}

/* Decodes input (unsigned char) into output (UINT4). Assumes len is
  a multiple of 4.
 */
// gamex86.dll: 1004F236..1004F2AF
// gamei386.so: 0006FE4E..0006FEA6
static void Decode (output, input, len)
UINT4 *output;
unsigned char *input;
unsigned int len;
{
  unsigned int i, j;

  for (i = 0, j = 0; j < len; i++, j += 4)
 output[i] = ((UINT4)input[j]) | (((UINT4)input[j+1]) << 8) |
   (((UINT4)input[j+2]) << 16) | (((UINT4)input[j+3]) << 24);
}

/* Note: Replace "for loop" with standard memcpy if possible.
 */

// gamex86.dll: 1004F2AF..1004F2E3
// gamei386.so: 0006FEA6..0006FECC
static void MD5_memcpy (output, input, len)
POINTER output;
POINTER input;
unsigned int len;
{
  unsigned int i;

  for (i = 0; i < len; i++)
 output[i] = input[i];
}

/* Note: Replace "for loop" with standard memset if possible.
 */
// gamex86.dll: 1004F2E3..1004F312
// gamei386.so: 0006FECC..0006FEF8
static void MD5_memset (output, value, len)
POINTER output;
int value;
unsigned int len;
{
  unsigned int i;

  for (i = 0; i < len; i++)
 ((char *)output)[i] = (char)value;
}

//=============================================================================
// Aim-bot and speed-cheat detection -- still this object: real's ELF has an
// `as` nop (8d 76 00) after MD5_memset, its DLL starts OSP_botDetect at an
// unaligned 1004F312, and real's .rodata runs these strings on from the salt
// table above with no realignment.
//
// OSP_botDetect is a ZBOT detector.  It watches the view-angle deltas in
// consecutive usercmd_t frames around an attack: a human's aim drifts between
// the frame before a shot and the frame of the shot, an aim-bot's snaps and
// then is bit-for-bit identical, so a delta of exactly zero on the release
// frame -- twice in a row -- is the tell.  It also flags the two fixed
// signatures ZBOT leaves behind: a non-zero `impulse` in a movement command,
// and the sentinel yaw 0x3f49.
//=============================================================================

void ClientDisconnect(edict_t *ent);

// File statics.  The loop counter really is a static in the original, and the
// DECLARATION ORDER is read off real's .bss run, which lays these out in
// exactly this sequence -- including a 4-byte object between zb_delta and
// zb_shoot that nothing in the image references.  Only that object's
// existence and size are evidence; its name and type are <INVENTED>.
static int          zb_count;
static float        zb_delta[2];
static int          zb_pad;
static byte         zb_shoot;
static float        zb_distance;
static gclient_t    *zb_target;

// gamex86.dll: 1004F312..1004F692
// gamei386.so: 0006FEF8..000701B4
bool OSP_botDetect(edict_t *ent, usercmd_t *ucmd)
{
    char    why[32];

    zb_target = ent->client;

    if (zb_target->resp.entered != ENTERED_ENTERED ||
        zb_target->ping > 500 ||
        zb_target->osp_t024 == level.framenum ||
        zb_target->resp.osp_r07c[0])
        return false;

    // a movement command never carries an impulse
    if (ucmd->impulse) {
        OnBotDetection(ent, "i");
        return true;
    }

    zb_shoot = ucmd->buttons & BUTTON_ATTACK;
    // Written `^`, not `!=`: both operands are 0 or BUTTON_ATTACK, so the XOR
    // is the edge-detect spelling.
    if (zb_shoot ^ zb_target->osp_t01c[0]) {
        zb_target->osp_t01c[0] = zb_shoot;

        // An empty then-arm, the Gladiator SDK's idiom: /Od then jumps INTO
        // the else on the float test and skips it with a separate jmp.
        if (!zb_shoot && zb_target->osp_t020 < 39000) {
        } else {
            if (!ucmd->msec || (zb_shoot && abs(ucmd->angles[0]) == 0x3f49)) {
                zb_target->osp_t020 = 0;
            } else {
                for (zb_count = 0; zb_count < 2; zb_count++) {
                    zb_delta[zb_count] = (float)(ucmd->angles[zb_count] - zb_target->osp_t028[zb_count]);
                    if (zb_delta[zb_count] > 32768)
                        zb_delta[zb_count] -= 65536;
                    else if (zb_delta[zb_count] < -32768)
                        zb_delta[zb_count] += 65536;
                }

                zb_distance = (zb_delta[0] * zb_delta[0] + zb_delta[1] * zb_delta[1]) / ucmd->msec;

                if (zb_shoot) {
                    zb_target->osp_t020 = zb_distance;
                    return false;
                }

                if (zb_distance <= 0) {
                    zb_target->osp_t034[0]++;
                    zb_target->osp_t020 = 0;
                    zb_target->osp_t024 = level.framenum;
                    if (zb_target->osp_t034[0] >= 2) {
                        if (zb_distance <= 0)
                            sprintf(why, "r (%f)", zb_distance);
                        else
                            sprintf(why, "p (%f)", zb_distance);
                        OnBotDetection(ent, why);
                        return true;
                    }
                }
            }
        }
    }

    if (!zb_shoot)
        for (zb_count = 0; zb_count < 2; zb_count++)
            zb_target->osp_t028[zb_count] = ucmd->angles[zb_count];

    return false;
}

// gamex86.dll: 1004F692..1004F819
// gamei386.so: 000701B4..00070316
void OnBotDetection(edict_t *ent, char *why)
{
    int     tents[9] = { 1, 2, 3, 9, 12, 14, 17, 18, 20 };
    int     nrand;
    int     i;

    ent->client->resp.osp_r07c[0] = 1;
    ent->client->resp.score = -99;
    q2log_playerZBOT(ent, why);
    gi.bprintf(PRINT_HIGH, "%s was kicked for using a BOT!\n",
               ent->client->pers.netname);

    if (server_log) {
        OSP_getPlayerAddr(ent);
        OSP_logAdminLog("BotDetect: %s (%s) [%s]", ent->client->pers.netname,
                        why, ent->osp_e37c);
    }

    ent->movetype = MOVETYPE_NOCLIP;
    i = Q_rand() % 9;
    gi.WriteByte(tents[i]);
    nrand = Q_rand() % 3;
    for (i = 0; i < nrand; i++)
        gi.WriteByte(Q_rand() % 256);
    gi.unicast(ent, true);
    ent->client->osp_t034[0] = 0;
    gi.WriteByte(7);
    gi.unicast(ent, true);
    ClientDisconnect(ent);
}

// gamex86.dll: 1004F819..1004FA00
// gamei386.so: 00070318..00070500
void OSP_speedDetect(edict_t *ent)
{
    // FUNCTION-scope, although only the `else` uses it: real's PE gives it the
    // SHALLOWEST slot, and MSVC lays every nested block's locals out below all
    // the function-scope ones.
    int     when;

    gi.WriteByte(svc_stufftext);
    gi.WriteString("cmd _init_state $timescale\n");
    gi.unicast(ent, true);

    if (ent->client->pers.spectator >= 3) {
        // The temp-entity types the punishment picks from.  Declared here, not
        // at the top of the function.
        int     tents[9] = { 1, 2, 3, 9, 12, 14, 17, 18, 20 };
        int     num;
        int     i;

        gi.centerprintf(ent, "Speed cheating not allowed!\n");
        gi.bprintf(PRINT_HIGH, "%s was kicked for SPEED CHEATING!\n",
                   ent->client->pers.netname);

        if (server_log) {
            OSP_getPlayerAddr(ent);
            OSP_logAdminLog("SpeedDetect: %s [%f]", ent->client->pers.netname,
                            ent->client->pers.spectator);
        }

        ent->movetype = MOVETYPE_NOCLIP;
        i = Q_rand() % 9;
        gi.WriteByte(tents[i]);
        num = Q_rand() % 3;
        for (i = 0; i < num; i++)
            gi.WriteByte(Q_rand() % 256);
        gi.unicast(ent, true);
        ent->client->osp_t034[0] = 0;
        gi.WriteByte(7);
        gi.unicast(ent, true);
        ClientDisconnect(ent);
    } else {
        // The + 200 belongs to `when`'s own initialiser; the store adds
        // level.framenum to it separately.
        when = (int)((Q_rand() & 0x7fff) / 32767.0f * 30.0f) + 200;
        ent->client->resp.osp_r2b4 = level.framenum + when;
    }
}

// gamex86.dll: 1004FA00..1004FA60
// gamei386.so: 00070500..0007057C
void OSP_speedCheat_cmd(edict_t *ent)
{
    if (Q_atoi(gi.argv(1)) > 1) {
        ent->client->pers.spectator++;
        gi.dprintf("Speed > 1!!! (%d)\n", Q_atoi(gi.argv(1)));
    }
}
