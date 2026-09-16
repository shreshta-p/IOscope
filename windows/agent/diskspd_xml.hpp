#pragma once
#include "win_handle.hpp"
#include <objidl.h>
#include <xmllite.h>
#include <shlwapi.h>
#include <wrl/client.h>
#include <nlohmann/json.hpp>
#include <map>
#include <vector>
#include <cmath>
namespace ioscope {
struct DiskResult {double seconds=0,readBytes=0,writeBytes=0,readCount=0,writeCount=0,meanMs=0,p95Ms=0;};
inline DiskResult parse_diskspd_xml(const std::string& xml,std::uint64_t blockBytes){
 if(xml.empty()||xml.size()>16*1024*1024)throw std::runtime_error("DiskSpd XML size outside bounds");
 Microsoft::WRL::ComPtr<IStream> stream;stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(xml.data()),static_cast<UINT>(xml.size())));if(!stream)throw std::runtime_error("Cannot allocate XML stream");
 Microsoft::WRL::ComPtr<IXmlReader> reader;if(FAILED(CreateXmlReader(__uuidof(IXmlReader),reinterpret_cast<void**>(reader.GetAddressOf()),nullptr))||FAILED(reader->SetProperty(XmlReaderProperty_DtdProcessing,DtdProcessing_Prohibit))||FAILED(reader->SetInput(stream.Get())))throw std::runtime_error("Cannot initialize safe XML reader");
 struct Node{std::string name,text;};std::vector<Node> stack;std::map<std::string,std::vector<std::string>> fields;size_t nodes=0,threads=0,targets=0;
 const auto path=[&](){std::string value;for(const auto& node:stack){if(!value.empty())value+='/';value+=node.name;}return value;};
 const auto finish=[&](){if(stack.empty())throw std::runtime_error("Unbalanced XML");const auto& raw=stack.back().text;const auto first=raw.find_first_not_of(" \r\n\t");if(first!=std::string::npos)fields[path()].push_back(raw.substr(first,raw.find_last_not_of(" \r\n\t")-first+1));stack.pop_back();};
 XmlNodeType type;HRESULT status;
 while((status=reader->Read(&type))==S_OK){
  if(type==XmlNodeType_Element){const wchar_t* name=nullptr;UINT length=0;reader->GetLocalName(&name,&length);stack.push_back({utf8(std::wstring(name,length)),{}});if(stack.size()>32||++nodes>100000)throw std::runtime_error("XML structure limit exceeded");const auto current=path();if(current=="Results/TimeSpan/Thread")++threads;if(current=="Results/TimeSpan/Thread/Target")++targets;if(reader->IsEmptyElement())finish();}
  else if(type==XmlNodeType_EndElement)finish();
  else if(type==XmlNodeType_Text||type==XmlNodeType_CDATA||type==XmlNodeType_Whitespace){const wchar_t* value=nullptr;UINT length=0;reader->GetValue(&value,&length);if(!stack.empty()){stack.back().text+=utf8(std::wstring(value,length));if(stack.back().text.size()>65536)throw std::runtime_error("XML text limit exceeded");}}
 }
 if(status!=S_FALSE||!stack.empty()||threads!=1||targets!=1)throw std::runtime_error("Malformed XML or unexpected worker/target count");
 const auto numeric=[](const std::string& text){size_t used=0;const double value=std::stod(text,&used);if(used!=text.size()||!std::isfinite(value)||value<0)throw std::runtime_error("Invalid DiskSpd numeric result");return value;};
 const auto one=[&](const std::string& key){const auto found=fields.find(key);if(found==fields.end()||found->second.size()!=1)throw std::runtime_error("Missing or duplicated DiskSpd result: "+key);return numeric(found->second.front());};
 DiskResult result;result.seconds=one("Results/TimeSpan/TestTimeSeconds");const std::string target="Results/TimeSpan/Thread/Target/";
 result.readBytes=one(target+"ReadBytes");result.writeBytes=one(target+"WriteBytes");result.readCount=one(target+"ReadCount");result.writeCount=one(target+"WriteCount");result.meanMs=one("Results/TimeSpan/Latency/AverageTotalMilliseconds");
 if(result.seconds<=0||result.seconds>75||result.readCount+result.writeCount<=0||result.readBytes!=result.readCount*blockBytes||result.writeBytes!=result.writeCount*blockBytes)throw std::runtime_error("Inconsistent DiskSpd counts or duration");
 for(double count:{result.readCount,result.writeCount,result.readBytes,result.writeBytes})if(count>9007199254740991.0||std::floor(count)!=count)throw std::runtime_error("Invalid integral DiskSpd count");
 const auto& percentiles=fields["Results/TimeSpan/Latency/Bucket/Percentile"];const auto& latencies=fields["Results/TimeSpan/Latency/Bucket/TotalMilliseconds"];if(percentiles.size()!=latencies.size())throw std::runtime_error("Incomplete percentile distribution");bool found=false;
 for(size_t index=0;index<percentiles.size();++index)if(numeric(percentiles[index])==95){if(found)throw std::runtime_error("Duplicate p95");result.p95Ms=numeric(latencies[index]);found=true;}if(!found)throw std::runtime_error("DiskSpd p95 missing");
 return result;
}
inline nlohmann::json disk_summaries(const DiskResult& result){
 auto values=nlohmann::json::array();const auto add=[&](const char* id,const char* unit,double value){values.push_back({{"metricId",id},{"deviceId","nvme0"},{"scope","workload"},{"unit",unit},{"value",value},{"status","available"},{"provenance","measured"},{"source","diskspd.xml"},{"ageMs",0},{"windowMs",static_cast<unsigned long long>(std::llround(result.seconds*1000))},{"reason",nullptr}});};
 add("storage.read.bytes_per_second","bytes/s",result.readBytes/result.seconds);add("storage.write.bytes_per_second","bytes/s",result.writeBytes/result.seconds);add("storage.iops","ops/s",(result.readCount+result.writeCount)/result.seconds);add("storage.latency.mean","ms",result.meanMs);add("storage.latency.p95","ms",result.p95Ms);return values;
}
}
