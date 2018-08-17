#ifndef OTBR_BIT_EXTRACTION_HPP_
#define OTBR_BIT_EXTRACTION_HPP_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace ot {
namespace BorderRouter {

template <bool> struct Condition;
template <size_t length, typename = Condition<true> > class BitTrait;

template <size_t length> class BitTrait<length, Condition<(length <= 8)> >
{
public:
    typedef uint8_t ValueType;
};

template <size_t length> class BitTrait<length, Condition<(8 < length && length <= 16)> >
{
public:
    typedef uint16_t ValueType;
};

template <size_t length> class BitTrait<length, Condition<(16 < length && length <= 32)> >
{
public:
    typedef uint32_t ValueType;
};

template <size_t length> class BitTrait<length, Condition<(32 < length && length <= 64)> >
{
public:
    typedef uint64_t ValueType;
};

template <size_t end, typename = Condition<true> > class Extractor
{
public:
    static typename BitTrait<end>::ValueType ExtractBitsFromZero(const uint8_t *aBuf);
};

template <size_t end> class Extractor<end, Condition<(end < 8)> >
{
public:
    static typename BitTrait<end>::ValueType ExtractBitsFromZero(const uint8_t *aBuf) { return (aBuf[0] >> (8 - end)); }
};

template <size_t end> class Extractor<end, Condition<(end >= 8)> >
{
public:
    static typename BitTrait<end>::ValueType ExtractBitsFromZero(const uint8_t *aBuf)
    {
        typename BitTrait<end>::ValueType prefix = aBuf[0];
        typename BitTrait<end>::ValueType suffix = Extractor<end - 8>::ExtractBitsFromZero(aBuf + 1);

        return (prefix << (end - 8)) | suffix;
    }
};

/**
 * This method extracts bits within range [begin, end) from buffer
 * to varaible with proper bit width, in original byte order
 *
 * @param[in]   begin         Template parameter, start bit position, need to be compile-time constant
 * @param[in]   end           Template parameter, end bit position, need to be compile-time constant
 * @param[in]   aBuf          Buffer to extract bit from
 *
 * @returns value extracted from bit range, type will be the smallest unsigned type to fit the bit width
 *
 */
template <size_t begin, size_t end> typename BitTrait<end - begin>::ValueType ExtractBits(const uint8_t *aBuf)
{
    static const uint8_t masks[] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};

#define OFFSET_BYTE (begin / 8)
#define OFFSET_BIT (OFFSET_BYTE * 8)

#define BIT_START (begin - OFFSET_BIT)
#define BIT_END (end - OFFSET_BIT)
#define PREFIX_BIT_END (BIT_END > 8 ? 8 : BIT_END)
#define PREFIX_LENGTH (PREFIX_BIT_END - BIT_START)
#define SUFFIX_LENGTH (BIT_END - PREFIX_BIT_END)

    typename BitTrait<end - begin>::ValueType trailing =
        Extractor<SUFFIX_LENGTH>::ExtractBitsFromZero(aBuf + OFFSET_BYTE + 1);

    typename BitTrait<end - begin>::ValueType prefix =
        (aBuf[OFFSET_BYTE] >> (8 - PREFIX_BIT_END)) & masks[PREFIX_LENGTH];

    return (prefix << SUFFIX_LENGTH) | trailing;

#undef OFFSET_BYTE
#undef OFFSET_BIT
#undef BIT_START
#undef BIT_END
#undef PREFIX_BIT_END
#undef PREFIX_LENGTH
#undef SUFFIX_LENGTH
}

} // namespace BorderRouter
} // namespace ot

#endif
