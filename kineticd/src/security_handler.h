#ifndef KINETIC_SECURITY_HANDLER_H_
#define KINETIC_SECURITY_HANDLER_H_

#include <inttypes.h>
#include <string.h>
#include <cstdint>

#include "gmock/gmock.h"

#define RSAMAXMODULUSBITS  (2048)
#define RSA_2048_BYTES      256
#define RSAFLAGS_modulus          (1<<0)
#define RSAFLAGS_publicExponent   (1<<1)
#define RSAFLAGS_montN0           (1<<2)

#define EXPF0   ((uint) (3))

#define NUMDOUBLESIZED (10)
#define NUMFULLSIZED   (100)
#define NUMHALFSIZED   (30)
#define NUMFOURTHSIZED (30)

#define CBNFULLSIZE    (RSAMAXMODULUSBITS/(8*sizeof(DIGIT)))  // about 154 decimal digits
#define CBNDOUBLESIZE  (CBNFULLSIZE*2)
#define CBNHALFSIZE    (CBNFULLSIZE/2)                        // about 77 decimal digits
#define CBNFOURTHSIZE  (CBNFULLSIZE/4)

// Return codes, we can make this into an enum type
#define CBN_ERROR_NONE            (0)
#define CBN_ERROR                 (1)
#define CBN_ERROR_BUFFER_OVERFLOW (2)
#define CBN_CONTINUE              (3)

#define DIGIT2_ZERO     0LL
#define DIGITBITS      (8*sizeof(DIGIT))
#define DIGITMASK      ((DIGIT)(0xFFFFFFFF))
#define DIGITMSBIT     ((DIGIT)(0x80000000))

// Log base 2 of DIGITBITS for shifting bit counts.
#define DIGITLOGBITS   (5)
#define DIGITLOGMASK   (0x1F)

// Macros
#define M_MovePtrToDigits(digits, ptr) \
{ *((void **)(digits)) = (ptr); \
}

#define M_MoveDigitsToPtr(ptr, digits) \
{ (ptr) = *((DIGIT **)(digits)); }

#define M_StackOverflow(Session, Exit)

#define M_Memcpy(Dest, Source, Size) memcpy(Dest, Source, Size)

// (carry, sum) = a + m*b + carry
// All parameters are DIGIT values.
// Internally, this uses a DIGIT2 named temp2.
#define cbn_sum_mult(carry, sum, a, m, b) \
{ temp2 = ((DIGIT2)(a)) + (((DIGIT2)(m))*((DIGIT2)(b))) + ((DIGIT2)(carry));\
  sum = cbn_digit2_low(temp2);\
  carry = cbn_digit2_high(temp2);\
}

// (borrow, difference) = a - b - borrow
// All parameters are DIGIT values.
#define cbn_diff(borrow, difference, a, b) \
{ temp2 = (a);\
  temp2 = (temp2 - (b)) - (borrow);\
  difference = cbn_digit2_low(temp2);\
  borrow = cbn_digit2_high(temp2);\
  if (borrow != 0) borrow = 1; \
}

// (borrow, difference) = a - m*b - borrow
// All parameters are DIGIT values.
#define cbn_diff_mult(borrow, difference, a, m, b) \
{ temp2 = ((DIGIT2)(m)) * ((DIGIT2)(b)) + ((DIGIT2)(borrow));\
  borrow = cbn_digit2_high(temp2);\
  if (a < cbn_digit2_low(temp2)) ++borrow;\
  difference = (DIGIT)(a - cbn_digit2_low(temp2));\
}

// (carry, sum) = a + b + carry
// All parameters are DIGIT values.
// Internally, this uses a DIGIT2 named temp2.
#define cbn_sum(carry, sum, a, b) \
{ temp2 = ((DIGIT2)(a)) + ((DIGIT2)(b)) + ((DIGIT2)(carry));\
  sum = cbn_digit2_low(temp2);\
  carry = cbn_digit2_high(temp2);\
}

// Low digit of DIGIT2
#define cbn_digit2_low(a)  ((DIGIT)(a))
// High DIGIT of DIGIT2
#define cbn_digit2_high(a) ((DIGIT)(((DIGIT2)(a))>>(8*sizeof(DIGIT))))

namespace com {
namespace seagate {
namespace kinetic {

#if BUILD_FOR_ARM == 1
typedef unsigned long DIGIT; //NOLINT
#else
typedef unsigned int DIGIT;
#endif
typedef unsigned long long DIGIT2; //NOLINT
typedef unsigned char byte;

// Struct for LODHeaders of a LOD file
struct LODHeader {
    uint32_t signature;
    uint16_t block_point;
    uint8_t type;
    uint32_t input_size;
    uint32_t date;
    uint32_t file_size;
    uint16_t checksum;
    std::string header;
    std::string input;
};

// Struct use for calculations that requrie more than 4 bytes to compute
typedef struct CBN {
    DIGIT * digits;   // Ptr to LS digit or NULL
    byte negative;    // True if negative
    uint16_t width;   // Number of digits.
} CBN;

// Context struct for a given session
typedef struct CBNCTX {
    //  Head of linked list of free double-sized digit arrays
    DIGIT * headDoubleSized;
    //  Head of linked list of free full-sized digit arrays
    DIGIT * headFullSized;
    //  Head of linked list of free half-sized digit arrays
    DIGIT * headHalfSized;
    //  Head of linked list of free one fourth-sized digit arrays
    DIGIT * headFourthSized;
    DIGIT doubleSized[NUMDOUBLESIZED][CBNDOUBLESIZE + 2];
    DIGIT fullSized[NUMFULLSIZED][CBNFULLSIZE + 2];
    DIGIT halfSized[NUMHALFSIZED][CBNHALFSIZE + 2];
    DIGIT fourthSized[NUMFOURTHSIZED][CBNFOURTHSIZE + 2];
    uint initialized;
    uint8_t session;
} CBNCTX;

// Struct for rsa public key
typedef struct RSAPUB {
    uint16_t flags;
    CBN modulus;
    unsigned long long publicExponent; //NOLINT
    DIGIT montN0; // Montgomery word for N
} RSAPUB;

// Context for CbnModExp method calculation.
typedef struct CCTXMODEXP {
    DIGIT stage;
    uint16_t counter;
    CBN * a;
    CBN * b;
    CBN * n;
    DIGIT ni0;
    CBN am;
    CBN t;
    CBNCTX * ctx;
} CCTXMODEXP;

class SecurityHandlerInterface {
    public:
    virtual ~SecurityHandlerInterface() {}
    virtual bool VerifySecuritySignature(struct LODHeader *thumbprint_lodHeader,
        struct LODHeader *security_signature, struct LODHeader *security_info) = 0;
};

class SecurityHandler : public SecurityHandlerInterface {
    public:
    static const int LODHEADER_SIZE;
    static const uint8_t SHA2_PADDING_ID[];
    static const char KEY[];
    static const uint8_t KINETIC_MODULUS[];
    static const uint8_t KINETIC_MODULUS_KEY0[];
    static const uint8_t KINETIC_MODULUS_KEY1[];
    static const uint32_t KINETIC_PUBLIC_EXPONENT;
    static const uint8_t KATPkcsSignature[];
    static const uint8_t KATKeyModulus[];
    SecurityHandler();

    bool VerifySecuritySignature(struct LODHeader *thumbprint_lodHeader,
        struct LODHeader *security_signature, struct LODHeader *security_info);

    // Test PKCS implementation with a hardcode message
    bool KAT_PKCSVerify();

    private:
    bool SecuritySignaturePKCS(unsigned char* signature,
        unsigned char* encoded_message, const uint32_t encoded_message_length,
        const uint8_t * modulus);

    void ROM_PadPkcs21(byte *Data, uint32_t DataLen, byte *PaddedHash,
        uint32_t emLen);

    int RsaPublicRaw(CBN *num, RSAPUB *key, CBNCTX *ctx);

    void RsaReleasePublicKey(CBNCTX *CBNContext, uint8_t session, RSAPUB * key);

    int CbnInitializeContext(CBNCTX *CBNContext, uint8_t session, CBNCTX **pCtx);

    int CbnGetContext(CBNCTX *CBNContext, uint8_t session, CBNCTX **pCtx);

    int CbnFromBEBytes(CBN *out, byte *pIn, uint16_t nInLen, CBNCTX *ctx);

    int CbnGrabDigitsAndZero(CBN *bNum, uint16_t width, CBNCTX *ctx);

    int CbnGrabDigits(CBN *num, uint16_t width, CBNCTX *ctx);

    int CbnReleaseDigits(CBN * num, CBNCTX *ctx);

    int CbnNi0(DIGIT *ni0, CBN *n, CBNCTX *ctx);

    int CbnModMult(CBN *out, CBN *a, CBN *b, CBN *n, CBNCTX *ctx);

    int CbnUMult(CBN *out, CBN *a, CBN *b, CBNCTX *ctx);

    int CbnMod(CBN *out, CBN *a, CBN *n, CBNCTX *ctx);

    int CbnDivide(CBN *dividend1, CBN *divisor1, CBN *quotient, CBN *remainder, CBNCTX *ctx);

    int CbnCopy(CBN *out, CBN *a, CBNCTX *ctx);

    int CbnUCompare(CBN *a, CBN *b);

    void CbnUShiftLeft(DIGIT shiftSize, CBN *operand, DIGIT *opLen);

    int CbnSub(CBN *out, CBN *a, CBN *b, CBNCTX *ctx);

    void CbnEstimateQuotientDigit(DIGIT u_2, DIGIT u_1, DIGIT u_0,
        DIGIT v_1, DIGIT v_0, DIGIT *q);

    void CbnUShiftRight(DIGIT shiftSize, CBN *operand, DIGIT *opLen);

    int CbnToBEBytes(byte *out, uint16_t *pOutLen, CBN *in, CBNCTX *ctx);

    int CbnFourthFromDigit2(CBN * num, DIGIT2 value, CBNCTX *ctx);

    int CbnSizedFromDigit2(CBN * num, DIGIT2 value, CBNCTX * ctx, uint16_t width);

    int CbnModExp(CBN *out, CBN *a, CBN *b, CBN *n, DIGIT ni0, CBNCTX *ctx);

    int CbnModExpStart(CCTXMODEXP *cctx, CBN *a, CBN *b, CBN *n, DIGIT ni0, CBNCTX *ctx);

    int CbnModExpContinue(CCTXMODEXP *cctx, CBN *out);

    int CbnBitIsSet(CBN *a, uint16_t bitIndex);

    int CbnFromDigit2(CBN *num, DIGIT2 value, CBNCTX *ctx);

    int CbnMonRed(CBN *out, CBN *a, CBN *n, CBNCTX *ctx);

    int CbnMonPro(CBN *out, CBN *a, CBN *b, CBN *n, DIGIT ni0, CBNCTX *ctx);

    void MonProCore(int nSize, DIGIT ni0, DIGIT *tDigits, DIGIT *nDigits,
        DIGIT *aDigits, DIGIT *bDigits);

    DIGIT CbnNumSignificantDigits(CBN *bNum);

    void Borrow(DIGIT n);
};

class MockSecurityHandler : public SecurityHandlerInterface {
    public:
    MockSecurityHandler() {}
    MOCK_METHOD3(VerifySecuritySignature, bool(struct LODHeader *thumbprint_lodHeader,
        struct LODHeader *security_signature, struct LODHeader *security_info));
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_SECURITY_HANDLER_H_
