#include "openember/init.hpp"

#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

namespace openember {
namespace {

std::shared_ptr<Context> g_context;
std::mutex g_context_mutex;

void InitContextLocked(std::shared_ptr<Context> context) {
    if (g_context && g_context->Ok()) {
        return;
    }

    g_context = std::move(context);
    if (!g_context->Init()) {
        throw std::runtime_error("failed to initialize OpenEmber context");
    }
}

}  // namespace

void Init(int argc, char** argv) {
    (void)argc;
    (void)argv;

    RuntimeOptions options;
    Init(options);
}

void Init(const std::string& robot_id) {
    RuntimeOptions options;
    options.robot_id = robot_id;
    Init(options);
}

void Init(const RuntimeOptions& options) {
    std::lock_guard<std::mutex> lock(g_context_mutex);
    InitContextLocked(std::make_shared<Context>(options));
}

void Init(const ContextOptions& options) {
    std::lock_guard<std::mutex> lock(g_context_mutex);
    InitContextLocked(std::make_shared<Context>(options));
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(g_context_mutex);

    if (g_context) {
        g_context->Shutdown();
        g_context.reset();
    }
}

bool Ok() {
    std::lock_guard<std::mutex> lock(g_context_mutex);
    return g_context && g_context->Ok();
}

std::shared_ptr<Context> GetGlobalContext() {
    std::lock_guard<std::mutex> lock(g_context_mutex);

    if (!g_context) {
        throw std::runtime_error("OpenEmber is not initialized");
    }

    return g_context;
}

}  // namespace openember
