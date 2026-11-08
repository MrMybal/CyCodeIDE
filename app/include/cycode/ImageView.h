#pragma once
#include "cycode/ImageFile.h"
#include <cyui/modules/Images.h>
#include <nlohmann/json.hpp>

namespace cycode {
struct ImageTexture {
    cyui::ImageData data;
    size_t bytes = 0;
};
// The app supplies renderer operations; the common Images module only draws.
// Destruction is deferred until the next frame, after all platform viewports
// have consumed the previous frame's draw lists.
class ImageTextures : public std::enable_shared_from_this<ImageTextures> {
  public:
    struct Backend {
        std::function<ImTextureID(const ImagePixels &, bool)> upload;
        std::function<void(ImTextureID)> destroy;
        std::function<void(ImTextureID, bool)> filter;
    };
    explicit ImageTextures(Backend backend) : backend_(std::move(backend)) {}
    ~ImageTextures();
    std::shared_ptr<ImageTexture> upload(const ImagePixels &, bool);
    void filter(const ImageTexture &, bool);
    void collect();
  private:
    Backend backend_;
    size_t bytes_ = 0;
    std::vector<std::pair<ImTextureID, size_t>> retired_;
};
std::shared_ptr<ImageTextures> openGLImageTextures();
class ImageView {
  public:
    ImageView(std::shared_ptr<ImageLoader>, std::shared_ptr<ImageTextures>);
    ~ImageView();
    void load(ImageLoader::Path);
    void update();
    void draw(const char *id, const std::function<void()> &reload);
    void zoom(double scale);
    nlohmann::json sessionState() const;
    void restoreSession(const nlohmann::json &);
    bool loading() const { return bool(request_); }
    const ImageTexture *texture() const { return texture_.get(); }
    const std::string &error() const { return error_; }
    cyui::ImageState state;
    cyui::Rect bounds;
  private:
    std::shared_ptr<ImageLoader> loader_;
    std::shared_ptr<ImageTextures> textures_;
    std::shared_ptr<ImageRequest> request_;
    std::shared_ptr<ImageTexture> texture_;
    std::string error_;
    bool nearest_ = false, firstFrame_ = false, hdr_ = false;
    bool fitMode_ = true;
    nlohmann::json pendingSession_ = nullptr;
    std::uintmax_t fileBytes_ = 0;
};
} // namespace cycode
