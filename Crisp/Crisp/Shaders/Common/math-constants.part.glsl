#ifndef MATH_CONSTANTS_PART_GLSL
#define MATH_CONSTANTS_PART_GLSL

#define PI 3.1415926535897932384626433832795
#define InvPI 0.31830988618379067154
#define InvTwoPI 0.15915494309189533577

mat3 createCoordinateFrame(in vec3 normal) {
    if (abs(normal.x) > abs(normal.y)) {
        const float invLen = 1.0f / sqrt(normal.x * normal.x + normal.z * normal.z);
        const vec3 bitangent = vec3(normal.z * invLen, 0.0f, -normal.x * invLen);
        return mat3(cross(bitangent, normal), bitangent, normal);
    } else {
        const float invLen = 1.0f / sqrt(normal.y * normal.y + normal.z * normal.z);
        const vec3 bitangent = vec3(0.0f, normal.z * invLen, -normal.y * invLen);
        return mat3(cross(bitangent, normal), bitangent, normal);
    }
}

// void coordinateSystem(in vec3 v1, out vec3 v2, out vec3 v3)
// {
//     if (abs(v1.x) > abs(v1.y))
//     {
//         float invLen = 1.0f / sqrt(v1.x * v1.x + v1.z * v1.z);
//         v3 = vec3(v1.z * invLen, 0.0f, -v1.x * invLen);
//     }
//     else
//     {
//         float invLen = 1.0f / sqrt(v1.y * v1.y + v1.z * v1.z);
//         v3 = vec3(0.0f, v1.z * invLen, -v1.y * invLen);
//     }
//     v2 = cross(v3, v1);
// }


#endif