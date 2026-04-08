#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "../skeleton/Bone.hpp"

struct SkeletonComponent
{
    std::vector<SkeletonBone> bones;
    std::unordered_map<std::string, int> boneMap;
};