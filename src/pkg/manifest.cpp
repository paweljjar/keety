#include <keety/pkg/manifest.hpp>
#include <keety/pkg/diagnostics.hpp>

#include <keet/json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace keety::pkg {

namespace {

using JsonNode = keet::json::detail::JsonNode;

std::string formatContext(const std::filesystem::path& path, std::string_view field) {
    std::string prefix = path.empty() ? "" : path.filename().string() + ": ";
    if (field.empty()) return prefix;
    return prefix + std::string(field);
}

const JsonNode::Object& requireObject(const JsonNode& node, const std::filesystem::path& path, std::string_view field = {}) {
    if (!node.is_object()) {
        std::string ctx = formatContext(path, field);
        throw PkgError(
            ErrorCode::InvalidFieldType,
            ctx + (field.empty() ? "root must be a JSON object" : "must be a JSON object")
        );
    }
    return std::get<JsonNode::Object>(node.value);
}

std::optional<std::string> getStringField(
    const JsonNode::Object& obj,
    const std::string& key,
    const std::filesystem::path& path
) {
    auto it = obj.find(key);
    if (it == obj.end()) return std::nullopt;
    if (!it->second.is_string()) {
        throw PkgError(
            ErrorCode::InvalidFieldType,
            formatContext(path, key) + " must be a string"
        );
    }
    return std::get<std::string>(it->second.value);
}

std::map<std::string, SourceSpec> parseDependencyMap(
    const JsonNode::Object& root,
    const std::string& fieldName,
    const std::filesystem::path& path
) {
    std::map<std::string, SourceSpec> result;
    auto it = root.find(fieldName);
    if (it == root.end()) return result;

    if (!it->second.is_object()) {
        throw PkgError(
            ErrorCode::InvalidFieldType,
            formatContext(path, fieldName) + " must be a JSON object mapping names to source specs"
        );
    }

    const auto& mapObj = std::get<JsonNode::Object>(it->second.value);
    for (const auto& [depName, depVal] : mapObj) {
        validatePackageName(depName, formatContext(path, fieldName + "." + depName));
        if (!depVal.is_string()) {
            throw PkgError(
                ErrorCode::InvalidFieldType,
                formatContext(path, fieldName + "." + depName) + " must be a string source spec"
            );
        }
        const auto& specStr = std::get<std::string>(depVal.value);
        result.emplace(depName, SourceSpec::parse(specStr, formatContext(path, fieldName + "." + depName)));
    }
    return result;
}

std::map<std::string, std::string> parseScriptsMap(
    const JsonNode::Object& root,
    const std::filesystem::path& path
) {
    std::map<std::string, std::string> result;
    auto it = root.find("scripts");
    if (it == root.end()) return result;

    if (!it->second.is_object()) {
        throw PkgError(
            ErrorCode::InvalidFieldType,
            formatContext(path, "scripts") + " must be a JSON object mapping script names to commands"
        );
    }

    const auto& mapObj = std::get<JsonNode::Object>(it->second.value);
    for (const auto& [scriptName, scriptCmd] : mapObj) {
        if (scriptName.empty()) {
            throw PkgError(
                ErrorCode::InvalidFieldType,
                formatContext(path, "scripts") + ": script name cannot be empty"
            );
        }
        if (!scriptCmd.is_string()) {
            throw PkgError(
                ErrorCode::InvalidFieldType,
                formatContext(path, "scripts." + scriptName) + " must be a string command"
            );
        }
        result.emplace(scriptName, std::get<std::string>(scriptCmd.value));
    }
    return result;
}

std::vector<std::string> parseWorkspacesArray(
    const JsonNode::Object& root,
    const std::filesystem::path& path
) {
    std::vector<std::string> result;
    auto it = root.find("workspaces");
    if (it == root.end()) return result;

    if (!it->second.is_array()) {
        throw PkgError(
            ErrorCode::InvalidFieldType,
            formatContext(path, "workspaces") + " must be an array of member paths/globs"
        );
    }

    const auto& arr = std::get<JsonNode::Array>(it->second.value);
    for (size_t i = 0; i < arr.size(); ++i) {
        if (!arr[i].is_string()) {
            throw PkgError(
                ErrorCode::InvalidFieldType,
                formatContext(path, "workspaces[" + std::to_string(i) + "]") + " must be a string"
            );
        }
        std::string pattern = std::get<std::string>(arr[i].value);
        if (pattern.empty()) {
            throw PkgError(
                ErrorCode::InvalidWorkspaceEntry,
                formatContext(path, "workspaces[" + std::to_string(i) + "]") + " cannot be empty"
            );
        }
        // Normalize backslashes
        std::replace(pattern.begin(), pattern.end(), '\\', '/');
        // Strip trailing slash if present before a wildcard, or clean it up
        while (pattern.size() > 1 && pattern.back() == '/') {
            pattern.pop_back();
        }
        result.push_back(std::move(pattern));
    }
    return result;
}

} // namespace

Manifest Manifest::parse(std::string_view jsonText, const std::filesystem::path& sourcePath) {
    JsonNode rootNode;
    try {
        keet::json::detail::Parser parser(jsonText);
        rootNode = parser.parse_document();
    } catch (const keet::error& err) {
        throw PkgError(
            ErrorCode::InvalidJson,
            (sourcePath.empty() ? "" : sourcePath.filename().string() + ": ") + err.message
        );
    }

    const auto& root = requireObject(rootNode, sourcePath);

    Manifest m;

    // name (required)
    auto nameOpt = getStringField(root, "name", sourcePath);
    if (!nameOpt.has_value()) {
        throw PkgError(
            ErrorCode::MissingField,
            formatContext(sourcePath, "name") + " is required",
            "Add a 'name' field, e.g. \"name\": \"my-app\""
        );
    }
    validatePackageName(*nameOpt, formatContext(sourcePath, "name"));
    m.name = *nameOpt;

    // version (required)
    auto verOpt = getStringField(root, "version", sourcePath);
    if (!verOpt.has_value()) {
        throw PkgError(
            ErrorCode::MissingField,
            formatContext(sourcePath, "version") + " is required",
            "Add a 'version' field, e.g. \"version\": \"0.1.0\""
        );
    }
    validateSemver(*verOpt, formatContext(sourcePath, "version"));
    m.version = *verOpt;

    // description (optional)
    if (auto desc = getStringField(root, "description", sourcePath)) {
        m.description = *desc;
    }

    // entry (optional)
    if (auto entry = getStringField(root, "entry", sourcePath)) {
        m.entry = *entry;
    }

    // buildDir (optional)
    if (auto bd = getStringField(root, "buildDir", sourcePath)) {
        m.buildDir = *bd;
    }

    // keety (optional)
    if (auto k = getStringField(root, "keety", sourcePath)) {
        validateKeetyConstraint(*k, formatContext(sourcePath, "keety"));
        m.keety = *k;
    }

    // dependencies & devDependencies
    m.dependencies = parseDependencyMap(root, "dependencies", sourcePath);
    m.devDependencies = parseDependencyMap(root, "devDependencies", sourcePath);

    // Self dependency check
    if (m.dependencies.find(m.name) != m.dependencies.end()) {
        throw PkgError(
            ErrorCode::SelfDependency,
            formatContext(sourcePath, "dependencies." + m.name) + ": package cannot depend on itself"
        );
    }
    if (m.devDependencies.find(m.name) != m.devDependencies.end()) {
        throw PkgError(
            ErrorCode::SelfDependency,
            formatContext(sourcePath, "devDependencies." + m.name) + ": package cannot depend on itself in devDependencies"
        );
    }

    // scripts
    m.scripts = parseScriptsMap(root, sourcePath);

    // workspaces
    m.workspaces = parseWorkspacesArray(root, sourcePath);

    return m;
}

Manifest Manifest::load(const std::filesystem::path& manifestPath) {
    std::ifstream ifs(manifestPath, std::ios::binary);
    if (!ifs.is_open()) {
        throw PkgError(
            ErrorCode::ManifestNotFound,
            "manifest not found at '" + manifestPath.string() + "'",
            "Run 'kattery init' to scaffold a new pedigree.json"
        );
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return parse(ss.str(), manifestPath);
}

bool Manifest::hasDependency(const std::string& depName) const noexcept {
    return dependencies.find(depName) != dependencies.end() ||
           devDependencies.find(depName) != devDependencies.end();
}

std::string Manifest::toJsonString(int indent) const {
    JsonNode::Object root;
    root["name"] = JsonNode(name);
    root["version"] = JsonNode(version);

    if (!description.empty()) {
        root["description"] = JsonNode(description);
    }
    if (!entry.empty()) {
        root["entry"] = JsonNode(entry);
    }
    if (!buildDir.empty()) {
        root["buildDir"] = JsonNode(buildDir);
    }
    if (!keety.empty()) {
        root["keety"] = JsonNode(keety);
    }

    if (!dependencies.empty()) {
        JsonNode::Object depsObj;
        for (const auto& [depName, spec] : dependencies) {
            depsObj[depName] = JsonNode(spec.toString());
        }
        root["dependencies"] = JsonNode(std::move(depsObj));
    }

    if (!devDependencies.empty()) {
        JsonNode::Object devDepsObj;
        for (const auto& [depName, spec] : devDependencies) {
            devDepsObj[depName] = JsonNode(spec.toString());
        }
        root["devDependencies"] = JsonNode(std::move(devDepsObj));
    }

    if (!scripts.empty()) {
        JsonNode::Object scriptsObj;
        for (const auto& [k, v] : scripts) {
            scriptsObj[k] = JsonNode(v);
        }
        root["scripts"] = JsonNode(std::move(scriptsObj));
    }

    if (!workspaces.empty()) {
        JsonNode::Array wsArr;
        wsArr.reserve(workspaces.size());
        for (const auto& w : workspaces) {
            wsArr.push_back(JsonNode(w));
        }
        root["workspaces"] = JsonNode(std::move(wsArr));
    }

    JsonNode top(std::move(root));
    std::string out = keet::json::detail::dump_node(top, indent);
    // Ensure trailing newline
    if (out.empty() || out.back() != '\n') {
        out.push_back('\n');
    }
    return out;
}

void Manifest::save(const std::filesystem::path& manifestPath) const {
    std::string content = toJsonString(2);

    // Atomic write via temporary file in same directory
    std::filesystem::path parent = manifestPath.parent_path();
    if (parent.empty()) parent = ".";
    std::filesystem::create_directories(parent);

    std::filesystem::path tmpPath = parent / (manifestPath.filename().string() + ".tmp." + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));

    {
        std::ofstream ofs(tmpPath, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            throw PkgError(
                ErrorCode::ManifestNotFound,
                "failed to write manifest to '" + tmpPath.string() + "'"
            );
        }
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    std::error_code ec;
    std::filesystem::rename(tmpPath, manifestPath, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        throw PkgError(
            ErrorCode::ManifestNotFound,
            "failed to replace '" + manifestPath.string() + "': " + ec.message()
        );
    }
}

} // namespace keety::pkg
