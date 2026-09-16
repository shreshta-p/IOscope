#include "diskspd.hpp"
#include "diskspd_xml.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char** argv){try{
 if(ioscope::hash_bytes("abc")!="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")throw std::runtime_error("Artifact SHA256 mismatch");
 if(argc!=2)return 1;std::ifstream stream(argv[1]);nlohmann::json workload;stream>>workload;
 auto args=ioscope::diskspd_arguments(workload,L"C:\\owned path\\data.bin",L"Local\\ioscope-test");
 if(std::find(args.begin(),args.end(),L"-g33554")==args.end()||std::find(args.begin(),args.end(),L"-Sb")==args.end()||std::find(args.begin(),args.end(),L"-t1")==args.end())throw std::runtime_error("Control translation failed");
 workload["cacheMode"]="unbuffered";args=ioscope::diskspd_arguments(workload,L"C:\\owned\\data.bin",L"Local\\test");if(std::find(args.begin(),args.end(),L"-Su")==args.end()||std::find(args.begin(),args.end(),L"-Sh")!=args.end())throw std::runtime_error("Cache control conflated");
 const std::string xml="<Results><TimeSpan><TestTimeSeconds>2</TestTimeSeconds><Thread><Target><ReadBytes>8192</ReadBytes><ReadCount>2</ReadCount><WriteBytes>4096</WriteBytes><WriteCount>1</WriteCount></Target></Thread><Latency><AverageTotalMilliseconds>0.25</AverageTotalMilliseconds><Bucket><Percentile>95</Percentile><TotalMilliseconds>0.5</TotalMilliseconds></Bucket></Latency></TimeSpan></Results>";
 auto result=ioscope::parse_diskspd_xml(xml,4096);if(result.p95Ms!=.5||result.readBytes/result.seconds!=4096||ioscope::disk_summaries(result).size()!=5)throw std::runtime_error("XML normalization failed");
 for(const auto& bad:{xml.substr(0,xml.size()-5),std::string("<!DOCTYPE Results [<!ENTITY x SYSTEM 'file:///C:/Windows/win.ini'>]>")+xml,std::string("<Results/>")}){bool rejected=false;try{ioscope::parse_diskspd_xml(bad,4096);}catch(const std::exception&){rejected=true;}if(!rejected)throw std::runtime_error("Malformed/DTD XML accepted");}
 bool mismatch=false;try{ioscope::parse_diskspd_xml(xml,16384);}catch(const std::exception&){mismatch=true;}if(!mismatch)throw std::runtime_error("Byte/count mismatch accepted");
 std::cout<<"PASS: bounded controls, cache isolation, rate cap, XML units/percentile, malformed/DTD/count rejection\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what();return 1;}}
