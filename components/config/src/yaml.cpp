#include "openember/config/yaml.hpp"
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <set>
#include <utility>

namespace openember::config {
namespace detail {
struct Value {
    Kind kind = Kind::kNull;
    int line = 0, column = 0;
    std::string scalar;
    std::vector<std::pair<std::string, std::shared_ptr<const Value>>> map;
    std::vector<std::shared_ptr<const Value>> sequence;
};
}
namespace {
std::string Location(const std::string& source, const std::string& path, int line, int column) {
    return source + (line ? ":" + std::to_string(line) + ":" + std::to_string(column) : "") + " " + path;
}
std::string ChildPath(const std::string& parent, std::string_view key) {
    std::string escaped;
    for (const char c : key) {
        if (c == '"' || c == '\\') escaped += '\\';
        escaped += c;
    }
    return parent + "[\"" + escaped + "\"]";
}
void ValidateOptions(const LoadOptions& options) {
    if (!options.max_bytes || !options.max_depth || !options.max_nodes || options.max_depth > 128)
        throw std::invalid_argument("configuration limits must be positive; max_depth must be <=128");
}
struct Builder {
    const std::string& source;
    LoadOptions options;
    std::size_t nodes = 0;
    [[noreturn]] void Error(const YAML::Node& node, const std::string& path,
                           ErrorCode code, const std::string& message) const {
        const auto mark = node.Mark();
        throw ConfigError(code, source, path, message, mark.is_null() ? 0 : mark.line + 1, mark.is_null() ? 0 : mark.column + 1);
    }
    std::shared_ptr<const detail::Value> Build(const YAML::Node& node, const std::string& path, std::size_t depth) {
        if (depth > options.max_depth || ++nodes > options.max_nodes)
            Error(node, path, ErrorCode::kLimit, "expanded configuration exceeds depth/node limit (possibly cyclic alias)");
        const auto tag = node.Tag();
        if (!tag.empty() && tag != "?" && tag != "!")
            Error(node, path, ErrorCode::kParse, "explicit YAML tags are unsupported");
        auto value = std::make_shared<detail::Value>();
        const auto mark = node.Mark();
        if (!mark.is_null()) { value->line = mark.line + 1; value->column = mark.column + 1; }
        if (node.IsNull()) value->kind = Kind::kNull;
        else if (node.IsScalar()) { value->kind = Kind::kScalar; value->scalar = node.Scalar(); }
        else if (node.IsSequence()) {
            value->kind = Kind::kSequence;
            for (std::size_t i = 0; i < node.size(); ++i)
                value->sequence.push_back(Build(node[i], path + "[" + std::to_string(i) + "]", depth + 1));
        } else if (node.IsMap()) {
            value->kind = Kind::kMap;
            std::set<std::string> keys;
            for (const auto& entry : node) {
                if (!entry.first.IsScalar()) Error(entry.first, path, ErrorCode::kParse, "map keys must be scalars");
                const auto key = entry.first.Scalar(); const auto child = ChildPath(path, key);
                const auto key_tag = entry.first.Tag();
                if (key_tag != "?" && key_tag != "!" && !key_tag.empty())
                    Error(entry.first, child, ErrorCode::kParse, "explicit YAML key tags are unsupported");
                if (key == "<<") Error(entry.first, child, ErrorCode::kParse, "YAML merge keys are unsupported");
                if (!keys.insert(key).second) Error(entry.first, child, ErrorCode::kDuplicateKey, "duplicate key");
                value->map.emplace_back(key, Build(entry.second, child, depth + 1));
            }
        } else Error(node, path, ErrorCode::kParse, "unsupported YAML node");
        return value;
    }
};
}
ConfigError::ConfigError(ErrorCode code, std::string source, std::string path, std::string message, int line, int column)
    : std::runtime_error(Location(source, path, line, column) + ": " + message), code_(code),
      source_(std::move(source)), path_(std::move(path)), line_(line), column_(column) {}
Node::Node(std::shared_ptr<const detail::Value> value, std::string source, std::string path)
    : value_(std::move(value)), source_(std::move(source)), path_(std::move(path)) {}
Kind Node::Type() const noexcept { return value_->kind; }
int Node::Line() const noexcept { return value_->line; }
int Node::Column() const noexcept { return value_->column; }
[[noreturn]] void Node::Fail(const std::string& message, ErrorCode code) const {
    throw ConfigError(code, source_, path_, message, Line(), Column());
}
void Node::Require(bool condition, const std::string& message) const { if (!condition) Fail(message); }
void Node::RequireKind(Kind kind, const char* expected) const {
    if (Type() != kind) Fail(std::string("expected ") + expected, ErrorCode::kType);
}
std::optional<Node> Node::Find(std::string_view key) const {
    RequireKind(Kind::kMap, "map");
    for (const auto& entry : value_->map)
        if (entry.first == key) return Node(entry.second, source_, ChildPath(path_, key));
    return std::nullopt;
}
Node Node::At(std::string_view key) const {
    const auto found = Find(key);
    if (!found) throw ConfigError(ErrorCode::kMissing, source_, ChildPath(path_, key), "required field is missing", Line(), Column());
    return *found;
}
Node Node::At(std::size_t index) const {
    RequireKind(Kind::kSequence, "sequence");
    const auto path = path_ + "[" + std::to_string(index) + "]";
    if (index >= value_->sequence.size()) throw ConfigError(ErrorCode::kMissing, source_, path, "sequence index out of range", Line(), Column());
    return Node(value_->sequence[index], source_, path);
}
std::size_t Node::Size() const {
    if (Type() == Kind::kSequence) return value_->sequence.size();
    RequireKind(Kind::kMap, "map or sequence"); return value_->map.size();
}
std::vector<std::string> Node::Keys() const {
    RequireKind(Kind::kMap, "map");
    std::vector<std::string> keys;
    for (const auto& entry : value_->map) keys.push_back(entry.first);
    return keys;
}
void Node::CheckKeys(std::initializer_list<std::string_view> allowed) const {
    for (const auto& key : Keys())
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) At(key).Fail("unknown field", ErrorCode::kUnknownKey);
}
const std::string& Node::Scalar() const { RequireKind(Kind::kScalar, "scalar"); return value_->scalar; }
bool Node::Boolean() const {
    const auto& value = Scalar();
    if (value == "true") return true;
    if (value == "false") return false;
    Fail("expected true or false", ErrorCode::kType);
}
std::int64_t Node::Signed() const {
    const auto& value = Scalar(); std::int64_t output = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), output, 10);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) Fail("expected decimal signed integer", ErrorCode::kType);
    return output;
}
std::uint64_t Node::Unsigned() const {
    const auto& value = Scalar(); std::uint64_t output = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), output, 10);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) Fail("expected decimal unsigned integer", ErrorCode::kType);
    return output;
}
double Node::Number() const {
    const auto& value = Scalar(); double output = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), output, std::chars_format::general);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || !std::isfinite(output))
        Fail("expected finite number", ErrorCode::kType);
    return output;
}
Node ParseYaml(std::string_view text, const std::string& source, LoadOptions options) {
    ValidateOptions(options);
    if (text.size() > options.max_bytes) throw ConfigError(ErrorCode::kLimit, source, "$", "input exceeds byte limit");
    try {
        const auto documents = YAML::LoadAll(std::string(text));
        if (documents.size() != 1) throw ConfigError(ErrorCode::kParse, source, "$", "expected exactly one YAML document");
        Builder builder{source, options};
        return Node(builder.Build(documents.front(), "$", 1), source, "$");
    } catch (const YAML::Exception& error) {
        throw ConfigError(ErrorCode::kParse, source, "$", error.msg,
                          error.mark.is_null() ? 0 : error.mark.line + 1, error.mark.is_null() ? 0 : error.mark.column + 1);
    }
}
Node LoadYamlFile(const std::string& path, LoadOptions options) {
    ValidateOptions(options);
    std::ifstream input(path, std::ios::binary);
    if (!input) throw ConfigError(ErrorCode::kIo, path, "$", "cannot open configuration file");
    std::string text; char buffer[4096];
    while (input.read(buffer, sizeof(buffer)) || input.gcount()) {
        const auto count = static_cast<std::size_t>(input.gcount());
        if (count > options.max_bytes - text.size()) throw ConfigError(ErrorCode::kLimit, path, "$", "input exceeds byte limit");
        text.append(buffer, count);
    }
    if (input.bad() || (!input.eof() && input.fail())) throw ConfigError(ErrorCode::kIo, path, "$", "cannot read configuration file");
    return ParseYaml(text, path, options);
}
}  // namespace openember::config
