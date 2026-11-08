#include "cycode/ImageView.h"
#include "cycode/WorkspaceSession.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cycode {
ImageTextures::~ImageTextures() { collect(); }
void ImageTextures::collect() {
    for (const auto &[texture, bytes] : retired_) { backend_.destroy(texture); bytes_ -= bytes; }
    retired_.clear();
}
std::shared_ptr<ImageTexture> ImageTextures::upload(const ImagePixels &image, bool nearest) {
    const size_t bytes = size_t(image.width) * image.height * 4;
    if (image.width <= 0 || image.height <= 0 || bytes != image.rgba.size()) throw std::runtime_error("Invalid decoded image buffer");
    if (bytes > 512ull * 1024 * 1024 - bytes_) throw std::runtime_error("Image preview texture budget reached; close another image and reload");
    const auto id = backend_.upload(image, nearest);
    if (!id) throw std::runtime_error("The renderer could not create the image texture");
    bytes_ += bytes;
    auto owner = shared_from_this();
    return std::shared_ptr<ImageTexture>(new ImageTexture{{id,image.width,image.height},bytes},
        [owner](ImageTexture *texture) {
            owner->retired_.push_back({texture->data.texture, texture->bytes}); delete texture;
        });
}
void ImageTextures::filter(const ImageTexture &texture, bool nearest) { if (backend_.filter) backend_.filter(texture.data.texture, nearest); }
ImageView::ImageView(std::shared_ptr<ImageLoader> loader, std::shared_ptr<ImageTextures> textures)
    : loader_(std::move(loader)), textures_(std::move(textures)) {}
ImageView::~ImageView() { loader_->cancel(request_); }
void ImageView::load(ImageLoader::Path path) {
    pendingSession_ = nullptr;
    fitMode_ = true;
    loader_->cancel(request_); request_ = loader_->request(std::move(path)); error_.clear();
}
void ImageView::update() {
    ImagePixels pixels; std::string error;
    if (!loader_->take(request_, pixels, error)) return;
    request_.reset();
    if (!error.empty()) { error_ = error; return; }
    try {
        auto texture = textures_->upload(pixels, nearest_);
        texture_ = std::move(texture); firstFrame_ = pixels.firstFrame; hdr_ = pixels.hdr; fileBytes_ = pixels.fileBytes;
        state.fitPending = true; error_.clear();
    } catch (const std::exception &e) { error_ = e.what(); }
}
void ImageView::zoom(double scale) {
    if (!std::isfinite(scale)) return;
    pendingSession_ = nullptr;
    fitMode_ = false;
    const auto size = bounds.size();
    state.canvas.view.zoomAt({bounds.min.x + size.x * .5f,bounds.min.y + size.y * .5f},
                            std::clamp(scale, .05, 64.), bounds.min);
    state.fitPending = false;
}
nlohmann::json ImageView::sessionState() const {
    if (pendingSession_.is_object()) return pendingSession_;
    const auto size = bounds.size();
    const auto center = state.canvas.view.world(
        {bounds.min.x + size.x * .5f, bounds.min.y + size.y * .5f}, bounds.min);
    const auto zoom = state.canvas.view.zoom;
    const bool usable = texture_ && size.x > 0 && size.y > 0 && std::isfinite(zoom) &&
        zoom >= .001 && zoom <= 1000. && std::isfinite(center.x) && std::isfinite(center.y) &&
        std::abs(center.x) <= 10000000. && std::abs(center.y) <= 10000000.;
    return {{"version",1},{"zoom",usable ? zoom : 1.},
        {"centerX",usable ? center.x : 0.},{"centerY",usable ? center.y : 0.},
        {"fit",!usable || fitMode_ || state.fitPending},{"pixelated",nearest_},
        {"width",texture_ ? texture_->data.width : 0},{"height",texture_ ? texture_->data.height : 0}};
}
void ImageView::restoreSession(const nlohmann::json &value) {
    pendingSession_ = validateSessionImage(value);
    nearest_ = value.at("pixelated").get<bool>();
    if (texture_) textures_->filter(*texture_,nearest_);
}
void ImageView::draw(const char *id, const std::function<void()> &reload) {
    update();
    if (cyui::button(cyui::trLabel("Fit image").c_str())) {
        pendingSession_=nullptr;fitMode_=true;state.fitPending=true;
    }
    ImGui::SameLine();if (cyui::button("100%")) zoom(1.);
    ImGui::SameLine();if (cyui::button("-##zoom-out")) zoom(state.canvas.view.zoom / 1.25);
    cyui::tooltip(cyui::tr("Zoom out").c_str());
    ImGui::SameLine();if (cyui::button("+##zoom-in")) zoom(state.canvas.view.zoom * 1.25);
    cyui::tooltip(cyui::tr("Zoom in").c_str());
    ImGui::SameLine();if (cyui::iconButton(cyui::icon::refresh, cyui::tr("Reload image").c_str())) reload();
    ImGui::SameLine();if (ImGui::Checkbox(cyui::trLabel("Pixelated").c_str(), &nearest_)) {
        if (pendingSession_.is_object()) pendingSession_["pixelated"]=nearest_;
        if (texture_) textures_->filter(*texture_, nearest_);
    }
    if (loading()) { ImGui::SameLine();cyui::spinner("##image-loading"); }
    if (!error_.empty()) cyui::callout(cyui::Tone::warning, (cyui::tr("Cannot load image") + ": " + cyui::tr(error_.c_str())).c_str());
    if (!texture_) {
        if (loading()) cyui::emptyState(cyui::tr("Loading image...").c_str(), cyui::tr("Decoding in the background.").c_str());
        return;
    }
    const auto &image = texture_->data;
    cyui::textDim("%d x %d px  |  %.1f%%  |  %.2f MiB", image.width, image.height, state.canvas.view.zoom * 100., double(fileBytes_) / 1048576.);
    if (firstFrame_) cyui::textDim("%s", cyui::tr("GIF preview: first frame").c_str());
    if (hdr_) cyui::textDim("%s", cyui::tr("HDR preview converted to 8-bit display").c_str());
    const ImVec2 available{std::max(cyui::dp(32), ImGui::GetContentRegionAvail().x),
                           std::max(cyui::dp(32), ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing())};
    if (pendingSession_.is_object()) {
        fitMode_ = pendingSession_.at("fit").get<bool>() ||
            pendingSession_.at("width") != image.width || pendingSession_.at("height") != image.height;
        if (!fitMode_) {
            state.canvas.view.zoom = pendingSession_.at("zoom").get<double>();
            const auto z = state.canvas.view.zoom;
            state.canvas.view.origin = {pendingSession_.at("centerX").get<double>() - available.x / (2*z),
                                        pendingSession_.at("centerY").get<double>() - available.y / (2*z)};
        }
        state.fitPending = fitMode_;
        pendingSession_ = nullptr;
    }
    if (fitMode_ && (bounds.size().x != available.x || bounds.size().y != available.y))
        state.fitPending = true;
    const auto before = state.canvas.view;
    const bool fitted = state.fitPending;
    const auto result = cyui::imageViewport(id, state, image, available);
    const bool doubleFit = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    if (fitted || doubleFit) fitMode_=true;
    else if (before.zoom != state.canvas.view.zoom || before.origin.x != state.canvas.view.origin.x ||
             before.origin.y != state.canvas.view.origin.y) fitMode_=false;
    bounds = {ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};
    if (result.pixelX >= 0) cyui::textDim("X: %d  Y: %d", result.pixelX, result.pixelY);
    else cyui::textFaint("%s", cyui::tr("Wheel: zoom | Middle mouse or Space+drag: pan | Double-click: fit").c_str());
}
} // namespace cycode
