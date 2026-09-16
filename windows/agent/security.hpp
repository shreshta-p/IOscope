#pragma once
#include <crow.h>
#include "platform.hpp"
#include <nlohmann/json.hpp>
namespace ioscope {
inline bool allowed_origin(const std::string& origin){return origin.empty()||origin=="http://127.0.0.1:5173"||origin=="http://127.0.0.1:8765";}
struct Security{
 struct context{};
 std::string token=random_id();
 void before_handle(crow::request& request,crow::response& response,context&){
  const auto origin=request.get_header_value("Origin"),host=request.get_header_value("Host");
  if(host!="127.0.0.1:8765"||!allowed_origin(origin)){response.code=403;response.end();return;}
  // Crow does not call this middleware's after_handle when before_handle ends
  // a request. Preflight and authorization failures need these headers too.
  if(!origin.empty()){response.set_header("Access-Control-Allow-Origin",origin);response.set_header("Vary","Origin");response.set_header("Access-Control-Allow-Headers","Authorization, Content-Type");response.set_header("Access-Control-Allow-Methods","GET, POST, DELETE, OPTIONS");}
  response.set_header("X-Content-Type-Options","nosniff");response.set_header("Cache-Control","no-store");
  if(request.method==crow::HTTPMethod::Options){response.code=204;response.end();return;}
  if(request.body.size()>32*1024*1024){response.code=413;response.end();return;}
  if(request.url.rfind("/api/v1/",0)==0&&request.url!="/api/v1/bootstrap"&&request.url!="/api/v1/stream"&&request.get_header_value("Authorization")!="Bearer "+token){response.code=401;response.end();}
 }
 void after_handle(crow::request& request,crow::response& response,context&){
  const auto origin=request.get_header_value("Origin");if(!origin.empty()&&allowed_origin(origin)){response.set_header("Access-Control-Allow-Origin",origin);response.set_header("Vary","Origin");response.set_header("Access-Control-Allow-Headers","Authorization, Content-Type");response.set_header("Access-Control-Allow-Methods","GET, POST, DELETE, OPTIONS");}
  response.set_header("X-Content-Type-Options","nosniff");response.set_header("Cache-Control","no-store");
 }
};
}
