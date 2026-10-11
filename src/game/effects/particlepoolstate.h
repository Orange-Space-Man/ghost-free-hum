#pragma once

#include "game/effects/particlenodestate.h"

#include <memory>
#include <stdexcept>

namespace particles {
    template<class Item>
    class Ring {
    public:
        Ring() : current(&sentinel) {}
        Ring(const Ring&) = delete;
        Ring& operator=(const Ring&) = delete;
        void append(Item& item) {
            item.next = &sentinel;
            item.previous = sentinel.previous;
            sentinel.previous->next = &item;
            sentinel.previous = &item;
            ++count;
        }
        void remove(Item& item) {
            item.next->previous = item.previous;
            item.previous->next = item.next;
            --count;
        }
        Item* take() {
            if (!count) return nullptr;
            current = nullptr;
            auto* item = sentinel.next;
            if (item != &sentinel) {
                current = item;
                item->next->previous = item->previous;
                item->previous->next = item->next;
                --count;
            }
            return current;
        }
        Item sentinel;
        Item* current;
        unsigned count = 0;
    };

    template<class Item>
    class Pool {
    public:
        explicit Pool(unsigned capacity) : storage(new Item[capacity]), capacity(capacity) {
            for (unsigned i = 0; i < capacity; ++i) free.append(storage[i]);
        }
        Pool(const Pool&) = delete;
        Pool& operator=(const Pool&) = delete;
        Item& at(unsigned index) {
            if (index >= capacity) throw std::out_of_range("Original particle pool index is invalid.");
            return storage[index];
        }
        Ring<Item> free;

    private:
        std::unique_ptr<Item[]> storage;
        unsigned capacity;
    };
}
