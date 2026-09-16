#pragma once
#include <nlohmann/json.hpp>
#include <set>
#include <vector>
#include <stdexcept>
namespace ioscope {
inline nlohmann::json parse_input(const std::string& input) {
 if(input.size()>32*1024*1024)throw std::runtime_error("JSON body exceeds 32 MiB");
 std::vector<std::set<std::string>> keys(34);
 return nlohmann::json::parse(input,[&](int depth,nlohmann::json::parse_event_t event,nlohmann::json& value){
  if(depth>32)throw std::runtime_error("JSON nesting exceeds 32 levels");
  if(event==nlohmann::json::parse_event_t::object_start)keys.at(static_cast<size_t>(depth)+1).clear();
  if(event==nlohmann::json::parse_event_t::key&&!keys.at(static_cast<size_t>(depth)).insert(value.get<std::string>()).second)throw std::runtime_error("Duplicate JSON property");
  return true;
 });
}
inline nlohmann::json parse_command(const std::string& input){if(input.size()>65536)throw std::runtime_error("Command body exceeds 64 KiB");return parse_input(input);}
}
