#ifdef WIN32
#include <SDL.h>
#undef main
#else
#include <SDL2/SDL.h>
#endif

#include <GL/glew.h>

#include <string_view>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <vector>
#include <map>
#include <random>

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"

using std::vector;

std::string to_string(std::string_view str) {
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

const char vertex_shader_source[] =
        R"(#version 330 core

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec3 in_color;

uniform mat4 view;

out vec4 color;

void main() {
	gl_Position = view * vec4(in_position, 0.0, 1.0);
	color = vec4(in_color, 1.0);
}
)";

const char fragment_shader_source[] =
        R"(#version 330 core

in vec4 color;

layout (location = 0) out vec4 out_color;

void main() {
	out_color = color;
}
)";

GLuint create_shader(GLenum type, const char * source) {
    GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    GLint status;
    glGetShaderiv(result, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Shader compilation failed: " + info_log);
    }
    return result;
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint result = glCreateProgram();
    glAttachShader(result, vertex_shader);
    glAttachShader(result, fragment_shader);
    glLinkProgram(result);

    GLint status;
    glGetProgramiv(result, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        GLint info_log_length;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Program linkage failed: " + info_log);
    }

    return result;
}

struct metaball {
    glm::vec2 position;
    glm::vec2 direction;
    float r;
    float c;
};

struct metaball_function {
    float threshold = 0.005f;
    float movement_scale = 0.5f;
    std::vector<metaball> metaballs = {
            {{1.f,   -0.3f}, {0.f,   0.5f},  1.2f,  1.2f},
            {{0.6f,  -0.4f}, {-0.5f, -0.7f}, -1.3f, 0.9f},
            {{1.f,   0.f},   {0.6f,  0.1f},  1.5f,  1.3f},
            {{0.f,   0.5f},  {1.f,   -0.3f}, 1.3f,  1.2f},
            {{0.6f,  0.1f},  {-0.5f, -0.7f}, 1.2f,  0.9f},
//            {{1.f, 0.f}, {0.6f, -0.4f}, 1.5f, -1.3f},
            {{1.f,   -0.7f}, {-1.f,  0.7f},  0.8f,  1.5f},
            {{0.f,   0.f},   {-0.9f, 0.5f},  1.5f,  0.5f},
            {{-1.f,  0.f},   {-1.f,  0.5f},  0.9f,  1.5f},
            {{-0.3f, 0.2f},  {-0.1f, 0.6f},  1.1f,  0.7f}
    };

    void apply_movement(float dt) {
        for (auto& metaball: metaballs) {
            metaball.position.x += dt * metaball.direction.x;
            metaball.position.y += dt * metaball.direction.y;

            if (abs(metaball.position.x) > 5.f) {
                metaball.direction.x *= -1;
            }
            if (abs(metaball.position.y) > 5.f) {
                metaball.direction.y *= -1;
            }
        }
    }

    float calculate(float x, float y) {
        x *= 5.f;
        y *= 5.f;

        float result = 0.0;
        for (auto& metaball : metaballs) {
            float x_i = metaball.position.x;
            float y_i = metaball.position.y;
            float c_i = metaball.c;
            float r_i = metaball.r;

            result += c_i * exp(-((x - x_i) * (x - x_i) + (y - y_i) * (y - y_i)) / r_i / r_i);
        }

        result /= 5.f;
        return result;
    }

    glm::vec3 value_to_color(float value, float min_value, float max_value) {
        float c = (value - min_value) / (max_value - min_value);

        return glm::vec3(1 - 0.4 * c, 1 - 0.6 * c, 1 - c);
    }
};

struct grid {
    vector<glm::vec2> positions;
    vector<int> indices;

    grid (int width, int height) {
        positions = {};
        for (int i = 0; i <= height; i++) {
            for (int j = 0; j <= width; j++) {
                float x = 2.f / width * j - 1;
                float y = -(2.f / height * i - 1);
                positions.push_back(glm::vec3(x, y, 0.f));
            }
        }

        indices = {};
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                int ind = i * (width + 1) + j;
                indices.push_back(ind + width + 1);
                indices.push_back(ind + 1);
                indices.push_back(ind);

                indices.push_back(ind + 1);
                indices.push_back(ind + width + 1);
                indices.push_back(ind + width + 2);
            }
        }
    }
};

vector<glm::vec3> calculate_colors(grid& grid, metaball_function& function) {
    vector<float> values;
    float min_value = 1e9;
    float max_value = -1e9;
    for (auto position : grid.positions) {
        values.push_back(function.calculate(position.x, position.y));
        min_value = std::min(min_value, values.back());
        max_value = std::max(max_value, values.back());
    }

    vector<glm::vec3> colors;
    for (auto value : values)
        colors.push_back(function.value_to_color(value, min_value, max_value));

    return colors;
}

vertex interpolate_coords(int first_index, int second_index, float value) {
    vertex vertex1 = grid_vertices[first_index];
    vertex vertex2 = grid_vertices[second_index];

    if (vertex1.position.y == vertex2.position.y) {
        return vertex{
                vec2({vertex1.position.x + (value - vertex1.height) / (vertex2.height - vertex1.height) * (vertex2.position.x - vertex1.position.x), vertex1.position.y}), value + 0.01f, {255, 255, 255, 255}
                // vec2({vertex1.position.x + (value - vertex1.height) / (vertex2.height - vertex1.height) * (vertex2.position.x - vertex1.position.x), vertex1.position.y}), 0.01f, {255, 255, 255, 255}
        };
    } else {
        return vertex{
                vec2({vertex1.position.x, vertex1.position.y + (value - vertex1.height) / (vertex2.height - vertex1.height) * (vertex2.position.y - vertex1.position.y)}), value + 0.01f, {255, 255, 255, 255}
                // vec2({vertex1.position.x, vertex1.position.y + (value - vertex1.height) / (vertex2.height - vertex1.height) * (vertex2.position.y - vertex1.position.y)}), 0.01f, {255, 255, 255, 255}
        };
    }
}

void add_isoline(float isoline_height) {
    std::map<std::pair<int, int>, int> indices_after_interpolation;
    for (int i = 0; i < grid_size - 1; i++) {
        for (int j = 0; j < grid_size - 1; j++) {

            int grid_cell_ids[4] = {
                    i * grid_size + j,
                    i * grid_size + j + 1,
                    (i + 1) * grid_size + j + 1,
                    (i + 1) * grid_size + j
            };

            float grid_cells_height[4] = {
                    grid_vertices[grid_cell_ids[0]].height - isoline_height,
                    grid_vertices[grid_cell_ids[1]].height - isoline_height,
                    grid_vertices[grid_cell_ids[2]].height - isoline_height,
                    grid_vertices[grid_cell_ids[3]].height - isoline_height
            };

            std::vector<std::pair<int, int>> needs_interpolation;
            float mean = (grid_cells_height[0] + grid_cells_height[1] + grid_cells_height[2] + grid_cells_height[3]) / 4;
            int positive = 0;

            for (int k = 0; k < 4; k++) {
                positive += grid_cells_height[k] > 0;
            }

            switch (positive) {
                case 1:
                case 3:
                    for (int k = 0; k < 4; k++) {
                        if ((grid_cells_height[k] * grid_cells_height[(k + 3) % 4] < 0) && (grid_cells_height[k] * grid_cells_height[(k + 1) % 4] < 0)) {
                            needs_interpolation.push_back(std::pair(k, (k + 3) % 4));
                            needs_interpolation.push_back(std::pair(k, (k + 1) % 4));
                        }
                    }
                    break;

                case 2:
                    if (grid_cells_height[0] * grid_cells_height[1] > 0) {
                        needs_interpolation.push_back(std::pair(0, 3));
                        needs_interpolation.push_back(std::pair(1, 2));
                    } else if (grid_cells_height[0] * grid_cells_height[3] > 0) {
                        needs_interpolation.push_back(std::pair(0, 1));
                        needs_interpolation.push_back(std::pair(2, 3));
                    } else if (grid_cells_height[0] * mean > 0) {
                        needs_interpolation.push_back(std::pair(0, 1));
                        needs_interpolation.push_back(std::pair(1, 2));
                        needs_interpolation.push_back(std::pair(0, 3));
                        needs_interpolation.push_back(std::pair(2, 3));
                    } else {
                        needs_interpolation.push_back(std::pair(0, 1));
                        needs_interpolation.push_back(std::pair(0, 3));
                        needs_interpolation.push_back(std::pair(1, 2));
                        needs_interpolation.push_back(std::pair(2, 3));
                    }
                    break;

                case 4:
                case 0:
                default:
                    continue;
                    break;
            }

            for (auto pair: needs_interpolation) {
                std::pair<int, int> indexes = std::pair(grid_cell_ids[pair.first], grid_cell_ids[pair.second]);
                if (indices_after_interpolation.contains(indexes)) {
                    isoline_indices.push_back(indices_after_interpolation[indexes]);
                } else {
                    isoline_vertices.push_back(interpolate_coords(indexes.first, indexes.second, isoline_height));
                    indices_after_interpolation[indexes] = isoline_vertices.size() - 1;
                    isoline_indices.push_back(isoline_vertices.size() - 1);
                }
            }
        }
    }
}

void update_isolines(float min_z, float max_z) {
    isoline_vertices.clear();
    isoline_indices.clear();

    for (int i = 1; i <= number_of_isolines; i++) {
        add_isoline(min_z + (max_z - min_z) * float(i) / (number_of_isolines + 1));
    }

    update_isolines_buffers();
}

int main() try {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window * window = SDL_CreateWindow(
            "Graphics course practice 5",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            800, 600,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED
    );

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    auto program = create_program(vertex_shader, fragment_shader);

    const int W = 400, H = 300;

    metaball_function function;
    grid grid(W, H);

    GLuint grid_vao;
    glGenVertexArrays(1, &grid_vao);
    glBindVertexArray(grid_vao);

    GLuint grid_positions_vbo;
    glGenBuffers(1, &grid_positions_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, grid_positions_vbo);
    glBufferData(GL_ARRAY_BUFFER, grid.positions.size() * sizeof(grid.positions[0]), grid.positions.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    GLuint colors_vbo;
    glGenBuffers(1, &colors_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, colors_vbo);
    glBufferData(GL_ARRAY_BUFFER, grid.positions.size() * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    GLuint grid_ebo;
    glGenBuffers(1, &grid_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grid_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, grid.indices.size() * sizeof(grid.indices[0]), grid.indices.data(), GL_STATIC_DRAW);

    GLint view_location = glGetUniformLocation(program, "view");

    glEnable(GL_DEPTH_TEST);

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    bool running = true;
    while (running) {
        for (SDL_Event event; SDL_PollEvent(&event);) switch (event.type)
            {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_WINDOWEVENT: switch (event.window.event)
                    {
                        case SDL_WINDOWEVENT_RESIZED:
                            width = event.window.data1;
                            height = event.window.data2;
                            glViewport(0, 0, width, height);
                            break;
                    }
                    break;
            }

        if (!running)
            break;

        glClearColor(0.8f, 0.8f, 1.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;

        float x_scale = (width > height) ? (float(height) / float(width)) : 1.f;
        float y_scale = (width <= height) ? (float(width) / float(height)) : 1.f;
        float view[16] = {
            x_scale, 0.f, 0.f, 0.f,
            0.f, y_scale, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };

        function.apply_movement(dt);

        glUseProgram(program);
        glUniformMatrix4fv(view_location, 1, GL_TRUE, view);
        glBindVertexArray(grid_vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grid_ebo);

        glBindBuffer(GL_ARRAY_BUFFER, colors_vbo);
        auto colors = calculate_colors(grid, function);
        glBufferData(GL_ARRAY_BUFFER, grid.positions.size() * sizeof(glm::vec3), colors.data(), GL_DYNAMIC_DRAW);

        glDrawElements(GL_TRIANGLES, grid.indices.size(), GL_UNSIGNED_INT, nullptr);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}