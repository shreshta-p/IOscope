#include <crow.h>
#include <nlohmann/json.hpp>
#include <sqlite3.h>

#include <array>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>

namespace {
struct DatabaseCloser {
  void operator()(sqlite3* database) const noexcept { sqlite3_close(database); }
};
struct StatementCloser {
  void operator()(sqlite3_stmt* statement) const noexcept { sqlite3_finalize(statement); }
};

void check_sqlite() {
  sqlite3* raw_database = nullptr;
  const auto open_result = sqlite3_open(":memory:", &raw_database);
  const std::unique_ptr<sqlite3, DatabaseCloser> database(raw_database);
  if (open_result != SQLITE_OK) {
    throw std::runtime_error("SQLite in-memory open failed");
  }
  sqlite3_stmt* raw_statement = nullptr;
  const auto prepare_result = sqlite3_prepare_v2(
    database.get(), "SELECT 6 * 7", -1, &raw_statement, nullptr);
  const std::unique_ptr<sqlite3_stmt, StatementCloser> statement(raw_statement);
  if (prepare_result != SQLITE_OK || sqlite3_step(statement.get()) != SQLITE_ROW ||
      sqlite3_column_int(statement.get(), 0) != 42) {
    throw std::runtime_error("SQLite query verification failed");
  }
}
} // namespace

int main() {
  try {
    static_assert(__cplusplus >= 202002L);
    const std::array values{20, 22};
    const std::span<const int> view(values);
    if (view.front() + view.back() != 42) {
      throw std::runtime_error("C++20 span verification failed");
    }
    check_sqlite();

    crow::SimpleApp app;
    app.bindaddr("127.0.0.1").port(0);
    CROW_ROUTE(app, "/health")([] {
      return crow::response(200, nlohmann::json({{"status", "preflight"}}).dump());
    });
    // Compile the WebSocket route machinery, but admit no connection.
    CROW_WEBSOCKET_ROUTE(app, "/preflight-websocket")
      .onaccept([](const crow::request&, void**) { return false; });
    app.validate();
    crow::request request;
    request.url = "/health";
    crow::response response;
    app.handle_full(request, response);
    if (response.code != 200 ||
        nlohmann::json::parse(response.body).at("status") != "preflight") {
      throw std::runtime_error("HTTP health handler verification failed");
    }
    // No app.run(): this executable never listens or starts a background service.
    std::cout << "PASS: C++20 span; SQLite " << sqlite3_libversion()
              << " in-memory query; HTTP health handler; WebSocket route compile\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
