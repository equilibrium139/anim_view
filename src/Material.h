#ifndef MATERIAL_H
#define MATERIAL_H

#include "Texture.h"
#include <glm/vec3.hpp>

enum class PhongMaterialFlags : std::uint32_t
{
    DEFAULT = 0,
    DIFFUSE_WITH_ALPHA = 1,
};

inline PhongMaterialFlags operator | (PhongMaterialFlags lhs, PhongMaterialFlags rhs)
{
    using T = std::underlying_type_t<PhongMaterialFlags>;
    return (PhongMaterialFlags)((T)lhs | (T)rhs);
}

inline PhongMaterialFlags& operator |= (PhongMaterialFlags& lhs, PhongMaterialFlags rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

inline PhongMaterialFlags operator & (PhongMaterialFlags lhs, PhongMaterialFlags rhs)
{
    using T = std::underlying_type_t<PhongMaterialFlags>;
    return (PhongMaterialFlags)((T)lhs & (T)rhs);
}

inline PhongMaterialFlags& operator &= (PhongMaterialFlags& lhs, PhongMaterialFlags rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

struct PhongMaterial
{
	glm::vec3 diffuse_coefficient;
	glm::vec3 specular_coefficient;
	float shininess;
    PhongMaterialFlags flags = PhongMaterialFlags::DEFAULT;

	Texture diffuse_map;
	Texture specular_map;
	Texture normal_map;

    inline bool HasFlag(PhongMaterialFlags flag)
    {
        return (std::underlying_type_t<PhongMaterialFlags>)(flags & flag) != 0;
    }
};

#endif