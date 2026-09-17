#include "fio.hpp"
#include "fio_json.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
int main(int argc,char** argv){try{
 if(ioscope::hash_bytes("abc")!="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")throw std::runtime_error("Artifact SHA256 mismatch");
 if(argc!=2)return 1;std::ifstream stream(argv[1]);nlohmann::json workload;stream>>workload;
 workload["intensity"]="moderate";
 auto args=ioscope::fio_arguments(workload,"/owned path/data.bin");
 const auto has=[&](const std::string& needle){return std::any_of(args.begin(),args.end(),[&](const std::string& a){return a==needle;});};
 if(!has("--rate=134217728")||!has("--direct=0")||!has("--ioengine=io_uring"))throw std::runtime_error("Control translation failed");
 workload["cacheMode"]="unbuffered";args=ioscope::fio_arguments(workload,"/owned/data.bin");
 if(std::find(args.begin(),args.end(),"--direct=1")==args.end()||std::find(args.begin(),args.end(),"--direct=0")!=args.end())throw std::runtime_error("Cache control conflated");
 const std::string json=R"({"jobs":[{"jobname":"ioscope-run",)"
  R"("read":{"io_bytes":8192,"total_ios":2,"runtime":2000,"clat_ns":{"mean":250000,"percentile":{"95.000000":500000}}},)"
  R"("write":{"io_bytes":4096,"total_ios":1,"runtime":0,"clat_ns":{"mean":0,"percentile":{}}}}]})";
 auto result=ioscope::parse_fio_json(json);
 if(result.p95Ms!=0.5||result.readBytes/result.seconds!=4096||ioscope::disk_summaries(result).size()!=5)throw std::runtime_error("JSON normalization failed");
 for(const auto& bad:{std::string("not json"),std::string(R"({"jobs":[]})"),std::string(R"({"jobs":[{},{}]})"),std::string(R"({"jobs":[{"jobname":"x","read":{"io_bytes":0,"total_ios":0,"runtime":0,"clat_ns":{"mean":0,"percentile":{}}},"write":{"io_bytes":0,"total_ios":0,"runtime":0,"clat_ns":{"mean":0,"percentile":{}}}}]})")}){
  bool rejected=false;try{ioscope::parse_fio_json(bad);}catch(const std::exception&){rejected=true;}if(!rejected)throw std::runtime_error("Malformed/empty fio JSON accepted");
 }
 std::cout<<"PASS: bounded controls, cache isolation, rate cap, JSON units/percentile, malformed/empty rejection\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what();return 1;}}
