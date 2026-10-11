#include "game/enemies/enemycreationrequests.h"

#include <stdexcept>

std::uint32_t enemycreation::resolveType(const jmp::ToolDataRef& source, const enemytypes::Catalog& catalog) {
    std::string_view name;
    if (source.table && source.row >= 0 && source.table->getValue(source.row, "name", name))
        return catalog.find(name);
    return 0xffffffff;
}

enemycreation::Request enemycreation::makeRequest(const jmp::ToolDataRef& source, const enemytypes::Catalog& catalog) {
    return {source, resolveType(source, catalog)};
}

std::optional<std::string_view> enemycreation::createName(const Request& request) {
    std::string_view name;
    const auto& source = request.source;
    if (source.table && source.row >= 0 && source.table->getValue(source.row, "create_name", name))
        return name;
    return std::nullopt;
}

void enemycreation::Requests::append(const Request& request) {
    if (count >= entries.size()) throw std::out_of_range("Pending character creation capacity exceeded.");
    entries[count++] = request;
}

unsigned enemycreation::Requests::erase(unsigned index) {
    if (index >= count) throw std::out_of_range("Pending character creation index.");
    for (unsigned i = index; i + 1 < count; ++i) entries[i] = entries[i + 1];
    --count;
    return index;
}

unsigned enemycreation::Requests::size() const {
    return count;
}

const enemycreation::Request& enemycreation::Requests::at(unsigned index) const {
    if (index >= count) throw std::out_of_range("Pending character creation index.");
    return entries[index];
}
