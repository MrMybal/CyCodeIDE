#include "cycode/Application.h"
#include "cycode/ImageView.h"
#include "cycode/FileIcons.h"
#include "cycode/TextFile.h"
#include <imgui_internal.h>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <thread>

using namespace cycode;
static void require(bool ok,const char *message) { if (!ok) throw std::runtime_error(message); }
static void write(const std::filesystem::path &path,const std::string &bytes) { std::ofstream out(path,std::ios::binary);out.write(bytes.data(),bytes.size()); }
static std::string base64(const std::string &text) {
    const std::string chars="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;unsigned value=0;int bits=-8;
    for (char c:text) { if (c=='=') break;const auto digit=chars.find(c);require(digit!=std::string::npos,"Invalid test fixture");value=(value<<6)|unsigned(digit);bits+=6;if (bits>=0) { result+=char(value>>bits);bits-=8; } }
    return result;
}
static void frame(const std::function<void()> &draw) {
    auto &io=ImGui::GetIO();ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(io.DisplaySize);
    draw();ImGui::Render();require(ImGui::GetDrawData()->TotalVtxCount>0,"Image view produced no drawing data");
}
static void wait(const std::function<bool()> &ready) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);
    while (!ready()) { require(std::chrono::steady_clock::now()<deadline,"Image worker did not complete");std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
}
namespace cycode {
struct ApplicationImageTests {
    static void run(const std::filesystem::path &root,const Json &fixtures) {
        const auto folder=root/"runtime/images-fixture";const auto workspace=folder/"workspace";
        std::filesystem::create_directories(workspace);std::filesystem::create_directories(folder/"app/locales");
        std::filesystem::copy_file(root/"app/locales/ui.json",folder/"app/locales/ui.json",std::filesystem::copy_options::overwrite_existing);
        for (const auto &[name,data]:fixtures.items()) {
            write(workspace/utf8Path(name),base64(data));
            const auto image=decodeImage(workspace/utf8Path(name));
            require(image.width==2 && image.height==2 && image.rgba.size()==16,"Decoder dimensions/channels differ from fixture");
            if (name=="alpha.png") require(image.rgba[0]==255 && image.rgba[1]==0 && image.rgba[3]==128,"RGBA orientation/transparency changed");
            if (name=="animated.gif") require(image.firstFrame,"GIF first-frame scope is not exposed");
            if (name=="light.hdr") require(image.hdr,"HDR display conversion metadata missing");
        }
        const auto png=base64(fixtures.at("alpha.png"));
        write(workspace/utf8Path("été 😀.PNG"),png);write(workspace/"main.cpp","int value = 1;\n");
        require(decodeImage(workspace/utf8Path("été 😀.PNG")).rgba[3]==128,"Unicode image path did not decode");
        for (const auto &name:{"empty.png","corrupt.png","unsupported.dds"}) {
            write(workspace/name,name==std::string("empty.png") ? "" : "not an image");bool failed=false;
            try { (void)decodeImage(workspace/name); } catch (const std::exception &) { failed=true; }
            require(failed,"Invalid image was accepted");
        }
        auto huge=png;huge[16]=0x7f;huge[17]=huge[18]=huge[19]=char(0xff);write(workspace/"huge.png",huge);
        bool failed=false;try { (void)decodeImage(workspace/"huge.png"); } catch (const std::exception &) { failed=true; }require(failed,"Oversized image was not bounded");
        // One ready result blocks the worker until consumed/cancelled.
        ImageLoader worker;auto abandoned=worker.request([&] { return workspace/"alpha.png"; });
        wait([&] { std::lock_guard lock(abandoned->mutex);return abandoned->ready; });worker.cancel(abandoned);
        auto second=worker.request([&] { return workspace/"lossless.webp"; });ImagePixels pixels;std::string error;
        wait([&] { return worker.take(second,pixels,error); });require(error.empty() && pixels.width==2,"Cancellation blocked the next decode");

        LaunchOptions options;options.installation=folder;options.workspace=workspace;options.smoke=true;options.testSession=true;
        ImTextureID next=100;std::vector<ImTextureID> destroyed;std::map<ImTextureID,std::vector<uint8_t>> uploaded;
        auto store=std::make_shared<ImageTextures>(ImageTextures::Backend{
            [&](const ImagePixels &image,bool) { uploaded[++next]=image.rgba;return next; },
            [&](ImTextureID id) { destroyed.push_back(id);uploaded.erase(id); },
            [&](ImTextureID id,bool) { require(uploaded.contains(id),"Filter update used a released texture"); }});
        Application app(options,nullptr);
        unsigned char *fontPixels=nullptr;int w=0,h=0;ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&fontPixels,&w,&h);ImGui::GetIO().Fonts->SetTexID(ImTextureID(1));
        app.imageTextures_=store;
        app.open("main.cpp");app.open("alpha.png");auto image=app.tabs_.back();
        wait([&] { app.updateImages();return !image->image->loading(); });
        require(image->image->error().empty() && image->image->texture(),"App did not publish decoded image");
        require(app.engine_->dispatch("documents.list").at("documents").size()==1,"Binary image entered the text document service");
        require(!app.commandEnabled("file.save") && !app.commandEnabled("edit.paste") && !app.commandEnabled("search.document") && !app.commandEnabled("debug.breakpoint"),"Text editing remained enabled for an image");
        require(app.commandEnabled("file.close") && app.commandEnabled("navigate.previousTab"),"Image lost normal tab actions");
        const auto texture=image->image->texture()->data.texture;
        auto draw=[&] { ImGui::Begin("Image fixture");app.imagePanel(image);ImGui::End(); };
        for (const auto *theme:{"graphite","mid-grey","light-grey","amber-night","violet","navy"})
            for (float dpi:{1.f,1.5f,2.f}) for (const auto *locale:{"en","fr"}) {
                cyui::Config config;config.themeKey=theme;config.dpiScale=dpi;cyui::apply(config);cyui::setLocale(locale);
                frame(draw);frame(draw);
            }
        bool drawn=false;for (const auto *list:ImGui::GetDrawData()->CmdLists) for (const auto &command:list->CmdBuffer) drawn|=command.GetTexID()==texture;
        require(drawn,"Shared Images viewport did not reference the decoded texture");
        auto &view=*image->image;auto &io=ImGui::GetIO();view.zoom(1);frame(draw);
        const auto center=ImVec2((view.bounds.min.x+view.bounds.max.x)/2,(view.bounds.min.y+view.bounds.max.y)/2);
        io.AddMousePosEvent(center.x,center.y);frame(draw);frame(draw);
        const auto zoom=view.state.canvas.view.zoom;io.AddMouseWheelEvent(0,1);frame(draw);frame(draw);
        require(view.state.canvas.view.zoom>zoom,"Wheel did not zoom the shared image viewport");
        const auto origin=view.state.canvas.view.origin;io.AddMouseButtonEvent(2,true);frame(draw);
        io.AddMousePosEvent(center.x+40,center.y+20);frame(draw);io.AddMouseButtonEvent(2,false);frame(draw);
        require(view.state.canvas.view.origin.x!=origin.x || view.state.canvas.view.origin.y!=origin.y,"Middle mouse did not pan the image");
        view.zoom(1);frame(draw);require(view.state.canvas.view.zoom==1,"Actual-pixel zoom failed");
        view.zoom(2);frame(draw);view.state.canvas.view.origin={10,20};frame(draw);
        auto savedView=view.sessionState();savedView["pixelated"]=true;
        const auto display=io.DisplaySize;
        view.zoom(.5);frame(draw);view.restoreSession(savedView);
        require(view.sessionState()==savedView,"Pending async/hidden image view was lost");
        io.DisplaySize={1000,700};frame(draw);
        const auto restoredView=view.sessionState();
        require(restoredView.at("zoom")==2. && restoredView.at("pixelated")==true &&
                !restoredView.at("fit").get<bool>() &&
                std::abs(restoredView.at("centerX").get<double>()-savedView.at("centerX").get<double>())<.01 &&
                std::abs(restoredView.at("centerY").get<double>()-savedView.at("centerY").get<double>())<.01,
                "Image center/zoom/filter did not restore in a resized viewport");
        auto resizedSource=savedView;resizedSource["width"]=savedView.at("width").get<int>()+1;
        view.restoreSession(resizedSource);frame(draw);
        require(view.sessionState().at("fit").get<bool>(),"Changed image dimensions reused an obsolete camera");
        io.DisplaySize=display;frame(draw);
        write(workspace/"alpha.png","broken");app.reloadImage(image);
        wait([&] { app.updateImages();return !view.loading(); });
        require(!view.error().empty() && view.texture()->data.texture==texture,"Failed reload discarded a valid image");
        write(workspace/"alpha.png",png);app.reloadImage(image);
        wait([&] { view.update();return !view.loading(); });
        require(view.error().empty() && destroyed.empty(),"Reload deleted a texture still in the previous draw list");
        store->collect();require(destroyed.size()==1 && destroyed.front()==texture,"Retired texture was not released after the frame");
        const auto count=app.tabs_.size();write(folder/"outside.png",png);app.open(pathUtf8(folder/"outside.png"));
        require(app.tabs_.size()==count && app.lastError_.find("outside")!=std::string::npos,"Image preview bypassed workspace path permissions");
        app.openImageFile(folder/"outside.png");auto external=app.tabs_.back();
        wait([&] { app.updateImages();return !external->image->loading(); });
        require(app.tabs_.size()==count+1 && external->readOnly && external->image->texture(),"Explicitly selected external image did not open");
        failed=false;try { (void)app.engine_->dispatch("documents.open",{{"path",pathUtf8(folder/"outside.png")}}); } catch (const std::exception &) { failed=true; }
        require(failed,"External image selection extended workspace document permissions");app.closeTab(external->path);external.reset();store->collect();
        const auto current=view.texture()->data.texture;app.closeTab(image->path);image.reset();
        require(uploaded.contains(current),"Closing an image deleted its in-flight texture");store->collect();require(!uploaded.contains(current),"Closing an image leaked its texture");
        app.open("corrupt.png");auto corrupt=app.tabs_.back();app.closeTab(corrupt->path);corrupt.reset(); // cancels pending work without waiting for decode
        app.open("main.cpp");require(app.commandEnabled("edit.undo") == app.tabs_.front()->editor.CanUndo(),"Text command routing did not recover");
        require(readTextFileBytes(workspace/"alpha.png")==png,"Read-only image preview modified source bytes");
    }
};
}
int main(int argc,char **argv) {
    try {
        require(argc==2,"Expected repository root");const auto root=utf8Path(argv[1]);std::ifstream in(root/"app/tests/image-fixtures.json");const auto fixtures=Json::parse(in);
        ImGui::CreateContext();auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1400,1000};io.DeltaTime=1.f/60;io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;io.BackendFlags|=ImGuiBackendFlags_RendererHasTextures;
        cyui::apply({});cyui::loadFonts();ApplicationImageTests::run(root,fixtures);ImGui::DestroyContext();
        std::cout<<"Image decoders, async loading, actual app views, input, guards and texture lifetimes passed without native windows\n";return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
