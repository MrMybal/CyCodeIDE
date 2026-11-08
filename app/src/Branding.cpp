#include "cycode/Application.h"
#include "cycode/ImageView.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace cycode {
void Application::applyWindowIcon(SDL_Window *target) {
    if (!target || !brandingIcon_)
        return;
    const bool applied = SDL_SetWindowIcon(target, brandingIcon_.get());
    if (target == window_)
        brandingWindowIcon_ = applied;
    if (!applied)
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "CyCode window icon: %s", SDL_GetError());
}
void Application::loadBranding() {
    // Headless model tests have neither an SDL window nor a GL context.
    if (!window_)
        return;
    try {
        const auto source = root_ / "icons/CyCode.png";
        if (!std::filesystem::is_regular_file(source))
            return; // An incomplete installation can still open project files.
        auto image = decodeImage(source);
        std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(
            SDL_CreateSurfaceFrom(image.width, image.height, SDL_PIXELFORMAT_RGBA32, image.rgba.data(),
                                  image.width * 4),
            SDL_DestroySurface);
        if (!surface)
            throw std::runtime_error(SDL_GetError());
        // Use a small display copy; preserve the supplied PNG/ICO source assets.
        const double scale = std::min(1., 256. / std::max(image.width, image.height));
        const int width = std::max(1, int(std::lround(image.width * scale)));
        const int height = std::max(1, int(std::lround(image.height * scale)));
        brandingIcon_.reset(SDL_ScaleSurface(surface.get(), width, height, SDL_SCALEMODE_LINEAR));
        if (!brandingIcon_)
            throw std::runtime_error(SDL_GetError());
        applyWindowIcon(window_);
        ImagePixels thumbnail;
        thumbnail.width = width;
        thumbnail.height = height;
        thumbnail.channels = 4;
        thumbnail.rgba.resize(size_t(width) * height * 4);
        if (!SDL_LockSurface(brandingIcon_.get()))
            throw std::runtime_error(SDL_GetError());
        const auto bytes = static_cast<const uint8_t *>(brandingIcon_->pixels);
        for (int row = 0; row < height; ++row)
            std::memcpy(thumbnail.rgba.data() + size_t(row) * width * 4,
                        bytes + size_t(row) * brandingIcon_->pitch, size_t(width) * 4);
        SDL_UnlockSurface(brandingIcon_.get());
        if (!imageTextures_)
            imageTextures_ = openGLImageTextures();
        brandingLogo_ = imageTextures_->upload(thumbnail, false);
    } catch (const std::exception &error) {
        brandingError_ = error.what();
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "CyCode logo: %s", error.what());
    }
}
} // namespace cycode
