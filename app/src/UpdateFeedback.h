#pragma once
#include "cycode/UpdateProgress.h"
#include "cycode/Updates.h"

namespace cycode::updates::detail {
struct DownloadPhaseFeedback {
    std::string_view code, label;
};
constexpr DownloadPhaseFeedback downloadPhaseFeedback(DownloadPhase phase) {
    switch (phase) {
    case DownloadPhase::preparing:
        return {"preparing", "Preparing update download..."};
    case DownloadPhase::receiving:
        return {"receiving", "Downloading update..."};
    case DownloadPhase::verifying:
        return {"verifying", "Verifying update archive..."};
    case DownloadPhase::staging:
        return {"staging", "Preparing update files..."};
    case DownloadPhase::ready:
        return {"ready", "Update download verified"};
    case DownloadPhase::cancelled:
        return {"cancelled", "Update cancelled"};
    default:
        return {"failed", "Update download stopped"};
    }
}
struct DownloadFeedback {
    std::string_view code, title, action;
};
constexpr DownloadFeedback downloadFeedback(DownloadIssue issue) {
    switch (issue) {
    case DownloadIssue::none:
        return {"none", "", ""};
    case DownloadIssue::access:
        return {"access", "GitHub access is unavailable.",
                "Check repository access and the update token environment, or retry after rate limiting."};
    case DownloadIssue::network:
        return {"network", "The update connection was interrupted.",
                "Check the connection, then retry to resume retained bytes when possible."};
    case DownloadIssue::response:
        return {"response", "The server response cannot be used safely.",
                "Try a fresh download, or open the release for a manual download."};
    case DownloadIssue::storage:
        return {"storage", "The update could not be saved or staged.",
                "Check free space, permissions and files in use. Retained downloads are not removed."};
    case DownloadIssue::space:
        return {"space", "There is not enough space for this update download.",
                "Free space on the installation drive, then retry. Retained files are not deleted "
                "automatically."};
    case DownloadIssue::integrity:
        return {"integrity", "The downloaded files failed integrity verification.",
                "Check the release again and start a fresh download. Rejected files remain on disk."};
    case DownloadIssue::busy:
        return {"busy", "Another instance is using update storage.",
                "Wait for the other update operation to finish, then retry. No instance is terminated."};
    case DownloadIssue::installation:
        return {
            "installation", "This installation cannot accept the update.",
            "Resolve the pending recovery or install a current portable distribution from GitHub Releases."};
    default:
        return {"other", "The update download could not be completed.",
                "Refresh the release and review the technical details before retrying."};
    }
}
} // namespace cycode::updates::detail
