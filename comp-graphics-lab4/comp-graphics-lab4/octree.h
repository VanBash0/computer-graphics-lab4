#ifndef OCTREE_H
#define OCTREE_H

#include <DirectXCollision.h>

#include <array>
#include <memory>
#include <vector>

class Octree {
public:
    struct Entry {
        size_t objectIndex = 0;
        DirectX::BoundingBox bounds = {};
    };

    Octree() = default;
    Octree(const std::vector<Entry>& entries, size_t maxObjectsPerNode = 16, size_t maxDepth = 8);

    void rebuild(const std::vector<Entry>& entries, size_t maxObjectsPerNode = 16, size_t maxDepth = 8);
    std::vector<size_t> query(const DirectX::BoundingFrustum& frustum) const;

private:
    struct Node {
        DirectX::BoundingBox bounds = {};
        std::vector<Entry> entries;
        std::array<std::unique_ptr<Node>, 8> children;

        bool isLeaf() const;
    };

    std::unique_ptr<Node> mRoot;
    size_t mMaxObjectsPerNode = 16;
    size_t mMaxDepth = 8;

    std::unique_ptr<Node> buildNode(const DirectX::BoundingBox& bounds, const std::vector<Entry>& entries, size_t depth) const;
    void queryNode(const Node* node, const DirectX::BoundingFrustum& frustum, std::vector<size_t>& visibleObjects) const;

    static DirectX::BoundingBox computeBounds(const std::vector<Entry>& entries);
    static std::array<DirectX::BoundingBox, 8> splitBounds(const DirectX::BoundingBox& bounds);
};

#endif // OCTREE_H