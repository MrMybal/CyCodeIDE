#include "cycode/ImageView.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <stdexcept>

namespace cycode {
namespace {
struct TextureBinding {
    using BindBuffer = void (APIENTRY *)(GLenum,GLuint);
    BindBuffer bindBuffer = reinterpret_cast<BindBuffer>(SDL_GL_GetProcAddress("glBindBuffer"));
    GLint texture = 0, alignment = 0, rowLength = 0, buffer = 0, skipRows = 0, skipPixels = 0;
    TextureBinding() {
        if (!bindBuffer) throw std::runtime_error("OpenGL buffer functions are unavailable");
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);glGetIntegerv(GL_UNPACK_ROW_LENGTH,&rowLength);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&buffer);glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skipRows);glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skipPixels);
        bindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    }
    ~TextureBinding() {
        glBindTexture(GL_TEXTURE_2D,GLuint(texture));glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);glPixelStorei(GL_UNPACK_ROW_LENGTH,rowLength);
        glPixelStorei(GL_UNPACK_SKIP_ROWS,skipRows);glPixelStorei(GL_UNPACK_SKIP_PIXELS,skipPixels);bindBuffer(GL_PIXEL_UNPACK_BUFFER,GLuint(buffer));
    }
};
void filterTexture(ImTextureID id, bool nearest) {
    TextureBinding previous;glBindTexture(GL_TEXTURE_2D,GLuint(id));
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,nearest ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,nearest ? GL_NEAREST : GL_LINEAR);
}
}
std::shared_ptr<ImageTextures> openGLImageTextures() {
    ImageTextures::Backend backend;
    backend.upload = [](const ImagePixels &image, bool nearest) -> ImTextureID {
        if (!glGetString(GL_VERSION)) throw std::runtime_error("An active OpenGL context is required to display the image");
        GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
        if (image.width>maximum || image.height>maximum) throw std::runtime_error("Image dimensions exceed this GPU's texture limit");
        TextureBinding previous;
        GLuint texture=0;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,nearest ? GL_NEAREST : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,nearest ? GL_NEAREST : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,image.width,image.height,0,GL_RGBA,GL_UNSIGNED_BYTE,image.rgba.data());
        if (glGetError()!=GL_NO_ERROR) { glDeleteTextures(1,&texture);throw std::runtime_error("The GPU could not allocate the image preview"); }
        return ImTextureID(texture);
    };
    backend.destroy = [](ImTextureID id) { const auto texture=GLuint(id);glDeleteTextures(1,&texture); };
    backend.filter = filterTexture;
    return std::make_shared<ImageTextures>(std::move(backend));
}
} // namespace cycode
