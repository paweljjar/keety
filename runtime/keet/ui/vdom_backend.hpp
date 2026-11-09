#pragma once

#include <keet/ui/component.hpp>
#include <keet/ui/vdom.hpp>
#include <keet/ui/client/keet_vdom_js.hpp>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace keet::ui {

using ComponentFactory = std::function<std::shared_ptr<Component>()>;

/// Represents an isolated client session connected over a WebSocket connection
class VDomSession {
public:
    explicit VDomSession(std::string sessionId, std::shared_ptr<Component> component)
        : sessionId_(std::move(sessionId)), component_(std::move(component)) {
        if (component_) {
            rootNode_ = component_->mount();
            component_->onMount();
            component_->flushUpdate();
            currentVdom_ = VNode::fromUINode(rootNode_);
        }
    }

    ~VDomSession() {
        if (component_) {
            component_->destroy();
        }
    }

    [[nodiscard]] const std::string& getSessionId() const noexcept { return sessionId_; }
    [[nodiscard]] std::shared_ptr<Component> getComponent() const noexcept { return component_; }
    [[nodiscard]] std::shared_ptr<UINode> getRootNode() const noexcept { return rootNode_; }

    /// Generates full snapshot message: `{"type": "vdom_full", "vdom": {...}}`
    [[nodiscard]] std::string getFullSyncMessage() const {
        if (!currentVdom_) return "{\"type\":\"vdom_full\",\"vdom\":null}";
        return "{\"type\":\"vdom_full\",\"vdom\":" + currentVdom_->toJson() + "}";
    }

    /// Ingests incoming client WebSocket message
    /// Format: `{"type": "event", "id": "42", "event": "click", "data": {}}` or `{"type": "sync"}`
    std::string handleClientMessage(const std::string& rawJson) {
        if (rawJson.find("\"type\":\"sync\"") != std::string::npos ||
            rawJson.find("\"type\": \"sync\"") != std::string::npos) {
            return getFullSyncMessage();
        }

        // Parse event parameters
        std::string targetId;
        std::string eventName;
        std::string argValue;

        // Simple lightweight extraction from JSON
        size_t idPos = rawJson.find("\"id\":");
        if (idPos != std::string::npos) {
            size_t valStart = idPos + 5;
            while (valStart < rawJson.size() && (rawJson[valStart] == ' ' || rawJson[valStart] == '\t')) valStart++;
            if (valStart < rawJson.size() && rawJson[valStart] == '"') {
                size_t q2 = rawJson.find('"', valStart + 1);
                if (q2 != std::string::npos) {
                    targetId = rawJson.substr(valStart + 1, q2 - valStart - 1);
                }
            } else {
                size_t valEnd = rawJson.find_first_of(",} \t\r\n", valStart);
                if (valEnd != std::string::npos) {
                    targetId = rawJson.substr(valStart, valEnd - valStart);
                }
            }
        }

        size_t evPos = rawJson.find("\"event\":");
        if (evPos != std::string::npos) {
            size_t q1 = rawJson.find('"', evPos + 8);
            size_t q2 = rawJson.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                eventName = rawJson.substr(q1 + 1, q2 - q1 - 1);
            }
        }

        size_t valPos = rawJson.find("\"value\":");
        if (valPos != std::string::npos) {
            size_t q1 = rawJson.find('"', valPos + 8);
            size_t q2 = rawJson.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                argValue = rawJson.substr(q1 + 1, q2 - q1 - 1);
            }
        }

        if (!targetId.empty() && !eventName.empty() && rootNode_) {
            uint32_t numericId = static_cast<uint32_t>(std::stoul(targetId));
            if (auto node = rootNode_->findById(numericId)) {
                if (eventName == "input" && node->getKind() == NodeKind::Input) {
                    auto inp = std::static_pointer_cast<InputNode>(node);
                    inp->setValue(argValue);
                } else {
                    node->emit(eventName, argValue);
                }
            }
        }

        // Flush microtask updates
        tick();

        // Calculate VDOM diff patch
        auto newVdom = VNode::fromUINode(rootNode_);
        auto patches = VDomDiffer::diff(currentVdom_, newVdom);
        currentVdom_ = newVdom;

        if (patches.empty()) {
            return ""; // No update needed
        }

        return "{\"type\":\"vdom_patch\",\"patches\":" + VDomDiffer::patchesToJson(patches) + "}";
    }

private:
    std::string sessionId_;
    std::shared_ptr<Component> component_;
    std::shared_ptr<UINode> rootNode_;
    std::shared_ptr<VNode> currentVdom_;
};

/// Web Virtual DOM Session Backend.
/// Manages per-client isolated component trees and bridges WebSocket events.
class WebVdomBackend {
public:
    explicit WebVdomBackend(ComponentFactory factory)
        : factory_(std::move(factory)) {}

    /// Creates an isolated session for a new connection
    std::shared_ptr<VDomSession> createSession(const std::string& sessionId) {
        if (!factory_) return nullptr;
        auto component = factory_();
        auto session = std::make_shared<VDomSession>(sessionId, component);
        sessions_[sessionId] = session;
        return session;
    }

    std::shared_ptr<VDomSession> getSession(const std::string& sessionId) {
        auto it = sessions_.find(sessionId);
        return it != sessions_.end() ? it->second : nullptr;
    }

    void removeSession(const std::string& sessionId) {
        sessions_.erase(sessionId);
    }

    [[nodiscard]] size_t sessionCount() const noexcept {
        return sessions_.size();
    }

    /// Returns embedded client script content
    [[nodiscard]] static std::string getClientScript() {
        return std::string(client::KEET_VDOM_JS);
    }

    /// Returns full HTML shell with embedded client script and container
    [[nodiscard]] static std::string getHtmlShell(const std::string& title = "Keet App", const std::string& scopedCss = "") {
        std::stringstream ss;
        ss << "<!DOCTYPE html>\n"
           << "<html lang=\"en\">\n"
           << "<head>\n"
           << "    <meta charset=\"UTF-8\">\n"
           << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
           << "    <title>" << title << "</title>\n";
        if (!scopedCss.empty()) {
            ss << "    <style>\n" << scopedCss << "\n    </style>\n";
        }
        ss << "</head>\n"
           << "<body>\n"
           << "    <div id=\"keet-app\"></div>\n"
           << "    <script>\n" << getClientScript() << "\n    </script>\n"
           << "</body>\n"
           << "</html>\n";
        return ss.str();
    }

private:
    ComponentFactory factory_;
    std::unordered_map<std::string, std::shared_ptr<VDomSession>> sessions_;
};

} // namespace keet::ui
