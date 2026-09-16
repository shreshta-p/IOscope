#include "scratch.hpp"
int main(int argc,char** argv){try{
 if(argc!=3)throw std::runtime_error("Internal preparation requires an owned ID and size");const std::string id=argv[1];if(id.size()!=48||id.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("Invalid preparation ID");
 size_t used=0;const auto size=std::stoull(argv[2],&used);if(used!=std::string(argv[2]).size())throw std::runtime_error("Invalid preparation size");
 const auto directory=ioscope::local_directory()/"scratch"/id;ioscope::reject_reparse_ancestors(directory);const HANDLE target=GetStdHandle(STD_INPUT_HANDLE);ioscope::verify_target(target,directory/"data.bin");
 LARGE_INTEGER current{};ioscope::win_require(GetFileSizeEx(target,&current)!=0&&current.QuadPart==0,"Preparation requires a new empty owned target");
 std::atomic<bool> cancel=false;ioscope::Scratch::initialize_handle(target,size,cancel,[]{return std::optional<std::string>{};});return 0;
}catch(const std::exception& error){std::cerr<<error.what();return 1;}}
