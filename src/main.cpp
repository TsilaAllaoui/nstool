#include <tc.h>
#include <tc/os/UnicodeMain.h>
#include "Settings.h"
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <fstream>


#include "GameCardProcess.h"
#include "PfsProcess.h"
#include "RomfsProcess.h"
#include "NcaProcess.h"
#include "MetaProcess.h"
#include "CnmtProcess.h"
#include "NsoProcess.h"
#include "NroProcess.h"
#include "NacpProcess.h"
#include "IniProcess.h"
#include "KipProcess.h"
#include "EsCertProcess.h"
#include "EsTikProcess.h"
#include "AssetProcess.h"

namespace fs = std::filesystem;

// Get title ID as hex
auto toLowerCaseHex = [](uint64_t number) {
	std::stringstream stream;
	stream << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << number;
	std::string result = stream.str();
	for (char& c : result) {
		if (std::isalpha(c)) {
			c = std::tolower(c);
		}
	}
	return result;
};

int umain(const std::vector<std::string>& args, const std::vector<std::string>& env)
{
	std::string outputFolder;
	try
	{
		nstool::Settings set = nstool::SettingsInitializer(args);
		std::shared_ptr<tc::io::IStream> infile_stream = std::make_shared<tc::io::FileStream>(tc::io::FileStream(set.infile.path.get(), tc::io::FileMode::Open, tc::io::FileAccess::Read));

		if (set.infile.filetype == nstool::Settings::FILE_TYPE_PARTITIONFS || set.infile.filetype == nstool::Settings::FILE_TYPE_NSP)
		{
			// Opening nsp file
			nstool::PfsProcess pfsProcess;
			pfsProcess.setInputFile(infile_stream);
			pfsProcess.setCliOutputMode(set.opt.cli_output_mode);
			pfsProcess.setVerifyMode(set.opt.verify);
			pfsProcess.setShowFsTree(set.fs.show_fs_tree);

			// Getting cnmt nca file name
			auto cnmtNcaFileName =  pfsProcess.getCnmtNcaFileName();
			
			// Extracting cnmt nca file
			nstool::ExtractJob extractJob = { tc::io::Path(cnmtNcaFileName), tc::io::Path(".")};
			pfsProcess.setExtractJobs({ extractJob });
			pfsProcess.process();
			
			// Extract cnmt nca file content
			nstool::NcaProcess cmntNcaProcess;
			std::shared_ptr<tc::io::IStream> cnmt_nca_infile_stream = std::make_shared<tc::io::FileStream>(tc::io::FileStream(tc::io::Path(cnmtNcaFileName), tc::io::FileMode::Open, tc::io::FileAccess::Read));
			cmntNcaProcess.setInputFile(cnmt_nca_infile_stream);
			cmntNcaProcess.setBaseNcaPath(set.nca.base_nca_path);
			cmntNcaProcess.setKeyCfg(set.opt.keybag);
			cmntNcaProcess.setCliOutputMode(set.opt.cli_output_mode);
			cmntNcaProcess.setVerifyMode(set.opt.verify);
			cmntNcaProcess.setShowFsTree(set.fs.show_fs_tree);
			cmntNcaProcess.processCnmt();

			// Reading cnmt content and getting control nacp file name
			nstool::CnmtProcess cnmtProcess;
			std::shared_ptr<tc::io::IStream> cnmt_infile_stream = std::make_shared<tc::io::FileStream>(tc::io::FileStream(tc::io::Path("Application_0" + toLowerCaseHex(cmntNcaProcess.mHdr.getProgramId()) + ".cnmt"), tc::io::FileMode::Open, tc::io::FileAccess::Read));
			cnmtProcess.setInputFile(cnmt_infile_stream);
			cnmtProcess.setCliOutputMode(set.opt.cli_output_mode);
			cnmtProcess.setVerifyMode(set.opt.verify);
			cnmtProcess.getNacpFileName();

			// Extract nca that contain nacp
			extractJob = { tc::io::Path(cnmtProcess.controlNcapFileName + ".nca"), tc::io::Path(".")};
			pfsProcess.setExtractJobs({ extractJob });
			pfsProcess.process();

			// Output folder
			outputFolder = toLowerCaseHex(cnmtProcess.titleId);

			// Extract icons from nacp nca file
			nstool::NcaProcess controlNcaProcess;
			std::shared_ptr<tc::io::IStream> controlNcaFile = std::make_shared<tc::io::FileStream>(tc::io::FileStream(tc::io::Path(cnmtProcess.controlNcapFileName + ".nca"), tc::io::FileMode::Open, tc::io::FileAccess::Read));
			controlNcaProcess.setInputFile(controlNcaFile);
			controlNcaProcess.setBaseNcaPath(set.nca.base_nca_path);
			controlNcaProcess.setKeyCfg(set.opt.keybag);
			controlNcaProcess.setCliOutputMode(set.opt.cli_output_mode);
			controlNcaProcess.setVerifyMode(set.opt.verify);
			controlNcaProcess.setShowFsTree(set.fs.show_fs_tree);
			extractJob = { tc::io::Path("\\"), tc::io::Path(outputFolder)};
			controlNcaProcess.setExtractJobs({ extractJob });
			controlNcaProcess.process();

			// Getting title
			nstool::NacpProcess controlNacpProcess;
			std::shared_ptr<tc::io::IStream> controlNacpFile = std::make_shared<tc::io::FileStream>(tc::io::FileStream(tc::io::Path(outputFolder + "\\0\\control.nacp"), tc::io::FileMode::Open, tc::io::FileAccess::Read));
			controlNacpProcess.setInputFile(controlNacpFile);
			controlNacpProcess.setCliOutputMode(set.opt.cli_output_mode);
			controlNacpProcess.setVerifyMode(set.opt.verify);
			controlNacpProcess.importNacp();
			auto output = controlNacpProcess.getTitleInfo();
			std::ofstream file(outputFolder + "\\title_info.json");
			file << output.str();
			file.close();

			// Renaming all dat to png
			try
			{
				for (auto& file : fs::directory_iterator(outputFolder + "\\0"))
				{
					if (file.path().extension() == ".dat")
					{
						auto p = file.path().string();
						auto name = file.path().filename().string();
						fs::rename(file.path(), p.substr(0, p.size() - 3) + "png");
						fs::copy_file(outputFolder + "\\0\\" + name.substr(0, name.size() - 3) + "png", outputFolder + "\\" + name.substr(0, name.size() - 3) + "png");
					}
				}
			}
			catch (std::exception& e)
			{
				std::cout << e.what();
			}
		}
	}
	catch (tc::Exception& e)
	{
		fmt::print("[{0}{1}ERROR] {2}\n", e.module(), (strlen(e.module()) != 0 ? " " : ""), e.error());
		return 1;
	}

	// Cleaning
	for (auto& file : fs::directory_iterator("."))
	{
		if (file.path().extension() == ".nca" || file.path().extension() == ".cnmt")
			fs::remove(file.path());
	}

	fs::remove_all(outputFolder + "\\0\\");
	return 0;
}