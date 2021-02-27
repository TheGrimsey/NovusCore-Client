#include "Chunk.h"

#include <Utils/ByteBuffer.h>
#include <Utils/FileReader.h>
/* PP-NoiseTerrain START */
#include "../../Utils/MapUtils.h"
#include "../../dep/FastNoiseLite/FastNoiseLite.h"
/* PP-NoiseTerrain END */

bool Terrain::Chunk::Read(FileReader& reader, Terrain::Chunk& chunk, StringTable& stringTable)
{
    Bytebuffer buffer(nullptr, reader.Length());
    reader.Read(&buffer, buffer.size);

    buffer.Get<Terrain::ChunkHeader>(chunk.chunkHeader);

    if (chunk.chunkHeader.token != Terrain::MAP_CHUNK_TOKEN)
    {
        DebugHandler::PrintFatal("Tried to load a map chunk file with the wrong token");
    }

    if (chunk.chunkHeader.version != Terrain::MAP_CHUNK_VERSION)
    {
        if (chunk.chunkHeader.version < Terrain::MAP_CHUNK_VERSION)
        {
            DebugHandler::PrintFatal("Loaded map chunk with too old version %u instead of expected version of %u, rerun dataextractor", chunk.chunkHeader.version, Terrain::MAP_CHUNK_VERSION);
        }
        else
        {
            DebugHandler::PrintFatal("Loaded map chunk with too new version %u instead of expected version of %u, update your client", chunk.chunkHeader.version, Terrain::MAP_CHUNK_VERSION);
        }
    }

    buffer.Get<Terrain::HeightHeader>(chunk.heightHeader);
    buffer.Get<Terrain::HeightBox>(chunk.heightBox);

    buffer.GetBytes(reinterpret_cast<u8*>(&chunk.cells[0]), sizeof(Terrain::Cell) * Terrain::MAP_CELLS_PER_CHUNK);

    /* PP-NoiseTerrain START */
    std::vector<std::string> splitName = StringUtils::SplitString(reader.FileName(), '_');
    size_t numberOfSplits = splitName.size();
    u16 chunkMapX = std::stoi(splitName[numberOfSplits - 2]);
    u16 chunkMapY = std::stoi(splitName[numberOfSplits - 1]);
    u32 chunkId = chunkMapX + (chunkMapY * Terrain::MAP_CHUNKS_PER_MAP_STRIDE);

    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);

    DebugHandler::Print("Chunk %d,%d", chunkMapX, chunkMapY);
    for (u16 cellId = 0; cellId < MAP_CELLS_PER_CHUNK; cellId++)
    {
        vec2 cellPos = MapUtils::GetCellPosition(chunkId, cellId);
        u16 id = 0;
        for (u16 y = 0; y < 17; y++)
        {
            bool outerGrid = y % 2 == 0;
            u16 iterations = outerGrid  ? 9 : 8;
            f64 patchY = (-static_cast<f64>(cellPos.x) + static_cast<f64>(y) * static_cast<f64>(MAP_PATCH_HALF_SIZE));// +(16 * MAP_PATCH_HALF_SIZE) * chunkMapY;
            for (u16 x = 0; x < iterations; x++)
            {
                f32 patchX = -cellPos.y + (!outerGrid * MAP_PATCH_HALF_SIZE) + (x * MAP_PATCH_SIZE);

                chunk.cells[cellId].heightData[id] = noise.GetNoise(patchX/10.f, (f32)patchY/10.f) * 100.f;
                id++;
            }
        }
    }
    DebugHandler::Print("==================================");
    /* PP-NoiseTerrain END */

    buffer.Get<u32>(chunk.alphaMapStringID);

    u32 numMapObjectPlacements;
    buffer.Get<u32>(numMapObjectPlacements);

    if (numMapObjectPlacements > 0)
    {
        chunk.mapObjectPlacements.resize(numMapObjectPlacements);
        buffer.GetBytes(reinterpret_cast<u8*>(&chunk.mapObjectPlacements[0]), sizeof(Terrain::Placement) * numMapObjectPlacements);
    }

    u32 numComplexModelPlacements;
    buffer.Get<u32>(numComplexModelPlacements);

    if (numComplexModelPlacements > 0)
    {
        chunk.complexModelPlacements.resize(numComplexModelPlacements);
        buffer.GetBytes(reinterpret_cast<u8*>(&chunk.complexModelPlacements[0]), sizeof(Terrain::Placement) * numComplexModelPlacements);
    }

    // Read Liquid Bytes
    {
        u32 numLiquidBytes;
        buffer.Get<u32>(numLiquidBytes);

        if (numLiquidBytes > 0)
        {
            chunk.liquidBytes.resize(numLiquidBytes);
            buffer.GetBytes(chunk.liquidBytes.data(), sizeof(u8) * numLiquidBytes);

            chunk.liquidHeaders.resize(Terrain::MAP_CELLS_PER_CHUNK);
            std::memcpy(chunk.liquidHeaders.data(), chunk.liquidBytes.data(), Terrain::MAP_CELLS_PER_CHUNK * sizeof(Terrain::CellLiquidHeader));

            u32 numInstances = 0;
            u32 firstInstanceOffset = std::numeric_limits<u32>().max();

            // We reserver memory for at least 1 layer for each liquid header
            for (u32 i = 0; i < Terrain::MAP_CELLS_PER_CHUNK; i++)
            {
                const Terrain::CellLiquidHeader& header = chunk.liquidHeaders[i];

                if (header.layerCount > 0)
                {
                    if (header.instancesOffset < firstInstanceOffset)
                        firstInstanceOffset = header.instancesOffset;

                    numInstances += header.layerCount;
                }
            }

            if (numInstances > 0)
            {
                chunk.liquidInstances.resize(numInstances);
                std::memcpy(chunk.liquidInstances.data(), &chunk.liquidBytes.data()[firstInstanceOffset], sizeof(Terrain::CellLiquidInstance) * numInstances);
            }
        }
    }

    stringTable.Deserialize(&buffer);
    assert(stringTable.GetNumStrings() > 0); // We always expect to have at least 1 string in our stringtable, a path for the base texture
    return true;
}
