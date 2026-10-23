#include "jlib/j3d/j3ddrawbuffer.h"

#include <cmath>
#include <stdexcept>

namespace {
    void prepare(j3dmodel::MaterialPacket& packet) {
        if (!packet.shapes) throw std::runtime_error("J3D packet admission requires a shape packet.");
        packet.next = nullptr;
        packet.shapes->next = nullptr;
    }
}

bool j3dmodel::DrawBuffer::entryMaterial(MaterialPacket& packet, std::uint32_t textureAddress) {
    if (packets.empty()) throw std::runtime_error("J3D packet admission requires a nonempty draw buffer.");
    prepare(packet);
    auto& head = packets[textureAddress & (packets.size() - 1)];
    for (auto* current = head; current; current = current->next) {
        if (current->field34 == packet.field34 && !(current->field34 & 0x80000000U)) {
            current->addShape(*packet.shapes);
            return false;
        }
    }
    packet.next = head;
    head = &packet;
    return true;
}

bool j3dmodel::DrawBuffer::entryDepth(MaterialPacket& packet, const Matrix& camera) {
    if (packets.empty() || !matrix) throw std::runtime_error("J3D depth sorting requires a draw buffer and joint matrix.");
    prepare(packet);
    const float depth = -(((camera[11] + camera[10] * (*matrix)[11]) +
        camera[8] * (*matrix)[3]) + camera[9] * (*matrix)[7]);
    std::size_t bucket;
    if (depth < nearPlane) bucket = 0;
    else if (farPlane < depth) bucket = packets.size() - 1;
    else {
        const float quotient = depth / interval;
        if (!std::isfinite(quotient) || quotient < 0 || quotient >= static_cast<float>(packets.size()))
            throw std::runtime_error("Original J3D depth bucket falls outside its draw buffer.");
        bucket = static_cast<std::size_t>(quotient);
    }
    auto& head = packets[packets.size() - 1 - bucket];
    packet.next = head;
    head = &packet;
    return true;
}

bool j3dmodel::DrawBuffer::entryUnsorted(MaterialPacket& packet) {
    if (packets.empty()) throw std::runtime_error("J3D packet admission requires a nonempty draw buffer.");
    prepare(packet);
    packet.next = packets[0];
    packets[0] = &packet;
    return true;
}

bool j3dmodel::DrawBuffer::entryCallbackMaterial(MaterialPacket& packet) {
    prepare(packet);
    if (!callback) return false;
    if (callback->child) packet.next = static_cast<MaterialPacket*>(callback->child);
    callback->child = &packet;
    return true;
}

bool j3dmodel::DrawBuffer::entryCallbackShape(MaterialPacket& packet) {
    prepare(packet);
    if (!callback) return false;
    if (callback->child) packet.shapes->next = static_cast<ShapePacket*>(callback->child);
    callback->child = packet.shapes;
    return true;
}

void j3dmodel::DrawBuffer::visit(const std::function<void(MaterialPacket&)>& draw) const {
    if (drawType > 1) throw std::runtime_error("Unknown original J3D draw traversal.");
    for (std::size_t i = 0; i < packets.size(); ++i) {
        const auto bucket = drawType ? packets.size() - 1 - i : i;
        for (auto* packet = packets[bucket]; packet; packet = packet->next) draw(*packet);
    }
}
