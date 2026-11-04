// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <keet/runtime.hpp>

namespace keet_module_class_inheritance_constructor {
    // --- Forward Declarations ---
    struct Entity;
    struct Player;
    struct Boss;

    // --- Classes and Structs ---
#line 1 "class_inheritance_constructor.keet"
    struct Entity {
        int64_t id = 1;
        bool active = true;

        Entity() = default;
        Entity(int64_t id, bool active) : id(std::move(id)), active(std::move(active)) {}
        bool isAlive();
        void deactivate();
    };

#line 14 "class_inheritance_constructor.keet"
    struct Player : public Entity {
        std::string name{};
        int64_t score = 0;

        Player() = default;
        Player(std::string name, int64_t score) : name(std::move(name)), score(std::move(score)) {}
        void addScore(int64_t pts);
    };

#line 23 "class_inheritance_constructor.keet"
    struct Boss : public Player {
        int64_t multiplier = 2;

        Boss() = default;
        Boss(int64_t multiplier) : multiplier(std::move(multiplier)) {}
        void specialScore(int64_t basePts);
    };

    // --- Exported Function Prototypes ---
#line 31 "class_inheritance_constructor.keet"
    int64_t main();
} // namespace keet_module_class_inheritance_constructor
