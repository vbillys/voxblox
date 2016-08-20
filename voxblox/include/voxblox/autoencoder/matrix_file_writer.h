#ifndef VOXBLOX_MATRIX_FILE_WRITER_H_
#define VOXBLOX_MATRIX_FILE_WRITER_H_

#include <fstream>

namespace voxblox {

// unnamed namespace to hide functions
namespace {

void writeMatrixHeader(std::ofstream& file_stream,
                       uint32_t element_size_in_bytes, uint32_t rows,
                       uint32_t columns) {
  file_stream.write((char*)&element_size_in_bytes, sizeof(uint32_t));
  file_stream.write((char*)&rows, sizeof(uint32_t));
  file_stream.write((char*)&columns, sizeof(uint32_t));
}
}

// simple file writer for creating matrix data in a format autoencoders can be
// trained on (used as pythons protobuf is very slow)
void writeLayerToMatrixFiles(const Layer<TsdfVoxel>& layer,
                             const TsdfIntegrator::Config& config,
                             const std::string& folder_path) {

  std::string file_path = folder_path + "/distance.bin";
  std::ofstream distance_file(file_path, std::ios::out | std::ios::binary);
  file_path = folder_path + "/weight.bin";
  std::ofstream weight_file(file_path, std::ios::out | std::ios::binary);
  file_path = folder_path + "/color.bin";
  std::ofstream color_file(file_path, std::ios::out | std::ios::binary);

  if (distance_file.is_open() &&
      weight_file.is_open() &&
      color_file.is_open()) {
    // get and write matrix sizes
    BlockIndexList blocks;
    layer.getAllAllocatedBlocks(&blocks);

    uint32_t num_blocks = blocks.size();
    uint32_t voxels_per_block = layer.voxels_per_side() *
                                layer.voxels_per_side() *
                                layer.voxels_per_side();

    writeMatrixHeader(distance_file, sizeof(float), voxels_per_block,
                      num_blocks);
    writeMatrixHeader(weight_file, sizeof(float), voxels_per_block, num_blocks);
    writeMatrixHeader(color_file, sizeof(uint8_t), 3 * voxels_per_block,
                      num_blocks);

    // write matrix data
    for (AnyIndex block_idx : blocks) {
      for (size_t i = 0; i < voxels_per_block; ++i) {
        TsdfVoxel voxel =
            layer.getBlockByIndex(block_idx).getVoxelByLinearIndex(i);

        // get data and normalize floating point data to be in range 0 to 1
        float distance =
            (voxel.distance / 2 * config.default_truncation_distance) + 0.5;
        float weight = voxel.weight / config.max_weight;
        uint8_t red = voxel.color.r;
        uint8_t green = voxel.color.g;
        uint8_t blue = voxel.color.b;

        distance_file.write((char*)&distance, sizeof(float));
        weight_file.write((char*)&weight, sizeof(float));
        color_file.write((char*)&red, sizeof(uint8_t));
        color_file.write((char*)&green, sizeof(uint8_t));
        color_file.write((char*)&blue, sizeof(uint8_t));
      }
    }
    distance_file.close();
    weight_file.close();
    color_file.close();
  }
}
}

#endif  // VOXBLOX_MATRIX_FILE_WRITER_H_