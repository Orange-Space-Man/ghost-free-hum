#pragma once

#include "game/actors/actormodelcatalog.h"
#include <optional>

namespace actorcollision {
    using Record = std::array<std::uint32_t, 5>;
    using ExtendedRecord = std::array<std::uint32_t, 6>;

    struct Volume {
        std::uint32_t field8;
        std::optional<Record> secondary;
        std::optional<ExtendedRecord> extended;
    };

    struct Descriptor : Volume {
        std::vector<Record> records;
    };

    class Catalog {
    public:
        Catalog(const std::vector<std::uint8_t>& executable, const actormodel::Catalog& models);
        const Descriptor& type(unsigned index) const;
        Record initial, initialSecondary;
        ExtendedRecord initialExtended;

    private:
        std::array<std::shared_ptr<Descriptor>, 381> types;
    };

    class Element {
    public:
        explicit Element(const Catalog& catalog);
        void initialize(const Descriptor& descriptor, unsigned model);
        void append(const Record& record);
        unsigned count, model = 0;
        std::uint32_t field8 = 1;
        std::uint8_t field18 = 1;
        std::array<Record, 8> records;
        unsigned stored = 0;
        Record secondary;
        ExtendedRecord extended;
    };

    class Manager {
    public:
        Manager(const Catalog& catalog, const actormodel::Catalog& models);
        ~Manager();
        std::array<std::unique_ptr<Element>, 381> elements;
    };
}
