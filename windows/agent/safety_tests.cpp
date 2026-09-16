#include "safety.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char** argv){try{
 if(argc!=2)return 1;std::ifstream stream(argv[1]);nlohmann::json workload;stream>>workload;
 ioscope::Resources resources;resources.diskTotal=1000*ioscope::GiB;resources.diskFree=200*ioscope::GiB;resources.ramTotal=32*ioscope::GiB;resources.ramAvailable=12*ioscope::GiB;
 auto require=[](bool value){if(!value)throw std::runtime_error("Safety assertion failed");};
 auto allowed=ioscope::admit(workload,resources);require(allowed.allowed()&&allowed.restrictedThermals);
 require(allowed.diskReserve==2*ioscope::GiB&&allowed.ramReserve==2*ioscope::GiB);
 auto largerDrive=resources;largerDrive.diskTotal=4000*ioscope::GiB;require(ioscope::admit(workload,largerDrive).diskReserve==allowed.diskReserve);
 const auto requiredDisk=allowed.diskReserve+workload["workingSetBytes"].get<std::uint64_t>()+64*ioscope::MiB;
 auto boundary=resources;boundary.diskFree=requiredDisk;require(ioscope::admit(workload,boundary).allowed());boundary.diskFree--;require(!ioscope::admit(workload,boundary).allowed());
 auto disk=resources;disk.diskFree=ioscope::GiB;require(!ioscope::admit(workload,disk).allowed());
 auto ram=resources;ram.ramAvailable=allowed.ramReserve+allowed.bufferBytes;require(ioscope::admit(workload,ram).allowed());ram.ramAvailable--;require(!ioscope::admit(workload,ram).allowed());
 auto changed=workload;changed["intensity"]="moderate";require(!ioscope::admit(changed,resources).allowed());
 changed=workload;changed["durationSeconds"]=16;require(!ioscope::admit(changed,resources).allowed());
 changed=workload;changed["queueDepth"]=33;require(!ioscope::admit(changed,resources).allowed());
 auto full=resources;full.cpuTemperature=50;full.ssdTemperature=40;full.gpuTemperature=45;
 changed=workload;changed["intensity"]="high";changed["readPercent"]=0;changed["durationSeconds"]=60;require(!ioscope::admit(changed,full).allowed());
 require(!ioscope::admit(workload,resources,8*ioscope::GiB).allowed());
 auto hot=full;hot.gpuTemperature=80;require(!ioscope::admit(workload,hot).allowed());
 require(ioscope::runtime_breach(disk,allowed,resources).has_value());
 auto lost=full;lost.cpuTemperature.reset();require(ioscope::runtime_breach(lost,allowed,full).has_value());
 require(!ioscope::runtime_breach(full,allowed,full));
 std::cout<<"PASS: reserves, missing thermal coverage, control bounds, cumulative writes, threshold and stale-sensor aborts\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what();return 1;}}
