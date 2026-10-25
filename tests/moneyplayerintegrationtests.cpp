#include "../recovered/moneyscatterowner.h"
#include "../recovered/billscatterscene.h"
#include "../recovered/moneyscatterframe.h"
#include "jlib/j3d/j3dmodelcalculation.h"
#include "jlib/j3d/j3dmaterialcalculation.h"
#include "jlib/j3d/j3dtevcommands.h"
#include "j3dmaterialloadtarget.h"
#include "game/scene/gameplayresourceowner.h"
#include "game/player/playerliferecovery.h"
#include "game/player/playeractorcomponents.h"
#include "game/startup/startupresources.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>

namespace {
    void check(bool condition, const std::string& message) {
        if (!condition) throw std::runtime_error(message);
    }

    struct Render : moneyscatter::RenderServices {
        playermodels::SharedModels& shared;
        j3dmodel::Matrix camera;
        j3dmodel::TransformTables tables;
        std::map<j3dmodel::ModelInstance*, int> ids;
        std::vector<int> calls;
        Render(playermodels::SharedModels& shared, const j3dmodel::Matrix& camera,
            const std::vector<std::uint8_t>& executable) : shared(shared), camera(camera), tables(executable) {}
        void selectBuffers(playermodels::DrawBuffer& opaque, playermodels::DrawBuffer& translucent) override {
            check(&opaque == &shared.buffer(8) && &translucent == &shared.buffer(9),
                "Money submission selects original shared buffer slots eight and nine.");
            calls.push_back(-1);
        }
        void setCamera(const j3dmodel::Matrix& value) override {
            check(value == camera, "Each owner installs the original current camera.");
            calls.push_back(-2);
        }
        void draw(j3dmodel::ModelInstance& model) override {
            j3dmodel::calculateBasic(model, tables);
            auto* normals = reinterpret_cast<j3dmodel::NormalMatrix*>(model.normalMatrices.data());
            for (unsigned i = 0; i < model.model->geometry.drawMatrices.size(); ++i) normals[i].fill(17.5f);
            check(model.model->geometry.joints.size() == 1, "Money material fixture requires the verified single-joint models.");
            for (auto& material : model.model->materials.entries) {
                j3dmodel::calculateMaterial(*material, model.jointMatrices[0], camera, model.field8 & 12, tables);
                for (const auto& texture : material->texture->matrices)
                    if (texture) for (float entry : texture->matrix)
                        check(std::isfinite(entry), "Original money texture matrix must be finite.");
                const auto commands = j3dmodel::buildTevCommands(*material->tev);
                check(!commands.empty(), "Original money material emits its texture-combiner commands.");
                materialtests::Target target;
                j3dmodel::loadMaterialDirect(*material, 0, target);
                check(target.calls.front() == "mode:" + std::to_string(material->mode) && target.calls.back() == "nbt",
                    "Original money materials traverse the assembled block loader.");
            }
            calls.push_back(ids.at(&model));
        }
        void view(j3dmodel::ModelInstance& model, const j3dmodel::Matrix& value) override {
            check(value == camera, "View calculation receives the current camera.");
            j3dmodel::calculateViewMatrices(model, value);
            const auto* matrices = reinterpret_cast<j3dmodel::Matrix*>(model.drawMatrices.data());
            const auto* normals = reinterpret_cast<j3dmodel::NormalMatrix*>(model.normalMatrices.data());
            for (unsigned i = 0; i < model.model->geometry.drawMatrices.size(); ++i) {
                for (float entry : matrices[i]) check(std::isfinite(entry), "Original money draw matrix must be finite.");
                for (float entry : normals[i]) check(std::isfinite(entry), "Original money normal matrix must be finite: model " + std::to_string(ids.at(&model)));
                j3dmodel::NormalMatrix inverse;
                const auto& source = model.model->geometry.drawMatrices[i];
                if (!source.weighted && model.jointScaleFlags[source.index] != 1 && !j3dmodel::inverseTranspose(matrices[i], inverse))
                    for (float entry : normals[i]) check(entry == 17.5f, "Singular money pose retains the supplied previous normal storage.");
            }
            calls.push_back(ids.at(&model));
        }
    };

    void drain(jaudio::PositionalPlayback& audio, jaudio::DspMixer& mixer, bool expected) {
        bool audible = false;
        std::string error;
        const std::array<float, 12> camera{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        for (unsigned block = 0; block < 10000 && !audio.finished(); ++block) {
            if (!(block % 8)) audio.update(camera);
            mixer.begin();
            check(audio.mix(mixer, error), error);
            std::array<std::int16_t, 160> samples{};
            mixer.end(samples, 1);
            audible |= std::any_of(samples.begin(), samples.end(), [](auto value) { return value != 0; });
        }
        check(audio.finished(), "Original pickup sequence must complete.");
        check(audible == expected, "Pickup playback must agree with the accepted event.");
    }
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Original package required.");
        gameassets::Package package;
        std::vector<std::uint8_t> executable, initialFlags;
        std::string error;
        check(package.open(argv[1], error) && package.read("sys/main.dol", executable, error) &&
            dolresources::read(executable, glme01::initialSaveFlags, 31, initialFlags, error), error);
        gamesave::State saves;
        check(gamesave::initialize(initialFlags, saves, error), error);
        gameentry::State entry;
        check(gameentry::selectNewFile(saves, 0, entry, error), error);
        gameplay::ResourceOwner resources(argv[1], entry);
        auto& managers = resources.managers();
        auto* player = dynamic_cast<playeractor::Player*>(managers.actors().actor(managers.enemies().appearPoint(0).actor));
        check(player != nullptr, "Original EnManager must own the collection recipient.");
        auto& counters = player->state.fields12F4;
        const auto retainedCounters = counters;
        const auto retainedPosition = player->position;
        player->position = {0, 0, -100};
        jaudio::Resources audioResources;
        check(jaudio::read(package, audioResources, error), error);
        jaudio::DspMixer mixer;
        check(mixer.configure(audioResources.effects, error), error);
        auto audio = managers.playback();
        auto& events = managers.playerEvents();
        playerevents::Payload payload;
        for (unsigned event : {0x26u, 0x27u}) {
            const unsigned slot = event == 0x26 ? 0 : 1;
            const unsigned offset = slot ? 0x98 : 0x94;
            for (unsigned rejected : {0u, 0xffffffffu, 0x80000000u}) {
                payload.word(offset, rejected);
                const auto before = counters;
                check(events.send(0, event, &payload) == 0 && counters == before,
                    "Contact collection rejects nonpositive signed counts without changing state.");
                drain(*audio, mixer, false);
            }
            counters[slot] = 0xffffffff;
            payload.word(offset, 2);
            check(events.send(0, event, &payload) == 1 && counters[slot] == 1,
                "Contact collection retains original unsigned counter wrap.");
            drain(*audio, mixer, true);
            check(events.send(0, event) == 0, "Missing contact payload uses original zero default.");
        }
        for (unsigned kind : {5u, 6u}) {
            payload.reset(); payload.word(0x60, kind); payload.word(0x64, 3);
            payload.word(0x70, 1); payload.word(0x74, 73);
            player->state.field1124 = 0;
            const auto before = counters[kind - 5];
            check(events.send(0, 0x13, &payload) == 1 && counters[kind - 5] == before + 3 &&
                player->state.field1124 == (kind == 5 ? 0 : 10) && player->state.field1128 == 73,
                "Suction dispatch decodes count and original delayed action payload into the real player.");
            drain(*audio, mixer, true);
            player->state.field1124 = std::numeric_limits<float>::quiet_NaN();
            player->state.field1128 = 91;
            payload.word(0x64, 0);
            check(events.send(0, 0x13, &payload) == 1 && std::isnan(player->state.field1124) &&
                player->state.field1128 == 91, "Zero suction count still plays feedback; NaN delay remains untouched.");
            drain(*audio, mixer, true);
        }
        bool rejected = false;
        payload.word(0x60, 3);
        try { events.send(0, 0x13, &payload); }
        catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "Non-money capture cannot bypass its unbound original services.");

        player->position = retainedPosition;
        managers.particles().refresh(nullptr);
        auto& context = managers.particles().contexts().player;
        actormodel::InitialPose pose(executable);
        std::array<std::uint16_t, 256> savedRooms{};
        unsigned mode = 2;
        float minimumScale = 0.3f;
        std::uint32_t random = 1, particleRandom = 2;
        auto dispatch = [&](unsigned event, const playerevents::Payload& value) { events.send(0, event, &value); };
        moneyscatter::RetainedState retainedMoney;
        moneyscatter::Owner coins({30, 5, 2}, managers.players().models->model(playermodels::SharedModel::Coin), pose,
            {resources.rooms(), resources.visibility(), resources.roomFlags(), savedRooms, mode, managers.collision(),
                player->position, context, minimumScale, random, managers.particles().effects(), managers.particles().batch(0),
                dispatch, [](float, moneyscatter::Particle&) { return 0; },
                [&](unsigned id, const auto& position, unsigned) { audio->play(id, position); }}, retainedMoney);
        std::int32_t audioLife = player->life;
        playerevents::LifeEnvironment life{audioLife, managers.particles().effects(), {}, {},
            [&](const auto& position, const auto& direction, auto count, auto lifetime, double speed, auto positionMode) {
                coins.scatter(position, direction, count, lifetime, speed, positionMode);
            }};
        counters[9] = 3;
        check(playerevents::loseMoney(*player, 2, life) == 1 && counters[9] == 1 && coins.storage().particles.free.count == 198,
            "Original damage money loss creates owned coin particles from the real player's weapon route.");
        auto* request = coins.storage().active.sentinel.previous;
        request->collectionReady = true;
        for (auto* particle = request->particles.sentinel.next; particle != &request->particles.sentinel; particle = particle->next) {
            particle->position = player->position;
            particle->position[1] += 1;
            particle->motion = 3;
        }
        const auto beforeCoins = counters[0];
        coins.updateCoins(resources.camera().position, {0, 0, 1});
        check(retainedMoney.auxiliarySpin == coins.modelStorage().auxiliary.spin,
            "Assembled coin owner publishes its retained auxiliary state.");
        check(counters[0] == beforeCoins + 2 && coins.storage().particles.free.count == 200,
            "Coin owner update delivers both contact events and recycles through original collection flow.");
        audio->stop();
        const auto& shared = *managers.players().models;
        for (auto kind : {playermodels::SharedModel::Coin, playermodels::SharedModel::Bill, playermodels::SharedModel::Bill2,
            playermodels::SharedModel::Bill3, playermodels::SharedModel::Leaf}) {
            const auto& data = *shared.model(kind);
            std::cout << "Shared model " << static_cast<unsigned>(kind) << ": calculator " << data.calculator->kind
                << ", joints " << data.geometry.joints.size() << ", envelopes " << data.geometry.envelopes.size()
                << ", draws " << data.geometry.drawMatrices.size() << '\n';
        }
        const std::array<std::shared_ptr<j3dmodel::ModelData>, 3> billModels{
            shared.model(playermodels::SharedModel::Bill), shared.model(playermodels::SharedModel::Bill2),
            shared.model(playermodels::SharedModel::Bill3)};
        bool paused = false;
        float flutter = 0;
        std::uint8_t blink = 2;
        billscatter::Scene bills(billModels, pose, {resources.rooms(), resources.visibility(), managers.collision(),
            player->position, context, random, particleRandom, flutter, paused, blink, dispatch,
            [](billscatter::Particle&) { return 0; }});
        bills.scatter(player->position, {0, 0, 1}, 0, 0, 3, 100);
        for (auto& pool : bills.storage().pools) {
            auto& particle = pool->particles.at(29);
            particle.position = player->position;
            particle.position[1] += 1;
            particle.motion = 1;
        }
        const auto beforeBills = counters[1];
        bills.update();
        check(counters[1] == beforeBills + 3, "All three bill owners deliver contact events to manager-owned Luigi.");
        audio->stop();
        coins.updateCoins(resources.camera().position, {0, 0, 1});
        bills.update();
        unsigned suctionMode = 0;
        float suctionRange = 200;
        jaudio::PositionOwners leafPositions;
        jaudio::PositionalPlayback leafAudio(leafPositions, executable, package, audioResources);
        leafscatter::Scene leaves(pose, {resources.rooms(), resources.visibility(), managers.collision(), player->position,
            context, suctionMode, suctionRange, random, particleRandom, flutter, paused, blink, leafAudio, leafPositions});
        leaves.initialize(shared.model(playermodels::SharedModel::Leaf));
        auto& leafPool = *leaves.storage().storage();
        auto* leafRequest = leafPool.requests.free.take();
        auto* leaf = leafPool.particles.free.take();
        leaf->visible = true;
        leafRequest->particles.append(*leaf);
        leafPool.active.append(*leafRequest);
        const j3dmodel::Matrix camera{1, 0, 0, 17, 0, 1, 0, 23, 0, 0, 1, 31};
        Render render(*managers.players().models, camera, executable);
        for (int i = 1; i <= 2; ++i) {
            auto* drawRequest = coins.scatter(player->position, {0, 0, 1}, 2, 100, 0, 0);
            auto* particle = drawRequest->particles.sentinel.next;
            particle->visible = true;
            particle->next->visible = false;
            render.ids[particle->model.get()] = i;
        }
        render.ids[coins.modelStorage().auxiliary.model.get()] = 3;
        bills.scatter(player->position, {0, 0, 1}, 0, 0, 3, 100);
        for (unsigned i = 0; i < 3; ++i) {
            auto* particle = bills.storage().pools[i]->active.sentinel.next->particles.sentinel.next;
            particle->visible = true;
            render.ids[particle->model.get()] = 4 + i;
        }
        render.ids[leaf->model.get()] = 7;
        coins.storage().paused = paused = true;
        moneyscatter::drawFrame(coins, bills, leaves, *managers.players().models, render);
        check(render.calls == std::vector<int>{-1, 1, 2, 3, 4, 5, 6, 7},
            "Draw traversal retains visible request order, unconditional auxiliary, three bills then active leaves under pause.");
        render.calls.clear();
        moneyscatter::viewFrame(coins, bills, leaves, camera, render);
        check(render.calls == std::vector<int>{-2, 3, 1, 3, 2, 3, -2, 4, -2, 5, -2, 6, -2, 7},
            "Camera traversal repeats auxiliary once per coin request, including empty persistent request, and copies each owner's camera.");
        leaves.release();
        render.calls.clear();
        moneyscatter::viewFrame(coins, bills, leaves, camera, render);
        check(render.calls == std::vector<int>{-2, 3, 1, 3, 2, 3, -2, 4, -2, 5, -2, 6},
            "Inactive leaves skip their camera copy as well as model traversal.");
        counters = retainedCounters;
        std::cout << "Money collection, damage spawning, real shared models and original pickup audio verified.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
