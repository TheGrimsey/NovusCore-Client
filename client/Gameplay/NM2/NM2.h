#pragma once
#include <NovusTypes.h>
#include <vector>

namespace NM2
{
    struct NM2Header
    {
        u32 token = 0;
        u32 version = 0;
    };

    struct M2Vertex
    {
        vec3 position = vec3(0, 0, 0);
        u8 boneWeights[4] = { 0, 0, 0, 0 };
        u8 boneIndices[4] = { 0, 0, 0, 0 };
        vec3 normal = vec3(0, 0, 0);
        vec2 uvCords[2] = { vec2(0, 0), vec2(0, 0) };
    };

    struct M2Texture
    {
        u32 type = 0; // Check https://wowdev.wiki/M2#Textures
        struct M2TextureFlags
        {
            u32 wrapX : 1;
            u32 wrapY : 1;
        } flags;

        u32 textureNameIndex = 0;
    };

    struct M2Skin
    {
        u32 token;

        std::vector<u16> vertexIndexes;
        std::vector<u16> indices;
    };

    struct NM2Root
    {
        NM2Header header;

        std::vector<M2Vertex> vertices;
        std::vector<M2Skin> skins;
    };
}