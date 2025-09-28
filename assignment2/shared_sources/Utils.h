#pragma once

#include <vector>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

using std::string;

// Exit with an error message formatted with sprintf
void fail(const string& reason);

// Portable 
string fileOpenDialog(const string& fileTypeName, const string& fileExtensions);
string loadTextFile(const std::filesystem::path& source);

// http://stackoverflow.com/questions/2270726/how-to-determine-the-size-of-an-array-of-strings-in-c
template <typename T, std::size_t N>
char (&static_sizeof_array( T(&)[N] ))[N];   // declared, not defined
#define SIZEOF_ARRAY( x ) sizeof(static_sizeof_array(x))


inline string GetGLTypeString(GLenum type)
{
    static const std::unordered_map<GLenum, string> typeMap =
    {
        {GL_FLOAT, "float"},
        {GL_FLOAT_VEC2, "vec2"},
        {GL_FLOAT_VEC3, "vec3"},
        {GL_FLOAT_VEC4, "vec4"},
        {GL_DOUBLE, "double"},
        {GL_INT, "int"},
        {GL_INT_VEC2, "ivec2"},
        {GL_INT_VEC3, "ivec3"},
        {GL_INT_VEC4, "ivec4"},
        {GL_UNSIGNED_INT, "unsigned int"},
        {GL_UNSIGNED_INT_VEC2, "uvec2"},
        {GL_UNSIGNED_INT_VEC3, "uvec3"},
        {GL_UNSIGNED_INT_VEC4, "uvec4"},
        {GL_BOOL, "bool"},
        {GL_BOOL_VEC2, "bvec2"},
        {GL_BOOL_VEC3, "bvec3"},
        {GL_BOOL_VEC4, "bvec4"},
        {GL_FLOAT_MAT2, "mat2"},
        {GL_FLOAT_MAT3, "mat3"},
        {GL_FLOAT_MAT4, "mat4"},
        {GL_FLOAT_MAT2x3, "mat2x3"},
        {GL_FLOAT_MAT2x4, "mat2x4"},
        {GL_FLOAT_MAT3x2, "mat3x2"},
        {GL_FLOAT_MAT3x4, "mat3x4"},
        {GL_FLOAT_MAT4x2, "mat4x2"},
        {GL_FLOAT_MAT4x3, "mat4x3"},
        {GL_SAMPLER_1D, "sampler1D"},
        {GL_SAMPLER_2D, "sampler2D"},
        {GL_SAMPLER_3D, "sampler3D"},
        {GL_SAMPLER_CUBE, "samplerCube"},
        {GL_SAMPLER_1D_SHADOW, "sampler1DShadow"},
        {GL_SAMPLER_2D_SHADOW, "sampler2DShadow"},
        {GL_SAMPLER_1D_ARRAY, "sampler1DArray"},
        {GL_SAMPLER_2D_ARRAY, "sampler2DArray"},
        {GL_SAMPLER_1D_ARRAY_SHADOW, "sampler1DArrayShadow"},
        {GL_SAMPLER_2D_ARRAY_SHADOW, "sampler2DArrayShadow"},
        {GL_SAMPLER_2D_MULTISAMPLE, "sampler2DMS"},
        {GL_SAMPLER_2D_MULTISAMPLE_ARRAY, "sampler2DMSArray"},
        {GL_SAMPLER_CUBE_SHADOW, "samplerCubeShadow"},
        {GL_SAMPLER_BUFFER, "samplerBuffer"},
        {GL_SAMPLER_2D_RECT, "sampler2DRect"},
        {GL_SAMPLER_2D_RECT_SHADOW, "sampler2DRectShadow"}
        // Add more types if needed
    };

    auto it = typeMap.find(type);
    if (it != typeMap.end()) {
        return it->second;
    }
    else {
        return "unknown";
    }
}

