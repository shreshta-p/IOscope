#include "json_input.hpp"
#include <iostream>
int main(){try{
 if(ioscope::parse_input("{\"a\":{\"x\":1},\"b\":{\"x\":2}}")["b"]["x"]!=2)throw std::runtime_error("Valid nested JSON rejected");
 bool oversized=false;try{ioscope::parse_command(std::string(65536,' ') + "{}");}catch(const std::exception&){oversized=true;}if(!oversized)throw std::runtime_error("Oversized command accepted");
 for(const auto& input:{std::string("{\"a\":1,\"a\":2}"),std::string("{\"a\":{\"x\":1,\"x\":2}}"),std::string(66,'[')+"0"+std::string(66,']')}){bool rejected=false;try{ioscope::parse_input(input);}catch(const std::exception&){rejected=true;}if(!rejected)throw std::runtime_error("Unsafe JSON accepted");}
 std::cout<<"PASS: duplicate-key and depth limits\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what();return 1;}}
