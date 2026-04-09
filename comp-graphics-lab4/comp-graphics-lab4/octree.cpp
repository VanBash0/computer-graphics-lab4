#include "octree.h"

#include <DirectXMath.h>

using namespace DirectX;

Octree::Octree(const std::vector<Entry>& entries, size_t maxObjectsPerNode, size_t maxDepth) {
    rebuild(entries, maxObjectsPerNode, maxDepth);
}

void Octree::rebuild(const std::vector<Entry>& entries, size_t maxObjectsPerNode, size_t maxDepth) {
    mMaxObjectsPerNode = maxObjectsPerNode;
    mMaxDepth = maxDepth;

    if (entries.empty()) {
        mRoot.reset();
        return;
    }

    mRoot = buildNode(computeBounds(entries), entries, 0);
}

std::vector<size_t> Octree::query(const BoundingFrustum& frustum) const {
    std::vector<size_t> visibleObjects;
    if (!mRoot) {
        return visibleObjects;
    }

    queryNode(mRoot.get(), frustum, visibleObjects);
    return visibleObjects;
}

bool Octree::Node::isLeaf() const {
    for (const auto& child : children) {
        if (child) {
            return false;
        }
    }

    return true;
}

std::unique_ptr<Octree::Node> Octree::buildNode(const BoundingBox& bounds, const std::vector<Entry>& entries, size_t depth) const {
    std::unique_ptr<Node> node = std::make_unique<Node>();
    node->bounds = bounds;

    if (entries.empty()) {
        return node;
    }

    if (depth >= mMaxDepth || entries.size() <= mMaxObjectsPerNode) {
        node->entries = entries;
        return node;
    }

    const auto childBounds = splitBounds(bounds);
    std::array<std::vector<Entry>, 8> childEntries;
    std::vector<Entry> stayAtNode;

    for (const auto& entry : entries) {
        bool insertedIntoChild = false;

        for (size_t childIndex = 0; childIndex < childBounds.size(); ++childIndex) {
            if (childBounds[childIndex].Contains(entry.bounds) == CONTAINS) {
                childEntries[childIndex].push_back(entry);
                insertedIntoChild = true;
                break;
            }
        }

        if (!insertedIntoChild) {
            stayAtNode.push_back(entry);
        }
    }

    node->entries = stayAtNode;

    for (size_t childIndex = 0; childIndex < childBounds.size(); ++childIndex) {
        if (!childEntries[childIndex].empty()) {
            node->children[childIndex] = buildNode(childBounds[childIndex], childEntries[childIndex], depth + 1);
        }
    }

    return node;
}

void Octree::queryNode(const Node* node, const BoundingFrustum& frustum, std::vector<size_t>& visibleObjects) const {
    if (!node) {
        return;
    }

    if (!frustum.Intersects(node->bounds)) {
        return;
    }

    for (const auto& entry : node->entries) {
        if (frustum.Intersects(entry.bounds)) {
            visibleObjects.push_back(entry.objectIndex);
        }
    }

    for (const auto& child : node->children) {
        if (child) {
            queryNode(child.get(), frustum, visibleObjects);
        }
    }
}

BoundingBox Octree::computeBounds(const std::vector<Entry>& entries) {
    BoundingBox bounds = entries.front().bounds;

    for (size_t i = 1; i < entries.size(); ++i) {
        BoundingBox::CreateMerged(bounds, bounds, entries[i].bounds);
    }

    return bounds;
}

std::array<BoundingBox, 8> Octree::splitBounds(const BoundingBox& bounds) {
    std::array<BoundingBox, 8> childBounds;

    const XMFLOAT3& center = bounds.Center;
    const XMFLOAT3& extents = bounds.Extents;

    const XMFLOAT3 childExtents = {
        extents.x * 0.5f,
        extents.y * 0.5f,
        extents.z * 0.5f
    };

    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            for (int z = 0; z < 2; ++z) {
                const int index = (x << 2) | (y << 1) | z;

                XMFLOAT3 childCenter = {
                    center.x + (x == 0 ? -childExtents.x : childExtents.x),
                    center.y + (y == 0 ? -childExtents.y : childExtents.y),
                    center.z + (z == 0 ? -childExtents.z : childExtents.z)
                };

                childBounds[index] = BoundingBox(childCenter, childExtents);
            }
        }
    }

    return childBounds;
}