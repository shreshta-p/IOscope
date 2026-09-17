#pragma once
#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
namespace ioscope {
class Store {
  struct DbCloser{void operator()(sqlite3* p)const noexcept{sqlite3_close(p);}};
  struct StmtCloser{void operator()(sqlite3_stmt* p)const noexcept{sqlite3_finalize(p);}};
  std::unique_ptr<sqlite3,DbCloser> db_;
  std::mutex mutex_;
  using Statement=std::unique_ptr<sqlite3_stmt,StmtCloser>;
  Statement prepare(const char* sql){sqlite3_stmt* raw=nullptr;if(sqlite3_prepare_v2(db_.get(),sql,-1,&raw,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db_.get()));return Statement(raw);}
  void exec(const char* sql){if(sqlite3_exec(db_.get(),sql,nullptr,nullptr,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db_.get()));}
  static std::string text(sqlite3_stmt* s,int col){const auto* p=sqlite3_column_text(s,col);return p?reinterpret_cast<const char*>(p):"";}
  static void bind(sqlite3_stmt* s,int index,const std::string& value){if(sqlite3_bind_text(s,index,value.data(),static_cast<int>(value.size()),SQLITE_TRANSIENT)!=SQLITE_OK)throw std::runtime_error("SQLite bind failed");}
  static void done(sqlite3_stmt* statement){if(sqlite3_step(statement)!=SQLITE_DONE)throw std::runtime_error("Run journal write failed");}
  struct Transaction {
    Store& store; bool committed=false;
    explicit Transaction(Store& value):store(value){store.exec("BEGIN IMMEDIATE");}
    void commit(){store.exec("COMMIT");committed=true;}
    ~Transaction(){if(!committed)sqlite3_exec(store.db_.get(),"ROLLBACK",nullptr,nullptr,nullptr);}
  };
public:
  explicit Store(const std::string& path){sqlite3* raw=nullptr;const int code=sqlite3_open_v2(path.c_str(),&raw,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,nullptr);db_.reset(raw);if(code!=SQLITE_OK)throw std::runtime_error("Cannot open run database");sqlite3_busy_timeout(db_.get(),2000);
    auto version=prepare("PRAGMA user_version");if(sqlite3_step(version.get())!=SQLITE_ROW||sqlite3_column_int(version.get(),0)>3)throw std::runtime_error("Unsupported database version");version.reset();
    exec("PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL; PRAGMA foreign_keys=ON;");
    Transaction migration(*this);
    exec("CREATE TABLE IF NOT EXISTS recordings(id TEXT PRIMARY KEY,origin TEXT NOT NULL,name TEXT NOT NULL,started TEXT NOT NULL,payload TEXT NOT NULL);"
      "CREATE TABLE IF NOT EXISTS workload_requests(request_id TEXT PRIMARY KEY,definition TEXT NOT NULL,run_id TEXT NOT NULL UNIQUE,status TEXT NOT NULL,recording_id TEXT);"
      "CREATE TABLE IF NOT EXISTS journal_runs(run_id TEXT PRIMARY KEY REFERENCES workload_requests(run_id),metadata TEXT NOT NULL);"
      "CREATE TABLE IF NOT EXISTS journal_samples(run_id TEXT NOT NULL REFERENCES journal_runs(run_id) ON DELETE CASCADE,sequence INTEGER NOT NULL,payload TEXT NOT NULL,PRIMARY KEY(run_id,sequence));"
      "CREATE TABLE IF NOT EXISTS artifacts(id TEXT PRIMARY KEY,recording_id TEXT NOT NULL REFERENCES recordings(id) ON DELETE CASCADE,kind TEXT NOT NULL,sha256 TEXT NOT NULL,payload TEXT NOT NULL);"
      "CREATE TABLE IF NOT EXISTS experiment_executions(id TEXT PRIMARY KEY,definition_id TEXT NOT NULL,started TEXT NOT NULL,status TEXT NOT NULL,payload TEXT NOT NULL);"
      "CREATE TABLE IF NOT EXISTS experiment_requests(request_id TEXT PRIMARY KEY,definition TEXT NOT NULL,execution_id TEXT NOT NULL UNIQUE);"
      "PRAGMA user_version=3;");migration.commit();}
  // Request tombstones survive history deletion: retrying an old request never starts work.
  nlohmann::json request(const std::string& requestId){std::lock_guard<std::mutex> lock(mutex_);auto stmt=prepare("SELECT definition,run_id,status,recording_id FROM workload_requests WHERE request_id=?");bind(stmt.get(),1,requestId);const int rc=sqlite3_step(stmt.get());if(rc==SQLITE_DONE)return nullptr;if(rc!=SQLITE_ROW)throw std::runtime_error("Request lookup failed");return {{"definition",nlohmann::json::parse(text(stmt.get(),0))},{"runId",text(stmt.get(),1)},{"status",nlohmann::json::parse(text(stmt.get(),2))},{"recordingId",sqlite3_column_type(stmt.get(),3)==SQLITE_NULL?nlohmann::json(nullptr):nlohmann::json(text(stmt.get(),3))}};}
  void begin_run(const std::string& requestId,const nlohmann::json& metadata,const nlohmann::json& sample){
    std::lock_guard<std::mutex> lock(mutex_);Transaction transaction(*this);
    auto active=prepare("SELECT 1 FROM journal_runs LIMIT 1");if(sqlite3_step(active.get())!=SQLITE_DONE)throw std::runtime_error("An unfinished native run already exists");active.reset();
    auto request=prepare("INSERT INTO workload_requests VALUES(?,?,?,?,NULL)");bind(request.get(),1,requestId);bind(request.get(),2,metadata.at("workload").dump());bind(request.get(),3,metadata.at("runId"));bind(request.get(),4,sample.at("workloadStatus").dump());done(request.get());
    auto run=prepare("INSERT INTO journal_runs VALUES(?,?)");bind(run.get(),1,metadata.at("runId"));bind(run.get(),2,metadata.dump());done(run.get());
    append_unlocked(metadata.at("runId"),sample);transaction.commit();
  }
private:
  void append_unlocked(const std::string& runId,const nlohmann::json& sample){
    const auto sequence=sample.at("telemetry").at("sequence").get<std::int64_t>();
    auto previous=prepare("SELECT MAX(sequence) FROM journal_samples WHERE run_id=?");bind(previous.get(),1,runId);if(sqlite3_step(previous.get())!=SQLITE_ROW)throw std::runtime_error("Journal sequence lookup failed");if(sqlite3_column_type(previous.get(),0)!=SQLITE_NULL&&sequence<=sqlite3_column_int64(previous.get(),0))throw std::runtime_error("Journal sequence did not advance");previous.reset();
    auto stmt=prepare("INSERT INTO journal_samples VALUES(?,?,?)");bind(stmt.get(),1,runId);sqlite3_bind_int64(stmt.get(),2,sequence);bind(stmt.get(),3,sample.dump());done(stmt.get());
    auto status=prepare("UPDATE workload_requests SET status=? WHERE run_id=?");bind(status.get(),1,sample.at("workloadStatus").dump());bind(status.get(),2,runId);done(status.get());
  }
public:
  void append_sample(const std::string& runId,const nlohmann::json& sample){std::lock_guard<std::mutex> lock(mutex_);Transaction transaction(*this);append_unlocked(runId,sample);transaction.commit();}
  nlohmann::json pending(){std::lock_guard<std::mutex> lock(mutex_);auto result=nlohmann::json::array();auto runs=prepare("SELECT run_id,metadata FROM journal_runs ORDER BY rowid");int rc;while((rc=sqlite3_step(runs.get()))==SQLITE_ROW){const auto id=text(runs.get(),0);auto samples=nlohmann::json::array();auto rows=prepare("SELECT payload FROM journal_samples WHERE run_id=? ORDER BY sequence");bind(rows.get(),1,id);int sampleRc;while((sampleRc=sqlite3_step(rows.get()))==SQLITE_ROW)samples.push_back(nlohmann::json::parse(text(rows.get(),0)));if(sampleRc!=SQLITE_DONE)throw std::runtime_error("Journal samples unavailable");result.push_back({{"schemaVersion","1.0.0"},{"metadata",nlohmann::json::parse(text(runs.get(),1))},{"samples",samples}});}if(rc!=SQLITE_DONE)throw std::runtime_error("Journal read failed");return result;}
  std::string finish_run(const nlohmann::json& recording,const nlohmann::json& artifacts=nlohmann::json::array()){
    std::lock_guard<std::mutex> lock(mutex_);Transaction transaction(*this);const auto& metadata=recording.at("metadata");const std::string runId=metadata.at("runId");
    auto exists=prepare("SELECT 1 FROM journal_runs WHERE run_id=?");bind(exists.get(),1,runId);if(sqlite3_step(exists.get())!=SQLITE_ROW)throw std::runtime_error("Run journal not found");exists.reset();
    auto idQuery=prepare("SELECT lower(hex(randomblob(16)))");if(sqlite3_step(idQuery.get())!=SQLITE_ROW)throw std::runtime_error("Recording ID unavailable");const auto id=text(idQuery.get(),0);idQuery.reset();
    auto save=prepare("INSERT INTO recordings VALUES(?,?,?,?,?)");bind(save.get(),1,id);bind(save.get(),2,metadata.at("origin"));bind(save.get(),3,metadata.at("workload").at("definitionId"));bind(save.get(),4,metadata.at("startedAt"));bind(save.get(),5,recording.dump());done(save.get());
    for(const auto& artifact:artifacts){auto stmt=prepare("INSERT INTO artifacts VALUES(?,?,?,?,?)");bind(stmt.get(),1,artifact.at("artifactId"));bind(stmt.get(),2,id);bind(stmt.get(),3,artifact.at("kind"));bind(stmt.get(),4,artifact.at("sha256"));bind(stmt.get(),5,artifact.at("payload"));done(stmt.get());}
    auto update=prepare("UPDATE workload_requests SET status=?,recording_id=? WHERE run_id=?");bind(update.get(),1,recording.at("samples").back().at("workloadStatus").dump());bind(update.get(),2,id);bind(update.get(),3,runId);done(update.get());
    auto remove=prepare("DELETE FROM journal_runs WHERE run_id=?");bind(remove.get(),1,runId);done(remove.get());transaction.commit();return id;
  }
  nlohmann::json artifact(const std::string& id){std::lock_guard<std::mutex> lock(mutex_);auto stmt=prepare("SELECT kind,sha256,payload FROM artifacts WHERE id=?");bind(stmt.get(),1,id);if(sqlite3_step(stmt.get())!=SQLITE_ROW)throw std::runtime_error("Artifact not found");return {{"artifactId",id},{"kind",text(stmt.get(),0)},{"sha256",text(stmt.get(),1)},{"payload",text(stmt.get(),2)}};}
  std::string save(const std::string& origin,const std::string& name,const std::string& started,const std::string& payload){
    std::lock_guard<std::mutex> lock(mutex_);auto idQuery=prepare("SELECT lower(hex(randomblob(16)))");if(sqlite3_step(idQuery.get())!=SQLITE_ROW)throw std::runtime_error("ID allocation failed");auto id=text(idQuery.get(),0);
    auto stmt=prepare("INSERT INTO recordings VALUES(?,?,?,?,?)");bind(stmt.get(),1,id);bind(stmt.get(),2,origin);bind(stmt.get(),3,name);bind(stmt.get(),4,started);bind(stmt.get(),5,payload);if(sqlite3_step(stmt.get())!=SQLITE_DONE)throw std::runtime_error("Run write failed");return id;}
  nlohmann::json list(){std::lock_guard<std::mutex> lock(mutex_);auto stmt=prepare("SELECT id,origin,name,started,length(payload) FROM recordings ORDER BY rowid DESC LIMIT 100");auto values=nlohmann::json::array();int rc;while((rc=sqlite3_step(stmt.get()))==SQLITE_ROW)values.push_back({{"id",text(stmt.get(),0)},{"origin",text(stmt.get(),1)},{"name",text(stmt.get(),2)},{"startedAt",text(stmt.get(),3)},{"bytes",sqlite3_column_int64(stmt.get(),4)}});if(rc!=SQLITE_DONE)throw std::runtime_error("History read failed");return values;}
  std::string get(const std::string& id){std::lock_guard<std::mutex> lock(mutex_);auto stmt=prepare("SELECT payload FROM recordings WHERE id=?");bind(stmt.get(),1,id);if(sqlite3_step(stmt.get())!=SQLITE_ROW)throw std::runtime_error("Recording not found");return text(stmt.get(),0);}
  void remove(const std::string& id){std::lock_guard<std::mutex> lock(mutex_);auto stmt=prepare("DELETE FROM recordings WHERE id=?");bind(stmt.get(),1,id);if(sqlite3_step(stmt.get())!=SQLITE_DONE)throw std::runtime_error("Run deletion failed");}
  // Same tombstone pattern as `request()`/`begin_run` for the single-workload
  // path: retrying an old experiment start request never launches a second
  // experiment, even after the original execution has finished.
  nlohmann::json experiment_request(const std::string& requestId){std::lock_guard<std::mutex> lock(mutex_);auto stmt=prepare("SELECT definition,execution_id FROM experiment_requests WHERE request_id=?");bind(stmt.get(),1,requestId);const int rc=sqlite3_step(stmt.get());if(rc==SQLITE_DONE)return nullptr;if(rc!=SQLITE_ROW)throw std::runtime_error("Experiment request lookup failed");return {{"definition",nlohmann::json::parse(text(stmt.get(),0))},{"executionId",text(stmt.get(),1)}};}
  void begin_experiment_request(const std::string& requestId,const nlohmann::json& definition,const std::string& executionId){std::lock_guard<std::mutex> lock(mutex_);auto stmt=prepare("INSERT INTO experiment_requests VALUES(?,?,?)");bind(stmt.get(),1,requestId);bind(stmt.get(),2,definition.dump());bind(stmt.get(),3,executionId);done(stmt.get());}
  // Experiment executions are a durable progress index, not a wire duplicate of
  // each phase's RunRecording (those stay in `recordings`, linked by
  // experimentExecutionId/phaseId). One row is upserted after every phase
  // transition, so a crash mid-experiment leaves an accurate last-known status.
  void save_experiment(const nlohmann::json& execution){
    std::lock_guard<std::mutex> lock(mutex_);
    auto stmt=prepare("INSERT INTO experiment_executions VALUES(?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET status=excluded.status,payload=excluded.payload");
    bind(stmt.get(),1,execution.at("executionId"));bind(stmt.get(),2,execution.at("definitionId"));bind(stmt.get(),3,execution.at("startedAt"));bind(stmt.get(),4,execution.at("status"));bind(stmt.get(),5,execution.dump());done(stmt.get());
  }
  nlohmann::json experiment(const std::string& id){std::lock_guard<std::mutex> lock(mutex_);auto stmt=prepare("SELECT payload FROM experiment_executions WHERE id=?");bind(stmt.get(),1,id);if(sqlite3_step(stmt.get())!=SQLITE_ROW)throw std::runtime_error("Experiment execution not found");return nlohmann::json::parse(text(stmt.get(),0));}
  nlohmann::json experiments(){std::lock_guard<std::mutex> lock(mutex_);auto stmt=prepare("SELECT id,definition_id,started,status FROM experiment_executions ORDER BY rowid DESC LIMIT 100");auto values=nlohmann::json::array();int rc;while((rc=sqlite3_step(stmt.get()))==SQLITE_ROW)values.push_back({{"executionId",text(stmt.get(),0)},{"definitionId",text(stmt.get(),1)},{"startedAt",text(stmt.get(),2)},{"status",text(stmt.get(),3)}});if(rc!=SQLITE_DONE)throw std::runtime_error("Experiment history read failed");return values;}
  // Recovery reads every execution still marked "running" from a prior process;
  // ExperimentController::recover() closes each out as "interrupted" the same
  // way WorkloadController::recover() closes out an orphaned run journal.
  nlohmann::json running_experiments(){std::lock_guard<std::mutex> lock(mutex_);auto stmt=prepare("SELECT payload FROM experiment_executions WHERE status='running'");auto values=nlohmann::json::array();int rc;while((rc=sqlite3_step(stmt.get()))==SQLITE_ROW)values.push_back(nlohmann::json::parse(text(stmt.get(),0)));if(rc!=SQLITE_DONE)throw std::runtime_error("Experiment recovery read failed");return values;}
};
}
