#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/actors/actorcollisionowner.h"
#include "game/startup/startupresources.h"

#include <map>
#include <stdexcept>

namespace {
    unsigned word(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        return (unsigned(bytes.at(offset)) << 24) | (unsigned(bytes.at(offset + 1)) << 16) |
            (unsigned(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
    }

    std::vector<std::uint8_t> range(const std::vector<std::uint8_t>& executable, unsigned address, std::size_t size) {
        std::vector<std::uint8_t> result;
        std::string error;
        if (!dolresources::read(executable, address, size, result, error)) throw std::runtime_error(error);
        return result;
    }

    template<std::size_t Count>
    std::array<std::uint32_t, Count> record(const std::vector<std::uint8_t>& executable, unsigned address) {
        const auto bytes = range(executable, address, Count * 4);
        std::array<std::uint32_t, Count> result{};
        for (unsigned i = 0; i < Count; ++i) result[i] = word(bytes, i * 4);
        return result;
    }
}

actorcollision::Catalog::Catalog(const std::vector<std::uint8_t>& executable, const actormodel::Catalog& models) {
    initial = record<5>(executable, glme01::collisionInitialPrimary);
    initialSecondary = record<5>(executable, glme01::collisionInitialSecondary);
    initialExtended = record<6>(executable, glme01::collisionInitialExtended);
    const auto modelTable = range(executable, glme01::collisionModelTable, 262 * 4);
    const auto typeTable = range(executable, glme01::collisionTypeTable, types.size() * 4);
    std::map<unsigned, std::shared_ptr<Descriptor>> loaded;
    for (unsigned type = 0; type < types.size(); ++type) {
        unsigned address = word(typeTable, type * 4);
        if (!address) address = word(modelTable, models.type(type).model * 4);
        auto& descriptor = loaded[address];
        if (!descriptor) {
            const auto fields = record<5>(executable, address);
            descriptor = std::make_shared<Descriptor>();
            if (fields[0] > 8) throw std::runtime_error("Actor collision descriptor exceeds the original eight-record owner.");
            for (unsigned i = 0; i < fields[0]; ++i) descriptor->records.push_back(record<5>(executable, fields[1] + i * 20));
            descriptor->field8 = fields[2];
            if (fields[3]) descriptor->secondary = record<5>(executable, fields[3]);
            if (fields[4]) descriptor->extended = record<6>(executable, fields[4]);
        }
        types[type] = descriptor;
    }
}

const actorcollision::Descriptor& actorcollision::Catalog::type(unsigned index) const {
    return *types.at(index);
}

actorcollision::Element::Element(const Catalog& catalog) {
    append(catalog.initial);
    count = stored;
    secondary = catalog.initialSecondary;
    extended = catalog.initialExtended;
}

void actorcollision::Element::append(const Record& record) {
    const auto index = stored++;
    records.at(index) = record;
}

void actorcollision::Element::initialize(const Descriptor& descriptor, unsigned modelIndex) {
    stored = 0;
    model = modelIndex;
    field18 = 0;
    for (const auto& record : descriptor.records) append(record);
    count = stored;
    field8 = descriptor.field8;
    if (descriptor.secondary) secondary = *descriptor.secondary;
    if (descriptor.extended) extended = *descriptor.extended;
}

actorcollision::Manager::Manager(const Catalog& catalog, const actormodel::Catalog& models) {
    for (auto& element : elements) element = std::make_unique<Element>(catalog);
    for (unsigned i = 0; i < elements.size(); ++i) elements[i]->initialize(catalog.type(i), models.type(i).model);
}

actorcollision::Manager::~Manager() {
    for (auto i = elements.size(); i != 0; --i) elements[i - 1].reset();
}
