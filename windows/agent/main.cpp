#include "security.hpp"
#include "json_input.hpp"
#include "contracts.hpp"
#include "store.hpp"
#include "telemetry.hpp"
#include "system_resources.hpp"
#include "diskspd.hpp"
#include "workload_controller.hpp"
#include <iostream>
#include <set>
#include <thread>
int main(int argc,char** argv){try{
 const auto root=argc>1?std::filesystem::absolute(argv[1]):std::filesystem::current_path();
 ioscope::Contracts contracts(root/"contracts");
 ioscope::Handle agentLock;
 if(!(argc>2&&std::string(argv[2])=="--probe")){agentLock.reset(CreateFileW((ioscope::local_directory()/"agent.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));ioscope::win_require(static_cast<bool>(agentLock),"Another native agent owns this workspace");}
 const auto databasePath=(ioscope::local_directory()/"runs.sqlite").u8string();
 ioscope::Store store(std::string(databasePath.begin(),databasePath.end()));
 ioscope::Telemetry telemetry;
 if(argc>2&&std::string(argv[2])=="--probe"){
  auto timings=ioscope::Json::array();double maximum=0;
  for(int i=0;i<10;i++){const auto start=std::chrono::steady_clock::now();auto frame=telemetry.sample();const double duration=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();contracts.validate("TelemetryFrame",frame);timings.push_back(duration);maximum=std::max(maximum,duration);std::this_thread::sleep_for(std::chrono::seconds(1));}
  std::cout<<ioscope::Json({{"polls",10},{"pollDurationMs",timings},{"maximumMs",maximum},{"inventory",telemetry.inventory()},{"kind","read-only adapter probe"}}).dump(2)<<"\n";return maximum<250?0:2;
 }
 auto latest=telemetry.sample();auto latestAt=std::chrono::steady_clock::now();std::mutex liveMutex;
 ioscope::DiskSpdEngine engine(root/"build/diskspd-v2.3/amd64/diskspd.exe");
 auto observed=[&]{std::lock_guard<std::mutex> lock(liveMutex);return ioscope::RecordedFrame{latest,std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-latestAt)};};
 ioscope::WorkloadController workloads(contracts,store,engine,telemetry.inventory(),ioscope::read_json(root/"contracts/flow-semantics.v1.json"),[&]{const auto value=observed();return ioscope::system_resources(value.frame,value.age);},observed);
 try{const auto scratchRoot=ioscope::local_directory()/"scratch";if(std::filesystem::exists(scratchRoot)){ioscope::reject_reparse_ancestors(scratchRoot);std::vector<std::string> ids;for(const auto& entry:std::filesystem::directory_iterator(scratchRoot))ids.push_back(entry.path().filename().string());for(const auto& id:ids)try{ioscope::recover_scratch(id);}catch(const std::exception& error){std::cerr<<"Scratch preserved: "<<error.what()<<"\n";}}}catch(const std::exception& error){std::cerr<<"Scratch recovery unavailable: "<<error.what()<<"\n";}
 workloads.recover();
 crow::App<ioscope::Security> app;
 app.bindaddr("127.0.0.1").port(8765).concurrency(2).websocket_max_payload(65536);
 auto& security=app.get_middleware<ioscope::Security>();
 const auto respond=[](const ioscope::Json& value){crow::response response(value.dump());response.set_header("Content-Type","application/json");return response;};
 const auto failure=[&](const std::exception& error){return crow::response(400,ioscope::Json({{"schemaVersion","1.0.0"},{"code","REQUEST_REJECTED"},{"operation","api"},{"correlationId",ioscope::random_id()},{"runId",nullptr},{"timestamp",ioscope::utc_now()},{"message",error.what()},{"retryable",false}}).dump());};
 CROW_ROUTE(app,"/api/v1/bootstrap")([&]{return respond({{"token",security.token},{"protocolVersion","1.0.0"},{"agentVersion","0.6.0"},{"telemetry",true},{"workloads",true}});});
 CROW_ROUTE(app,"/api/v1/inventory")([&]{auto inventory=telemetry.inventory();std::lock_guard<std::mutex> lock(liveMutex);for(auto& device:inventory["devices"])for(const auto& metric:latest["measurements"])if(metric["deviceId"]==device["deviceId"])device["metrics"].push_back(metric);return respond(inventory);});
 CROW_ROUTE(app,"/api/v1/telemetry")([&]{std::lock_guard<std::mutex> lock(liveMutex);return respond(latest);});
 CROW_ROUTE(app,"/api/v1/runs/admission").methods(crow::HTTPMethod::Post)([&](const crow::request& request){try{return respond(workloads.admission(ioscope::parse_command(request.body)));}catch(const std::exception& error){return failure(error);}});
 CROW_ROUTE(app,"/api/v1/runs").methods(crow::HTTPMethod::Post)([&](const crow::request& request){try{return respond(workloads.start(ioscope::parse_command(request.body)));}catch(const std::exception& error){return failure(error);}});
 CROW_ROUTE(app,"/api/v1/runs/active")([&]{return respond(workloads.snapshot());});
 CROW_ROUTE(app,"/api/v1/runs/cancel").methods(crow::HTTPMethod::Post)([&]{return respond(workloads.cancel());});
 CROW_ROUTE(app,"/api/v1/artifacts/<string>")([&](const std::string& id){try{if(id.size()!=48||id.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("Invalid artifact ID");return respond(store.artifact(id));}catch(const std::exception& error){return failure(error);}});
 CROW_ROUTE(app,"/api/v1/capabilities")([&]{std::lock_guard<std::mutex> lock(liveMutex);auto result=ioscope::Json::array();const auto inventory=telemetry.inventory();for(const auto& device:inventory["devices"]){auto entries=ioscope::Json::array();for(const auto& metric:latest["measurements"])if(metric["deviceId"]==device["deviceId"])entries.push_back({{"metricId",metric["metricId"]},{"status",metric["status"]},{"requiresElevation",false},{"reason",metric["reason"]}});result.push_back({{"schemaVersion","1.0.0"},{"deviceId",device["deviceId"]},{"capabilities",entries}});}return respond(result);});
 struct Client{bool authenticated=false;std::optional<unsigned long long> pending;std::chrono::steady_clock::time_point deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);};
 std::map<crow::websocket::connection*,Client> clients;std::recursive_mutex clientsMutex;
 CROW_WEBSOCKET_ROUTE(app,"/api/v1/stream")
 .onaccept([&](const crow::request& request,void**){std::lock_guard<std::recursive_mutex> lock(clientsMutex);return clients.size()<8&&request.get_header_value("Host")=="127.0.0.1:8765"&&ioscope::allowed_origin(request.get_header_value("Origin"));})
 .onopen([&](crow::websocket::connection& connection){std::lock_guard<std::recursive_mutex> lock(clientsMutex);if(clients.size()>=8){connection.close("Connection limit");return;}clients[&connection]=Client{};})
 .onmessage([&](crow::websocket::connection& connection,const std::string& message,bool binary){
  std::lock_guard<std::recursive_mutex> lock(clientsMutex);
  try{if(binary)throw std::runtime_error("Text required");auto value=ioscope::parse_input(message);contracts.validate("StreamRequest",value);auto it=clients.find(&connection);if(it==clients.end())throw std::runtime_error("Unknown connection");auto& client=it->second;
   if(!client.authenticated&&value.contains("token")&&value["token"]==security.token){client.authenticated=true;return;}
   if(client.authenticated&&value.contains("ack")&&client.pending&&value["ack"]==*client.pending){client.pending.reset();return;}
  }catch(const std::exception&){}
  connection.close("Invalid authentication or acknowledgment");
 })
 .onclose([&](crow::websocket::connection& connection,const std::string&,uint16_t){std::lock_guard<std::recursive_mutex> lock(clientsMutex);clients.erase(&connection);});
 app.tick(std::chrono::seconds(1),[&]{try{
  auto frame=telemetry.sample();contracts.validate("TelemetryFrame",frame);{std::lock_guard<std::mutex> lock(liveMutex);latest=frame;latestAt=std::chrono::steady_clock::now();}
  const auto message=ioscope::Json({{"kind","telemetry"},{"schemaVersion","1.0.0"},{"sessionId",frame["sessionId"]},{"sequence",frame["sequence"]},{"payload",frame}}).dump();
  std::lock_guard<std::recursive_mutex> lock(clientsMutex);std::vector<crow::websocket::connection*> expired;
  const auto now=std::chrono::steady_clock::now();
  for(auto& entry:clients){auto& client=entry.second;if((!client.authenticated||client.pending)&&now>client.deadline){expired.push_back(entry.first);continue;}if(client.authenticated&&!client.pending){client.pending=frame["sequence"].get<unsigned long long>();client.deadline=now+std::chrono::seconds(3);entry.first->send_text(message);}}
  for(auto* client:expired)client->close("Stream deadline exceeded; reconnect receives a fresh snapshot");
 }catch(const std::exception& e){std::cerr<<"Telemetry sample rejected: "<<e.what()<<"\n";}});
 CROW_ROUTE(app,"/api/v1/recordings").methods(crow::HTTPMethod::Get,crow::HTTPMethod::Post)([&](const crow::request& request){try{
  if(request.method==crow::HTTPMethod::Get)return respond(store.list());
  auto value=ioscope::parse_input(request.body);contracts.validate("Recording",value);const auto& metadata=value["metadata"];
  auto id=store.save(metadata["origin"],value.contains("config")?value["config"]["scenario"]:metadata["workload"]["definitionId"],metadata["startedAt"],value.dump());return respond({{"id",id}});
 }catch(const std::exception& e){return failure(e);}});
 CROW_ROUTE(app,"/api/v1/recordings/<string>").methods(crow::HTTPMethod::Get,crow::HTTPMethod::Delete)([&](const crow::request& request,const std::string& id){try{
  if(id.size()!=32||id.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("Invalid recording ID");
  if(request.method==crow::HTTPMethod::Delete){store.remove(id);return respond({{"deleted",true}});}
  return respond(ioscope::Json::parse(store.get(id)));
 }catch(const std::exception& e){return failure(e);}});
 // Crow's filename sanitizer rewrites the colon in an absolute Windows path.
 // These paths are server-owned; validate URL basenames before using the raw API.
 CROW_ROUTE(app,"/")([&]{const auto index=root/"apps/ui/dist/index.html";if(!std::filesystem::is_regular_file(index))return crow::response(503,"UI build missing. Run npm run build from the IOscope project directory.");crow::response response;response.set_static_file_info_unsafe(index.string());return response;});
 CROW_ROUTE(app,"/assets/<path>")([&](const std::string& path){if(path.empty()||path.find("..")!=std::string::npos||path.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-")!=std::string::npos)return crow::response(403);const auto directory=std::filesystem::weakly_canonical(root/"apps/ui/dist/assets");const auto file=std::filesystem::weakly_canonical(directory/path);if(file.parent_path()!=directory)return crow::response(403);crow::response response;response.set_static_file_info_unsafe(file.string());return response;});
 std::cout<<"IOscope native agent: http://127.0.0.1:8765 / local SQLite storage\n";
 app.loglevel(crow::LogLevel::Warning);app.run();return 0;
}catch(const std::exception& error){std::cerr<<"Agent failed: "<<error.what()<<"\n";return 1;}}
