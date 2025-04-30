#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <cmath>  
#include <string> 
#include <sstream>
#include <chrono> 
#include <functional>

using Coord = std::pair<int, int>;

struct CoordHash {
  std::size_t operator()(const Coord& p) const noexcept {
    size_t hash_1 = std::hash<int>()(p.first);
    size_t hash_2 = std::hash<int>()(p.second);
    return hash_1 ^ (hash_2 << 1);
  }
};

using CellGrid = std::unordered_map<Coord, bool, CoordHash>;
using CandidateSet = std::unordered_set<Coord, CoordHash>;

const int SCREEN_WIDTH = 1000;
const int SCREEN_HEIGHT = 700;
const float BASE_CELL_SIZE = 10.0f;
const float ZOOM_INCREMENT = 0.125f;
const float SPEED_ADJUST = 0.05f;

// Convert screen coordinates to world grid coordinates
Coord screen_to_world_coord(Vector2 screen_pos, const Camera2D& camera, float current_cell_size) {
  Vector2 world_pos = GetScreenToWorld2D(screen_pos, camera);
  return {
    static_cast<int>(floor(world_pos.x / current_cell_size)),
    static_cast<int>(floor(world_pos.y / current_cell_size))
  };
}

int count_active_neighbours(const CellGrid& grid, const Coord& cell) {
  int count = 0;
  const Coord neighbour_offsets[8] = {
    {-1, -1}, {0, -1}, {1, -1},
    {-1,  0},          {1,  0},
    {-1,  1}, {0,  1}, {1,  1}
  };

  for (const auto& offset : neighbour_offsets) {
    Coord neighbour = {cell.first + offset.first, cell.second + offset.second};
    if (grid.count(neighbour)) {
      count++;
    }
  }
  return count;
}

void update_grid(CellGrid& grid, long long& generation) {
  CandidateSet candidates;
  CellGrid new_grid;

  for (const auto& [coord, _] : grid) {
    candidates.insert(coord);
    const Coord neighbour_offsets[8] = {
      {-1, -1}, {0, -1}, {1, -1},
      {-1,  0},          {1,  0},
      {-1,  1}, {0,  1}, {1,  1}
    };
    for (const auto& offset : neighbour_offsets) {
      candidates.insert({coord.first + offset.first, coord.second + offset.second});
    }
  }

  for (const Coord& coord : candidates) {
    int neighbours = count_active_neighbours(grid, coord);
    bool is_alive = grid.count(coord);

    if (is_alive) {
      if (neighbours == 2 || neighbours == 3) {
        new_grid[coord] = true;
      }
    } else {
      if (neighbours == 3) {
        new_grid[coord] = true;
      }
    }
  }

  grid = std::move(new_grid);
  generation++;
}

bool is_time_to_step(float rate_in_seconds) {
  static auto last_time = std::chrono::high_resolution_clock::now();
  auto current_time = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float> elapsed = current_time - last_time;

  if (elapsed.count() >= rate_in_seconds) {
    last_time = current_time;
    return true;
  }
  return false;
}

// Handle camera panning and zooming
void handle_camera_input(Camera2D& camera) {
  if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    Vector2 delta = GetMouseDelta();
    delta = Vector2Scale(delta, -1.0f / camera.zoom);
    camera.target = Vector2Add(camera.target, delta);
  }

  float wheel = GetMouseWheelMove();
  if (wheel != 0) {
    Vector2 mouse_world_pos = GetScreenToWorld2D(GetMousePosition(), camera);
    camera.offset = GetMousePosition();
    camera.target = mouse_world_pos;

    camera.zoom += (wheel * ZOOM_INCREMENT);
    if (camera.zoom < ZOOM_INCREMENT) camera.zoom = ZOOM_INCREMENT;
    if (camera.zoom > 8.0f) camera.zoom = 8.0f;
  }
}

// Handle toggling/painting cells
void handle_mouse_interaction(CellGrid& grid, const Camera2D& camera, float current_cell_size) {
  static bool is_painting = false;
  static bool paint_mode_is_drawing = false;
  static CandidateSet cells_modified_this_stroke;

  Vector2 mouse_pos = GetMousePosition();
  Coord current_cell_coord = screen_to_world_coord(mouse_pos, camera, current_cell_size);

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) return;
    is_painting = true;
    cells_modified_this_stroke.clear();
    if (grid.count(current_cell_coord)) {
      paint_mode_is_drawing = false;
      grid.erase(current_cell_coord);
    } else {
      paint_mode_is_drawing = true;
      grid[current_cell_coord] = true;
    }
    cells_modified_this_stroke.insert(current_cell_coord);
  }
  else if (is_painting && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
      is_painting = false; return;
    }
    if (cells_modified_this_stroke.find(current_cell_coord) == cells_modified_this_stroke.end()) {
      if (paint_mode_is_drawing) {
        if (!grid.count(current_cell_coord)) grid[current_cell_coord] = true;
      } else {
        if (grid.count(current_cell_coord)) grid.erase(current_cell_coord);
      }
      cells_modified_this_stroke.insert(current_cell_coord);
    }
  }
  else if (is_painting && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    is_painting = false;
  }
  else if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) && is_painting) {
    is_painting = false;
  }
}

// Handle UI controls
void handle_ui_input(bool& paused, float& step_rate, CellGrid& grid, Camera2D& camera, long long& generation) {
  if (IsKeyPressed(KEY_SPACE)) {
    paused = !paused;
  }

  if (IsKeyPressed(KEY_S)) {
    update_grid(grid, generation);
  }

  if (IsKeyPressed(KEY_UP)) {
    step_rate -= SPEED_ADJUST;
    if (step_rate < 0.01f) step_rate = 0.01f;
  }
  if (IsKeyPressed(KEY_DOWN)) {
    step_rate += SPEED_ADJUST;
    if (step_rate > 2.0f) step_rate = 2.0f;
  }

  if (IsKeyPressed(KEY_R)) {
    grid.clear();
    paused = true;
    generation = 0;
    step_rate = 0.2f;
    camera.target = {0.0f, 0.0f};
    camera.offset = {(float)SCREEN_WIDTH / 2.0f, (float)SCREEN_HEIGHT / 2.0f};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;
  }
}

void draw_grid_lines(const Camera2D& camera, float current_cell_size) {
  Vector2 top_left = GetScreenToWorld2D({0, 0}, camera);
  Vector2 bottom_right = GetScreenToWorld2D({(float)SCREEN_WIDTH, (float)SCREEN_HEIGHT}, camera);
  int start_x = static_cast<int>(floor(top_left.x / current_cell_size));
  int start_y = static_cast<int>(floor(top_left.y / current_cell_size));
  int end_x = static_cast<int>(ceil(bottom_right.x / current_cell_size));
  int end_y = static_cast<int>(ceil(bottom_right.y / current_cell_size));

  const int LINE_LIMIT = 200;
  if ((end_x - start_x) > LINE_LIMIT || (end_y - start_y) > LINE_LIMIT) return;

  Color grid_color = {50, 50, 50, 255};
  for (int i = start_x; i <= end_x; ++i) {
    DrawLineV({(float)i * current_cell_size, top_left.y}, {(float)i * current_cell_size, bottom_right.y}, grid_color);
  }
  for (int i = start_y; i <= end_y; ++i) {
    DrawLineV({top_left.x, (float)i * current_cell_size}, {bottom_right.x, (float)i * current_cell_size}, grid_color);
  }
}

// Draw the live cells
void draw_simulation_cells(const CellGrid& grid, float current_cell_size) {
  Color cell_color = RAYWHITE;
  for (const auto& [coord, _] : grid) {
    DrawRectangleV(
      {(float)coord.first * current_cell_size, (float)coord.second * current_cell_size},
      {current_cell_size, current_cell_size},
      cell_color
    );
  }
}

// Draw UI Text Info
void draw_ui(bool paused, float step_rate, long long generation, size_t population) {
  std::stringstream ss;
  ss.precision(2);

  ss << "Status: " << (paused ? "Paused" : "Running") << " (Space)\n";
  ss << "Speed: " << std::fixed << (1.0f / step_rate) << " steps/sec (Up/Down)\n";
  ss << "Step: S Key\n";
  ss << "Generation: " << generation << "\n";
  ss << "Population: " << population << "\n";
  ss << "Zoom: Wheel | Pan: R-Drag | Paint: L-Drag\n";
  ss << "Reset: R Key";

  DrawRectangle(5, 5, 300, 130, Fade(BLACK, 0.7f));
  DrawText(ss.str().c_str(), 10, 10, 15, LIGHTGRAY);
}

int main() {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Game of Life");
  SetTargetFPS(60);

  CellGrid grid;
  long long generation = 0; 
  bool paused = true;
  float step_rate = 0.2f;

  Camera2D camera = { 0 };
  camera.target = { 0.0f, 0.0f };
  camera.offset = { (float)SCREEN_WIDTH/2.0f, (float)SCREEN_HEIGHT/2.0f };
  camera.rotation = 0.0f;
  camera.zoom = 1.0f;

  while (!WindowShouldClose()) {
    float current_cell_size = BASE_CELL_SIZE;

    handle_camera_input(camera);
    handle_mouse_interaction(grid, camera, current_cell_size);
    handle_ui_input(paused, step_rate, grid, camera, generation);

    if (!paused && is_time_to_step(step_rate)) {
      update_grid(grid, generation);
    }

    BeginDrawing();
    ClearBackground(DARKGRAY);

    BeginMode2D(camera);
      if (camera.zoom > 0.5f) {
        draw_grid_lines(camera, current_cell_size);
      }
      draw_simulation_cells(grid, current_cell_size);
    EndMode2D();

    draw_ui(paused, step_rate, generation, grid.size());

    EndDrawing();
  }

  CloseWindow();
  return 0;
}