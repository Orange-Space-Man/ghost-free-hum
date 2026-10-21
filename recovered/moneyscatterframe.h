#pragma once

#include "moneyscatterowner.h"
#include "billscatterscene.h"
#include "leafscatterscene.h"
#include "game/player/playersharedmodels.h"

namespace moneyscatter {
    struct RenderServices : leafscatter::RenderServices {
        virtual void selectBuffers(playermodels::DrawBuffer& opaque, playermodels::DrawBuffer& translucent) = 0;
        virtual void setCamera(const j3dmodel::Matrix& camera) = 0;
    };

    void updateFrame(Owner& coins, billscatter::Scene& bills, leafscatter::Scene& leaves,
        const Vector& cameraPosition, const Vector& cameraDirection);
    void drawFrame(Owner& coins, billscatter::Scene& bills, leafscatter::Scene& leaves,
        playermodels::SharedModels& models, RenderServices& services);
    void viewFrame(Owner& coins, billscatter::Scene& bills, leafscatter::Scene& leaves,
        const j3dmodel::Matrix& camera, RenderServices& services);
}
