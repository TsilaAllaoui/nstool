#include "FsProcess.h"
#include "util.h"

#include <memory>
#include <tc/io/FileNotFoundException.h>
#include <tc/io/DirectoryNotFoundException.h>

nstool::FsProcess::FsProcess() :
	mModuleLabel("nstool::FsProcess"),
	mInputFs(),
	mFsFormatName(),
	mShowFsInfo(false),
	mProperties(),
	mShowFsTree(false),
	mFsRootLabel(),
	mExtractJobs(),
	mDataCache(0x10000)
{

}

void nstool::FsProcess::process()
{
	if (mInputFs == nullptr)
	{
		throw tc::InvalidOperationException(mModuleLabel, "No input filesystem");
	}

	/*if (mShowFsInfo)
	{
		fmt::print("[{:s}]\n", mFsFormatName.isSet() ? mFsFormatName.get() : "FileSystem/Info");
		for (auto itr = mProperties.begin(); itr != mProperties.end(); itr++)
		{
			fmt::print("  {:s}\n", *itr);
		}
	}*/  

	/*if (mShowFsTree)
	{
		printFs();
	}*/

	if (mExtractJobs.empty() == false)
	{
		extractFs();
	}
}

void nstool::FsProcess::setInputFileSystem(const std::shared_ptr<tc::io::IFileSystem>& input_fs)
{
	mInputFs = input_fs;
}

void nstool::FsProcess::setFsFormatName(const std::string& fs_format_name)
{
	mFsFormatName = fs_format_name;
}

void nstool::FsProcess::setShowFsInfo(bool show_fs_info)
{
	mShowFsInfo = show_fs_info;
}

void nstool::FsProcess::setFsProperties(const std::vector<std::string>& properties)
{
	mProperties = properties;
}

void nstool::FsProcess::setShowFsTree(bool show_fs_tree)
{
	mShowFsTree = show_fs_tree;
}

void nstool::FsProcess::setFsRootLabel(const std::string& root_label)
{
	mFsRootLabel = root_label;
}

void nstool::FsProcess::setExtractJobs(const std::vector<nstool::ExtractJob>& extract_jobs)
{
	mExtractJobs = extract_jobs;
}

void nstool::FsProcess::printFs()
{
	fmt::print("[{:s}/Tree]\n", (mFsFormatName.isSet() ? mFsFormatName.get() : "FileSystem"));
	visitDir(tc::io::Path("/"), tc::io::Path("/"), false, true);
}

void nstool::FsProcess::extractFs()
{
	fmt::print("[{:s}/Extract]\n", (mFsFormatName.isSet() ? mFsFormatName.get() : "FileSystem"));

	for (auto itr = mExtractJobs.begin(); itr != mExtractJobs.end(); itr++)
	{
		// check if root path (legacy case)
		if (itr->virtual_path == tc::io::Path("/"))
		{
			visitDir(tc::io::Path("/"), itr->extract_path, true, false);

			//fmt::print("Root Dir Virtual Path: \"{:s}\"\n", itr->virtual_path.to_string());

			// root directory extract successful, continue to next job
			continue;
		}

		// otherwise determine if this is a file or subdirectory
		try {
			std::shared_ptr<tc::io::IStream> file_stream;

			// this will throw tc::io::FileNotFoundException if the virtual path doesn't point to a file
			mInputFs->openFile(itr->virtual_path, tc::io::FileMode::Open, tc::io::FileAccess::Read, file_stream);

			//fmt::print("Valid File Path: \"{:s}\"\n", itr->virtual_path.to_string());

			// the output path for this file will depend on the user specified extract path
			std::shared_ptr<tc::io::IFileSystem> local_fs = std::make_shared<tc::io::LocalFileSystem>(tc::io::LocalFileSystem());

			// case: the extract_path is a valid path to an existing directory
			// behaviour: extract the file, preserving the original filename, to the specified directory
			// method: try getDirectoryListing(itr->extract_path), if this is does not throw, then we can be sure this is a valid path to a directory, file_extract_path = itr->extract_path + itr->virtual_path.back()

			try {
				tc::io::sDirectoryListing dir_listing;
				//local_fs->getDirectoryListing(itr->extract_path, dir_listing);

				tc::io::Path file_extract_path = itr->extract_path + itr->virtual_path.back();

				fmt::print("Saving {:s}...\n", file_extract_path.to_string());

				writeStreamToFile(file_stream, itr->extract_path + itr->virtual_path.back(), mDataCache);

				continue;

			}
			catch (tc::io::DirectoryNotFoundException&) {
				// acceptable exception, just means directory didn't exist
			}
			catch (tc::UnauthorisedAccessException&) {
				// acceptable exception, just means directory didn't exist
			}

			// case: the extract_path up until the last element is a valid path to an existing directory, but the full path specifies neither a directory or a file
			// behaviour: treat extract_path as the intended location to write the extracted file (the original filename is not preserved, instead specified by the user in the final element of the extract path)
			// method: since this checks n-1 elements, it implies a path with more than one element, so that must be accounted for, as relative paths are valid and single element paths aren't always root

			try {
				// get path to parent directory
				tc::io::Path parent_dir_path = itr->extract_path;

				// replace final path element with the current directory alias 
				parent_dir_path.pop_back(); // remove filename
				parent_dir_path.push_back("."); // replace with the current dir name alias

				// test parent directory exists
				tc::io::sDirectoryListing dir_listing;
				//local_fs->getDirectoryListing(parent_dir_path, dir_listing);

				fmt::print("Saving {:s} as {:s}...\n", itr->virtual_path.to_string(), itr->extract_path.to_string());

				writeStreamToFile(file_stream, itr->extract_path, mDataCache);

				continue;
			}
			catch (tc::io::DirectoryNotFoundException&) {
				// acceptable exception, just means the parent directory didn't exist
			}
			catch (tc::UnauthorisedAccessException&) {
				// acceptable exception, just means directory didn't exist
			}


			// extract path could not be determined, inform the user and skip this job
			fmt::print("[WARNING] Extract path was invalid, and was skipped: {:s}\n", itr->extract_path.to_string());
			continue;
		}
		catch (tc::io::FileNotFoundException&) {
			// acceptable exception, just means file didn't exist
		}

		// not a file, attempt to process this as a directory
		try {
			tc::io::sDirectoryListing dir_listing;
			mInputFs->getDirectoryListing(itr->virtual_path, dir_listing);

			visitDir(itr->virtual_path, itr->extract_path, true, false);

			//fmt::print("Valid Directory Path: \"{:s}\"\n", itr->virtual_path.to_string());

			// directory extract successful, continue to next job
			continue;

		}
		catch (tc::io::DirectoryNotFoundException&) {
			// acceptable exception, just means directory didn't exist
		}

		fmt::print("[WARNING] Failed to extract virtual path: \"{:s}\"\n", itr->virtual_path.to_string());
	}

}

void nstool::FsProcess::extractIcons(const tc::io::Path& v_path, const tc::io::Path& l_path, bool extract_fs, bool print_fs)
{
	tc::io::LocalFileSystem local_fs;

	// get listing for directory
	tc::io::sDirectoryListing info;
	mInputFs->getDirectoryListing(v_path, info);

	if (extract_fs)
	{
		// create local dir
		local_fs.createDirectory(l_path);
	}

	// iterate thru child files
	size_t cache_read_len;
	tc::io::Path out_path;
	std::shared_ptr<tc::io::IStream> in_stream;
	std::shared_ptr<tc::io::IStream> out_stream;
	for (auto itr = info.file_list.begin(); itr != info.file_list.end(); itr++)
	{
		if (extract_fs && (*itr).find(".dat"))
		{
			// build out path
			out_path = l_path + *itr;

			// begin export
			mInputFs->openFile(v_path + *itr + std::string(".png"), tc::io::FileMode::Open, tc::io::FileAccess::Read, in_stream);
			local_fs.openFile(out_path, tc::io::FileMode::OpenOrCreate, tc::io::FileAccess::Write, out_stream);

			in_stream->seek(0, tc::io::SeekOrigin::Begin);
			out_stream->seek(0, tc::io::SeekOrigin::Begin);
			for (int64_t remaining_data = in_stream->length(); remaining_data > 0;)
			{
				cache_read_len = in_stream->read(mDataCache.data(), mDataCache.size());
				if (cache_read_len == 0)
				{
					throw tc::io::IOException(mModuleLabel, fmt::format("Failed to read from {:s}file.", (mFsFormatName.isSet() ? (mFsFormatName.get() + " ") : "")));
				}

				out_stream->write(mDataCache.data(), cache_read_len);

				remaining_data -= int64_t(cache_read_len);
			}
		}
	}
}

void nstool::FsProcess::visitDir(const tc::io::Path& v_path, const tc::io::Path& l_path, bool extract_fs, bool print_fs)
{
	tc::io::LocalFileSystem local_fs;

	// get listing for directory
	tc::io::sDirectoryListing info;
	mInputFs->getDirectoryListing(v_path, info);

	if (print_fs)
	{
		for (size_t i = 0; i < v_path.size(); i++)
			fmt::print(" ");

		fmt::print("{:s}/\n", ((v_path.size() == 1) ? (mFsRootLabel.isSet() ? (mFsRootLabel.get() + ":") : "Root:") : v_path.back()));
	}
	if (extract_fs)
	{
		// create local dir
		local_fs.createDirectory(l_path);
	}

	// iterate thru child files
	size_t cache_read_len;
	tc::io::Path out_path;
	std::shared_ptr<tc::io::IStream> in_stream;
	std::shared_ptr<tc::io::IStream> out_stream;
	for (auto itr = info.file_list.begin(); itr != info.file_list.end(); itr++)
	{
		if (print_fs)
		{
			for (size_t i = 0; i < v_path.size(); i++)
				fmt::print(" ");
			fmt::print(" {:s}\n", *itr);
		}
		if (extract_fs)
		{
			// build out path
			out_path = l_path + *itr;

			fmt::print("Saving {:s}...\n", out_path.to_string());

			// begin export
			mInputFs->openFile(v_path + *itr, tc::io::FileMode::Open, tc::io::FileAccess::Read, in_stream);
			local_fs.openFile(out_path, tc::io::FileMode::OpenOrCreate, tc::io::FileAccess::Write, out_stream);

			in_stream->seek(0, tc::io::SeekOrigin::Begin);
			out_stream->seek(0, tc::io::SeekOrigin::Begin);
			for (int64_t remaining_data = in_stream->length(); remaining_data > 0;)
			{
				cache_read_len = in_stream->read(mDataCache.data(), mDataCache.size());
				if (cache_read_len == 0)
				{
					throw tc::io::IOException(mModuleLabel, fmt::format("Failed to read from {:s}file.", (mFsFormatName.isSet() ? (mFsFormatName.get() + " ") : "")));
				}

				out_stream->write(mDataCache.data(), cache_read_len);

				remaining_data -= int64_t(cache_read_len);
			}
		}
	}

	// iterate thru child dirs
	for (auto itr = info.dir_list.begin(); itr != info.dir_list.end(); itr++)
	{
		visitDir(v_path + *itr, l_path + *itr, extract_fs, print_fs);
	}
}



//#include "FsProcess.h"
//#include "util.h"
//
//#include <memory>
//#include <tc/io/FileNotFoundException.h>
//#include <tc/io/DirectoryNotFoundException.h>
//
//nstool::FsProcess::FsProcess() :
//	mModuleLabel("nstool::FsProcess"),
//	mInputFs(),
//	mFsFormatName(),
//	mShowFsInfo(false),
//	mProperties(),
//	mShowFsTree(false),
//	mFsRootLabel(),
//	mExtractJobs(),
//	mDataCache(0x10000)
//{
//
//}
//
//void nstool::FsProcess::process()
//{
//	if (mInputFs == nullptr)
//	{
//		throw tc::InvalidOperationException(mModuleLabel, "No input filesystem");
//	}
//
//	if (mShowFsInfo)
//	{
//		fmt::print("[{:s}]\n", mFsFormatName.isSet() ? mFsFormatName.get() : "FileSystem/Info");
//		for (auto itr = mProperties.begin(); itr != mProperties.end(); itr++)
//		{
//			fmt::print("  {:s}\n", *itr);
//		}
//	}
//
//	if (mShowFsTree)
//	{
//		printFs();
//	}
//	
//	if (mExtractJobs.empty() == false)
//	{
//		extractFs();
//	}
//}
//
//void nstool::FsProcess::setInputFileSystem(const std::shared_ptr<tc::io::IFileSystem>& input_fs)
//{
//	mInputFs = input_fs;
//}
//
//void nstool::FsProcess::setFsFormatName(const std::string& fs_format_name)
//{
//	mFsFormatName = fs_format_name;
//}
//
//void nstool::FsProcess::setShowFsInfo(bool show_fs_info)
//{
//	mShowFsInfo = show_fs_info;
//}
//
//void nstool::FsProcess::setFsProperties(const std::vector<std::string>& properties)
//{
//	mProperties = properties;
//}
//
//void nstool::FsProcess::setShowFsTree(bool show_fs_tree)
//{
//	mShowFsTree = show_fs_tree;
//}
//
//void nstool::FsProcess::setFsRootLabel(const std::string& root_label)
//{
//	mFsRootLabel = root_label;
//}
//
//void nstool::FsProcess::setExtractJobs(const std::vector<nstool::ExtractJob>& extract_jobs)
//{
//	mExtractJobs = extract_jobs;
//}
//
//void nstool::FsProcess::printFs()
//{
//	fmt::print("[{:s}/Tree]\n", (mFsFormatName.isSet() ? mFsFormatName.get() : "FileSystem"));
//	visitDir(tc::io::Path("/"), tc::io::Path("/"), false, true);
//}
//
//void nstool::FsProcess::extractFs()
//{
//	fmt::print("[{:s}/Extract]\n", (mFsFormatName.isSet() ? mFsFormatName.get() : "FileSystem"));
//
//	for (auto itr = mExtractJobs.begin(); itr != mExtractJobs.end(); itr++)
//	{
//
//		/*// ******************************** TEST
//		tc::io::Path out_path = itr->extract_path + itr->virtual_path;
//		tc::io::LocalFileSystem local_fs;
//		local_fs.createDirectory(itr->extract_path);
//		local_fs.createFile(itr->extract_path.to_string() + "Application_01000b900d8b0000.cnmt");
//		std::shared_ptr<tc::io::IStream> in_stream;
//		std::shared_ptr<tc::io::IStream> out_stream;
//		//Application_01000b900d8b0000.cnmt
//		//std::map<tc::io::Path, size_t> file_entry_path_map;
//		mInputFs->openFile(tc::io::Path({ "", "4779e4ab67d7e80e222d2d78d690db2c.nca","0","Application_01000b900d8b0000.cnmt"}), tc::io::FileMode::Open, tc::io::FileAccess::Read, in_stream);
//		local_fs.openFile(itr->extract_path.to_string() + "Application_01000b900d8b0000.cnmt", tc::io::FileMode::Open, tc::io::FileAccess::Write, out_stream);
//		size_t cache_read_len;
//		in_stream->seek(0, tc::io::SeekOrigin::Begin);
//		out_stream->seek(0, tc::io::SeekOrigin::Begin);
//		for (int64_t remaining_data = in_stream->length(); remaining_data > 0;)
//		{
//			cache_read_len = in_stream->read(mDataCache.data(), mDataCache.size());
//			if (cache_read_len == 0)
//			{
//				throw tc::io::IOException(mModuleLabel, fmt::format("Failed to read from {:s}file.", (mFsFormatName.isSet() ? (mFsFormatName.get() + " ") : "")));
//			}
//
//			out_stream->write(mDataCache.data(), cache_read_len);
//
//			remaining_data -= int64_t(cache_read_len);
//		}
//		getchar();
//		return;
//
//		// ******************************** EXTRACT CNMT
//		if (true == false)
//		{
//			tc::io::Path out_path = itr->extract_path + itr->virtual_path;
//			tc::io::LocalFileSystem local_fs;
//			local_fs.createDirectory(itr->extract_path);
//			local_fs.createFile(itr->extract_path.to_string() + "Application_01000b900d8b0000.cnmt");
//			std::shared_ptr<tc::io::IStream> in_stream;
//			std::shared_ptr<tc::io::IStream> out_stream;
//			//Application_01000b900d8b0000.cnmt
//			//std::map<tc::io::Path, size_t> file_entry_path_map;
//			mInputFs->openFile(tc::io::Path({ "","0","Application_01000b900d8b0000.cnmt" }), tc::io::FileMode::Open, tc::io::FileAccess::Read, in_stream);
//			local_fs.openFile(itr->extract_path.to_string() + "Application_01000b900d8b0000.cnmt", tc::io::FileMode::Open, tc::io::FileAccess::Write, out_stream);
//			size_t cache_read_len;
//			in_stream->seek(0, tc::io::SeekOrigin::Begin);
//			out_stream->seek(0, tc::io::SeekOrigin::Begin);
//			for (int64_t remaining_data = in_stream->length(); remaining_data > 0;)
//			{
//				cache_read_len = in_stream->read(mDataCache.data(), mDataCache.size());
//				if (cache_read_len == 0)
//				{
//					throw tc::io::IOException(mModuleLabel, fmt::format("Failed to read from {:s}file.", (mFsFormatName.isSet() ? (mFsFormatName.get() + " ") : "")));
//				}
//
//				out_stream->write(mDataCache.data(), cache_read_len);
//
//				remaining_data -= int64_t(cache_read_len);
//			}
//			getchar();
//			return;
//		}
//		
//
//		// ******************************** EXTRACT ICON NCA
//		if (true == false && itr->virtual_path == tc::io::Path("4779e4ab67d7e80e222d2d78d690db2c.nca"))
//		{
//			auto extractNcmtLayoutFile = [&](const tc::io::Path& v_path, const tc::io::Path& l_path, bool extract_fs = true, bool print_fs = true)
//			{
//				auto rootPath = tc::io::Path("/");
//
//				tc::io::LocalFileSystem local_fs;
//
//				// get listing for directory
//				tc::io::sDirectoryListing info;
//				mInputFs->getDirectoryListing(rootPath, info);
//
//				if (extract_fs)
//				{
//					// create local dir
//					local_fs.createDirectory(l_path);
//				}
//
//				// iterate thru child files
//				size_t cache_read_len;
//				tc::io::Path out_path;
//				std::shared_ptr<tc::io::IStream> in_stream;
//				std::shared_ptr<tc::io::IStream> out_stream;
//
//				// build out path
//				out_path = l_path + v_path;
//
//				fmt::print("Saving {:s}...\n", out_path.to_string());
//
//				// begin export
//				mInputFs->openFile(v_path, tc::io::FileMode::Open, tc::io::FileAccess::Read, in_stream);
//				local_fs.openFile(out_path, tc::io::FileMode::OpenOrCreate, tc::io::FileAccess::Write, out_stream);
//
//				in_stream->seek(0, tc::io::SeekOrigin::Begin);
//				out_stream->seek(0, tc::io::SeekOrigin::Begin);
//				for (int64_t remaining_data = in_stream->length(); remaining_data > 0;)
//				{
//					cache_read_len = in_stream->read(mDataCache.data(), mDataCache.size());
//					if (cache_read_len == 0)
//					{
//						throw tc::io::IOException(mModuleLabel, fmt::format("Failed to read from {:s}file.", (mFsFormatName.isSet() ? (mFsFormatName.get() + " ") : "")));
//					}
//
//					out_stream->write(mDataCache.data(), cache_read_len);
//
//					remaining_data -= int64_t(cache_read_len);
//				}
//			};
//			extractNcmtLayoutFile(itr->virtual_path, itr->extract_path, true, true);
//			return;
//		}*/
//
//		// check if root path (legacy case)
//		if (itr->virtual_path == tc::io::Path("/"))
//		{
//			visitDir(tc::io::Path("/"), itr->extract_path, true, false);
//
//			//fmt::print("Root Dir Virtual Path: \"{:s}\"\n", itr->virtual_path.to_string());
//
//			// root directory extract successful, continue to next job
//			continue;
//		}
//
//		// otherwise determine if this is a file or subdirectory
//		try {
//			std::shared_ptr<tc::io::IStream> file_stream;
//			mInputFs->openFile(itr->virtual_path, tc::io::FileMode::Open, tc::io::FileAccess::Read, file_stream);
//
//			//fmt::print("Valid File Path: \"{:s}\"\n", itr->virtual_path.to_string());
//
//			// the output path for this file will depend on the user specified extract path
//			std::shared_ptr<tc::io::IFileSystem> local_fs = std::make_shared<tc::io::LocalFileSystem>(tc::io::LocalFileSystem());
//
//			// case: the extract_path is a valid path to an existing directory
//			// behaviour: extract the file, preserving the original filename, to the specified directory
//			// method: try getDirectoryListing(itr->extract_path), if this is does not throw, then we can be sure this is a valid path to a directory, file_extract_path = itr->extract_path + itr->virtual_path.back()
//
//			//try {
//			//	tc::io::sDirectoryListing dir_listing;
//			//	//local_fs->getDirectoryListing(itr->extract_path, dir_listing);
//
//			//	tc::io::Path file_extract_path = itr->extract_path + itr->virtual_path.back();
//
//			//	fmt::print("Saving {:s}...\n", file_extract_path.to_string());
//
//			//	writeStreamToFile(file_stream, itr->extract_path + itr->virtual_path.back(), mDataCache);
//
//			//	continue;
//
//			//} catch (tc::io::DirectoryNotFoundException&) {
//			//	// acceptable exception, just means directory didn't exist
//			//}
//
//			// case: the extract_path up until the last element is a valid path to an existing directory, but the full path specifies neither a directory or a file
//			// behaviour: treat extract_path as the intended location to write the extracted file (the original filename is not preserved, instead specified by the user in the final element of the extract path)
//			// method: since this checks n-1 elements, it implies a path with more than one element, so that must be accounted for, as relative paths are valid and single element paths aren't always root
//
//			try {
//				// get path to parent directory
//				tc::io::Path parent_dir_path = itr->extract_path;
//
//				// replace final path element with the current directory alias 
//				parent_dir_path.pop_back(); // remove filename
//				parent_dir_path.push_back("."); // replace with the current dir name alias
//
//				// test parent directory exists
//				tc::io::sDirectoryListing dir_listing;
//				//local_fs->getDirectoryListing(parent_dir_path, dir_listing);
//
//				fmt::print("Saving {:s} as {:s}...\n", itr->virtual_path.to_string(), itr->extract_path.to_string());
//
//				writeStreamToFile(file_stream, itr->extract_path, mDataCache);
//
//				continue;
//			} catch (tc::io::DirectoryNotFoundException&) {
//				// acceptable exception, just means the parent directory didn't exist
//			}
//
//
//			// extract path could not be determined, inform the user and skip this job
//			fmt::print("[WARNING] Extract path was invalid, and was skipped: {:s}\n", itr->extract_path.to_string());
//			continue;
//		} catch (tc::io::FileNotFoundException&) {
//			// acceptable exception, just means file didn't exist
//		}
//
//		// not a file, attempt to process this as a directory
//		try {
//			tc::io::sDirectoryListing dir_listing;
//			mInputFs->getDirectoryListing(itr->virtual_path, dir_listing);
//
//			visitDir(itr->virtual_path, itr->extract_path, true, false);
//
//			//fmt::print("Valid Directory Path: \"{:s}\"\n", itr->virtual_path.to_string());
//
//			// directory extract successful, continue to next job
//			continue;
//
//		} catch (tc::io::DirectoryNotFoundException&) {
//			// acceptable exception, just means directory didn't exist
//		}
//
//		fmt::print("[WARNING] Failed to extract virtual path: \"{:s}\"\n", itr->virtual_path.to_string());
//	}
//	
//}
//
//void nstool::FsProcess::visitDir(const tc::io::Path& v_path, const tc::io::Path& l_path, bool extract_fs, bool print_fs)
//{
//	tc::io::LocalFileSystem local_fs;
//
//	// get listing for directory
//	tc::io::sDirectoryListing info;
//	mInputFs->getDirectoryListing(v_path, info);
//
//	if (print_fs)
//	{
//		for (size_t i = 0; i < v_path.size(); i++)
//			fmt::print(" ");
//
//		fmt::print("{:s}/\n", ((v_path.size() == 1) ? (mFsRootLabel.isSet() ? (mFsRootLabel.get() + ":")  : "Root:") : v_path.back()));
//	}
//	if (extract_fs)
//	{
//		// create local dir
//		local_fs.createDirectory(l_path);
//	}
//
//	// iterate thru child files
//	size_t cache_read_len;
//	tc::io::Path out_path;
//	std::shared_ptr<tc::io::IStream> in_stream;
//	std::shared_ptr<tc::io::IStream> out_stream;
//	for (auto itr = info.file_list.begin(); itr != info.file_list.end(); itr++)
//	{
//		if (print_fs)
//		{
//			for (size_t i = 0; i < v_path.size(); i++)
//				fmt::print(" ");
//			fmt::print(" {:s}\n", *itr);
//		}
//		if (extract_fs)
//		{
//			// build out path
//			out_path = l_path + *itr;
//
//			fmt::print("Saving {:s}...\n", out_path.to_string());
//
//			// begin export
//			mInputFs->openFile(v_path + *itr, tc::io::FileMode::Open, tc::io::FileAccess::Read, in_stream);
//			local_fs.openFile(out_path, tc::io::FileMode::OpenOrCreate, tc::io::FileAccess::Write, out_stream);
//
//			in_stream->seek(0, tc::io::SeekOrigin::Begin);
//			out_stream->seek(0, tc::io::SeekOrigin::Begin);
//			for (int64_t remaining_data = in_stream->length(); remaining_data > 0;)
//			{
//				cache_read_len = in_stream->read(mDataCache.data(), mDataCache.size());
//				if (cache_read_len == 0)
//				{
//					throw tc::io::IOException(mModuleLabel, fmt::format("Failed to read from {:s}file.", (mFsFormatName.isSet() ? (mFsFormatName.get() + " ") : "")));
//				}
//
//				out_stream->write(mDataCache.data(), cache_read_len);
//
//				remaining_data -= int64_t(cache_read_len);
//			}
//		}
//	}
//
//	// iterate thru child dirs
//	for (auto itr = info.dir_list.begin(); itr != info.dir_list.end(); itr++)
//	{
//		visitDir(v_path + *itr, l_path + *itr, extract_fs, print_fs);
//	}
//}