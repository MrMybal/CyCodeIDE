#pragma once
#include "cycode/UpdateRecoveryProgress.h"

namespace cycode::updates::detail {
constexpr const char *recoveryPhaseLabel(RecoveryPhase phase) noexcept {
    switch (phase) {
    case RecoveryPhase::inspecting:
        return "Inspecting recovery files...";
    case RecoveryPhase::preparing:
        return "Preparing recovery copies...";
    case RecoveryPhase::verifying:
        return "Verifying prepared recovery files...";
    case RecoveryPhase::restoring:
        return "Restoring installed files...";
    case RecoveryPhase::checking:
        return "Verifying the restored installation...";
    case RecoveryPhase::complete:
        return "Recovery completed";
    case RecoveryPhase::cancelled:
        return "Recovery cancelled";
    default:
        return "Recovery failed";
    }
}
} // namespace cycode::updates::detail
