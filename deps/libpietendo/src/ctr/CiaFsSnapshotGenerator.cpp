#include <pietendo/ctr/CiaFsSnapshotGenerator.h>
#include <tc/io/SubStream.h>
#include <tc/crypto/Sha2256Generator.h>

#include <pietendo/ctr/cia.h>
#include <pietendo/es/tmd.h>

#include <fmt/format.h>
#include <tc/cli/FormatUtil.h>


pie::ctr::CiaFsSnapshotGenerator::CiaFsSnapshotGenerator(const std::shared_ptr<tc::io::IStream>& stream) :
	FileSystemSnapshot()
{
	mBaseStream = stream;

	// validate stream properties
	if (mBaseStream == nullptr)
	{
		throw tc::ObjectDisposedException("pie::ctr::CiaFsSnapshotGenerator", "Failed to open input stream.");
	}
	if (mBaseStream->canRead() == false || mBaseStream->canSeek() == false)
	{
		throw tc::NotSupportedException("pie::ctr::CiaFsSnapshotGenerator", "Input stream requires read/seek permissions.");
	}

	// validate and read CIA header
	pie::ctr::CiaHeader hdr;
	if (mBaseStream->length() < sizeof(pie::ctr::CiaHeader))
	{
		throw tc::ArgumentOutOfRangeException("pie::ctr::CiaFsSnapshotGenerator", "Input stream is too small.");
	}
	mBaseStream->seek(0, tc::io::SeekOrigin::Begin);
	mBaseStream->read((byte_t*)(&hdr), sizeof(pie::ctr::CiaHeader));

	if (hdr.header_size.unwrap() != sizeof(pie::ctr::CiaHeader))
	{
		throw tc::ArgumentOutOfRangeException("pie::ctr::CiaFsSnapshotGenerator", "CIA header had unexpected size.");
	}
	if (hdr.format_version.unwrap() != pie::ctr::CiaHeader::FormatVersion_Default)
	{
		throw tc::ArgumentOutOfRangeException("pie::ctr::CiaFsSnapshotGenerator", "CIA header had unexpected format version.");
	}
	if (hdr.type.unwrap() != pie::ctr::CiaHeader::Type_Normal)
	{
		throw tc::ArgumentOutOfRangeException("pie::ctr::CiaFsSnapshotGenerator", "CIA header had unexpected type.");
	}

	// parse header sections

	enum CiaSectionIndex
	{
		Header,
		Certificate,
		Tik,
		Tmd,
		Content,
		Footer
	};

	struct CiaSectionInformation
	{
		int64_t offset;
		int64_t size;
	};
	
	std::array<CiaSectionInformation, 6> section;

	section[Header].size = hdr.header_size.unwrap();
	section[Header].offset = 0;
	section[Certificate].size = hdr.certificate_size.unwrap();
	section[Certificate].offset = align<int64_t>(section[Header].offset + section[Header].size, CiaHeader::kCiaSectionAlignment);
	section[Tik].size = hdr.ticket_size.unwrap();
	section[Tik].offset = align<int64_t>(section[Certificate].offset + section[Certificate].size, CiaHeader::kCiaSectionAlignment);
	section[Tmd].size = hdr.tmd_size.unwrap();
	section[Tmd].offset = align<int64_t>(section[Tik].offset + section[Tik].size, CiaHeader::kCiaSectionAlignment);
	if (uint64_t(std::numeric_limits<int64_t>::max()) < hdr.content_size.unwrap())
	{
		throw tc::ArgumentOutOfRangeException("pie::ctr::CiaFsSnapshotGenerator", "CIA header had too large content section.");
	}
	section[Content].size = hdr.content_size.unwrap();
	section[Content].offset = align<int64_t>(section[Tmd].offset + section[Tmd].size, CiaHeader::kCiaSectionAlignment);
	section[Footer].size = hdr.footer_size.unwrap();
	section[Footer].offset = align<int64_t>(section[Content].offset + section[Content].size, CiaHeader::kCiaSectionAlignment);

	int64_t total_size = section[Footer].offset + section[Footer].size;
	if (stream->length() < total_size)
	{
		throw tc::ArgumentOutOfRangeException("pie::ctr::CiaFsSnapshotGenerator", "Input stream is too small, given calculated CIA section geometry.");
	}

	// Add root directory
	dir_entries.push_back(DirEntry());
	mCurDir = dir_entries.size() - 1;
	dir_entries[mCurDir].dir_listing.abs_path = tc::io::Path("/");
	dir_entry_path_map[tc::io::Path("/")] = mCurDir;

	// populate virtual filesystem
	if (section[Certificate].size != 0)
	{
		addFile("cert", section[Certificate].offset, section[Certificate].size);
	}
	if (section[Tik].size != 0)
	{
		addFile("tik", section[Tik].offset, section[Tik].size);
	}
	if (section[Tmd].size != 0)
	{
		addFile("tmd", section[Tmd].offset, section[Tmd].size);
	}
	if (section[Footer].size != 0)
	{
		addFile("footer", section[Footer].offset, section[Footer].size);	
	}
	if (section[Content].size != 0)
	{
		// if TMD exists, add the (included) <cid>.app files to the file system
		if (section[Tmd].size != 0)
		{
			// test size before reading
			if (section[Tmd].size < (sizeof(pie::es::ESV1TitleMeta) + sizeof(pie::es::ESV1ContentMeta)))
			{
				throw tc::ArgumentOutOfRangeException("pie::ctr::CiaFsSnapshotGenerator", "TMD was too small.");
			}

			// import TMD v1 data
			if (tc::is_int64_t_too_large_for_size_t(section[Tmd].size))
			{
				throw tc::ArgumentOutOfRangeException("pie::ctr::CiaFsSnapshotGenerator", "TMD was too large to read into memory.");
			}
			tc::ByteData tmd_data = tc::ByteData(static_cast<size_t>(section[Tmd].size));
			mBaseStream->seek(section[Tmd].offset, tc::io::SeekOrigin::Begin);
			if (mBaseStream->read(tmd_data.data(), tmd_data.size()) < tmd_data.size())
			{
				throw tc::ArgumentOutOfRangeException("pie::ctr::CiaFsSnapshotGenerator", "TMD had unexpected size after reading.");
			}


			// get pointer
			pie::es::ESV1TitleMeta* tmd = (pie::es::ESV1TitleMeta*)tmd_data.data();
			if (tmd->sig.sigType.unwrap() != pie::es::ESSigType_RSA2048_SHA2256)
			{
				throw tc::ArgumentOutOfRangeException("pie::ctr::CiaFsSnapshotGenerator", "TMD had unexpected signature type.");
			}
			if (tmd->head.version != 1)
			{
				throw tc::ArgumentOutOfRangeException("pie::ctr::CiaFsSnapshotGenerator", "TMD had unexpected format version.");
			}

			// hash for staged validiation
			std::array<byte_t, tc::crypto::Sha2256Generator::kHashSize> hash;

			// TODO validate signature
			/*
			tc::crypto::GenerateSha2256Hash(hash.data(), (byte_t*)&tmd->sig.issuer, (size_t)((byte_t*)&tmd->v1Head.cmdGroups - (byte_t*)&tmd->sig.issuer));
			if (tc::crypto::VerifyRsa2048Pkcs1Sha2256(tmd->sig.sig.data(), hash.data(), <rsa key here>))
			{
				throw tc::ArgumentOutOfRangeException("pie::ctr::CiaFsMetaGenerator", "TMD had invalid signature.");
			}
			*/

			tc::crypto::GenerateSha2256Hash(hash.data(), (byte_t*)&tmd->v1Head.cmdGroups, sizeof(tmd->v1Head.cmdGroups));
			if (memcmp(hash.data(), tmd->v1Head.hash.data(), hash.size()) != 0)
			{
				throw tc::ArgumentOutOfRangeException("pie::ctr::CiaFsSnapshotGenerator", "TMD had invalid CMD group hash.");
			}

			/*
			std::cout << "[Tmd]" << std::endl;
			std::cout << "  > Issuer:            " << tmd->sig.issuer.data() << std::endl;
			std::cout << "  > Version:           " << (int)tmd->head.version << std::endl;
			std::cout << "  > CACrl Version:     " << (int)tmd->head.caCrlVersion << std::endl;
			std::cout << "  > SignerCrl Version: " << (int)tmd->head.signerCrlVersion << std::endl;
			std::cout << "  > TitleId:           " << std::hex << std::setfill('0') << std::setw(16) << tmd->head.titleId.unwrap() << std::endl;
			*/

			size_t cmd_table_num = tmd->v1Head.cmdGroups[0].nCmds.unwrap();
			if (tmd_data.size() != (sizeof(pie::es::ESV1TitleMeta) + (cmd_table_num * sizeof(pie::es::ESV1ContentMeta))))
			{
				throw tc::ArgumentOutOfRangeException("pie::ctr::CiaFsSnapshotGenerator", "TMD had unexpected size");
			}

			tc::crypto::GenerateSha2256Hash(hash.data(), (byte_t*)&tmd->contents, cmd_table_num * sizeof(pie::es::ESV1ContentMeta));
			if (memcmp(hash.data(), tmd->v1Head.cmdGroups[0].groupHash.data(), hash.size()) != 0)
			{
				throw tc::ArgumentOutOfRangeException("pie::ctr::CiaFsSnapshotGenerator", "TMD had invalid CMD group[0] hash.");
			}
	
			int64_t content_offset = 0;
			for (size_t i = 0; i < cmd_table_num; i++)
			{
				/*
				std::cout << "Found content ";
				std::cout << "cid(" << std::hex << std::setfill('0') << std::setw(8) << tmd->contents[i].cid.unwrap() << "), ";
				std::cout << "index(" << std::hex << std::setfill('0') << std::setw(4) << tmd->contents[i].index.unwrap() << "), ";
				std::cout << "type(" << std::hex << std::setfill('0') << std::setw(4) << tmd->contents[i].type.unwrap() << "), ";
				std::cout << "size(" << std::hex << std::setfill('0') << std::setw(16) << tmd->contents[i].size.unwrap() << "), ";
				std::cout << "" << std::endl;
				*/
				if (hdr.content_bitarray.test(tmd->contents[i].index.unwrap()))				
				{
					std::string content_file_name = fmt::format("{:08x}.app", i, tmd->contents[i].cid.unwrap());

					int64_t content_size = align<int64_t>(tmd->contents[i].size.unwrap(), CiaHeader::kCiaContentAlignment);

					addFile(content_file_name, section[Content].offset + content_offset, content_size);
					
					content_offset += content_size;
				}
			}
		}
		// otherwise include the content blob as one file (00000000.app)
		else
		{
			addFile("00000000.app", section[Content].offset, section[Content].size);
		}
	}
}

void pie::ctr::CiaFsSnapshotGenerator::addFile(const std::string& name, int64_t offset, int64_t size)
{
	FileEntry tmp;

	tmp.stream = std::make_shared<tc::io::SubStream>(tc::io::SubStream(mBaseStream, offset, size));

	// create file path
	tc::io::Path file_path = dir_entries[mCurDir].dir_listing.abs_path + std::string(name);

	// add file entry to list
	file_entries.push_back(std::move(tmp));

	// add file entry to map
	file_entry_path_map[file_path] = file_entries.size()-1;

	// add name to parent directory listing
	dir_entries[mCurDir].dir_listing.file_list.push_back(name);
}