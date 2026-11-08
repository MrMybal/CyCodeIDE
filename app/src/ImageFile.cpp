#include "cycode/ImageFile.h"
#include "cycode/Paths.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <webp/decode.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 32768
#include <stb_image.h>

namespace cycode {
bool imageFileExtension(const std::string &path) {
    auto ext = pathUtf8(utf8Path(path).extension());
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    for (const char *value : {".png", ".jpg", ".jpeg", ".jpe", ".jfif", ".webp", ".gif", ".bmp", ".dib",
                             ".tga", ".psd", ".hdr", ".pic", ".pnm", ".ppm", ".pgm", ".tif", ".tiff", ".ico", ".dds", ".avif", ".exr"})
        if (ext == value) return true;
    return false; // SVG remains an editable XML document.
}
static void dimensions(int width, int height) {
    if (width <= 0 || height <= 0 || width > 32768 || height > 32768 ||
        uint64_t(width) * uint64_t(height) > 64ull * 1024 * 1024)
        throw std::runtime_error("Image dimensions exceed the preview limit (32768 per side, 64 megapixels)");
}
ImagePixels decodeImage(const std::filesystem::path &path, const std::atomic<bool> *cancel) {
    auto check = [&] { if (cancel && cancel->load()) throw std::runtime_error("Image loading cancelled"); };
    check();
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("Cannot read image file");
    const auto length = input.tellg();
    if (length <= 0 || length > 64 * 1024 * 1024) throw std::runtime_error("Image file is empty or exceeds the 64 MiB input limit");
    std::vector<uint8_t> data(static_cast<size_t>(length));
    input.seekg(0); input.read(reinterpret_cast<char *>(data.data()), length);
    if (!input) throw std::runtime_error("Image file changed or could not be read completely");
    check();
    ImagePixels result; result.fileBytes = data.size();
    const bool webp = data.size() >= 12 && std::equal(data.begin(), data.begin() + 4, "RIFF") &&
                      std::equal(data.begin() + 8, data.begin() + 12, "WEBP");
    if (webp) {
        WebPBitstreamFeatures features{};
        if (WebPGetFeatures(data.data(), data.size(), &features) != VP8_STATUS_OK) throw std::runtime_error("Invalid WebP image");
        dimensions(features.width, features.height);
        if (features.has_animation) throw std::runtime_error("Animated WebP preview is not supported yet");
        result.width = features.width; result.height = features.height; result.channels = features.has_alpha ? 4 : 3;
        result.rgba.resize(size_t(result.width) * result.height * 4);
        if (!WebPDecodeRGBAInto(data.data(), data.size(), result.rgba.data(), result.rgba.size(), result.width * 4))
            throw std::runtime_error("Cannot decode WebP image");
    } else {
        const auto bytes = static_cast<int>(data.size());
        if (!stbi_info_from_memory(data.data(), bytes, &result.width, &result.height, &result.channels))
            throw std::runtime_error("Unsupported or invalid image (PNG, JPEG, WebP, GIF, BMP, TGA, PSD, HDR, PIC, PGM/PPM are supported)");
        dimensions(result.width, result.height);
        result.hdr = stbi_is_hdr_from_memory(data.data(), bytes) != 0;
        result.firstFrame = data.size() >= 6 && std::equal(data.begin(), data.begin() + 3, "GIF");
        std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
            stbi_load_from_memory(data.data(), bytes, &result.width, &result.height, &result.channels, 4), stbi_image_free);
        if (!pixels) throw std::runtime_error(std::string("Cannot decode image: ") + (stbi_failure_reason() ? stbi_failure_reason() : "invalid data"));
        dimensions(result.width, result.height);
        result.rgba.assign(pixels.get(), pixels.get() + size_t(result.width) * result.height * 4);
    }
    check(); return result;
}
ImageLoader::ImageLoader() : worker_([this] { run(); }) {}
ImageLoader::~ImageLoader() {
    { std::lock_guard lock(mutex_); stopping_ = true; if (active_) active_->cancelled = true; queue_.clear(); }
    changed_.notify_all(); worker_.join();
}
std::shared_ptr<ImageRequest> ImageLoader::request(Path path) {
    auto result = std::make_shared<ImageRequest>();
    { std::lock_guard lock(mutex_);
      std::erase_if(queue_, [](const Job &job) { const auto request=job.request.lock();return !request || request->cancelled; });
      if (queue_.size()>=128) { result->ready=true;result->error="Too many image previews queued";return result; }
      queue_.push_back({result, std::move(path)}); }
    changed_.notify_one(); return result;
}
bool ImageLoader::take(const std::shared_ptr<ImageRequest> &request, ImagePixels &pixels, std::string &error) {
    if (!request) return false;
    { std::lock_guard lock(request->mutex);
      if (!request->ready || request->consumed) return false;
      if (request->pixels) pixels = std::move(*request->pixels);
      request->pixels.reset(); error = request->error; request->consumed = true; }
    changed_.notify_all(); return true;
}
void ImageLoader::cancel(const std::shared_ptr<ImageRequest> &request) {
    if (request) request->cancelled = true;
    changed_.notify_all();
}
void ImageLoader::run() {
    for (;;) {
        Job job;
        { std::unique_lock lock(mutex_); changed_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
          if (stopping_) return;
          job = std::move(queue_.front()); queue_.pop_front(); active_ = job.request.lock(); }
        auto request = active_;
        if (!request || request->cancelled) continue;
        std::optional<ImagePixels> pixels; std::string error;
        try { pixels = decodeImage(job.path(), &request->cancelled); }
        catch (const std::exception &e) { error = e.what(); }
        if (request->cancelled) continue;
        { std::lock_guard lock(request->mutex); request->pixels = std::move(pixels); request->error = std::move(error); request->ready = true; }
        // Bound completed CPU images even when the UI is busy or tabs are hidden.
        std::unique_lock lock(mutex_);
        changed_.wait(lock, [&] { return stopping_ || request->cancelled || request->consumed; });
        active_.reset(); if (stopping_) return;
    }
}
} // namespace cycode
