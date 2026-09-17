#pragma once
// Linux replacement for windows/agent/telemetry.hpp (baseline commit 280ced5).
// Same metric IDs, device IDs, units and capability-state shape as the Windows
// adapter; sources are /proc, /sys and dlopen'd NVML instead of PDH/NVML-via-LoadLibrary.
// See ../docs/ADAPTER-BOUNDARIES.md for the mapping this implements.
#include "platform.hpp"
#include "contracts.hpp"
#include "linux_proc.hpp"
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <optional>
#include <vector>
#include <map>
#include <cmath>
namespace ioscope {
struct CpuSnapshot{unsigned long long idle=0,total=0;};
struct DiskSnapshot{unsigned long long readSectors=0,writeSectors=0,readCount=0,writeCount=0,readTicks=0,writeTicks=0,ioTicksWeighted=0;};
inline std::optional<CpuSnapshot> read_proc_stat(){
 std::ifstream file("/proc/stat");if(!file)return std::nullopt;std::string label;CpuSnapshot snapshot;
 unsigned long long user=0,nice=0,system=0,idle=0,iowait=0,irq=0,softirq=0,steal=0;
 file>>label;if(label!="cpu")return std::nullopt;
 if(!(file>>user>>nice>>system>>idle>>iowait>>irq>>softirq>>steal))return std::nullopt;
 snapshot.idle=idle+iowait;snapshot.total=user+nice+system+idle+iowait+irq+softirq+steal;return snapshot;
}
inline std::string cpu_model_name(){
 std::ifstream file("/proc/cpuinfo");std::string line;
 while(std::getline(file,line)){if(line.rfind("model name",0)==0){const auto colon=line.find(':');if(colon!=std::string::npos)return line.substr(colon+2);}}
 return "Linux CPU";
}
// Resolve the whole-disk sysfs block device (e.g. "sda") backing a filesystem path,
// walking up from a partition device the way Windows matches the scratch drive letter
// to a \PhysicalDisk(N) PDH instance. Returns nullopt if resolution fails (e.g. an
// overlay, network filesystem, or device-mapper volume with no simple sysfs mapping).
inline std::optional<std::string> block_device_for_path(const std::filesystem::path& path){
 struct stat info{};if(stat(path.c_str(),&info)!=0)return std::nullopt;
 const unsigned int maj=static_cast<unsigned int>(major(info.st_dev)),min=static_cast<unsigned int>(minor(info.st_dev));
 char linkPath[64];std::snprintf(linkPath,sizeof(linkPath),"/sys/dev/block/%u:%u",maj,min);
 char target[4096]{};const auto count=readlink(linkPath,target,sizeof(target)-1);if(count<=0)return std::nullopt;
 std::filesystem::path resolved=std::filesystem::path("/sys/dev/block")/std::filesystem::path(std::string(target,static_cast<size_t>(count)));
 resolved=std::filesystem::weakly_canonical(resolved);
 if(std::filesystem::exists(resolved/"partition"))return resolved.parent_path().filename().string();
 return resolved.filename().string();
}
inline std::optional<DiskSnapshot> read_block_stat(const std::string& device){
 std::ifstream file("/sys/block/"+device+"/stat");if(!file)return std::nullopt;
 unsigned long long readCount=0,readMerged=0,readSectors=0,readTicks=0,writeCount=0,writeMerged=0,writeSectors=0,writeTicks=0,inFlight=0,ioTicks=0,weighted=0;
 if(!(file>>readCount>>readMerged>>readSectors>>readTicks>>writeCount>>writeMerged>>writeSectors>>writeTicks>>inFlight>>ioTicks>>weighted))return std::nullopt;
 return DiskSnapshot{readSectors,writeSectors,readCount,writeCount,readTicks,writeTicks,weighted};
}
class Telemetry {
 void* nvml_=nullptr;void* gpu_=nullptr;
 std::chrono::steady_clock::time_point epoch_=std::chrono::steady_clock::now();
 unsigned long long sequence_=0;
 std::optional<CpuSnapshot> previousCpu_;std::optional<DiskSnapshot> previousDisk_;
 std::chrono::steady_clock::time_point previousDiskAt_;
 std::string session_=random_id(),gpuName_="GPU unavailable",cpuName_=cpu_model_name(),storageName_="Storage counter unavailable for scratch volume",storageDevice_;
 template<typename Function> Function nv(const char* name){return nvml_?reinterpret_cast<Function>(dlsym(nvml_,name)):nullptr;}
public:
 Telemetry(){
  if(auto device=block_device_for_path(local_directory())){storageDevice_=*device;storageName_="Block device "+storageDevice_+" (system scope)";}
  nvml_=dlopen("libnvidia-ml.so.1",RTLD_NOW|RTLD_GLOBAL);
  auto init=nv<int(*)()>("nvmlInit_v2");auto handle=nv<int(*)(unsigned int,void**)>("nvmlDeviceGetHandleByIndex_v2");
  if(init&&handle&&init()==0&&handle(0,&gpu_)==0){char name[128]{};auto getName=nv<int(*)(void*,char*,unsigned int)>("nvmlDeviceGetName");if(getName&&getName(gpu_,name,sizeof(name))==0)gpuName_=name;}else gpu_=nullptr;
  previousDiskAt_=std::chrono::steady_clock::now();
 }
 ~Telemetry(){auto shutdown=nv<int(*)()>("nvmlShutdown");if(shutdown)shutdown();if(nvml_)dlclose(nvml_);}
 Telemetry(const Telemetry&)=delete;Telemetry& operator=(const Telemetry&)=delete;
 Json sample(){
  Json values=Json::array();
  auto add=[&](const char* id,const char* device,const char* unit,std::optional<double> value,const char* source,const char* provenance="measured",const char* reason="Sensor unavailable"){
   if(value&&(!std::isfinite(*value)||*value<0))value.reset();
   values.push_back({{"metricId",id},{"deviceId",device},{"scope","system"},{"unit",unit},{"value",value?Json(*value):Json(nullptr)},{"status",value?"available":"unavailable"},{"provenance",provenance},{"source",source},{"ageMs",0},{"windowMs",1000},{"reason",value?Json(nullptr):Json(reason)}});};
  std::optional<double> cpu;
  if(auto now=read_proc_stat()){if(previousCpu_&&now->total>previousCpu_->total&&now->idle>=previousCpu_->idle){const auto totalDelta=now->total-previousCpu_->total;const auto idleDelta=now->idle-previousCpu_->idle;if(idleDelta<=totalDelta)cpu=100.0*(1.0-static_cast<double>(idleDelta)/static_cast<double>(totalDelta));}previousCpu_=now;}
  add("cpu.utilization","cpu0","percent",cpu,"procfs","derived","CPU counter warmup");
  const auto total=meminfo_field("MemTotal:"),available=meminfo_field("MemAvailable:");
  add("ram.total","ram0","bytes",total,"procfs");
  add("ram.available","ram0","bytes",available,"procfs");
  const auto now=std::chrono::steady_clock::now();const auto elapsedSeconds=std::chrono::duration<double>(now-previousDiskAt_).count();
  std::optional<DiskSnapshot> disk;if(!storageDevice_.empty())disk=read_block_stat(storageDevice_);
  std::optional<double> readBps,writeBps,iops,queueAvg,latencyMean;
  if(disk&&previousDisk_&&elapsedSeconds>0&&disk->readSectors>=previousDisk_->readSectors&&disk->writeSectors>=previousDisk_->writeSectors&&disk->readCount>=previousDisk_->readCount&&disk->writeCount>=previousDisk_->writeCount){
   readBps=static_cast<double>(disk->readSectors-previousDisk_->readSectors)*512.0/elapsedSeconds;
   writeBps=static_cast<double>(disk->writeSectors-previousDisk_->writeSectors)*512.0/elapsedSeconds;
   const auto readOps=disk->readCount-previousDisk_->readCount,writeOps=disk->writeCount-previousDisk_->writeCount;
   iops=static_cast<double>(readOps+writeOps)/elapsedSeconds;
   const auto elapsedMs=elapsedSeconds*1000.0;if(elapsedMs>0)queueAvg=static_cast<double>(disk->ioTicksWeighted-previousDisk_->ioTicksWeighted)/elapsedMs;
   if(readOps+writeOps>0)latencyMean=static_cast<double>((disk->readTicks-previousDisk_->readTicks)+(disk->writeTicks-previousDisk_->writeTicks))/static_cast<double>(readOps+writeOps);
  }
  previousDisk_=disk;previousDiskAt_=now;
  const char* diskReason=storageDevice_.empty()?"No sysfs block device resolved for scratch volume":"sysfs counter warmup or unavailable";
  add("storage.read.bytes_per_second","nvme0","bytes/s",readBps,"sysfs","derived",diskReason);
  add("storage.write.bytes_per_second","nvme0","bytes/s",writeBps,"sysfs","derived",diskReason);
  add("storage.iops","nvme0","ops/s",iops,"sysfs","derived",diskReason);
  add("storage.queue.average","nvme0","count",queueAvg,"sysfs","derived",diskReason);
  add("storage.latency.mean","nvme0","ms",latencyMean,"sysfs","derived",diskReason);
  struct Util{unsigned int gpu;unsigned int memory;};Util util{};auto getUtil=nv<int(*)(void*,Util*)>("nvmlDeviceGetUtilizationRates");
  add("gpu.utilization","gpu0","percent",gpu_&&getUtil&&getUtil(gpu_,&util)==0?std::optional<double>(util.gpu):std::nullopt,"nvml");
  struct Memory{unsigned long long total,free,used;};Memory deviceMemory{};auto getMem=nv<int(*)(void*,Memory*)>("nvmlDeviceGetMemoryInfo");const bool memOk=gpu_&&getMem&&getMem(gpu_,&deviceMemory)==0;
  add("vram.total","vram0","bytes",memOk?std::optional<double>(static_cast<double>(deviceMemory.total)):std::nullopt,"nvml");
  add("vram.used","vram0","bytes",memOk?std::optional<double>(static_cast<double>(deviceMemory.used)):std::nullopt,"nvml");
  unsigned int temperature=0,clock=0,power=0;auto getTemp=nv<int(*)(void*,unsigned int,unsigned int*)>("nvmlDeviceGetTemperature");auto getClock=nv<int(*)(void*,unsigned int,unsigned int*)>("nvmlDeviceGetClockInfo");auto getPower=nv<int(*)(void*,unsigned int*)>("nvmlDeviceGetPowerUsage");
  add("gpu.temperature","gpu0","celsius",gpu_&&getTemp&&getTemp(gpu_,0,&temperature)==0?std::optional<double>(temperature):std::nullopt,"nvml");
  add("gpu.clock","gpu0","mhz",gpu_&&getClock&&getClock(gpu_,0,&clock)==0?std::optional<double>(clock):std::nullopt,"nvml");
  add("gpu.power","gpu0","watts",gpu_&&getPower&&getPower(gpu_,&power)==0?std::optional<double>(power/1000.0):std::nullopt,"nvml");
  add("cpu.temperature","cpu0","celsius",std::nullopt,"sysfs","measured","No supported hwmon CPU temperature sensor in this guest");
  add("storage.temperature","nvme0","celsius",std::nullopt,"sysfs","measured","No reliable SSD temperature adapter; virtualized storage has no thermal sensor");
  add("storage.latency.p95","nvme0","ms",std::nullopt,"sysfs","measured","sysfs block stats provide mean, not p95, latency");
  add("pipeline.stage","gpu0","count",std::nullopt,"sysfs","measured","No instrumented pipeline running");
  add("pipeline.h2d.bytes_per_second","gpu0","bytes/s",std::nullopt,"sysfs","measured","No instrumented host-to-device transfer");
  for(auto& metric:values)if(metric["value"].is_null()){
   const auto id=metric["metricId"].get<std::string>();
   if(id=="cpu.temperature"||id=="storage.temperature"||id=="storage.latency.p95")metric["status"]="unsupported";
   else if(sequence_>0&&metric["source"]=="sysfs"&&!storageDevice_.empty()&&(id.rfind("storage.",0)==0))metric["status"]="temporarily_errored";
  }
  return {{"schemaVersion","1.0.0"},{"sessionId",session_},{"runId",nullptr},{"origin","live"},{"sequence",sequence_++},{"elapsedUs",std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-epoch_).count()},{"capturedAt",utc_now()},{"measurements",values}};
 }
 Json inventory()const{return {{"schemaVersion","1.0.0"},{"inventoryId",session_},{"platform","linux"},{"devices",Json::array({{{"deviceId","cpu0"},{"kind","cpu"},{"name",cpuName_},{"metrics",Json::array()}},{{"deviceId","ram0"},{"kind","ram"},{"name","Physical system RAM"},{"metrics",Json::array()}},{{"deviceId","nvme0"},{"kind","storage"},{"name",storageName_},{"metrics",Json::array()}},{{"deviceId","gpu0"},{"kind","gpu"},{"name",gpuName_},{"metrics",Json::array()}},{{"deviceId","vram0"},{"kind","vram"},{"name","NVIDIA device memory"},{"metrics",Json::array()}}})}};}
};
}
