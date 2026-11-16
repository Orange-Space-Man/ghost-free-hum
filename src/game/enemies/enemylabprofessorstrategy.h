#pragma once

#include "game/enemies/enemystrategy.h"

namespace enemystrategy {
    class LabProfessor final : public Strategy {
    public:
        void behavior() override;

    private:
        bool materialHidden = false;
    };
}
