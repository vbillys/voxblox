#ifndef VOXBLOX_UTILS_EVALUATION_UTILS_H_
#define VOXBLOX_UTILS_EVALUATION_UTILS_H_

#include <algorithm>

#include "voxblox/core/layer.h"
#include "voxblox/core/voxel.h"

namespace voxblox {
namespace utils {

enum class VoxelEvaluationResult { kNoOverlap, kIgnored, kEvaluated };

enum class VoxelEvaluationMode {
  kEvaluateAllVoxels,
  kIgnoreErrorBehindTestSurface,
  kIgnoreErrorBehindGtSurface,
  kIgnoreErrorBehindAllSurfaces
};

struct VoxelEvaluationDetails {
  FloatingPoint rmse = 0.0;
  // Max and min of absolute distance error.
  FloatingPoint max_error = 0.0;
  FloatingPoint min_error = 0.0;
  size_t num_evaluated_voxels = 0u;
  size_t num_ignored_voxels = 0u;
  size_t num_overlapping_voxels = 0u;
  size_t num_non_overlapping_voxels = 0u;
};

template <typename VoxelType>
VoxelEvaluationResult computeVoxelError(
    const VoxelType& voxel_gt, const VoxelType& voxel_test,
    const VoxelEvaluationMode evaluation_mode, FloatingPoint* error);

// Returns true if the voxel has been observed.
template <typename VoxelType>
bool isObservedVoxel(const VoxelType& voxel);

// Allow this class to be templated on all kinds of voxels.
template <typename VoxelType>
FloatingPoint getVoxelSdf(const VoxelType& voxel);

// Evaluate a test layer vs a ground truth layer. The comparison is symmetrical
// unless the VoxelEvaluationMode is set to ignore the voxels of one of the two
// layers behind the surface. The parameter 'evaluation_result' can be a
// nullptr.
template <typename VoxelType>
FloatingPoint evaluateLayersRmse(
    const Layer<VoxelType>& layer_gt, const Layer<VoxelType>& layer_test,
    const VoxelEvaluationMode& voxel_evaluation_mode,
    VoxelEvaluationDetails* evaluation_result) {
  // Iterate over all voxels in the test layer and look them up in the ground
  // truth layer. Then compute RMSE.
  BlockIndexList block_list;
  layer_test.getAllAllocatedBlocks(&block_list);
  size_t vps = layer_test.voxels_per_side();
  size_t num_voxels_per_block = vps * vps * vps;

  VoxelEvaluationDetails evaluation_details;

  double total_squared_error = 0.0;

  for (const BlockIndex& block_index : block_list) {
    const Block<VoxelType>& test_block =
        layer_test.getBlockByIndex(block_index);

    if (!layer_gt.hasBlock(block_index)) {
      for (size_t linear_index = 0u; linear_index < num_voxels_per_block;
           ++linear_index) {
        const VoxelType& voxel = test_block.getVoxelByLinearIndex(linear_index);
        if (isObservedVoxel(voxel)) {
          ++evaluation_details.num_non_overlapping_voxels;
        }
      }
      continue;
    }
    const Block<VoxelType>& gt_block = layer_gt.getBlockByIndex(block_index);

    for (size_t linear_index = 0u; linear_index < num_voxels_per_block;
         ++linear_index) {
      FloatingPoint error = 0.0;
      const VoxelEvaluationResult result =
          computeVoxelError(gt_block.getVoxelByLinearIndex(linear_index),
                            test_block.getVoxelByLinearIndex(linear_index),
                            voxel_evaluation_mode, &error);

      switch (result) {
        case VoxelEvaluationResult::kEvaluated:
          total_squared_error += error * error;
          evaluation_details.min_error =
              std::min(evaluation_details.min_error, std::abs(error));
          evaluation_details.max_error =
              std::max(evaluation_details.max_error, std::abs(error));
          ++evaluation_details.num_evaluated_voxels;
          ++evaluation_details.num_overlapping_voxels;
          break;
        case VoxelEvaluationResult::kIgnored:
          ++evaluation_details.num_ignored_voxels;
          ++evaluation_details.num_overlapping_voxels;
          break;
        case VoxelEvaluationResult::kNoOverlap:
          ++evaluation_details.num_non_overlapping_voxels;
          break;
        default:
          LOG(FATAL) << "Unkown voxel evaluation result: "
                     << static_cast<int>(result);
      }
    }
  }

  // Iterate over all blocks in the grond truth layer and look them up in the
  // test truth layer. This is only done to get the exact number of
  // non-overlapping voxels.
  BlockIndexList gt_block_list;
  layer_gt.getAllAllocatedBlocks(&gt_block_list);
  for (const BlockIndex& gt_block_index : gt_block_list) {
    const Block<VoxelType>& gt_block = layer_gt.getBlockByIndex(gt_block_index);
    if (!layer_test.hasBlock(gt_block_index)) {
      for (size_t linear_index = 0u; linear_index < num_voxels_per_block;
           ++linear_index) {
        const VoxelType& voxel = gt_block.getVoxelByLinearIndex(linear_index);
        if (isObservedVoxel(voxel)) {
          ++evaluation_details.num_non_overlapping_voxels;
        }
      }
    }
  }

  // Return the RMSE.
  if (evaluation_details.num_evaluated_voxels == 0) {
    evaluation_details.rmse = 0.0;
  } else {
    evaluation_details.rmse =
        sqrt(total_squared_error / evaluation_details.num_evaluated_voxels);
  }

  LOG(INFO) << "\nLayer comparison result:"
            << "\n\tnum evaluated voxels:\t\t "
            << evaluation_details.num_evaluated_voxels
            << "\n\tnum overlapping voxels:\t\t "
            << evaluation_details.num_overlapping_voxels
            << "\n\tnum non-overlapping voxels:\t "
            << evaluation_details.num_non_overlapping_voxels
            << "\n\tnum ignored voxels:\t\t "
            << evaluation_details.num_ignored_voxels << "\n\tRMSE:\t\t\t\t "
            << evaluation_details.rmse;

  // If the details are requested, output them.
  if (evaluation_result != nullptr) {
    *evaluation_result = evaluation_details;
  }

  return evaluation_details.rmse;
}

// Overload for convenient RMSE calculation. Per default this function does not
// evaluate errors behind the test surface.
template <typename VoxelType>
FloatingPoint evaluateLayersRmse(const Layer<VoxelType>& layer_gt,
                                 const Layer<VoxelType>& layer_test) {
  return evaluateLayersRmse<VoxelType>(
      layer_gt, layer_test, VoxelEvaluationMode::kIgnoreErrorBehindTestSurface,
      nullptr);
}

template <typename VoxelType>
VoxelEvaluationResult computeVoxelError(
    const VoxelType& voxel_gt, const VoxelType& voxel_test,
    const VoxelEvaluationMode evaluation_mode, FloatingPoint* error) {
  CHECK_NOTNULL(error);
  *error = 0.0;

  // Ignore voxels that are not observed in both layers.
  if (!isObservedVoxel(voxel_gt) || !isObservedVoxel(voxel_test)) {
    return VoxelEvaluationResult::kNoOverlap;
  }

  const bool ignore_behind_test_surface =
      (evaluation_mode == VoxelEvaluationMode::kIgnoreErrorBehindTestSurface) ||
      (evaluation_mode == VoxelEvaluationMode::kIgnoreErrorBehindAllSurfaces);

  const bool ignore_behind_gt_surface =
      (evaluation_mode == VoxelEvaluationMode::kIgnoreErrorBehindGtSurface) ||
      (evaluation_mode == VoxelEvaluationMode::kIgnoreErrorBehindAllSurfaces);

  if ((ignore_behind_test_surface && voxel_test.distance < 0.0) ||
      (ignore_behind_gt_surface && voxel_gt.distance < 0.0)) {
    return VoxelEvaluationResult::kIgnored;
  }

  *error = getVoxelSdf(voxel_test) - getVoxelSdf(voxel_gt);

  return VoxelEvaluationResult::kEvaluated;
}

template <>
inline bool isObservedVoxel(const TsdfVoxel& voxel) {
  return voxel.weight > 1e-6;
}

template <>
inline bool isObservedVoxel(const EsdfVoxel& voxel) {
  return voxel.observed;
}

template <>
inline FloatingPoint getVoxelSdf(const TsdfVoxel& voxel) {
  return voxel.distance;
}

template <>
inline FloatingPoint getVoxelSdf(const EsdfVoxel& voxel) {
  return voxel.distance;
}

}  // namespace utils
}  // namespace voxblox

#endif  // VOXBLOX_UTILS_EVALUATION_UTILS_H_
