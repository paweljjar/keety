// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "class_inheritance_constructor.hpp"
#include <iostream>

namespace keet_module_class_inheritance_constructor {
    bool Entity::isAlive() {
        return this->active;
    }

    void Entity::deactivate() {
        this->active = false;
    }

    void Player::addScore(int64_t pts) {
        this->score += pts;
    }

    void Boss::specialScore(int64_t basePts) {
        this->addScore(basePts * this->multiplier);
    }

    int64_t _keet_main() {
        Player p1 = Player();
        if (p1.id != 1) {
            return 1;
        }
        if (!p1.isAlive()) {
            return 2;
        }
        p1.deactivate();
        if (p1.isAlive()) {
            return 3;
        }
        Player p2 = Player("Hero", 150);
        if (p2.name != "Hero") {
            return 4;
        }
        if (p2.score != 150) {
            return 5;
        }
        if (!p2.isAlive()) {
            return 6;
        }
        p2.addScore(50);
        if (p2.score != 200) {
            return 7;
        }
        Boss b = Boss(3);
        if (b.multiplier != 3) {
            return 8;
        }
        if (b.id != 1) {
            return 9;
        }
        if (!b.isAlive()) {
            return 10;
        }
        b.specialScore(25);
        if (b.score != 75) {
            return 11;
        }
        return 0;
    }
} // namespace keet_module_class_inheritance_constructor

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_class_inheritance_constructor::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
