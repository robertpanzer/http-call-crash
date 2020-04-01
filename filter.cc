#include <string>
#include <unordered_map>

#include "proxy_wasm_intrinsics.h"

class CrashFilterRootContext : public RootContext {
public:
  explicit CrashFilterRootContext(uint32_t id, StringView root_id) : RootContext(id, root_id) {}
};

class CrashFilterContext : public Context {
public:
  explicit CrashFilterContext(uint32_t id, RootContext* root) : Context(id, root), root_(static_cast<CrashFilterRootContext *>(static_cast<void*>(root))) {}

  FilterHeadersStatus onRequestHeaders(uint32_t headers) override;
private:
  CrashFilterRootContext * root_;
};
static RegisterContextFactory register_AddHeaderContext(CONTEXT_FACTORY(CrashFilterContext),
                                                        ROOT_FACTORY(CrashFilterRootContext));


FilterHeadersStatus call(Context* context) {

  auto callback = [context](uint32_t, size_t body_size, uint32_t) {
    LOG_WARN("Called callback");
    auto response_headers =
        getHeaderMapPairs(HeaderMapType::HttpCallResponseHeaders);
    for (auto &p : response_headers->pairs()) {
      LOG_INFO(std::string(p.first) + std::string(" -> ") +
               std::string(p.second));
    }
    auto body_ptr =
        getBufferBytes(BufferType::HttpCallResponseBody, 0, body_size);
    context->setEffectiveContext();
    sendLocalResponse(555, "no details", body_ptr->toString(), {});
  };

  HeaderStringPairs headers {};
  if (getRequestHeader("withheader")->toString() == "yes") {
    LOG_WARN("With headers");
    headers = {
        {":authority", "externalserver"},
        {"content-type", "application/json"},
        {"accept", "application/json"},
        {":path", "/get"},
        {":method", "GET"},
    };
  }

  std::string request_body {R"(
{
  "Hello": "World",
  "Foo": "Bar"
}
)"};

  auto result = context->root()->httpCall("externalserver", headers, request_body, {}, 5000,
                                 callback);
  if (result != WasmResult::Ok) {
    LOG_WARN("Sending failed");
    sendLocalResponse(403,
                      "failed", "failed", {},
                      GrpcStatus::Unauthenticated);
  }
  return FilterHeadersStatus::StopIteration;
}

FilterHeadersStatus CrashFilterContext::onRequestHeaders(uint32_t) {
  LOG_DEBUG(std::string("onRequestHeaders ") + std::to_string(id()));
  return call(this);
}
