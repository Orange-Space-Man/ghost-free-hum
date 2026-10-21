#include "moneyscatterframe.h"

namespace {
    template<class Request, class Callback>
    void visibleModels(Request& request, const Callback& callback) {
        for (auto* particle = request.particles.sentinel.next; particle != &request.particles.sentinel; particle = particle->next)
            if (particle->visible) callback(*particle->model);
    }

    template<class Pool, class Callback>
    void requests(Pool& pool, const Callback& callback) {
        for (auto* request = pool.active.sentinel.next; request != &pool.active.sentinel; request = request->next)
            callback(*request);
    }
}

void moneyscatter::updateFrame(Owner& coins, billscatter::Scene& bills, leafscatter::Scene& leaves,
    const Vector& cameraPosition, const Vector& cameraDirection) {
    coins.updateCoins(cameraPosition, cameraDirection);
    bills.update();
    leaves.update();
}

void moneyscatter::drawFrame(Owner& coins, billscatter::Scene& bills, leafscatter::Scene& leaves,
    playermodels::SharedModels& models, RenderServices& services) {
    services.selectBuffers(models.buffer(8), models.buffer(9));
    const auto draw = [&](j3dmodel::ModelInstance& model) { services.draw(model); };
    requests(coins.storage(), [&](auto& request) { visibleModels(request, draw); });
    services.draw(*coins.modelStorage().auxiliary.model);
    for (auto& pool : bills.storage().pools)
        requests(*pool, [&](auto& request) { visibleModels(request, draw); });
    leaves.draw(services);
}

void moneyscatter::viewFrame(Owner& coins, billscatter::Scene& bills, leafscatter::Scene& leaves,
    const j3dmodel::Matrix& camera, RenderServices& services) {
    const auto view = [&](j3dmodel::ModelInstance& model) { services.view(model, camera); };
    services.setCamera(camera);
    requests(coins.storage(), [&](auto& request) {
        visibleModels(request, view);
        services.view(*coins.modelStorage().auxiliary.model, camera);
    });
    for (auto& pool : bills.storage().pools) {
        services.setCamera(camera);
        requests(*pool, [&](auto& request) { visibleModels(request, view); });
    }
    if (leaves.storage().active()) {
        services.setCamera(camera);
        leaves.view(camera, services);
    }
}
