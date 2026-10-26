#include "jlib/j3d/j3dmaterialdisplaylist.h"

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <utility>

namespace {
    using Commands = std::vector<std::function<void(j3dmodel::MaterialLoadTarget&)>>;

    class Recorder : public j3dmodel::MaterialLoadTarget {
    public:
        explicit Recorder(Commands& commands) : commands(commands) {}
        void tev(const j3dmodel::TevCommand& value) override { append(&MaterialLoadTarget::tev, value); }
        void indirectCount(unsigned count) override { append(&MaterialLoadTarget::indirectCount, count); }
        void indirectMatrix(unsigned index, const std::array<float, 6>& matrix, int exponent) override {
            append(&MaterialLoadTarget::indirectMatrix, index, matrix, exponent);
        }
        void indirectScale(unsigned index, const std::array<std::uint8_t, 2>& scale) override {
            append(&MaterialLoadTarget::indirectScale, index, scale);
        }
        void indirectOrder(unsigned index, const std::array<std::uint8_t, 2>& order) override {
            append(&MaterialLoadTarget::indirectOrder, index, order);
        }
        void pixel(const j3dmodel::MaterialPixelBlock& block) override {
            auto copy = std::make_shared<j3dmodel::MaterialPixelBlock>();
            copy->kind = block.kind;
            if (block.fog) copy->fog = std::make_unique<std::array<std::uint8_t, 44>>(*block.fog);
            copy->alpha = block.alpha; copy->alphaReferences = block.alphaReferences;
            copy->blend = block.blend; copy->depth = block.depth;
            copy->depthLocation = block.depthLocation; copy->dither = block.dither;
            commands.push_back([copy](auto& target) { target.pixel(*copy); });
        }
        void materialColor(unsigned index, const std::array<std::uint8_t, 4>& color) override {
            append(&MaterialLoadTarget::materialColor, index, color);
        }
        void ambientColor(unsigned index, const std::array<std::uint8_t, 4>& color) override {
            append(&MaterialLoadTarget::ambientColor, index, color);
        }
        void channelCount(unsigned count) override { append(&MaterialLoadTarget::channelCount, count); }
        void channel(unsigned index, const j3dmodel::ChannelControl& control) override {
            append(&MaterialLoadTarget::channel, index, control);
        }
        void light(unsigned index, const j3dmodel::MaterialLight& light) override {
            append(&MaterialLoadTarget::light, index, light);
        }
        void cull(unsigned mode) override { append(&MaterialLoadTarget::cull, mode); }
        void textureCount(unsigned count) override { append(&MaterialLoadTarget::textureCount, count); }
        void textureCoordinate(unsigned index, const std::array<std::uint8_t, 3>& info, unsigned normalize, unsigned post) override {
            append(&MaterialLoadTarget::textureCoordinate, index, info, normalize, post);
        }
        void textureMatrix(unsigned index, const std::array<float, 12>& matrix, unsigned projection) override {
            if (projection > 1) throw std::runtime_error("Unknown original J3D texture matrix projection.");
            std::array<float, 12> copy{};
            std::copy_n(matrix.begin(), projection ? 8 : 12, copy.begin());
            append(&MaterialLoadTarget::textureMatrix, index, copy, projection);
        }
        void materialMode(std::uint32_t) override { throw std::runtime_error("Material mode does not belong in a J3D display list."); }
        void normalScale(const std::array<std::uint8_t, 16>*) override { throw std::runtime_error("NBT ownership does not belong in a J3D display list."); }

    private:
        template<class Method, class... Args> void append(Method method, Args... args) {
            commands.push_back([method, args...](auto& target) { (target.*method)(args...); });
        }
        Commands& commands;
    };
}

struct j3dmodel::MaterialDisplayList::State { Commands commands; };

j3dmodel::MaterialDisplayList::MaterialDisplayList(const Material& material) : state(std::make_unique<State>()) {
    Recorder recorder(state->commands);
    loadMaterialBlocks(material, recorder);
}

j3dmodel::MaterialDisplayList::~MaterialDisplayList() = default;

void j3dmodel::MaterialDisplayList::replay(MaterialLoadTarget& target) const {
    for (const auto& command : state->commands) command(target);
}

std::size_t j3dmodel::MaterialDisplayList::commandCount() const { return state->commands.size(); }

void j3dmodel::updateMaterialPacket(MaterialPacket& packet) {
    if (!packet.material) throw std::runtime_error("J3D packet has no material.");
    const auto& material = *packet.material;
    if (!(material.flags & 1) && (packet.flags & 1)) return;
    packet.materialIdentity = material.identity;
    packet.identityFlags = material.identityFlags;
    if (material.flags & 1) return;
    std::swap(packet.displayLists[0], packet.displayLists[1]);
    std::swap(packet.nativeDisplayLists[0], packet.nativeDisplayLists[1]);
    packet.nativeDisplayLists[0] = std::make_shared<MaterialDisplayList>(material);
}

void j3dmodel::loadMaterialPacket(const MaterialPacket& packet, std::uint32_t systemFlags, MaterialLoadTarget& target) {
    if (!packet.material) throw std::runtime_error("J3D packet has no material.");
    const auto& material = *packet.material;
    if (material.flags & 1) {
        loadMaterialDirect(material, systemFlags, target);
        return;
    }
    target.materialMode(material.mode);
    if (systemFlags & 2) return;
    if (!packet.nativeDisplayLists[0]) throw std::runtime_error("J3D material packet has no recorded display list.");
    packet.nativeDisplayLists[0]->replay(target);
    if (!material.texture) throw std::runtime_error("J3D packet has no NBT scale owner.");
    const auto& scale = material.texture->nbtScale;
    target.normalScale(scale[0] == 1 ? &scale : nullptr);
}

void j3dmodel::drawMaterialPacket(MaterialPacket& packet, std::uint32_t systemFlags, std::uint32_t drawMode,
    MaterialLoadTarget& materialTarget, PacketDrawTarget& drawTarget) {
    drawTarget.select(packet);
    loadMaterialPacket(packet, systemFlags, materialTarget);
    drawTarget.flush();
    if (drawMode & packet.material->mode)
        for (auto* shape = packet.shapes; shape; shape = shape->next) drawTarget.shape(*shape);
}
