#include "jlib/j3d/j3dmodelhierarchy.h"

#include <stdexcept>

namespace {
    unsigned half(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (unsigned(bytes.at(offset)) << 8) | bytes.at(offset + 1);
    }

    class Reader {
    public:
        Reader(const std::vector<std::uint8_t>& bytes, unsigned offset, j3dmodel::Hierarchy& hierarchy) :
            bytes(bytes), cursor(offset), hierarchy(hierarchy) {}

        void read(int parent) {
            int current = parent;
            std::vector<std::pair<int, int>> parents;
            while (true) {
                const unsigned type = half(bytes, cursor);
                const unsigned index = half(bytes, cursor + 2);
                if (!type) {
                    return;
                }
                cursor += 4;
                if (type == 1) {
                    parents.emplace_back(parent, current);
                    parent = current;
                } else if (type == 2) {
                    if (parents.empty()) {
                        return;
                    }
                    parent = parents.back().first;
                    current = parents.back().second;
                    parents.pop_back();
                } else if (type == 0x10) {
                    if (index >= hierarchy.joints.size()) {
                        throw std::out_of_range("J3D hierarchy joint index.");
                    }
                    current = static_cast<int>(index);
                    if (parent < 0) {
                        hierarchy.root = current;
                    } else {
                        auto& joint = hierarchy.joints.at(parent);
                        if (joint.child < 0) {
                            joint.child = current;
                        } else {
                            int last = joint.child;
                            unsigned visited = 0;
                            while (hierarchy.joints.at(last).sibling >= 0) {
                                if (++visited > hierarchy.joints.size()) {
                                    throw std::runtime_error("Cyclic J3D joint sibling chain.");
                                }
                                last = hierarchy.joints[last].sibling;
                            }
                            hierarchy.joints[last].sibling = current;
                        }
                    }
                } else if (type == 0x11) {
                    if (parent < 0) {
                        throw std::runtime_error("J3D material has no parent joint.");
                    }
                    auto& material = hierarchy.materials.at(index);
                    auto& joint = hierarchy.joints.at(parent);
                    if (joint.material >= 0) {
                        material.next = joint.material;
                    }
                    joint.material = static_cast<int>(index);
                } else if (type == 0x12) {
                    if (parent < 0 || hierarchy.joints.at(parent).material < 0) {
                        throw std::runtime_error("J3D shape has no parent material.");
                    }
                    hierarchy.boundShapes.at(index) = true;
                    hierarchy.materials.at(hierarchy.joints[parent].material).shape = static_cast<int>(index);
                } else {
                    throw std::runtime_error("Unsupported J3D hierarchy command.");
                }
            }
        }

    private:
        const std::vector<std::uint8_t>& bytes;
        unsigned cursor;
        j3dmodel::Hierarchy& hierarchy;
    };
}

bool j3dmodel::readHierarchy(const std::vector<std::uint8_t>& bytes, unsigned offset, unsigned jointCount,
    unsigned materialCount, unsigned shapeCount, Hierarchy& result, std::string& error) {
    error.clear();
    try {
        Hierarchy pending;
        pending.joints.resize(jointCount);
        pending.materials.resize(materialCount);
        pending.boundShapes.resize(shapeCount);
        Reader reader(bytes, offset, pending);
        reader.read(-1);
        result = std::move(pending);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
