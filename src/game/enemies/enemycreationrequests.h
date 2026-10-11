#pragma once

#include "game/enemies/enemytypecatalog.h"
#include "game/events/jmptooldata.h"

#include <optional>

namespace enemycreation {
    struct Request {
        jmp::ToolDataRef source;
        std::uint32_t type = 0xffffffff;
    };

    std::uint32_t resolveType(const jmp::ToolDataRef& source, const enemytypes::Catalog& catalog);
    Request makeRequest(const jmp::ToolDataRef& source, const enemytypes::Catalog& catalog);
    std::optional<std::string_view> createName(const Request& request);

    class Requests {
    public:
        void append(const Request& request);
        unsigned erase(unsigned index);
        unsigned size() const;
        const Request& at(unsigned index) const;

    private:
        std::array<Request, 128> entries{};
        unsigned count = 0;
    };
}
