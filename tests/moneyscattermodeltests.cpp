#include "../recovered/moneyscattermodels.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    void check(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
    bool near(float first, float second) { return std::abs(first - second) < 0.0001f; }
}

int main() {
    try {
        auto resource = std::make_shared<j3dmodel::ModelData>();
        moneyscatter::Pool pool({30, 5, 2});
        moneyscatter::Models models(pool, resource, 357);
        for (unsigned i = 0; i < 200; ++i) {
            auto& particle = pool.particles.at(i);
            check(particle.model && particle.model->model == resource && particle.model.get() != models.auxiliary.model.get(),
                "Each particle requires a distinct model instance sharing the original resource.");
            if (i) check(particle.model.get() != pool.particles.at(i - 1).model.get(), "Particle models cannot alias.");
        }
        check(pool.particles.free.count == 200 && pool.requests.free.count == 34,
            "Model initialization must not allocate active requests or particles.");
        bool rejected = false;
        try { moneyscatter::Models duplicate(pool, resource, 0); }
        catch (const std::logic_error&) { rejected = true; }
        check(rejected, "Reinitialization must not replace owned models.");
        auto* particle = pool.particles.free.take();
        auto* retained = particle->model.get();
        pool.persistent().particles.append(*particle);
        particle->position = {10, 20, 30};
        particle->forward = {0, 1, 0};
        particle->up = {0, 1, 2};
        particle->scale = 0.25f;
        particle->spin = 0;
        moneyscatter::updateModel(*particle);
        check(particle->up == moneyscatter::Vector{0, 0, 1} && particle->forward == moneyscatter::Vector{0, 1, 0},
            "Model update orthogonalizes the retained up vector without replacing forward.");
        check(particle->model->scale == moneyscatter::Vector{0.25f, 0.25f, 0.25f} &&
            particle->model->matrix == j3dmodel::Matrix{1, 0, 0, 10, 0, 1, 0, 20, 0, 0, 1, 30},
            "Uniform model scale remains separate from the translated basis matrix.");
        particle->spin = 90;
        moneyscatter::updateModel(*particle);
        check(near(particle->model->matrix[0], 0) && near(particle->model->matrix[2], 1) &&
            near(particle->model->matrix[8], -1) && particle->model->matrix[3] == 10,
            "Spin postmultiplies the basis about its local Y axis without rotating translation.");
        pool.recycle(pool.persistent(), *particle);
        check(particle->model.get() == retained, "Recycling retains model ownership.");
        models.updateAuxiliary({10, 20, 30}, {0, 0, 2});
        check(models.auxiliary.spin == 360 && models.auxiliary.position == moneyscatter::Vector{10, 20, -9970},
            "Auxiliary model advances three degrees and offsets normalized camera direction by minus 10000.");
        models.updateAuxiliary({1, 2, 3}, {});
        check(models.auxiliary.spin == 3 && models.auxiliary.position == moneyscatter::Vector{1, 2, 3},
            "Auxiliary angle wraps only above 360 and zero camera direction retains camera position.");
        particle->forward = {};
        particle->up = {1, 2, 3};
        particle->spin = 0;
        moneyscatter::updateModel(*particle);
        check(particle->up == moneyscatter::Vector{} && particle->model->matrix[0] == 0 && particle->model->matrix[10] == 0,
            "Degenerate bases remain degenerate rather than receiving an invented fallback axis.");
        moneyscatter::RetainedState state;
        for (unsigned cycle = 0; cycle < 2; ++cycle) {
            moneyscatter::Pool rebuiltPool({30, 5, 2});
            moneyscatter::Models rebuilt(rebuiltPool, resource, state);
            check(rebuilt.auxiliary.spin == cycle * 3, "Auxiliary spin survives scene-owned model destruction.");
            rebuilt.updateAuxiliary({}, {0, 0, 1});
            check(state.auxiliarySpin == (cycle + 1) * 3, "Each auxiliary update publishes original retained state.");
        }
        check(moneyscatter::RetainedState{}.auxiliarySpin == 0, "A new game process starts with zero-initialized BSS spin.");
        std::cout << "Money model ownership and transform checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
