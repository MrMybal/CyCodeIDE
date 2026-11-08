#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace cycode {
bool imageFileExtension(const std::string &);
struct ImagePixels {
    int width = 0, height = 0, channels = 0;
    bool hdr = false, firstFrame = false;
    std::uintmax_t fileBytes = 0;
    std::vector<uint8_t> rgba;
};
// Portable binary decoder, independent of SDL/ImGui, without file writes.
ImagePixels decodeImage(const std::filesystem::path &, const std::atomic<bool> *cancel = nullptr);
struct ImageRequest {
    std::atomic<bool> cancelled{false}, consumed{false};
    std::mutex mutex;
    bool ready = false;
    std::optional<ImagePixels> pixels;
    std::string error;
};
// One decoder worker and at most one unconsumed RGBA result. Path permissions
// are resolved by the host on this worker, immediately before reading the file.
class ImageLoader {
  public:
    ImageLoader();
    ~ImageLoader();
    using Path = std::function<std::filesystem::path()>;
    std::shared_ptr<ImageRequest> request(Path);
    bool take(const std::shared_ptr<ImageRequest> &, ImagePixels &, std::string &);
    void cancel(const std::shared_ptr<ImageRequest> &);
  private:
    struct Job { std::weak_ptr<ImageRequest> request; Path path; };
    std::mutex mutex_;
    std::condition_variable changed_;
    std::deque<Job> queue_;
    std::shared_ptr<ImageRequest> active_;
    bool stopping_ = false;
    std::thread worker_;
    void run();
};
} // namespace cycode
