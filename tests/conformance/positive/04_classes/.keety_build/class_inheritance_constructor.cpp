// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "class_inheritance_constructor.hpp"
#include <iostream>

namespace keet_module_class_inheritance_constructor {
#line 5 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
    bool Entity::isAlive() {
#line 6 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        return this->active;
    }

#line 9 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
    void Entity::deactivate() {
#line 10 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        this->active = false;
    }

#line 18 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
    void Player::addScore(int64_t pts) {
#line 19 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        this->score += pts;
    }

#line 26 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
    void Boss::specialScore(int64_t basePts) {
#line 27 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        this->addScore(basePts * this->multiplier);
    }

#line 31 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
    int64_t _keet_main() {
#line 32 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        Player p1 = Player();
#line 33 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        if (p1.id != 1) {
#line 34 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
            return 1;
        }
#line 36 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        if (!p1.isAlive()) {
#line 37 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
            return 2;
        }
#line 39 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        p1.deactivate();
#line 40 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        if (p1.isAlive()) {
#line 41 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
            return 3;
        }
#line 44 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        Player p2 = Player("Hero", 150);
#line 45 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        if (p2.name != "Hero") {
#line 46 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
            return 4;
        }
#line 48 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        if (p2.score != 150) {
#line 49 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
            return 5;
        }
#line 51 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        if (!p2.isAlive()) {
#line 52 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
            return 6;
        }
#line 54 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        p2.addScore(50);
#line 55 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        if (p2.score != 200) {
#line 56 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
            return 7;
        }
#line 59 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        Boss b = Boss(3);
#line 60 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        if (b.multiplier != 3) {
#line 61 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
            return 8;
        }
#line 63 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        if (b.id != 1) {
#line 64 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
            return 9;
        }
#line 66 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        if (!b.isAlive()) {
#line 67 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
            return 10;
        }
#line 69 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        b.specialScore(25);
#line 70 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
        if (b.score != 75) {
#line 71 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
            return 11;
        }
#line 74 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/04_classes/class_inheritance_constructor.keet"
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
