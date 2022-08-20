#pragma once

#include <memory>
#include <array>
#include <vector>
#include "BoundingBox.hpp"

namespace crisp
{
    template <typename T>
    struct OctNode
    {
        static constexpr int NumChildren = 8;
        std::array<std::unique_ptr<OctNode>, NumChildren> children;
        std::vector<T> data;
    };

    template <typename T>
    class Octree
    {
    public:
        Octree(const BoundingBox3& boundingBox, int maxDepth = 16)
            : m_boundingBox(boundingBox)
            , m_maxDepth(maxDepth)
        {
        }

        void add(const T& item, const BoundingBox3& itemBoundingBox)
        {
            if (!m_boundingBox.overlaps(itemBoundingBox))
                return;

            auto extents = itemBoundingBox.getExtents();
            add(m_root, m_boundingBox, item, itemBoundingBox, glm::dot(extents, extents));
        }

        template <typename Func>
        void lookup(const glm::vec3& point, Func& f)
        {
            if (m_boundingBox.contains(point))
                return;

            lookup(m_root, m_boundingBox, point, f);
        }

        inline BoundingBox3 getChildBound(int child, const BoundingBox3& bounds, const glm::vec3& mid)
        {
            BoundingBox3 bound;
            bound.min.x = (child & 4) ? mid.x : bounds.min.x;
            bound.min.y = (child & 2) ? mid.y : bounds.min.y;
            bound.min.z = (child & 1) ? mid.z : bounds.min.z;
            bound.max.x = (child & 4) ? bounds.max.x : mid.x;
            bound.max.y = (child & 2) ? bounds.max.y : mid.y;
            bound.max.z = (child & 1) ? bounds.max.z : mid.z;
            return bound;
        }

    private:
        void add(OctNode<T>& node, const BoundingBox3& nodeBoundingBox, const T& item, const BoundingBox3& itemBoundingBox, float diag2, int depth = 0)
        {
            auto nodeExtents = nodeBoundingBox.getExtents();
            if (depth == m_maxDepth || glm::dot(nodeExtents, nodeExtents) < diag2)
            {
                node.data.emplace_back(item);
                return;
            }

            glm::vec3 mid = nodeBoundingBox.getCenter();

            bool ltX = itemBoundingBox.min.x < mid.x;
            bool gtX = itemBoundingBox.max.x > mid.x;
            bool ltY = itemBoundingBox.min.y < mid.y;
            bool gtY = itemBoundingBox.max.y > mid.y;
            bool ltZ = itemBoundingBox.min.z < mid.z;
            bool gtZ = itemBoundingBox.max.z > mid.z;
            bool over[8] =
            {
                ltX && ltY && ltZ, ltX && ltY && gtZ,
                ltX && gtY && ltZ, ltX && gtY && gtZ,
                gtX && ltY && ltZ, gtX && ltY && gtZ,
                gtX && gtY && ltZ, gtX && gtY && gtZ
            };

            for (int child = 0; child < OctNode<T>::NumChildren; ++child)
            {
                if (!over[child])
                    continue;

                if (node.children[child] == nullptr)
                    node.children[child] = std::make_unique<OctNode<T>>();

                BoundingBox3 childBox = getChildBound(child, nodeBoundingBox, mid);
                add(*node.children[child], childBox, item, itemBoundingBox, diag2, depth + 1);
            }
        }

        template <typename Func>
        bool lookup(OctNode<T>& node, const BoundingBox3& nodeBounds, const glm::vec3& point, Func& f)
        {
            for (size_t i = 0; i < node.data.size(); ++i)
                if (!f(node.data[i]))
                    return false;

            glm::vec3 mid = nodeBounds.getCenter();
            int child =
                (point.x > mid.x ? 4 : 0) +
                (point.y > mid.y ? 2 : 0) +
                (point.z > mid.z ? 1 : 0);

            if (node.children[child] == nullptr)
                return true;

            BoundingBox3 childBounds = getChildBound(child, nodeBounds, mid);
            return lookup(*node.children[child], childBounds, point, f);
        }

        OctNode<T> m_root;
        BoundingBox3 m_boundingBox;
        int m_maxDepth;
    };
}