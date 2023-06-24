	/**
	 * @file exefs.h
	 * @brief Declaration of ExeFs structs and enums for the CTR library.
	 * @author Jack (jakcron)
	 * @version 0.1
	 * @date 2022/06/27
	 **/
#pragma once
#include <tc/types.h>

namespace pie { namespace ctr {

#pragma pack(push,1)

struct ExeFsHeader
{
	static const size_t kFileNum = 8;
	static const size_t kExeFsSectionAlignSize = 0x200;

	struct FileEntry
	{
		tc::bn::string<8> name;
		tc::bn::le32<uint32_t> offset;
		tc::bn::le32<uint32_t> size;
	};

	using FileHash = std::array<byte_t, 0x20>;

	std::array<FileEntry, kFileNum> file_table;
	tc::bn::pad<0x80> reserved;
	std::array<FileHash, kFileNum> hash_table;

	// inline method to get pointer to file hash, since the ordering is unintuitive
	FileHash* getFileHash(size_t index) { return (index < kFileNum) ? &hash_table[kFileNum - 1 - index] : nullptr; }
};
static_assert(sizeof(ExeFsHeader) == 0x200, "ExeFsHeader had invalid size");

#pragma pack(pop)

}} // namespace pie::ctr