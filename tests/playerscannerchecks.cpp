#include "game/player/playerscannerinteraction.h"
#include "game/player/playerfurniturescancache.h"
#include "game/player/playeractorscanbindings.h"
#include "game/player/playersheetlayout.h"
#include "game/player/playersheettextures.h"
#include "platform/d3d11/j3dtexturerenderer.h"
#include "game/scene/gameplayresourceowner.h"

#include <stdexcept>
#include <string>
#include <cmath>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    class Services final : public playerscanner::Services {
    public:
        std::int16_t delay() const override { return frames; }
        void playScanSound(unsigned sound) override { require(sound == 0x3813, "Original scan sound."); events += 's'; }
        playerscanner::Hit furniture(const playerscanner::Vector&, const playerscanner::Vector& direction) override {
            require(direction == playerscanner::Vector{0, 0, 1}, "Scanner uses camera-to-target direction.");
            events += 'f'; return furnitureHit;
        }
        playerscanner::Hit actor(const playerscanner::Vector&, const playerscanner::Vector&) override { events += 'a'; return actorHit; }
        unsigned actorType(int) override { events += 't'; return type; }
        std::optional<unsigned> actorMessage(int) override { events += 'r'; return message; }
        void setDiscoveryFlag(unsigned flag) override { require(flag == 0xad, "Original discovery flag."); events += 'd'; }
        void selectMessage(unsigned player, unsigned value) override {
            require(player == 0 && value == expectedMessage, "Original message player and low-byte payload."); events += 'm';
        }
        void openMessage() override { events += 'o'; }
        void markFurniture(int) override { events += 'k'; }
        bool mirror(int) override { events += 'w'; return isMirror; }
        bool furnitureMessage(int) override { events += 'q'; return hasMessage; }
        void openFurnitureMessage() override { events += 'v'; }
        std::int16_t frames = 2;
        playerscanner::Hit furnitureHit{}, actorHit{};
        unsigned type = 0x31, expectedMessage = 0x1a;
        std::optional<unsigned> message;
        bool isMirror = false, hasMessage = false;
        std::string events;
    };
}

void checkScannerGeometry() {
    using namespace playerscanner;
    FurnitureTarget box{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}, {2, 3, 4}, true};
    float distance = 99;
    require(intersect(box, {0, 0, 0}, {0, 0, 1}, distance) && distance == 0, "Inside scanner box hits immediately.");
    require(!intersect(box, {5, 0, 0}, {1, 0, 0}, distance) && distance == 0,
        "Outward scan misses without changing distance.");
    require(!intersect(box, {5, 0, 0}, {-1, 0, 0}, distance), "Original outside-box test rejects a zero direction axis.");
    require(intersect(box, {5, 0, 0}, {-1, 0.1f, 0.1f}, distance) && distance == 5,
        "Furniture distance is origin magnitude, not surface intersection distance.");
    require(furnitureTarget({box, box}, {5, 0, 0}, {-1, 0.1f, 0.1f}, 100).index == 0,
        "Equal furniture distances retain the first entry.");
    require(furnitureTarget({box}, {5, 0, 0}, {-1, 0.1f, 0.1f}, 5).index == -1,
        "Furniture maximum distance is exclusive.");
    ActorTarget actor{{0, 0, 10}, 2, 17, true, true, true, true, true};
    require(actorTarget({actor}, {}, {0, 0, 1}, 100).index == 17, "Actor scan returns owner slot.");
    actor.position[0] = 2;
    require(actorTarget({actor}, {}, {0, 0, 1}, 100).index == -1, "Actor radius boundary is exclusive.");
    actor.position = {0, 0, -10};
    require(actorTarget({actor}, {}, {0, 0, 1}, 100).index == -1, "Actor behind scanner is excluded.");
}

void checkSheetMotion(const std::vector<std::uint8_t>& executable);
void checkSheetSuction(const std::vector<std::uint8_t>& executable);
void checkSheetMaterial(gameplay::ResourceOwner& owner);
void checkSheetFrame(gameplay::ResourceOwner& owner);
void checkSheetDraw(gameplay::ResourceOwner& owner);

void checkScannerInteraction(gameplay::ResourceOwner& owner) {
    checkSheetMotion(owner.executable());
    checkSheetSuction(owner.executable());
    checkSheetMaterial(owner);
    checkSheetDraw(owner);
    playersheets::Builder sheetBuilder(owner.executable());
    const auto& shared = *owner.managers().players().models;
    playermodels::GridModels textureGrid(shared, 16, 16);
    playermodels::GridModels otherGrid(shared, 16, 16);
    Microsoft::WRL::ComPtr<ID3D11Device> textureDevice;
    require(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, textureDevice.GetAddressOf(), nullptr, nullptr)), "Sheet texture validation device.");
    for (unsigned mode = 0; mode <= 3; ++mode) {
        for (unsigned texture = 0; texture < 16; ++texture) {
            playersheets::Layout parameters;
            parameters.mode = mode;
            parameters.texture = texture;
            parameters.secondaryTexture = 13;
            const auto binding = playersheets::bindTextures(parameters, shared, textureGrid);
            require(binding.primary == shared.texture(texture) && binding.secondary == shared.texture(13),
                "Sheet textures retain the original manager-owned resources.");
            require(binding.wrapS == 0 && binding.wrapT == 0 && binding.minFilter == 1 && binding.magFilter == 1 &&
                !binding.mipmap && binding.minLod == 0 && binding.maxLod == 0 && binding.lodBias == 0,
                "Direct cloth sampling uses the original clamp, linear and zero-LOD settings.");
            if (!mode) {
                for (const auto& instance : textureGrid.instances)
                    require(instance->model->geometry.textureReplacements.empty(),
                        "Direct cloth mode does not overwrite any model texture table.");
                continue;
            }
            auto& geometry = textureGrid.instances[mode - 1]->model->geometry;
            require(!geometry.textureHeaders.empty(), "Original sheet model has a texture table.");
            require(otherGrid.instances[mode - 1]->model.get() == textureGrid.instances[mode - 1]->model.get(),
                "Sheet instances share the original mutable model texture table.");
            for (const auto& replacement : geometry.textureReplacements)
                require(replacement == binding.primary, "Every original model texture entry receives the selected sheet image.");
            j3drenderer::Textures uploaded;
            uploaded.load(textureDevice.Get(), geometry);
            D3D11_SAMPLER_DESC sampler{};
            uploaded.sampler(0)->GetDesc(&sampler);
            const auto& source = *binding.primary;
            const D3D11_TEXTURE_ADDRESS_MODE addresses[]{D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_MIRROR};
            require(sampler.AddressU == addresses[source.at(6)] && sampler.AddressV == addresses[source.at(7)],
                "Model sampling preserves the copied BTI wrap modes independently of direct cloth sampling.");
        }
    }
    playersheets::Layout invalidTexture;
    invalidTexture.texture = 16;
    const auto previousTexture = textureGrid.texture.primary;
    bool rejectedTexture = false;
    try { playersheets::bindTextures(invalidTexture, shared, textureGrid); }
    catch (const std::out_of_range&) { rejectedTexture = true; }
    require(rejectedTexture && textureGrid.texture.primary == previousTexture,
        "Invalid resource selection cannot partially replace the owned texture binding.");
    for (auto& instance : textureGrid.instances) instance->model->geometry.textureReplacements.clear();
    playerscanner::SheetInput sample{0, 0, 0, 1, 0, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}, {100, 50, 100}, false, false};
    playersheets::Grid grid(256);
    for (unsigned kind = 1; kind <= 24; ++kind) {
        sample.kind = kind;
        const auto layout = sheetBuilder.build(sample, 256, 3, 13, false);
        require(layout.secondaryTexture == 13 && layout.lifetime == 45,
            "Secondary resource selection does not overwrite the original timing default.");
        grid.initialize(layout);
        require(layout.width * layout.height == 256, "Sheet orientation preserves original point capacity.");
        require(layout.mode == (kind >= 13 && kind <= 16 ? 2u : kind >= 17 && kind <= 20 ? 3u : 0u),
            "All sheet kinds preserve their original presentation mode.");
        for (const auto& point : grid.points()) {
            require(point.active && point.velocity == playerscanner::Vector{}, "Initial sheet points are active and stationary.");
            for (float value : point.position) require(std::isfinite(value), "Sheet initialization produces finite positions.");
            for (unsigned link = 0; link < point.linkCount; ++link)
                require(std::isfinite(point.links[link].length), "Sheet constraint lengths are finite.");
        }
        const bool vertical = (kind >= 9 && kind <= 12) || kind >= 21;
        require(layout.splitRows == vertical && layout.anchored == (kind < 21), "Vertical sheet modes retain split-row and anchoring flags.");
    }
    sample.kind = 1;
    auto layout = sheetBuilder.build(sample, 256, 3, 45, false);
    require(layout.corners[0] == playerscanner::Vector{-100, 50, -100} &&
        layout.corners[3] == playerscanner::Vector{100, 50, 100} && layout.border == std::array<std::int16_t, 4>{4, 4, 10, 10},
        "First sheet kind preserves original winding and asymmetric fixed border.");
    grid.initialize(layout);
    require(grid.points()[4 * 16 + 4].fixed && !grid.points()[0].fixed && grid.points()[0].position[1] < 50,
        "Interior anchors stay fixed while hanging corners sag.");
    require(std::abs(grid.points()[4 * 16 + 4].weight - 1.0f / 7) < 0.00001f,
        "Original border weight uses inclusive grid bounds.");
    sample.extent = {1000, 50, 100};
    layout = sheetBuilder.build(sample, 256, 3, 45, false);
    require(layout.width == 32 && layout.height == 8, "Wide sheet reshapes the existing point allocation.");
    sample.extent = {100, 50, 1000};
    layout = sheetBuilder.build(sample, 256, 3, 45, false);
    require(layout.width == 8 && layout.height == 32, "Long sheet reshapes the existing point allocation.");
    sample.extent = {200, 50, 100};
    layout = sheetBuilder.build(sample, 256, 3, 45, false);
    require(layout.width == 16 && layout.height == 16, "Exact aspect threshold does not reshape the grid.");
    sample.kind = 9;
    sample.extent = {100, 50, 100};
    layout = sheetBuilder.build(sample, 256, 3, 45, false);
    grid.initialize(layout);
    require(layout.corners[0] == playerscanner::Vector{-100, 50, 100} &&
        layout.corners[3] == playerscanner::Vector{100, -50, 100}, "Vertical sheet corner winding matches the original face selector.");
    const auto normal = grid.planes()[0].normal;
    layout.reversePlanes = true;
    grid.initialize(layout);
    for (unsigned i = 0; i < 3; ++i) require(grid.planes()[0].normal[i] == -normal[i],
        "Room orientation flag reverses collision plane winding.");
    sample.kind = 1;
    sample.elastic = true;
    layout = sheetBuilder.build(sample, 256, 3, 45, false);
    require(layout.elasticity == 450 && layout.range == layout.drops[0] * 0.5f,
        "Elastic sheets retain the original range and response constants.");
    const auto& models = owner.managers().preparePlayerDoor(0).catalog;
    actorcollision::Catalog collisions(owner.executable(), models);
    playerscanner::ActorBindings actorBindings(owner.managers().enemies(), owner.managers().actors(), models, collisions, owner.visibility());
    const auto* player = owner.managers().playerEvents().player(0);
    require(actorBindings.type(player->actorId) == player->actorType, "Actor scanner resolves the real registry identity.");
    const auto hit = actorBindings.query(player->position, {0, 0, 1});
    require(hit.index == -1, "Initial foyer has no eligible visible attached actor scan proxy.");
    playerscanner::FurnitureCache cache(owner.executable(), owner.furniture(), owner.managers().flags());
    std::vector<playerscanner::SheetInput> sheets;
    const auto initializeSheet = [&](const playerscanner::SheetInput& input) {
        require(input.slot < 4 && input.kind >= 1 && input.kind <= 24, "Original sheet binding range.");
        const auto layout = sheetBuilder.build(input, 256, input.texture, 13, false);
        auto& ownedGrid = *player->components->grids->grids.at(input.slot);
        const auto& binding = playersheets::bindTextures(layout, shared, ownedGrid);
        require(binding.primary == shared.texture(input.texture) && layout.lifetime == 45,
            "Actual foyer texture selection retains the independent timing default.");
        require(&binding == &ownedGrid.texture, "The original player grid owns the retained texture binding.");
        grid.initialize(layout);
        for (const auto& point : grid.points()) for (float value : point.position)
            require(std::isfinite(value), "Actual foyer sheet geometry initializes without nonfinite points.");
        sheets.push_back(input);
    };
    require(cache.refresh(2, initializeSheet), "First room builds actual mission furniture scan entries.");
    require(cache.targets().size() == 8 && cache.row(0) == 99 && cache.row(7) == 296,
        "Foyer scanner retains mission order including model-less furniture.");
    require(!cache.refresh(2, {}), "Unchanged room retains the original scan cache.");
    require(cache.contacts().size() == cache.targets().size(), "Furniture interaction retains one contact bound per selected row.");
    bool contactFound = false;
    for (unsigned i = 0; i < cache.contacts().size(); ++i) {
        const auto& bounds = cache.contacts()[i];
        require(bounds.minimumY <= bounds.center[1] && bounds.maximumY >= bounds.center[1],
            "Rotated furniture contact bounds must include the retained center.");
        auto position = bounds.center;
        position[1] = bounds.minimumY;
        position[2] -= 10;
        const playerscanner::FurnitureInput input{position, {0, 0, 1}, 40, 120, 84, 200, false};
        const auto hit = cache.press(input, {}, [] { return 0u; });
        if (hit.target < 0) continue;
        contactFound = true;
        require(hit.reaction >= 1 && hit.reaction <= 5 && !hit.invert,
            "Furniture button contact must select an original action reaction.");
        float length = 0;
        for (float value : hit.direction) length += value * value;
        require(std::abs(length - 1) < 0.001f, "Furniture contact publishes a normalized facing direction.");
    }
    require(contactFound, "Actual foyer furniture must be reachable by the original interaction query.");
    const playerscanner::FurnitureInput distant{{100000, 100000, 100000}, {0, 0, 1}, 40, 120, 84, 200, false};
    require(cache.press(distant, {}, {}).target == -1 && cache.automatic(distant).target == -1,
        "Out-of-room player positions must not activate furniture or inversion volumes.");
    bool foundMirror = false, foundHit = false;
    for (unsigned i = 0; i < cache.targets().size(); ++i) {
        if (cache.row(i) == 208) {
            bool covered = false;
            for (const auto& sheet : sheets) if (sheet.target == i) covered = true;
            require(cache.mirror(i) == !covered, "Sheet-bound mirrors are excluded by the original scanner gate.");
            foundMirror = true;
        }
        const auto before = owner.furniture().rowState(cache.row(i)).flags;
        cache.mark(i);
        require(owner.furniture().rowState(cache.row(i)).flags == (before | 0x20),
            "Scan discovery updates the shared furniture row without replacing its flags.");
        require(!cache.occupied(i), "Initial furniture occupant sentinel remains empty.");
        const auto& target = cache.targets()[i];
        if (!target.enabled) continue;
        playerscanner::Vector center;
        for (unsigned axis = 0; axis < 3; ++axis) center[axis] =
            -(target.inverse[axis] * target.inverse[3] + target.inverse[axis + 4] * target.inverse[7] +
                target.inverse[axis + 8] * target.inverse[11]);
        if (cache.query(center, {0.1f, 0.2f, 1}).index >= 0) foundHit = true;
    }
    require(foundMirror && foundHit, "Real foyer records provide mirror classification and scanner intersections.");
    require(cache.refresh(30, initializeSheet) && cache.targets().size() == 3 && cache.row(0) == 335,
        "Room change replaces scan entries with the adjoining room's mission rows.");
    require(cache.refresh(2, initializeSheet) && (owner.furniture().rowState(208).flags & 0x20),
        "Returning to a room retains shared discovery state.");
    auto& state = *owner.managers().playerEvents().player(0)->components->scanner;
    const auto savedState = state.state;
    const auto saved8 = state.field8, savedC = state.fieldC;
    const float savedTimer = state.field10;
    auto* savedCamera = state.camera;
    gameplaycamera::CameraOwner camera;
    camera.position = {};
    camera.target = {0, 0, 10};
    playerscanner::Input input{true, false, camera};
    Services services;
    bool warp = true;
    state.state = {};
    require(playerscanner::update(state, input, warp, services) && !warp && state.field10 == 2 && services.events == "s",
        "Starting scan sets delay without scanning in the same frame.");
    input.pressed = false;
    require(playerscanner::update(state, input, warp, services) && std::abs(state.field10 - 0.8f) < 0.00001f,
        "Scanner decrements the original 1.2-unit clock.");
    services.furnitureHit = {3, 10}; services.actorHit = {4, 10}; services.isMirror = true;
    require(playerscanner::update(state, input, warp, services) && warp && !state.state[0] && state.fieldC == 0xffffffff &&
        services.events == "sfakw", "Furniture wins equal distance and mirror warp remains active after timer expiry.");
    services = Services{};
    services.actorHit = {4, 3};
    state.state = {1, 0, 1}; state.field10 = 1;
    warp = true;
    require(!playerscanner::update(state, input, warp, services) && warp && services.events == "fatdmo" && !state.state[2],
        "Actor scan publishes message in original order while leaving inactive output untouched.");
    services = Services{}; services.actorHit = {4, 3}; services.type = 0x80;
    services.message = 0x123; services.expectedMessage = 0x23;
    state.state = {1, 0, 0}; state.field10 = 1;
    playerscanner::update(state, input, warp, services);
    require(services.events == "fatrdmo", "Actor placement message truncates to one byte.");
    services = Services{};
    state.state = {1, 0, 0}; state.field10 = 5; input.cancel = true;
    require(!playerscanner::update(state, input, warp, services) && services.events.empty() && state.field10 == 0,
        "Early cancellation does not query targets.");
    state.state = savedState; state.field8 = saved8; state.fieldC = savedC;
    state.field10 = savedTimer; state.camera = savedCamera;
    checkSheetFrame(owner);
}
