#pragma once
#include "platform.hpp"
#include "contracts.hpp"
#include <pdh.h>
#include <pdhmsg.h>
#include <vector>
#include <optional>
#include <intrin.h>
#include "counter_math.hpp"
namespace ioscope {
class Telemetry {
  PDH_HQUERY query_=nullptr;
  std::map<std::string,PDH_HCOUNTER> counters_;
  HMODULE nvml_=nullptr;
  void* gpu_=nullptr;
  std::chrono::steady_clock::time_point epoch_=std::chrono::steady_clock::now();
  unsigned long long sequence_=0,idle_=0,kernel_=0,user_=0;
  std::string session_=random_id(),gpuName_="GPU unavailable",cpuName_="Windows CPU",storageName_="Storage counter unavailable for scratch volume";
  std::wstring storagePrefix_;
  static unsigned long long ticks(FILETIME t){ULARGE_INTEGER v;v.LowPart=t.dwLowDateTime;v.HighPart=t.dwHighDateTime;return v.QuadPart;}
  template<typename Function> Function nv(const char* name){return nvml_?reinterpret_cast<Function>(GetProcAddress(nvml_,name)):nullptr;}
  void addCounter(const std::string& metric,const std::wstring& counter){PDH_HCOUNTER handle=nullptr;if(query_&&PdhAddEnglishCounterW(query_,counter.c_str(),0,&handle)==ERROR_SUCCESS)counters_[metric]=handle;}
public:
 Telemetry(){
  int registers[4]{};__cpuid(registers,static_cast<int>(0x80000000));if(static_cast<unsigned int>(registers[0])>=0x80000004){char brand[49]{};for(unsigned int i=0;i<3;i++){__cpuid(registers,static_cast<int>(0x80000002+i));memcpy(brand+i*16,registers,16);}cpuName_=brand;}
  const auto drive=local_directory().root_name().wstring();DWORD required=0;
  if(PdhExpandWildCardPathW(nullptr,L"\\PhysicalDisk(*)\\Disk Read Bytes/sec",nullptr,&required,0)==PDH_MORE_DATA&&required<65536){std::vector<wchar_t> expanded(required);if(PdhExpandWildCardPathW(nullptr,L"\\PhysicalDisk(*)\\Disk Read Bytes/sec",expanded.data(),&required,0)==ERROR_SUCCESS){for(const wchar_t* entry=expanded.data();*entry;entry+=wcslen(entry)+1){std::wstring path(entry);const auto open=path.find(L"PhysicalDisk("),close=path.find(L")",open);if(open==std::wstring::npos||close==std::wstring::npos)continue;const auto instance=path.substr(open+13,close-open-13);if(instance.find(drive)!=std::wstring::npos){storagePrefix_=path.substr(0,close+1);storageName_="Physical disk "+std::string(instance.begin(),instance.end())+" (system scope)";break;}}}}
  PdhOpenQueryW(nullptr,0,&query_);
  if(!storagePrefix_.empty()){
   addCounter("storage.read.bytes_per_second",storagePrefix_+L"\\Disk Read Bytes/sec");
   addCounter("storage.write.bytes_per_second",storagePrefix_+L"\\Disk Write Bytes/sec");
   addCounter("storage.iops",storagePrefix_+L"\\Disk Transfers/sec");
   addCounter("storage.queue.average",storagePrefix_+L"\\Avg. Disk Queue Length");
   addCounter("storage.latency.mean",storagePrefix_+L"\\Avg. Disk sec/Transfer");
  }
  if(query_)PdhCollectQueryData(query_);
  nvml_=LoadLibraryExW(L"nvml.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
  auto init=nv<int(*)()>("nvmlInit_v2");auto handle=nv<int(*)(unsigned int,void**)>("nvmlDeviceGetHandleByIndex_v2");
  if(init&&handle&&init()==0&&handle(0,&gpu_)==0){char name[128]{};auto getName=nv<int(*)(void*,char*,unsigned int)>("nvmlDeviceGetName");if(getName&&getName(gpu_,name,sizeof(name))==0)gpuName_=name;}else gpu_=nullptr;
 }
 ~Telemetry(){if(query_)PdhCloseQuery(query_);auto shutdown=nv<int(*)()>("nvmlShutdown");if(shutdown)shutdown();if(nvml_)FreeLibrary(nvml_);}
 Telemetry(const Telemetry&)=delete;Telemetry& operator=(const Telemetry&)=delete;
 Json sample(){
  Json values=Json::array();
  auto add=[&](const char* id,const char* device,const char* unit,std::optional<double> value,const char* source,const char* provenance="measured",const char* reason="Sensor unavailable"){
    if(value&&(!std::isfinite(*value)||*value<0))value.reset();
    values.push_back({{"metricId",id},{"deviceId",device},{"scope","system"},{"unit",unit},{"value",value?Json(*value):Json(nullptr)},{"status",value?"available":"unavailable"},{"provenance",provenance},{"source",source},{"ageMs",0},{"windowMs",1000},{"reason",value?Json(nullptr):Json(reason)}});};
  FILETIME idle,kernel,user;std::optional<double> cpu;
  if(GetSystemTimes(&idle,&kernel,&user)){const auto i=ticks(idle),k=ticks(kernel),u=ticks(user);if(sequence_>0)cpu=cpu_percent({i,k,u},{idle_,kernel_,user_});idle_=i;kernel_=k;user_=u;}
  add("cpu.utilization","cpu0","percent",cpu,"win32","derived","CPU counter warmup");
  MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);const bool memoryOk=GlobalMemoryStatusEx(&memory)!=0;
  add("ram.total","ram0","bytes",memoryOk?std::optional<double>(static_cast<double>(memory.ullTotalPhys)):std::nullopt,"win32");
  add("ram.available","ram0","bytes",memoryOk?std::optional<double>(static_cast<double>(memory.ullAvailPhys)):std::nullopt,"win32");
  if(query_)PdhCollectQueryData(query_);
  for(const auto& entry:std::vector<std::pair<std::string,std::string>>{{"storage.read.bytes_per_second","bytes/s"},{"storage.write.bytes_per_second","bytes/s"},{"storage.iops","ops/s"},{"storage.queue.average","count"},{"storage.latency.mean","ms"}}){
    std::optional<double> value;const auto found=counters_.find(entry.first);PDH_FMT_COUNTERVALUE counter{};DWORD type=0;
    if(sequence_>0&&found!=counters_.end()&&PdhGetFormattedCounterValue(found->second,PDH_FMT_DOUBLE,&type,&counter)==ERROR_SUCCESS&&(counter.CStatus==PDH_CSTATUS_VALID_DATA||counter.CStatus==PDH_CSTATUS_NEW_DATA))value=counter.doubleValue*(entry.second=="ms"?1000.0:1.0);
    add(entry.first.c_str(),"nvme0",entry.second.c_str(),value,"pdh","measured","PDH unavailable or warming up; physical disk hosting scratch volume");
  }
  struct Util{unsigned int gpu;unsigned int memory;};Util util{};auto getUtil=nv<int(*)(void*,Util*)>("nvmlDeviceGetUtilizationRates");
  add("gpu.utilization","gpu0","percent",gpu_&&getUtil&&getUtil(gpu_,&util)==0?std::optional<double>(util.gpu):std::nullopt,"nvml");
  struct Memory{unsigned long long total,free,used;};Memory deviceMemory{};auto getMem=nv<int(*)(void*,Memory*)>("nvmlDeviceGetMemoryInfo");const bool memOk=gpu_&&getMem&&getMem(gpu_,&deviceMemory)==0;
  add("vram.total","vram0","bytes",memOk?std::optional<double>(static_cast<double>(deviceMemory.total)):std::nullopt,"nvml");
  add("vram.used","vram0","bytes",memOk?std::optional<double>(static_cast<double>(deviceMemory.used)):std::nullopt,"nvml");
  unsigned int temperature=0,clock=0,power=0;auto getTemp=nv<int(*)(void*,unsigned int,unsigned int*)>("nvmlDeviceGetTemperature");auto getClock=nv<int(*)(void*,unsigned int,unsigned int*)>("nvmlDeviceGetClockInfo");auto getPower=nv<int(*)(void*,unsigned int*)>("nvmlDeviceGetPowerUsage");
  add("gpu.temperature","gpu0","celsius",gpu_&&getTemp&&getTemp(gpu_,0,&temperature)==0?std::optional<double>(temperature):std::nullopt,"nvml");
  add("gpu.clock","gpu0","mhz",gpu_&&getClock&&getClock(gpu_,0,&clock)==0?std::optional<double>(clock):std::nullopt,"nvml");
  add("gpu.power","gpu0","watts",gpu_&&getPower&&getPower(gpu_,&power)==0?std::optional<double>(power/1000.0):std::nullopt,"nvml");
  add("cpu.temperature","cpu0","celsius",std::nullopt,"win32","measured","No supported standard CPU temperature sensor");
  add("storage.temperature","nvme0","celsius",std::nullopt,"win32","measured","No reliable SSD temperature adapter");
  add("storage.latency.p95","nvme0","ms",std::nullopt,"pdh","measured","PDH provides mean latency, not p95");
  add("pipeline.stage","gpu0","count",std::nullopt,"win32","measured","No instrumented pipeline running");
  add("pipeline.h2d.bytes_per_second","gpu0","bytes/s",std::nullopt,"win32","measured","No instrumented host-to-device transfer");
  for(auto& metric:values)if(metric["value"].is_null()){
   const auto id=metric["metricId"].get<std::string>();
   if(id=="cpu.temperature"||id=="storage.temperature"||id=="storage.latency.p95")metric["status"]="unsupported";
   else if(sequence_>0&&metric["source"]=="pdh"&&counters_.contains(id))metric["status"]="temporarily_errored";
  }
  return {{"schemaVersion","1.0.0"},{"sessionId",session_},{"runId",nullptr},{"origin","live"},{"sequence",sequence_++},{"elapsedUs",std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-epoch_).count()},{"capturedAt",utc_now()},{"measurements",values}};
 }
 Json inventory()const{return {{"schemaVersion","1.0.0"},{"inventoryId",session_},{"platform","windows"},{"devices",Json::array({{{"deviceId","cpu0"},{"kind","cpu"},{"name",cpuName_},{"metrics",Json::array()}},{{"deviceId","ram0"},{"kind","ram"},{"name","Physical system RAM"},{"metrics",Json::array()}},{{"deviceId","nvme0"},{"kind","storage"},{"name",storageName_},{"metrics",Json::array()}},{{"deviceId","gpu0"},{"kind","gpu"},{"name",gpuName_},{"metrics",Json::array()}},{{"deviceId","vram0"},{"kind","vram"},{"name","NVIDIA device memory"},{"metrics",Json::array()}}})}};}
};
}
