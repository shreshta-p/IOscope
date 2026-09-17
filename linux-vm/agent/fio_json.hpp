#pragma once
// Linux replacement for windows/agent/diskspd_xml.hpp (baseline commit 280ced5).
// Parses fio's --output-format=json into the same DiskResult/disk_summaries shape
// Windows derives from DiskSpd's XML. fio separates read/write stats; where the
// workload mixes both, mean/p95 latency are combined as an IO-count-weighted
// average — an explicit approximation, not a claim that this equals a DiskSpd
// aggregate figure for the same workload (see fio.hpp and PORT-PLAN.md L2).
#include <nlohmann/json.hpp>
#include <cmath>
#include <stdexcept>
#include <string>
namespace ioscope {
struct DiskResult {double seconds=0,readBytes=0,writeBytes=0,readCount=0,writeCount=0,meanMs=0,p95Ms=0;};
inline double fio_percentile(const nlohmann::json& clat,const char* key){
 if(!clat.contains("percentile")||!clat["percentile"].contains(key))return 0;
 return clat["percentile"][key].get<double>()/1'000'000.0;
}
inline DiskResult parse_fio_json(const std::string& text){
 if(text.empty()||text.size()>16*1024*1024)throw std::runtime_error("fio JSON size outside bounds");
 nlohmann::json root;
 try{root=nlohmann::json::parse(text);}catch(const std::exception&){throw std::runtime_error("Malformed fio JSON output");}
 if(!root.contains("jobs")||!root["jobs"].is_array()||root["jobs"].size()!=1)throw std::runtime_error("Expected exactly one fio job in output");
 const auto& job=root["jobs"][0];
 const auto& read=job.at("read");const auto& write=job.at("write");
 DiskResult result;
 result.readBytes=read.at("io_bytes").get<double>();result.writeBytes=write.at("io_bytes").get<double>();
 result.readCount=read.at("total_ios").get<double>();result.writeCount=write.at("total_ios").get<double>();
 const auto readRuntimeMs=read.value("runtime",0.0),writeRuntimeMs=write.value("runtime",0.0);
 const auto runtimeMs=std::max(readRuntimeMs,writeRuntimeMs);
 if(runtimeMs<=0)throw std::runtime_error("fio reported zero runtime");
 result.seconds=runtimeMs/1000.0;
 if(result.seconds>75)throw std::runtime_error("Inconsistent fio duration");
 if(result.readCount+result.writeCount<=0)throw std::runtime_error("fio reported no completed I/O");
 // fio's io_bytes can include a partial final block; unlike DiskSpd's exact
 // readBytes==readCount*blockBytes accounting, this is not enforced as a hard check.
 const auto weight=result.readCount+result.writeCount;
 const auto readMeanNs=read.at("clat_ns").at("mean").get<double>(),writeMeanNs=write.at("clat_ns").at("mean").get<double>();
 result.meanMs=((readMeanNs*result.readCount)+(writeMeanNs*result.writeCount))/weight/1'000'000.0;
 const auto readP95=fio_percentile(read.at("clat_ns"),"95.000000"),writeP95=fio_percentile(write.at("clat_ns"),"95.000000");
 result.p95Ms=result.readCount>=result.writeCount?(readP95>0?readP95:writeP95):(writeP95>0?writeP95:readP95);
 if(!std::isfinite(result.meanMs)||result.meanMs<0||!std::isfinite(result.p95Ms)||result.p95Ms<0)throw std::runtime_error("Invalid fio latency values");
 return result;
}
inline nlohmann::json disk_summaries(const DiskResult& result){
 auto values=nlohmann::json::array();const auto add=[&](const char* id,const char* unit,double value){values.push_back({{"metricId",id},{"deviceId","nvme0"},{"scope","workload"},{"unit",unit},{"value",value},{"status","available"},{"provenance","measured"},{"source","fio.json"},{"ageMs",0},{"windowMs",static_cast<unsigned long long>(std::llround(result.seconds*1000))},{"reason",nullptr}});};
 add("storage.read.bytes_per_second","bytes/s",result.readBytes/result.seconds);add("storage.write.bytes_per_second","bytes/s",result.writeBytes/result.seconds);add("storage.iops","ops/s",(result.readCount+result.writeCount)/result.seconds);add("storage.latency.mean","ms",result.meanMs);add("storage.latency.p95","ms",result.p95Ms);return values;
}
}
