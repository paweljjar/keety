#pragma once

#include <keety/diag/source_location.hpp>
#include <keety/lexer/lexer.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace keety::fmt {

// Re-export comment types from lexer
using Comment = lexer::Comment;
using CommentKind = lexer::CommentKind;

/// Placement classification of comments relative to AST nodes
enum class CommentPlacement : uint8_t {
    Leading,   // Placed immediately preceding an AST node
    Trailing,  // Placed on the same line following an AST node
    Dangling   // Placed inside an empty container or before container close delimiter
};

/// Comments associated with a specific AST node or member
struct NodeComments {
    std::vector<Comment> leading;
    std::vector<Comment> trailing;
    std::vector<Comment> dangling;

    [[nodiscard]] bool empty() const noexcept {
        return leading.empty() && trailing.empty() && dangling.empty();
    }
};

/// Lookup table mapping AST nodes and source containers to their attached comments
class CommentMap {
public:
    void addLeading(const void* nodeKey, Comment comment) {
        map_[nodeKey].leading.push_back(std::move(comment));
        ++uniqueAttachedCount_;
    }

    void addTrailing(const void* nodeKey, Comment comment) {
        map_[nodeKey].trailing.push_back(std::move(comment));
        ++uniqueAttachedCount_;
    }

    void addDangling(const void* nodeKey, Comment comment) {
        map_[nodeKey].dangling.push_back(std::move(comment));
        ++uniqueAttachedCount_;
    }

    void addFileHeader(Comment comment) {
        fileHeader_.push_back(std::move(comment));
        ++uniqueAttachedCount_;
    }

    void addFileFooter(Comment comment) {
        fileFooter_.push_back(std::move(comment));
        ++uniqueAttachedCount_;
    }

    void addLeadingAlias(const void* aliasKey, const Comment& comment) {
        if (aliasKey) {
            map_[aliasKey].leading.push_back(comment);
        }
    }

    void addTrailingAlias(const void* aliasKey, const Comment& comment) {
        if (aliasKey) {
            map_[aliasKey].trailing.push_back(comment);
        }
    }

    [[nodiscard]] const std::vector<Comment>& getLeading(const void* nodeKey) const {
        auto it = map_.find(nodeKey);
        if (it != map_.end()) return it->second.leading;
        return emptyList_;
    }

    [[nodiscard]] const std::vector<Comment>& getTrailing(const void* nodeKey) const {
        auto it = map_.find(nodeKey);
        if (it != map_.end()) return it->second.trailing;
        return emptyList_;
    }

    [[nodiscard]] const std::vector<Comment>& getDangling(const void* nodeKey) const {
        auto it = map_.find(nodeKey);
        if (it != map_.end()) return it->second.dangling;
        return emptyList_;
    }

    [[nodiscard]] const std::vector<Comment>& getFileHeader() const noexcept {
        return fileHeader_;
    }

    [[nodiscard]] const std::vector<Comment>& getFileFooter() const noexcept {
        return fileFooter_;
    }

    [[nodiscard]] bool hasComments(const void* nodeKey) const {
        auto it = map_.find(nodeKey);
        return it != map_.end() && !it->second.empty();
    }

    [[nodiscard]] size_t totalAttachedCount() const noexcept {
        return uniqueAttachedCount_;
    }

    [[nodiscard]] const std::unordered_map<const void*, NodeComments>& getNodeMap() const noexcept {
        return map_;
    }

private:
    std::unordered_map<const void*, NodeComments> map_;
    std::vector<Comment> fileHeader_;
    std::vector<Comment> fileFooter_;
    size_t uniqueAttachedCount_{0};
    static inline const std::vector<Comment> emptyList_{};
};

} // namespace keety::fmt
