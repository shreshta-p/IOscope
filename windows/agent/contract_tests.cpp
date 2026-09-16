#include "contracts.hpp"
#include "native_recording.hpp"
#include <iostream>
int main(int argc,char** argv){try{if(argc!=2)throw std::runtime_error("Contract directory required");ioscope::Contracts contracts(argv[1]);int count=0;for(auto& entry:std::filesystem::directory_iterator(std::filesystem::path(argv[1])/"fixtures")){if(entry.path().extension()==".json"){contracts.validate(entry.path().stem().string(),ioscope::read_json(entry.path()));++count;}}
 auto value=ioscope::read_json(std::filesystem::path(argv[1])/"fixtures"/"WorkloadDefinition.json");value["queueDepth"]=33;bool denied=false;try{contracts.validate("WorkloadDefinition",value);}catch(const std::exception&){denied=true;}if(!denied)throw std::runtime_error("Unsafe queue accepted");
 auto fixture=ioscope::read_json(std::filesystem::path(argv[1])/"fixtures/RunRecording.json");const auto mapping=ioscope::read_json(std::filesystem::path(argv[1])/"flow-semantics.v1.json");
 auto fresh=ioscope::native_sample(fixture["samples"][0]["telemetry"],"native-test",0,0,"preparing",nullptr,fixture["metadata"]["workload"],mapping,std::chrono::milliseconds(100));contracts.validate("ReplaySample",fresh);
 auto stale=ioscope::native_sample(fixture["samples"][0]["telemetry"],"native-test",1,4000000,"running",nullptr,fixture["metadata"]["workload"],mapping,std::chrono::milliseconds(4000));contracts.validate("ReplaySample",stale);
 if(fresh["telemetry"]["measurements"][0]["ageMs"]!=100||stale["telemetry"]["measurements"][0]["value"]!=nullptr||stale["flow"]["paths"][0]["status"]!="unknown")throw std::runtime_error("Native recorder freshness failed");
 std::cout<<"PASS: "<<count<<" native contract fixtures, queue rejection and recorder freshness\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
