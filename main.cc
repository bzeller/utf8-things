#include <cstdint>
#include <type_traits>
#include <optional>
#include <string_view>
#include <string>

// convert a half byte in hex notation into its value representation
// e.g. A becomes 10
inline constexpr char hexCharToByte( char c ){
    return (((c)>='0' && (c)<='9') ? ((c)-'0')
                                   : ((c)>='a' && (c)<='f') ? ((c)-('a'-10))
                                   : ((c)>='A' && (c)<='F') ? ((c)-('A'-10))
                                   : -1);
  }

// UTF-8 Byte prefixes:
constexpr char Utf8ContPrefix       = 0x80;  // 0b10000000  continuation byte
constexpr char Utf8LatinPrefix      = 0xC0;  // 0b11000000
constexpr char Utf8MultiLingPrefix  = 0xE0;  // 0b11100000
constexpr char Utf8ExtendedPrefix   = 0xF0;  // 0b11110000

// in which code plane is a given codepoint. The codepoint is the unicode int32_t representation of the UTF8 character
enum class Utf8CPType {
    Ascii,
    Latin,
    MultiLingual,
    Extended,
    Invalid
};

constexpr Utf8CPType utf8CodePointType( int32_t codepoint )
{
    if ( codepoint >= 0x00000       && codepoint <= 0x0007F )
        return Utf8CPType::Ascii;
    else if ( codepoint >= 0x00080  && codepoint <= 0x007FF )
        return Utf8CPType::Latin;
    // the definition of UTF-8 prohibits encoding character numbers between U+D800 and U+DFFF ( UTF-16 surrogate pair )
    else if ( (codepoint >= 0x00800  && codepoint < 0x0D800 ) || (codepoint > 0x0DFFF && codepoint <= 0x0FFFF)   )
        return Utf8CPType::MultiLingual;
    else if ( codepoint >= 0x010000 && codepoint <= 0x10FFFF )
        return Utf8CPType::Extended;

    return Utf8CPType::Invalid;
}

/**
 * Converts a value in hex notation ( e.g 0048 ) into its integral value,
 * or a empty optional if the hexStr contains invalid characters or is too long for the target
 * type
 */
template<typename T=uint32_t>
constexpr std::enable_if_t< std::is_integral_v<T>, std::optional<T> > hexCharToValue( std::string_view hexChar ) {

    // the max length of the hexChar array for the target type
    constexpr auto l = sizeof(T) * 2;
    if ( hexChar.length() == 0 || hexChar.length() % 2 || hexChar.length() > l )
    return {};

    int cp = 0;
    int shift = 0;
    for ( const auto c : hexChar ) {
    const char b = hexCharToByte(c);
    if ( b < 0 )
        return {};
    cp = (cp << shift) | b;
    shift = 4;
    }

    return cp;
}


std::string hexCodepointToUtf8String(std::string_view hexChar)
{
    auto codepoint = hexCharToValue<int32_t>(hexChar);
        if ( !codepoint )
            return {};

    const auto cpType = utf8CodePointType(*codepoint);

    /*
    Utf8 encodes with one to four bytes per utf-8 character.

    When represented as its codepoint the bits of the intial value are
    mapped like this:  U+(uv)(wx)(yz), where each position (u-z) represents
    4 bits. ( 0 - F ). The value itself would already fit into 3 bytes, but
    since Utf8 uses prefixes to specify in which plane the character is and
    if its a continuation or not 4 bytes is needed:

    First code point  | Last code point | Byte 1   | Byte 2   | Byte 3   |  Byte 4  |
    Byte 4 U+0000     | U+007F          | 0yyyzzzz |          |          |          |
    U+0080            | U+07FF          | 110xxxyy | 10yyzzzz |          |          |
    U+0800            | U+FFFF          | 1110wwww | 10xxxxyy | 10yyzzzz |          |
    U+010000          | U+10FFFF        | 11110uvv | 10vvwwww | 10xxxxyy | 10yyzzzz |

    The codepoint in binary would be: 00000000 000uvvvv wwwwxxxx yyyyzzzz

    When represented as a code point in hex notation, 2 character always map
    into one 8bit value, just like converting any hexstring into its binary
    representation ( e.g. "0F" -> 0x0F == 0b1111) the results bits are then
    distributed into the 1 to 4 bytes as shown in the table above

    Combined the chars look like:
    Plane 1 :                               0yyyzzzz
    Plane 2 :                      110xxxyy 10yyzzzz
    Plane 3 :             1110wwww 10xxxxyy 10yyzzzz
    Plane 4 :    11110uvv 10vvwwww 10xxxxyy 10yyzzzz

    @TODO we could calculate the bytes directly from the codepoint value instead of
            using seperate hexchars.. but we need to be careful with endianess in that case and use 32bit masks
    */

    std::string res;
    switch (cpType) {
        case Utf8CPType::Ascii: {
            // one byte , easy
            res.push_back((char)*codepoint);
            break;
        }
        case Utf8CPType::Latin: {
            // two bytes representation
            // hexstr has length 4, first byte is always 0 so we can ignore it
            // we need 2 bytes to represent the unicode char
            // str will look like:   Byte1, Byte2
            // Byte1 is: 110xxxyy
            // Byte2 is: 10yyzzzz
            char c1 = hexCharToByte(hexChar[1]); // we need the low 3 bits (x) here
            char c2 = hexCharToByte(hexChar[2]); // we need the low 4 bits (y) here
            char c3 = hexCharToByte(hexChar[3]); // we need the low 4 bits (z) here

            res.push_back( char(Utf8LatinPrefix | (c1 & 0b00000111) << 2 | (c2 & 0b00001100) >> 2) );
            res.push_back( char(Utf8ContPrefix  | (c2 & 0b00000011) << 4 | (c3 & 0b00001111)) );
            break;
        }
        case Utf8CPType::MultiLingual: {
            // three bytes representation
            // hexstr has length 4
            // we need 3 bytes to represent the unicode char
            // str will look like:   Byte1, Byte2, Byte3
            // Byte1 is: 1110wwww
            // Byte2 is: 10xxxxyy
            // Byte3 is: 10yyzzzz
            char c0 = hexCharToByte(hexChar[0]); // we need the low 4 bits (w) here
            char c1 = hexCharToByte(hexChar[1]); // we need the low 4 bits (x) here
            char c2 = hexCharToByte(hexChar[2]); // we need the low 4 bits (y) here
            char c3 = hexCharToByte(hexChar[3]); // we need the low 4 bits (z) here
            res.push_back(char(Utf8MultiLingPrefix | (c0 & 0b00001111)));
            res.push_back(char(Utf8ContPrefix | ((c1 & 0b00001111) << 2) | ((c2 & 0b00001100) >> 2)));
            res.push_back(char(Utf8ContPrefix | ((c2 & 0b00000011) << 4) |  (c3 & 0b00001111)));
            break;
        }
        case Utf8CPType::Extended: {
            // four bytes representation
            // hexstr has length 5
            // we need 4 bytes to represent the unicode char
            // str will look like:   Byte1, Byte2, Byte3
            // Byte1 is: 11110uvv
            // Byte2 is: 10vvwwww
            // Byte3 is: 10xxxxyy
            // Byte4 is: 10yyzzzz
            char c0 = hexCharToByte(hexChar[0]); // we need the low 1 bits (u) here
            char c1 = hexCharToByte(hexChar[1]); // we need the low 4 bits (v) here
            char c2 = hexCharToByte(hexChar[2]); // we need the low 4 bits (w) here
            char c3 = hexCharToByte(hexChar[3]); // we need the low 4 bits (x) here
            char c4 = hexCharToByte(hexChar[4]); // we need the low 4 bits (y) here
            char c5 = hexCharToByte(hexChar[5]); // we need the low 4 bits (z) here
            res.push_back(char(Utf8ExtendedPrefix | ((c0 & 0b00000001) << 2)) | ((c1 & 0b00001100) >> 2));
            res.push_back(char(Utf8ContPrefix | ((c1 & 0b00000011) << 4) | (c2 & 0b00001111)));
            res.push_back(char(Utf8ContPrefix | ((c3 & 0b00001111) << 2) | ((c4 & 0b00001100) >> 2)));
            res.push_back(char(Utf8ContPrefix | ((c4 & 0b00000011) << 4) | (c5 & 0b00001111)));
            break;
        }
        case Utf8CPType::Invalid:
            return {};
            break;
    }
    return res;
}


int main( int argc, const char *argv[] )
{
    return 0;
}
