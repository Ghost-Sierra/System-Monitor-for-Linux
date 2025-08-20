#define _GNU_SOURCE
#define STB_TRUETYPE_IMPLEMENTATION

#include <GL/glew.h>
#include <GL/glx.h>
#include <dlfcn.h>
#include <iostream>
#include <chrono>
#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include <sstream>
#include <algorithm>
#include <cstring> // For strlen

#include "stats.hpp"
#include "stb_truetype.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- A struct to hold our settings ---
struct OverlaySettings {
    enum Corner { TOP_LEFT, TOP_RIGHT };
    Corner position = TOP_LEFT;
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 0.0f); // Default to yellow
};

// --- Global state for our overlay ---
struct Overlay {
    bool initialized = false;
    GLuint vao = 0, vbo = 0;
    GLuint font_texture = 0;
    GLuint shader_program = 0;
    stbtt_bakedchar cdata[96];
    
    // Stats
    double fps = 0.0;
    double cpu_usage = 0.0;

    // Add settings to our state
    OverlaySettings settings;
};

static std::unique_ptr<Overlay> overlay_state;

// --- Shader code ---
const char* vertex_shader_source = R"glsl(
    #version 330 core
    layout (location = 0) in vec4 vertex; // x, y, u, v
    out vec2 TexCoords;
    uniform mat4 projection;
    void main() {
        gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
        TexCoords = vertex.zw;
    }
)glsl";

const char* fragment_shader_source = R"glsl(
    #version 330 core
    in vec2 TexCoords;
    out vec4 color;
    uniform sampler2D text;
    uniform vec3 textColor;
    void main() {
        vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
        color = vec4(textColor, 1.0) * sampled;
    }
)glsl";

// --- Simple function to parse our config.ini file ---
void parse_config() {
    std::ifstream config_file("config.ini");
    if (!config_file) {
        std::cout << "Overlay: config.ini not found. Using default settings." << std::endl;
        return;
    }
    std::string line;
    while (std::getline(config_file, line)) {
        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            key.erase(0, key.find_first_not_of(" \t\n\r"));
            key.erase(key.find_last_not_of(" \t\n\r") + 1);
            value.erase(0, value.find_first_not_of(" \t\n\r"));
            value.erase(value.find_last_not_of(" \t\n\r") + 1);

            if (key == "position") {
                if (value == "top_right") overlay_state->settings.position = OverlaySettings::TOP_RIGHT;
                else overlay_state->settings.position = OverlaySettings::TOP_LEFT;
            } else if (key == "color_r") {
                overlay_state->settings.color.r = std::stof(value);
            } else if (key == "color_g") {
                overlay_state->settings.color.g = std::stof(value);
            } else if (key == "color_b") {
                overlay_state->settings.color.b = std::stof(value);
            }
        }
    }
    std::cout << "Overlay: Loaded settings from config.ini" << std::endl;
}

// --- Helper function to draw text ---
void render_text(const std::string& text, float x, float y, float scale) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(overlay_state->vao);
    glBindBuffer(GL_ARRAY_BUFFER, overlay_state->vbo);
    for (char c : text) {
        if (c >= 32 && c < 128) {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(overlay_state->cdata, 512, 512, c - 32, &x, &y, &q, 1);
            
            // --- THIS IS THE FIX FOR THE UPSIDE-DOWN TEXT ---
            // We swap the t0 and t1 texture coordinates to flip the character vertically.
            float vertices[] = {
                // Pos(x,y)   Tex(u,v)
                q.x0, q.y1,   q.s0, q.t1,
                q.x0, q.y0,   q.s0, q.t0,
                q.x1, q.y0,   q.s1, q.t0,

                q.x0, q.y1,   q.s0, q.t1,
                q.x1, q.y0,   q.s1, q.t0,
                q.x1, q.y1,   q.s1, q.t1
            };
            
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

// --- Initialization function ---
void initialize_overlay(int viewport_width, int viewport_height) {
    overlay_state = std::make_unique<Overlay>();
    parse_config();
    if (glewInit() != GLEW_OK) { std::cerr << "Overlay Error: Failed to initialize GLEW" << std::endl; return; }
    std::ifstream font_file("DejaVuSans.ttf", std::ios::binary);
    if (!font_file) { std::cerr << "Overlay Error: Could not open font file." << std::endl; return; }
    std::vector<unsigned char> font_buffer(std::istreambuf_iterator<char>(font_file), {});
    std::vector<unsigned char> bitmap(512 * 512);
    stbtt_BakeFontBitmap(font_buffer.data(), 0, 16.0f, bitmap.data(), 512, 512, 32, 96, overlay_state->cdata);
    glGenTextures(1, &overlay_state->font_texture);
    glBindTexture(GL_TEXTURE_2D, overlay_state->font_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 512, 512, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertex_shader_source, NULL);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragment_shader_source, NULL);
    glCompileShader(fs);
    overlay_state->shader_program = glCreateProgram();
    glAttachShader(overlay_state->shader_program, vs);
    glAttachShader(overlay_state->shader_program, fs);
    glLinkProgram(overlay_state->shader_program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glGenVertexArrays(1, &overlay_state->vao);
    glGenBuffers(1, &overlay_state->vbo);
    glBindVertexArray(overlay_state->vao);
    glBindBuffer(GL_ARRAY_BUFFER, overlay_state->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    // Use an orthographic projection where Y=0 is the TOP of the screen
    glm::mat4 projection = glm::ortho(0.0f, (float)viewport_width, (float)viewport_height, 0.0f);
    
    glUseProgram(overlay_state->shader_program);
    glUniformMatrix4fv(glGetUniformLocation(overlay_state->shader_program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    overlay_state->initialized = true;
    std::cout << "Overlay Initialized Successfully!" << std::endl;
}

// --- Our Hooked Function ---
typedef void (*glXSwapBuffers_t)(Display *dpy, GLXDrawable drawable);

void glXSwapBuffers(Display *dpy, GLXDrawable drawable) {
    static glXSwapBuffers_t original_glXSwapBuffers = (glXSwapBuffers_t)dlsym(RTLD_NEXT, "glXSwapBuffers");
    static unsigned int width = 0, height = 0;

    if (!overlay_state) {
        Window root; int x, y; unsigned int border, depth;
        XGetGeometry(dpy, drawable, &root, &x, &y, &width, &height, &border, &depth);
        initialize_overlay(width, height);
    }
    
    if (overlay_state && overlay_state->initialized) {
        GLint last_program, last_texture, last_vao, last_blend_src_alpha, last_blend_dst_alpha;
        glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vao);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &last_blend_src_alpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &last_blend_dst_alpha);
        GLboolean last_blend_enabled = glIsEnabled(GL_BLEND);

        glUseProgram(overlay_state->shader_program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, overlay_state->font_texture);
        glUniform3fv(glGetUniformLocation(overlay_state->shader_program, "textColor"), 1, glm::value_ptr(overlay_state->settings.color));

        static auto last_time = std::chrono::high_resolution_clock::now();
        static int frame_count = 0;
        frame_count++;
        auto current_time = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(current_time - last_time) >= std::chrono::seconds{1}) {
            overlay_state->fps = frame_count;
            overlay_state->cpu_usage = get_cpu_usage();
            frame_count = 0;
            last_time = current_time;
        }

        char text_buffer[128];
        snprintf(text_buffer, sizeof(text_buffer), "FPS: %.0f | CPU: %.1f%%", overlay_state->fps, overlay_state->cpu_usage);

        // Position text from the top-left corner
        float x_pos = 10.0f;
        float y_pos = 20.0f; 

        if (overlay_state->settings.position == OverlaySettings::TOP_RIGHT) {
            float text_width = strlen(text_buffer) * 8.0f; // Simple approximation
            x_pos = width - text_width - 10.0f;
        }
        render_text(text_buffer, x_pos, y_pos, 1.0f);

        glUseProgram(last_program);
        glBindTexture(GL_TEXTURE_2D, last_texture);
        glBindVertexArray(last_vao);
        if (last_blend_enabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        glBlendFunc(last_blend_src_alpha, last_blend_dst_alpha);
    }

    original_glXSwapBuffers(dpy, drawable);
}