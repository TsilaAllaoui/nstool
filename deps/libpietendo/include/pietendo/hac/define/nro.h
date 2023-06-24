	/**
	 * @file nro.h
	 * @brief Declaration of NRO0 structs and constants for the HAC library.
	 * @author Jack (jakcron)
	 * @version 0.1
	 * @date 2022/06/28
	 **/
#pragma once
#include <pietendo/hac/define/types.h>

namespace pie { namespace hac {

namespace nro
{
	static const uint32_t kNroStructMagic = tc::bn::make_struct_magic_uint32("NRO0");
	static const uint32_t kDefaultFormatVersion = 0;
	static const size_t kRoCrtSize = 8;
	static const size_t kModuleIdSize = 32;

	static const uint64_t kNroHomebrewStructMagic = tc::bn::make_struct_magic_uint64("HOMEBREW");
}

#pragma pack(push,1)

struct sNroHeader
{
	struct sRoCrt
	{
		tc::bn::le32<uint32_t> entry_point_insn;
		tc::bn::le32<uint32_t> mod_offset;
	};

	struct sSection
	{
		tc::bn::le32<uint32_t> memory_offset;
		tc::bn::le32<uint32_t> size;
	};

	sRoCrt ro_crt;
	std::array<byte_t, 0x8> reserved_0;
	tc::bn::le32<uint32_t> st_magic;
	tc::bn::le32<uint32_t> format_version;
	tc::bn::le32<uint32_t> size;
	tc::bn::le32<uint32_t> flags;
	sSection text;
	sSection ro;
	sSection data;
	tc::bn::le32<uint32_t> bss_size;
	std::array<byte_t, 0x4> reserved_1;
	detail::module_id_t module_id;
	std::array<byte_t, 0x8> reserved_2;
	sSection embedded;
	sSection dyn_str;
	sSection dyn_sym;
};
static_assert(sizeof(sNroHeader) == 0x80, "sNroHeader size.");

#pragma pack(pop)

}} // namespace pie::hac