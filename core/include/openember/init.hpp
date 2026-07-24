#pragma once

#include <memory>
#include <string>

#include "openember/context.hpp"

namespace openember {

void Init(int argc, char** argv);
void Init(const std::string& robot_id);
void Init(const RuntimeOptions& options);
void Init(const ContextOptions& options);
void Shutdown();
bool Ok();

std::shared_ptr<Context> GetGlobalContext();

}  // namespace openember
