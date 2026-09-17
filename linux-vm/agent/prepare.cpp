// Linux replacement for windows/agent/prepare.cpp (baseline commit 280ced5).
// Isolated preparation helper: receives the already-exclusively-opened scratch file
// as fd 0 (inherited from the parent, not reopened by path) and fills it, so a path
// substitution race after admission cannot redirect the write.
#include "scratch.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
int main(int argc,char** argv){try{
 if(argc!=3)throw std::runtime_error("Internal preparation requires an owned ID and size");
 const std::string id=argv[1];if(id.size()!=48||id.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("Invalid preparation ID");
 size_t used=0;const auto size=std::stoull(argv[2],&used);if(used!=std::string(argv[2]).size())throw std::runtime_error("Invalid preparation size");
 const auto directory=ioscope::local_directory()/"scratch"/id;ioscope::reject_symlink_ancestors(directory);
 struct stat info{};ioscope::posix_require(fstat(0,&info)==0&&S_ISREG(info.st_mode)&&info.st_nlink==1,"Preparation stdin must be a regular owned file");
 ioscope::posix_require(info.st_size==0,"Preparation requires a new empty owned target");
 std::ifstream manifestStream(directory/"ownership.json");std::ostringstream buffer;buffer<<manifestStream.rdbuf();
 const auto owner=ioscope::parse_input(buffer.str());
 ioscope::posix_require(owner.at("schemaVersion")=="1.0.0"&&owner.at("runId")==id
  &&owner.at("device").get<std::uint64_t>()==static_cast<std::uint64_t>(info.st_dev)
  &&owner.at("inode").get<std::uint64_t>()==static_cast<std::uint64_t>(info.st_ino),
  "Preparation stdin identity does not match ownership manifest");
 std::atomic<bool> cancel=false;
 ioscope::Scratch::initialize_fd(0,size,cancel,[]{return std::optional<std::string>{};});
 return 0;
}catch(const std::exception& error){std::cerr<<error.what();return 1;}}
