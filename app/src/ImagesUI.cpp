#include "cycode/Application.h"
#include "cycode/ImageView.h"

namespace cycode {
bool Application::openImage(const std::string &path, bool activate) {
    if (path.starts_with("sdk://") || path.starts_with("debug://") || !imageFileExtension(path)) return false;
    try {
        const auto existing=std::find_if(tabs_.begin(),tabs_.end(),[&](const auto &tab) { return tab->image && tab->path==path; });
        const auto canonical = existing!=tabs_.end() ? path : pathUtf8(engine_->resolve(path).lexically_relative(engine_->workspace()));
        auto found = std::find_if(tabs_.begin(), tabs_.end(), [&](const auto &tab) { return tab->path == canonical; });
        std::shared_ptr<Tab> tab;
        if (found != tabs_.end()) tab = *found;
        else {
            if (!imageLoader_) imageLoader_ = std::make_shared<ImageLoader>();
            if (!imageTextures_) imageTextures_ = openGLImageTextures();
            tab = std::make_shared<Tab>(); tab->path = tab->displayPath = canonical; tab->readOnly = true;
            tab->image = std::make_shared<ImageView>(imageLoader_, imageTextures_);
            tabs_.push_back(tab);reloadImage(tab);
        }
        if (activate) {
            if (!historyMoving_ && active_ != canonical) rememberNavigation();
            std::erase(recentFiles_,canonical);recentFiles_.insert(recentFiles_.begin(),canonical);
            if (recentFiles_.size()>100) recentFiles_.resize(100);
            active_=focusTab_=revealPath_=canonical;
        }
    } catch (const std::exception &e) { error(e.what()); }
    return true;
}
void Application::openImageFile(const std::filesystem::path &path) {
    try {
        const auto source=std::filesystem::canonical(path);const auto key=pathUtf8(source);
        if (!imageFileExtension(key)) throw std::runtime_error("Select an image file");
        bool workspace=true;
        try { (void)engine_->resolve(key); } catch (const std::exception &) { workspace=false; }
        if (workspace) { open(key);return; }
        if (std::any_of(tabs_.begin(),tabs_.end(),[&](const auto &tab) { return tab->image && tab->path==key; })) { open(key);return; }
        if (!imageLoader_) imageLoader_=std::make_shared<ImageLoader>();
        if (!imageTextures_) imageTextures_=openGLImageTextures();
        auto tab=std::make_shared<Tab>();tab->path=tab->displayPath=key;tab->imageSource=source;tab->readOnly=true;
        tab->image=std::make_shared<ImageView>(imageLoader_,imageTextures_);tabs_.push_back(tab);reloadImage(tab);open(key);
    } catch (const std::exception &e) { error(e.what()); }
}
void Application::reloadImage(const std::shared_ptr<Tab> &tab) {
    if (!tab->imageSource.empty()) {
        const auto source=tab->imageSource;
        tab->image->load([source] {
            const auto current=std::filesystem::canonical(source);
            if (current!=source) throw std::runtime_error("The selected image path now points to another file");
            return current;
        });return;
    }
    auto engine=engine_;const auto path=tab->path;
    tab->image->load([engine,path] { return engine->resolve(path); });
}
void Application::updateImages() {
    if (imageTextures_) imageTextures_->collect();
    for (const auto &tab : tabs_) if (tab->image) tab->image->update();
}
void Application::imagePanel(const std::shared_ptr<Tab> &tab) {
    tab->image->draw(("##image/"+tab->path).c_str(), [this,tab] { reloadImage(tab); });
}
void Application::chooseImage() {
    {
        std::lock_guard lock(dialog_->mutex);dialog_->path.clear();dialog_->error.clear();dialog_->newWindow=false;dialog_->image=true;
    }
    static const SDL_DialogFileFilter filters[] = {{"Images", "png;jpg;jpeg;jpe;jfif;webp;gif;bmp;dib;tga;psd;hdr;pic;pnm;ppm;pgm"}, {"All files", "*"}};
    auto *state=new std::shared_ptr<Dialog>(dialog_);
    SDL_ShowOpenFileDialog([](void *user,const char *const *paths,int) {
        auto state=*static_cast<std::shared_ptr<Dialog> *>(user);delete static_cast<std::shared_ptr<Dialog> *>(user);
        std::lock_guard lock(state->mutex);state->ready=true;
        if (paths && paths[0]) state->path=paths[0];else if (!paths) state->error=SDL_GetError();
    },state,window_,filters,2,pathUtf8(engine_->workspace()).c_str(),false);
}
} // namespace cycode
