#pragma once

#include <goopax_draw/window_gl.h>
#ifdef __linux__
#include <GL/glx.h>
#endif
#ifdef __APPLE__
#if __has_include(<OpenGL/gl.h>)
#include <OpenGL/gl.h>
#define GOOPAX_DRAW_FOUND_OPENGL_HEADERS 1
#else
#define GOOPAX_DRAW_FOUND_OPENGL_HEADERS 0
#endif
#else
#if __has_include(<glatter/glatter.h>)
#include <glatter/glatter.h>
#define GOOPAX_DRAW_FOUND_OPENGL_HEADERS 1
#else
#define GOOPAX_DRAW_FOUND_OPENGL_HEADERS 0
#endif
#endif

#if GOOPAX_DRAW_FOUND_OPENGL_HEADERS

#include <goopax>

struct gl_object
{
    unsigned int gl_id = 0;

    inline gl_object() = default;
    gl_object(const gl_object&) = delete;
    gl_object& operator=(const gl_object&) = delete;
    inline gl_object(gl_object&& b)
        : gl_id(b.gl_id)
    {
        b.gl_id = 0;
    }
    inline gl_object& operator=(gl_object&& b)
    {
        gl_id = b.gl_id;
        b.gl_id = 0;
        return *this;
    }
};

struct gl_buffer : public gl_object
{
    gl_buffer(size_t size)
    {
        glGenBuffers(1, &gl_id);
        glBindBuffer(GL_ARRAY_BUFFER, gl_id);

        // buffer data
        glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    ~gl_buffer()
    {
        if (gl_id)
            glDeleteBuffers(1, &gl_id);
    }

#ifdef _MSC_VER
    gl_buffer(gl_buffer&& b)
        : gl_object(std::move(b))
    {
    }
    gl_buffer& operator=(gl_buffer&& b)
    {
        gl_object::operator=(std::move(b));
        return *this;
    }
#else
    gl_buffer(gl_buffer&&) = default;
    gl_buffer& operator=(gl_buffer&&) = default;
#endif
    gl_buffer() = default;
};

template<class T>
struct opengl_buffer
    : public gl_buffer
    , public goopax::buffer<T>
{
    opengl_buffer(goopax::goopax_device device,
                  const size_t size,
                  goopax::BUFFER_FLAGS flags = goopax::BUFFER_READ_WRITE)
        : gl_buffer(size * sizeof(T))
        , goopax::buffer<T>(goopax::buffer<T>::create_from_gl(device, this->gl_buffer::gl_id, flags))
    {
    }

    opengl_buffer() = default;
    opengl_buffer(opengl_buffer&&) = default;
    opengl_buffer& operator=(opengl_buffer&&) = default;
};

void render(SDL_Window* window, const opengl_buffer<Eigen::Vector3<float>>& x)
{
    goopax::goopax_device device = x.get_device();

    goopax::flush_graphics_interop(device);
    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // glEnable(GL_BLEND);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(3.0);
    glColor4f(1.0f, 1, 1, 1.0);

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    Tdouble ar = Tdouble(width) / height;
    Tdouble scale = 0.7;
    glOrtho(-scale * ar, scale * ar, -scale, scale, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glBindBuffer(GL_ARRAY_BUFFER, x.gl_id);
    glVertexPointer(3, GL_FLOAT, 0, nullptr);

    glEnableClientState(GL_VERTEX_ARRAY);
    glDrawArrays(GL_POINTS, 0, x.size());
    glDisableClientState(GL_VERTEX_ARRAY);
}

void render(SDL_Window* window,
            const opengl_buffer<Eigen::Vector3<float>>& x,
            const opengl_buffer<Eigen::Vector4<float>>* color)
{
    goopax::goopax_device device = x.get_device();

    goopax::flush_graphics_interop(device);
    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glPointSize(1.0);
    if (color)
    {
        glBindBuffer(GL_ARRAY_BUFFER, color->gl_id);
        glColorPointer(4, GL_FLOAT, 0, nullptr);
        glEnableClientState(GL_COLOR_ARRAY);
    }
    else
    {
        glColor4f(1.0f, 1, 1, 1.0);
    }

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    Tdouble ar = Tdouble(width) / height;
    Tdouble scale = 0.7;
    glOrtho(-scale * ar, scale * ar, -scale, scale, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glBindBuffer(GL_ARRAY_BUFFER, x.gl_id);
    glVertexPointer(3, GL_FLOAT, 0, nullptr);

    glEnableClientState(GL_VERTEX_ARRAY);
    glDrawArrays(GL_POINTS, 0, x.size());
    glDisableClientState(GL_VERTEX_ARRAY);
    if (color)
    {
        glDisableClientState(GL_COLOR_ARRAY);
    }
}

#endif
