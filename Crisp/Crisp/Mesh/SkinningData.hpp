#pragma once

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Math/Headers.hpp>

#include <vector>

namespace crisp {
struct Joint {
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 scale;
};

struct Skeleton {
    std::vector<Joint> joints;
    std::vector<int32_t> parents;
    std::vector<glm::mat4> jointTransforms;

    inline void setJointCount(size_t jointCount) {
        joints.resize(jointCount);
        parents.resize(jointCount, -1);
        jointTransforms.resize(jointCount);
    }

    inline glm::mat4 getJointTransform(const int32_t idx, std::vector<bool>& processed) const {
        if (processed[idx]) {
            return jointTransforms[idx];
        }

        if (parents[idx] == -1) {
            processed[idx] = true;
            return jointTransforms[idx];
        }

        processed[idx] = true;
        return getJointTransform(parents[idx], processed) * jointTransforms[idx];
    }

    inline void updateJointTransforms(const std::vector<glm::mat4>& inverseBindTransforms) {
        // Recalculate joint transforms in their own space each.
        for (uint32_t i = 0; i < joints.size(); ++i) {
            jointTransforms[i] = glm::translate(joints[i].translation) * glm::toMat4(joints[i].rotation);
        }

        // Recalculate the transformation hierarchy.
        std::vector<bool> processedJoints(joints.size(), false);
        for (uint32_t i = 0; i < joints.size(); ++i) {
            jointTransforms[i] = getJointTransform(i, processedJoints);
        }

        // Add the inverse bind transforms.
        for (uint32_t i = 0; i < joints.size(); ++i) {
            jointTransforms[i] = jointTransforms[i] * inverseBindTransforms[i];
        }
    }
};

struct SkinningData {
    static constexpr uint32_t JointsPerVertex{4};

    Skeleton skeleton;

    std::vector<glm::mat4> modelJointTransforms;

    std::vector<glm::mat4> inverseBindTransforms;

    FlatHashMap<int32_t, int32_t> modelNodeToLinearIdx;
};

struct AnimationSampler {
    enum class Interpolation {
        Linear,
        Step,
        CubicSpline
    };

    Interpolation interpolation;

    std::vector<float> inputs;
    std::vector<std::byte> outputs;
};

struct AnimationChannel {
    AnimationSampler sampler;

    uint32_t targetNode;
    std::string propertyName;
};

struct GltfAnimation {
    std::vector<AnimationChannel> channels;

    inline void updateJoints(std::vector<Joint>& joints, float time) {
        auto getTranslation = [this](const AnimationChannel& channel, float t) -> glm::vec3 {
            auto v = std::upper_bound(channel.sampler.inputs.begin(), channel.sampler.inputs.end(), t);
            int hi = std::clamp(
                (int)std::distance(channel.sampler.inputs.begin(), v), 1, (int)channel.sampler.inputs.size() - 1);
            int lo = std::clamp(hi - 1, 0, (int)channel.sampler.inputs.size() - 1);

            t = std::min(t, channel.sampler.inputs[hi]);

            glm::vec3 hiv{};
            glm::vec3 lov{};
            memcpy(&lov, &channel.sampler.outputs[sizeof(glm::vec3) * lo], sizeof(glm::vec3));
            memcpy(&hiv, &channel.sampler.outputs[sizeof(glm::vec3) * hi], sizeof(glm::vec3));
            float interp = (t - channel.sampler.inputs[lo]) / (channel.sampler.inputs[hi] - channel.sampler.inputs[lo]);
            return glm::mix(lov, hiv, interp);
        };

        auto getRotation = [this](const AnimationChannel& channel, float t) -> glm::quat {
            auto v = std::upper_bound(channel.sampler.inputs.begin(), channel.sampler.inputs.end(), t);
            int hi = std::clamp(
                (int)std::distance(channel.sampler.inputs.begin(), v), 1, (int)channel.sampler.inputs.size() - 1);
            int lo = std::clamp(hi - 1, 0, (int)channel.sampler.inputs.size() - 1);

            t = std::min(t, channel.sampler.inputs[hi]);

            glm::quat hiv{};
            glm::quat lov{};
            memcpy(&lov, &channel.sampler.outputs[sizeof(glm::quat) * lo], sizeof(glm::quat));
            memcpy(&hiv, &channel.sampler.outputs[sizeof(glm::quat) * hi], sizeof(glm::quat));
            float interp = (t - channel.sampler.inputs[lo]) / (channel.sampler.inputs[hi] - channel.sampler.inputs[lo]);
            return glm::mix(lov, hiv, interp);
        };

        for (auto& ch : channels) {
            if (ch.propertyName == "translation") {
                joints[ch.targetNode].translation = getTranslation(ch, time);
            } else if (ch.propertyName == "rotation") {
                joints[ch.targetNode].rotation = getRotation(ch, time);
            }
        }
    }
};

} // namespace crisp
