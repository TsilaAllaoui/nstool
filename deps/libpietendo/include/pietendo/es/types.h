	/**
	 * @file types.h
	 * @brief Declaration of common structs and data types for the ES library
	 * @author Jack (jakcron)
	 * @version 0.2
	 * @date 2022/09/22
	 **/
#pragma once
#include <tc/types.h>

	/**
	 * @namespace   pie::es::detail
	 * @brief       Namespace of the BroadON ES library common structs and data types
	 */
namespace pie { namespace es { namespace detail {

	/**
	 * @brief Template for defining padding in ES structures.
	 * 
	 * @tparam _size Padding size in bytes.
	 */
template <size_t _size>
using Padding = std::array<byte_t, _size>; 

#pragma pack(push,4)

static const size_t kAes128KeySize = 16;
using Aes128Key = std::array<uint8_t, kAes128KeySize>;

static const size_t kAes192KeySize = 24;
using Aes192Key = std::array<uint8_t, kAes192KeySize>;

static const size_t kAes256KeySize = 32;
using Aes256Key = std::array<uint8_t, kAes256KeySize>;

static const size_t kSha1Size = 20;
using Sha1Hash = std::array<uint8_t, kSha1Size>;
using Sha1Hmac = std::array<uint8_t, kSha1Size>;

static const size_t kSha2256Size = 32;
using Sha2256Hash = std::array<uint8_t, kSha2256Size>;
using Sha2256Hmac = std::array<uint8_t, kSha2256Size>;

static const size_t kRsaPublicExponentSize = 4;
using RsaPublicExponent = std::array<uint8_t, kRsaPublicExponentSize>;

static const size_t kRsa2048Size = 0x100;
using Rsa2048Integer = std::array<uint8_t, kRsa2048Size>;
struct Rsa2048PublicKey
{
	Rsa2048Integer m; /**< modulus */
	RsaPublicExponent e; /**< public_exponent */
};
struct Rsa2048PrivateKey
{
	Rsa2048Integer m; /**< modulus */
	Rsa2048Integer d; /**< private_exponent */
};
using Rsa2048Sig = Rsa2048Integer;

static const size_t kRsa4096Size = 0x200;
using Rsa4096Integer = std::array<uint8_t, kRsa4096Size>;
struct Rsa4096PublicKey
{
	Rsa4096Integer m; /**< modulus */
	RsaPublicExponent e; /**< public_exponent */
};
struct Rsa4096PrivateKey
{
	Rsa4096Integer m; /**< modulus */
	Rsa4096Integer d; /**< private_exponent */
};
using Rsa4096Sig = Rsa4096Integer;

static const size_t kEccB233Size = 0x1E; //  = (233 / 8) + ((233 % 8) ? 1 : 0)
using EccB233Integer = std::array<uint8_t, kEccB233Size>;
struct EccB233Point
{
	EccB233Integer x;
	EccB233Integer y;
};
using EccB233PrivateKey = EccB233Integer;
using EccB233PublicKey = EccB233Point;
struct EccB233Sig
{
	EccB233Integer r;
	EccB233Integer s;
};

#pragma pack(pop)

}}} // namespace pie::es::detail