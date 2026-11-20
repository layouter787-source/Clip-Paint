#include "Gfx.h"

#include <EGL/eglext.h>
#include <android/log.h>

#define EGL_TAG "ClipPaint-EGL"
#define EGL_LOG(...) __android_log_print(ANDROID_LOG_ERROR, EGL_TAG, __VA_ARGS__)
#define EGL_INFO(...) __android_log_print(ANDROID_LOG_INFO, EGL_TAG, __VA_ARGS__)

namespace ClipPaint {

bool EGLContextManager::initialize(ANativeWindow* window) {
    if (!window) {
        EGL_LOG("ANativeWindow is null");
        return false;
    }
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) {
        EGL_LOG("eglGetDisplay failed: 0x%x", eglGetError());
        return false;
    }
    EGLint major = 0, minor = 0;
    if (!eglInitialize(display_, &major, &minor)) {
        EGL_LOG("eglInitialize failed: 0x%x", eglGetError());
        destroy();
        return false;
    }
    EGL_INFO("EGL version: %d.%d", major, minor);
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        EGL_LOG("eglBindAPI failed: 0x%x", eglGetError());
        destroy();
        return false;
    }

    const EGLint withAlpha[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                                EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                                EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_NONE};
    const EGLint noAlpha[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                              EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 0, EGL_NONE};
    const EGLint* candidates[2] = {withAlpha, noAlpha};
    EGLConfig config = nullptr;
    EGLint configCount = 0;
    for (const EGLint* cand : candidates) {
        if (eglChooseConfig(display_, cand, &config, 1, &configCount) && configCount > 0) break;
    }
    if (configCount == 0) {
        EGL_LOG("No compatible ES3 EGL configuration: 0x%x", eglGetError());
        destroy();
        return false;
    }
    const EGLint ctxAttr[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, ctxAttr);
    if (context_ == EGL_NO_CONTEXT) {
        EGL_LOG("eglCreateContext failed: 0x%x", eglGetError());
        destroy();
        return false;
    }
    surface_ = eglCreateWindowSurface(display_, config, window, nullptr);
    if (surface_ == EGL_NO_SURFACE) {
        EGL_LOG("eglCreateWindowSurface failed: 0x%x", eglGetError());
        destroy();
        return false;
    }
    if (!eglMakeCurrent(display_, surface_, surface_, context_)) {
        EGL_LOG("eglMakeCurrent failed: 0x%x", eglGetError());
        destroy();
        return false;
    }
    eglSwapInterval(display_, 1);
    width_ = ANativeWindow_getWidth(window);
    height_ = ANativeWindow_getHeight(window);
    querySize(width_, height_);
    EGL_INFO("EGL initialized: %d x %d", width_, height_);
    EGL_INFO("OpenGL: %s", (const char*)glGetString(GL_VERSION));
    EGL_INFO("GPU: %s", (const char*)glGetString(GL_RENDERER));
    return true;
}

void EGLContextManager::destroy() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (surface_ != EGL_NO_SURFACE) eglDestroySurface(display_, surface_);
        if (context_ != EGL_NO_CONTEXT) eglDestroyContext(display_, context_);
        eglTerminate(display_);
    }
    display_ = EGL_NO_DISPLAY;
    surface_ = EGL_NO_SURFACE;
    context_ = EGL_NO_CONTEXT;
    width_ = height_ = 0;
}

bool EGLContextManager::swapBuffers() {
    if (!ready()) return false;
    if (!eglSwapBuffers(display_, surface_)) {
        EGL_LOG("eglSwapBuffers failed: 0x%x", eglGetError());
        return false;
    }
    return true;
}

bool EGLContextManager::querySize(int& w, int& h) const {
    if (!ready()) return false;
    EGLint ew = 0, eh = 0;
    if (!eglQuerySurface(display_, surface_, EGL_WIDTH, &ew) ||
        !eglQuerySurface(display_, surface_, EGL_HEIGHT, &eh)) {
        return false;
    }
    w = ew;
    h = eh;
    return ew > 0 && eh > 0;
}

GLuint buildProgram(const char* vs, const char* fs, const char* label) {
    auto compile = [&](GLenum type, const char* src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = GL_FALSE;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (ok != GL_TRUE) {
            char log[1024] = {0};
            glGetShaderInfoLog(s, sizeof(log) - 1, nullptr, log);
            EGL_LOG("%s shader compile failed: %s", label, log);
            glDeleteShader(s);
            return 0;
        }
        return s;
    };
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) {
        if (v) glDeleteShader(v);
        if (f) glDeleteShader(f);
        return 0;
    }
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint linked = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[1024] = {0};
        glGetProgramInfoLog(p, sizeof(log) - 1, nullptr, log);
        EGL_LOG("%s link failed: %s", label, log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

}  // namespace ClipPaint
