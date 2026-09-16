#include "store.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <windows.h>
#include <process.h>
int main(int argc,char** argv){try{
if(argc==3&&std::string(argv[1])=="--crash"){ioscope::Store child(argv[2]);child.save("simulated","crash-committed","2026-09-06T00:00:00Z","{\"durable\":true}");child.begin_run("crash-request",{{"runId","crash-run"},{"workload",{{"definitionId","fake-crash"}}}},{{"telemetry",{{"sequence",0}}},{"workloadStatus",{{"state","preparing"}}}});_exit(99);}
ioscope::Store store(":memory:");
auto id=store.save("simulated","fixture","2026-09-06T00:00:00Z","{\"value\":42}");
if(store.list().size()!=1||store.get(id)!="{\"value\":42}")throw std::runtime_error("Round trip failed");
store.remove(id);if(!store.list().empty())throw std::runtime_error("Delete failed");
bool missing=false;try{store.get(id);}catch(const std::exception&){missing=true;}if(!missing)throw std::runtime_error("Missing record accepted");
auto quoted=store.save("simulated","'); DROP TABLE recordings;--","2026-09-06T00:00:00Z","{}");
if(store.list().size()!=1||store.get(quoted)!="{}")throw std::runtime_error("Prepared statement failed");
const nlohmann::json metadata={{"runId","journal-test"},{"origin","live"},{"startedAt","2026-09-07T00:00:00Z"},{"workload",{{"definitionId","fake-test"}}}};
nlohmann::json sample={{"telemetry",{{"sequence",0}}},{"workloadStatus",{{"state","preparing"}}}};
store.begin_run("request-test",metadata,sample);
if(store.request("unknown")!=nullptr||store.request("request-test")["runId"]!="journal-test")throw std::runtime_error("Idempotency lookup failed");
bool duplicate=false;try{store.begin_run("request-test",metadata,sample);}catch(const std::exception&){duplicate=true;}if(!duplicate||store.pending().size()!=1)throw std::runtime_error("Duplicate start mutated journal");
bool backwards=false;try{store.append_sample("journal-test",sample);}catch(const std::exception&){backwards=true;}if(!backwards||store.pending()[0]["samples"].size()!=1)throw std::runtime_error("Sequence rollback failed");
sample["telemetry"]["sequence"]=1;sample["workloadStatus"]["state"]="cancelled";store.append_sample("journal-test",sample);
auto recording=store.pending()[0];const auto artifact=nlohmann::json({{"artifactId","artifact-test"},{"kind","diagnostic"},{"sha256",std::string(64,'0')},{"payload","authored fixture"}});const auto before=store.list().size();bool rolledBack=false;try{store.finish_run(recording,nlohmann::json::array({artifact,artifact}));}catch(const std::exception&){rolledBack=true;}if(!rolledBack||store.pending().size()!=1||store.list().size()!=before||!store.request("request-test")["recordingId"].is_null())throw std::runtime_error("Failed artifact transaction did not roll back");
const auto finished=store.finish_run(recording,nlohmann::json::array({artifact}));if(!store.pending().empty()||store.request("request-test")["recordingId"]!=finished||nlohmann::json::parse(store.get(finished))!=recording||store.artifact("artifact-test")["payload"]!="authored fixture")throw std::runtime_error("Atomic journal completion failed");
store.remove(finished);if(store.request("request-test")["recordingId"]!=finished)throw std::runtime_error("Deletion lost idempotency tombstone");
bool removedArtifact=false;try{store.artifact("artifact-test");}catch(const std::exception&){removedArtifact=true;}if(!removedArtifact)throw std::runtime_error("Deleted run retained its artifact");
const auto directory=std::filesystem::temp_directory_path()/("ioscope-store-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
std::filesystem::create_directory(directory);
struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;for(const char* name:{"runs.sqlite","runs.sqlite-wal","runs.sqlite-shm","corrupt.sqlite","crash.sqlite","crash.sqlite-wal","crash.sqlite-shm"})std::filesystem::remove(path/name,error);std::filesystem::remove(path,error);}} cleanup{directory};
const auto persistent=(directory/"runs.sqlite").string();std::string saved;
{ioscope::Store first(persistent);saved=first.save("simulated","durability","2026-09-06T00:00:00Z","{\"samples\":[1,2,3]}");first.begin_run("durable-request",metadata,sample);}
{ioscope::Store reopened(persistent);if(reopened.get(saved)!="{\"samples\":[1,2,3]}"||reopened.pending().size()!=1||reopened.request("durable-request")["runId"]!="journal-test")throw std::runtime_error("Reopen lost data");}
sqlite3* future=nullptr;if(sqlite3_open(persistent.c_str(),&future)!=SQLITE_OK)throw std::runtime_error("Fixture open failed");sqlite3_exec(future,"PRAGMA user_version=99",nullptr,nullptr,nullptr);sqlite3_close(future);
bool futureRejected=false;try{ioscope::Store unknown(persistent);}catch(const std::exception&){futureRejected=true;}if(!futureRejected)throw std::runtime_error("Future database accepted");
const auto broken=(directory/"corrupt.sqlite").string();{std::ofstream output(broken);output<<"Not a SQLite database";}
bool corruptRejected=false;try{ioscope::Store corrupt(broken);}catch(const std::exception&){corruptRejected=true;}if(!corruptRejected)throw std::runtime_error("Corruption accepted");
wchar_t executable[32768]{};if(!GetModuleFileNameW(nullptr,executable,32768))throw std::runtime_error("Test executable unavailable");
const auto crashPath=directory/"crash.sqlite";std::wstring command=L"\""+std::wstring(executable)+L"\" --crash \""+crashPath.wstring()+L"\"";
STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
if(!CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process))throw std::runtime_error("Crash fixture failed to launch");
const auto waited=WaitForSingleObject(process.hProcess,5000);if(waited!=WAIT_OBJECT_0)TerminateProcess(process.hProcess,100);DWORD exitCode=0;GetExitCodeProcess(process.hProcess,&exitCode);CloseHandle(process.hThread);CloseHandle(process.hProcess);
if(waited!=WAIT_OBJECT_0||exitCode!=99)throw std::runtime_error("Crash fixture failed");
{ioscope::Store recovered(crashPath.string());auto rows=recovered.list();if(recovered.pending().size()!=1||recovered.request("crash-request")["runId"]!="crash-run"||rows.size()!=1||recovered.get(rows[0]["id"])!="{\"durable\":true}")throw std::runtime_error("WAL crash recovery failed");}
std::cout<<"PASS: SQLite migration, roundtrip, deletion, missing record, SQL parameterization\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what();return 1;}}
