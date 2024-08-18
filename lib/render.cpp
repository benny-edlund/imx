#include "imx/context.h"
#include "imx/imx.h"
#include <algorithm>
#include <blend2d.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fmt/core.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <limits>
#include <map>
#include <memory>
#include <string_view>
#include <tracy/Tracy.hpp>
#include <variant>
#include <vector>

#define IMBLEND_COLOR_PICKER_HACK 1;

namespace imx {

struct face_offset {
  ImWchar c;
  float x;
  float y;
};
namespace {
std::map<std::uint64_t, face_offset> g_font_look_up{};
std::array<float, 2> g_h_uv{};
} // namespace

template <std::size_t N> struct text {
  static constexpr std::size_t size = N;
  std::array<ImWchar, N> chars;
  BLPoint pt;
  BLRgba32 color;
  BLFont const *font;
  std::uint32_t depth;
};

struct polygon {
  std::vector<BLPoint> points;
  std::vector<BLPoint> uvs;
  BLRgba32 color;
  std::uint32_t depth;
  ImTextureID texture;
};

#if defined(IMBLEND_COLOR_PICKER_HACK)
struct graded_quad {
  std::array<BLPoint, 4> points;
  std::array<BLRgba32, 4> colors;
  std::uint32_t depth;
  ImTextureID texture;
};
#endif

struct line {
  std::vector<BLPoint> points;
  BLRgba32 color;
  float size;
  std::uint32_t depth;
};

#if defined(IMBLEND_COLOR_PICKER_HACK)
using shape = std::variant<text<1>, polygon, graded_quad, line>;
#else
using shape = std::variant<text<1>, polygon, line>;
#endif

using draw_command = std::pair<BLRect, std::vector<shape>>;
using draw_list = std::vector<draw_command>;

struct imblend_context {
  BLContext ctx{};
  BLImage img{};
  BLImageData data{};
  BLContextCreateInfo info{};
  BLFont font{};
  ImVec4 clear_color = ImVec4(0.45F, 0.55F, 0.60F, 1.00F);
  std::array<std::vector<draw_list>, 2> draw_buffers;
  std::size_t buffer = 0;
  std::vector<BLImage> textures{};

  explicit imblend_context(std::string_view font_filename,
                           ImVec4 clear_color = {0.45F, 0.55F, 0.60F, 1.00F},
                           BLContextCreateInfo context_creation_info = {},
                           BLImageData shared_image_data = {});
};

struct edge_t {
  ImDrawIdx p0;
  ImDrawIdx p1;
  ImU32 col;
  std::uint32_t depth;
  ImTextureID texture;
};

constexpr std::uint64_t hash_edge(std::uint32_t const a,
                                  std::uint32_t const b) noexcept {
  return (static_cast<std::uint64_t>(a) << 32U) | static_cast<std::uint64_t>(b);
}

constexpr std::uint32_t hash_edge(std::uint16_t a, std::uint16_t b) {
  return (static_cast<std::uint32_t>(a) << 16U) | static_cast<std::uint32_t>(b);
}

constexpr std::uint64_t uv_to_key(float u, float v) {
  return hash_edge(static_cast<std::uint32_t>(u * 1000U),
                   static_cast<std::uint32_t>(v * 1000U));
}

constexpr bool almostEqual(double a, double b, double epsilon = 1e-6) {
  return std::fabs(a - b) < epsilon;
}

template <typename Container>
constexpr BLRect get_bounds(Container const &points) {
  double max_x = -std::numeric_limits<double>::max();
  double min_x = std::numeric_limits<double>::max();
  double min_y = min_x;
  double max_y = max_x;
  for (auto const &pt : points) {
    max_x = std::max(pt.x, max_x);
    min_x = std::min(pt.x, min_x);
    max_y = std::max(pt.y, max_y);
    min_y = std::min(pt.y, min_y);
  }
  return {min_x, min_y, max_x - min_x, max_y - min_y};
}

template <> constexpr BLRect get_bounds<ImVec4>(ImVec4 const &vec4) {

  return {vec4.x, vec4.y, vec4.z - vec4.x, vec4.w - vec4.y};
}

constexpr bool operator==(edge_t const &a, edge_t const &b) {
  return a.p0 == b.p0 && a.p1 == b.p1;
}

constexpr bool operator<(edge_t const &a, edge_t const &b) {
  if (a.p0 < b.p0)
    return true;
  if (a.p0 > b.p0)
    return false;
  return a.p1 > b.p1;
}

template <std::size_t N> void draw(BLContext &ctx, text<N> const &unicode) {
  ZoneScopedN("Draw utf16");
  ctx.fillUtf16Text(unicode.pt, *(unicode.font), unicode.chars.data(), N,
                    unicode.color);
}

BLMatrix2D as_transform(BLRect uvs, double width, double height) {
  BLMatrix2D M{};
  M.scale(uvs.w, uvs.h);
  M.translate(BLPoint(-uvs.x * width, -uvs.y * height));
  return M;
}

void draw(BLContext &ctx, polygon const &poly) {
  ZoneScopedN("Draw polygon");
  auto uvs = get_bounds(poly.uvs);
  if (poly.texture != nullptr && uvs.h != 0) {
    BLImage const &texture = *reinterpret_cast<BLImage const *>(poly.texture);
    auto orig = BLRect(0, 0, texture.width(), texture.height());
    auto src = orig;
    src.x *= 1.0 / uvs.w;
    src.y *= 1.0 / uvs.h;
    src.w *= 1.0 / uvs.w;
    src.h *= 1.0 / uvs.h;
    auto trg = get_bounds(poly.points);
    BLPattern pattern(texture);
    pattern.translate(src.x, src.y);
    pattern.scale(trg.w / src.w, trg.h / src.h);
    pattern.postTranslate(trg.x, trg.y);
    ctx.setCompOp(BL_COMP_OP_SRC_ATOP);
    ctx.fillPolygon(poly.points.data(), poly.points.size(), pattern);
  } else {
    ctx.fillPolygon(poly.points.data(), poly.points.size(), poly.color);
  }
}

#if defined(IMBLEND_COLOR_PICKER_HACK)
void draw(BLContext &ctx, graded_quad const &poly) {
  ZoneScopedN("Draw graded_quad");
  double max_x = -std::numeric_limits<double>::max();
  double min_x = std::numeric_limits<double>::max();
  double min_y = min_x;
  double max_y = max_x;
  for (auto const &pt : poly.points) {
    max_x = std::max(pt.x, max_x);
    min_x = std::min(pt.x, min_x);
    max_y = std::max(pt.y, max_y);
    min_y = std::min(pt.y, min_y);
  }
  auto pt0_idx = std::numeric_limits<std::size_t>::max();
  auto pt1_idx = std::numeric_limits<std::size_t>::max();
  auto pt2_idx = std::numeric_limits<std::size_t>::max();
  auto pt3_idx = std::numeric_limits<std::size_t>::max();

  for (auto idx = 0UL; idx != poly.points.size(); ++idx) {
    auto const &pnt = poly.points[idx];
    if (pnt.x == min_x && pnt.y == min_y) {
      pt0_idx = idx;
      continue;
    }
    if (pnt.x == max_x && pnt.y == min_y) {
      pt1_idx = idx;
      continue;
    }
    if (pnt.x == max_x && pnt.y == max_y) {
      pt2_idx = idx;
      continue;
    }
    if (pnt.x == min_x && pnt.y == max_y) {
      pt3_idx = idx;
      continue;
    }
  }
  if (pt0_idx != std::numeric_limits<std::size_t>::max() &&
      pt1_idx != std::numeric_limits<std::size_t>::max() &&
      pt2_idx != std::numeric_limits<std::size_t>::max() &&
      pt3_idx != std::numeric_limits<std::size_t>::max()) {

    auto is_horizontal = [](std::array<BLRgba32, 4> const &cols) {
      return cols[0] == cols[3] && cols[1] == cols[2];
    };
    auto is_vertical = [](std::array<BLRgba32, 4> const &cols) {
      return cols[0] == cols[1] && cols[3] == cols[2];
    };

    BLRectI rect(static_cast<int>(min_x), static_cast<int>(min_y),
                 static_cast<int>(max_x - min_x),
                 static_cast<int>(max_y - min_y));

    auto tmp = poly.colors;
    tmp[0] = poly.colors[pt0_idx];
    tmp[1] = poly.colors[pt1_idx];
    tmp[2] = poly.colors[pt2_idx];
    tmp[3] = poly.colors[pt3_idx];

    if (is_horizontal(tmp)) {
      BLGradient g1(
          BLLinearGradientValues(rect.x, rect.y, rect.x + rect.w, rect.y));
      g1.addStop(0.0, tmp[0]);
      g1.addStop(1.0, tmp[1]);
      ctx.fillRect(rect, g1);
    }
    if (is_vertical(tmp)) {
      BLGradient g1(
          BLLinearGradientValues(rect.x, rect.y, rect.x, rect.y + rect.h));
      g1.addStop(0.0, tmp[1]);
      g1.addStop(1.0, tmp[2]);
      ctx.fillRect(rect, g1);
    }
  }
}
#endif

void draw(BLContext &ctx, line const &line) {
  ZoneScopedN("Draw outline");
  BLPath path;
  path.moveTo(line.points.front());
  for (auto it = line.points.begin() + 1; it != line.points.end(); ++it) {
    path.lineTo(*it);
  }
  ctx.strokePath(path, line.color);
}

constexpr bool operator<(shape const &a, shape const &b) {
  return std::visit([](auto const &i) { return i.depth; }, a) <
         std::visit([](auto const &i) { return i.depth; }, b);
}

constexpr bool operator==(shape const &a, shape const &b) {
  return std::visit([](auto const &i) { return i.depth; }, a) ==
         std::visit([](auto const &i) { return i.depth; }, b);
}

void draw(BLContext &ctx, std::vector<shape> const &shapes) {
  for (auto const &s : shapes) {
    std::visit([&](auto const &x) { draw(ctx, x); }, s);
  }
}

constexpr BLRgba32 as_rgba(ImU32 x) {
  return BLRgba32((x & 0xFF00FF00) | ((x >> 16) & 0xFF) |
                  ((x << 16) & 0xFF0000));
}

bool create_glyph(std::vector<shape> &output, ImDrawVert const &vtx,
                  std::uint32_t current_depth) {
  ZoneScoped;

  if (auto *context = static_cast<imblend_context *>(
          ImGui::GetIO().BackendRendererUserData)) {
    std::array<float, 2> uv = {vtx.uv.x, vtx.uv.y};
    auto key = uv_to_key(uv[0], uv[1]);
    auto H = g_font_look_up.begin()->first;
    auto found = g_font_look_up.find(key);
    if (found != g_font_look_up.cend()) { // TODO: Try a vector instead
      output.push_back(text<1>{
          {found->second.c},
          BLPoint(vtx.pos.x - found->second.x + 0.5F, // TODO: Validate this if
                                                      // we need the +0.5F
                  vtx.pos.y - found->second.y),
          as_rgba(vtx.col),
          &context->font,
          current_depth});
      return true;
    }
  }
  return false;
}

// TODO: replace pointer interface with span
void generate_edges(std::map<edge_t, bool> &output, ImDrawIdx const *idx_buffer,
                    ImDrawVert const *vtx_buffer, std::uint32_t start,
                    std::uint32_t depth, ImTextureID texture) {
  ZoneScoped;
  std::map<edge_t, bool>::const_iterator iter;
  bool inserted = true;
  std::tie(iter, inserted) = output.try_emplace(
      edge_t{idx_buffer[start + 0], idx_buffer[start + 1],
             vtx_buffer[idx_buffer[start + 0]].col, depth, texture},
      true);
  output.at(iter->first) = inserted;
  std::tie(iter, inserted) = output.try_emplace(
      edge_t{idx_buffer[start + 1], idx_buffer[start + 2],
             vtx_buffer[idx_buffer[start + 1]].col, depth, texture},
      true);
  output.at(iter->first) = inserted;
  std::tie(iter, inserted) = output.try_emplace(
      edge_t{idx_buffer[start + 0], idx_buffer[start + 2],
             vtx_buffer[idx_buffer[start + 2]].col, depth, texture},
      true);
  output.at(iter->first) = inserted;
}

bool is_graded_quad(std::vector<BLPoint> const &outline,
                    std::vector<BLRgba32> const &colors) {
  // Very ugly hack here...the imgui colorpicker is rendered with a
  // collection of vertex colored quads. blend2d does not (at the time
  // of writing) have a fill type that is similar to vertex colors but
  // as luck would have it ImGui (at time time of writing) actually
  // implements this shading using overlayed linear gradings. One
  // horizontal from white to color and one vertical from transparent to
  // opaque black. We can exploit this fact by first detecting if the
  // current shape is a quad and more then that if it is a quad that is
  // also its bounds. If this is the case and vertex colors are used
  // then we create the special shape that attempts to shade using
  // linear gradient
  //
  // This obviously depends on logic in the rendering of imgui so will
  // likely break at some point however at the moment it is the only
  // option available to us if we want to render the colorpicker
  //
  // This section is overly commented so we will remember all our
  // assumtions later on when this breaks
  //
  auto is_rect = [](std::vector<BLPoint> const &pnts) {
    // We assume a rectangle is made up of points 1, 2, 3, 4 and 5
    // replicating point 1
    if (pnts.size() != 5) {
      return false;
    }
    // We assume quads of interest must have as its corners its own
    // bounds, it seems that imgui has may triangle shapes that are
    // infact quads in the topology with the second triangle
    // collapsed so we want to make sure we dont pick up any of these
    // shapes
    double max_x = -std::numeric_limits<double>::max();
    double min_x = std::numeric_limits<double>::max();
    double max_y = max_x;
    double min_y = min_x;
    for (auto const &pt : pnts) {
      max_x = std::max(pt.x, max_x);
      min_x = std::min(pt.x, min_x);
      max_y = std::max(pt.y, max_y);
      min_y = std::min(pt.y, min_y);
      if (!almostEqual(pt.x, min_x) && !almostEqual(pt.x, max_x)) {
        return false;
      }
      if (!almostEqual(pt.y, min_y) && !almostEqual(pt.y, max_y)) {
        return false;
      }
    }
    return true;
  };
  // So we check if the outline is a rectangular four sided polygon and
  // also if vertex colors are not all the same for all vertices. Since
  // ImGui allows us to use rounded corners in a topology that is all
  // triangles it seems unlikely that they will ever be able to support
  // things like graded shading on item frames simply because vertex
  // colors wont be able to provide a consistent grading due to the
  // triangle topology. So its "probably" pretty safe to assume if they
  // do have multiple vertex colors used in a shape this is because the
  // shape is rectangular so linear grading is possible and in this case
  // our algorithm should work....fingers crossed...
  return is_rect(outline) &&
         !std::all_of(colors.cbegin(), colors.cend(),
                      [&](auto const &col) { return col == colors[0]; });
}

shape generate_shape(std::vector<BLPoint> const &outline,
                     std::vector<BLPoint> const &uvs,
                     std::vector<BLRgba32> const &colors, std::uint32_t depth,
                     ImTextureID texid) {
#if defined IMBLEND_COLOR_PICKER_HACK
  if (is_graded_quad(outline, colors)) {
    return graded_quad{{outline[0], outline[1], outline[2], outline[3]},
                       {colors[0], colors[1], colors[2], colors[3]},
                       depth,
                       texid};
  } else {
    return polygon{outline, uvs, colors.front(), depth, texid};
  }
#else
  return polygon{outline, uvs, colors.front(), depth, texid};
#endif
}

void generate_topology(std::vector<shape> &output,
                       std::map<edge_t, bool> const &edges,
                       ImDrawVert const *vtx_buffer) {
  ZoneScoped;
  std::vector<edge_t> unique_edges;
  {
    for (const auto &edge : edges) {
      if (edge.second) {
        unique_edges.push_back(edge.first);
      }
    }
    std::sort(unique_edges.begin(), unique_edges.end(),
              [](auto const &a, auto const &b) { return a.depth < b.depth; });
  }
  std::vector<BLPoint> outline;
  std::vector<BLPoint> uvs;
  std::vector<BLRgba32> colors;
  {
    ZoneScopedN("connecting edges");
    auto end = unique_edges.end();
    for (auto begin = unique_edges.begin(); begin != end;) {
      auto edge = begin++; // Start from the first unprocessed edge
      ImDrawIdx start = edge->p0;
      std::uint32_t depth = edge->depth;
      ImTextureID texid = edge->texture;
      ImDrawIdx currentStart = edge->p0;
      ImDrawIdx currentEnd = edge->p1;
      bool shapeClosed = false;
      const ImDrawVert &vtxStart = vtx_buffer[start];
      outline.emplace_back(vtxStart.pos.x, vtxStart.pos.y); // Add start vertex
      uvs.emplace_back(vtxStart.uv.x, vtxStart.uv.y);
      colors.push_back(as_rgba(vtxStart.col));

      while (!shapeClosed) {
        const ImDrawVert &vtx = vtx_buffer[currentEnd];
        outline.emplace_back(vtx.pos.x, vtx.pos.y);
        uvs.emplace_back(vtx.uv.x, vtx.uv.y);
        colors.push_back(as_rgba(vtx.col));
        if (currentEnd == start) {
          shapeClosed = true;
          output.push_back(generate_shape(outline, uvs, colors, depth, texid));
          outline = {};
          uvs = {};
          colors = {};
          break; // Start a new shape
        } else {
          auto nextEdgeIt = std::find_if(begin, end, [&](const edge_t &e) {
            return currentEnd == e.p0 || currentEnd == e.p1;
          });
          if (nextEdgeIt != end) {
            if (currentEnd == nextEdgeIt->p0) {
              currentStart = nextEdgeIt->p0;
              currentEnd = nextEdgeIt->p1;
            } else {
              currentStart = nextEdgeIt->p1;
              currentEnd = nextEdgeIt->p0;
            }
            // The edge type must be trivially movable for this to remain
            // performant, if constructors need to be invoked by rotate our
            // performance drops a lot
            end = std::rotate(nextEdgeIt, nextEdgeIt + 1, end);
          } else {
            fmt::print("Invalid topology\n");
            break; // Force start a new shape
          }
        }
      }
    }
  }
}

void process_draw_data(std::vector<draw_list> &blend_data,
                       ImDrawData const *draw_data) {
  // Iterate over all draw lists
  ZoneScoped;
  blend_data.clear();
  for (int n = 0; n < draw_data->CmdListsCount; n++) {
    ZoneScopedN("Draw list");
    const ImDrawList *cmd_list = draw_data->CmdLists[n];
    const ImDrawVert *vtx_buffer = cmd_list->VtxBuffer.Data;
    const ImDrawIdx *idx_buffer = cmd_list->IdxBuffer.Data;

    draw_list &list = blend_data.emplace_back();

    for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
      const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];
      if (pcmd->UserCallback != nullptr) {
        pcmd->UserCallback(cmd_list, pcmd);
      } else {
        std::map<edge_t, bool> edges;
        std::uint32_t current_depth = 0;
        draw_command &data = list.emplace_back();

        data.first = get_bounds(pcmd->ClipRect);
        // font glyphs are always rendered on quads but as we are going to
        // use the blend2d glyph renderer and not the imgui font texture we
        // can skip the second triangle of the quad. skip_next is used to
        // signal this
        bool skip_next = false;
        {
          ZoneScopedN("Collect data");
          for (unsigned int i = 0; i < pcmd->ElemCount; i += 3) {
            if (skip_next) {
              skip_next = false;
              continue;
            }
            ImTextureID texture = pcmd->TextureId;
            if (texture == ImGui::GetFont()->ContainerAtlas->TexID) {
              ZoneScopedN("Check font");
              const ImDrawVert &vtx = vtx_buffer[idx_buffer[i + 0]];
              if (create_glyph(data.second, vtx, current_depth++)) {
                skip_next = true;
                continue;
              }
            }
            generate_edges(edges, idx_buffer, vtx_buffer, i, current_depth++,
                           texture);
          }
          generate_topology(data.second, edges, vtx_buffer);
          std::sort(data.second.begin(), data.second.end());
        }
      }
      idx_buffer += pcmd->ElemCount;
    }
  }
}

auto get_glyph_offset(ImFontGlyph const *glyph, float font_size) {
  static const float s_magic_ratio = 0.875F;
  return std::make_pair(glyph->X0, glyph->Y0 - font_size * s_magic_ratio);
}

void render_draw_list(BLContext &ctx, std::vector<draw_list> const &lists,
                      BLRgba32 clear_color) {
  ZoneScoped;
  ctx.fillAll(clear_color);
  for (auto const &list : lists) {
    for (auto const &cmd : list) {
      ctx.clipToRect(cmd.first);
      draw(ctx, cmd.second);
      ctx.restoreClipping();
    }
  }
}

imblend_context::imblend_context(std::string_view font_filename,
                                 ImVec4 clear_color,
                                 BLContextCreateInfo context_creation_info,
                                 BLImageData shared_image_data)
    : data(shared_image_data), info(context_creation_info),
      clear_color(clear_color) {
  ImGuiIO &io = ImGui::GetIO();
  auto &style = ImGui::GetStyle();
  ImFont *fnt = io.Fonts->AddFontFromFileTTF(font_filename.data(), 24);
  io.FontDefault = fnt;
  ImFontConfig fontConfig;
  fontConfig.GlyphMinAdvanceX = 1.0f;
  fontConfig.SizePixels = 24.00;
  io.Fonts->AddFontDefault(&fontConfig);
  io.FontGlobalScale = 1.;
  // Build atlas
  unsigned char *tex_pixels = nullptr;
  int tex_w = 0;
  int tex_h = 0;
  io.Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);

  for (auto const &character : fnt->IndexLookup) {
    const auto *glyph = fnt->FindGlyph(character);
    if (glyph == nullptr) {
      fmt::print("Failed to find glyph for {}\n", character);
      std::terminate();
    }

    std::uint64_t uv = uv_to_key(glyph->U0, glyph->V0);
    face_offset offset{character, 0, 0};
    std::tie(offset.x, offset.y) =
        get_glyph_offset(glyph, fontConfig.SizePixels);
    g_font_look_up[uv] = offset;
  }
  textures.reserve(1024);

  BLImage &fonts = textures.emplace_back(tex_w, tex_h, BL_FORMAT_PRGB32);
  fonts.createFromData(tex_w, tex_h, BL_FORMAT_PRGB32, tex_pixels, 4 * tex_w);
  // fonts.writeToFile("fonts.png");
  ImTextureID font_texture = ImGui::GetIO().Fonts->TexID;

  img.createFromData(shared_image_data.size.w, shared_image_data.size.w,
                     BL_FORMAT_PRGB32, shared_image_data.pixelData,
                     shared_image_data.stride);

  BLFontFace face;
  if (face.createFromFile(font_filename.data()) != BL_SUCCESS) {
    fmt::print("Failed to load a font face from {}\n", font_filename);
    std::terminate();
  }

  if (font.createFromFace(face, fontConfig.SizePixels) != BL_SUCCESS) {
    fmt::print("Failed to create font from {}\n", font_filename);
    std::terminate();
  }
}

bool initialize_platform() {
  static std::unique_ptr<imx_context> s_context;
  if (s_context) {
    fmt::print("ImX context already initialized\n");
    return false;
  }
  s_context = std::make_unique<imx_context>();
  ImGui::GetIO().BackendPlatformUserData = s_context.get();
  return true;
}

bool initialize_renderer(std::string_view font_filename, ImVec4 clear_color,
                         BLContextCreateInfo context_creation_info,
                         BLImageData shared_image_data) {
  static std::unique_ptr<imblend_context> s_context;
  if (s_context == nullptr) {
    s_context = std::make_unique<imblend_context>(
        font_filename, clear_color, context_creation_info, shared_image_data);
    ImGui::GetIO().BackendRendererUserData = s_context.get();
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(shared_image_data.size.w, shared_image_data.size.h);
  }
  return s_context != nullptr;
}

bool initialize(std::string_view font_filename, ImVec4 clear_color,
                BLContextCreateInfo context_creation_info,
                BLImageData shared_image_data) {
  return initialize_platform() &&
         initialize_renderer(font_filename, clear_color, context_creation_info,
                             shared_image_data);
}

bool begin_frame() {
  auto &io = ImGui::GetIO();
  if (auto *data = static_cast<imblend_context *>(io.BackendRendererUserData)) {
    if (auto *platform_data =
            static_cast<imx_context *>(io.BackendPlatformUserData)) {
      auto &image = *platform_data->windows.front().image;
      if (data->img.createFromData(image.width(), image.height(),
                                   BL_FORMAT_PRGB32, image.data(),
                                   image.stride()) != BL_SUCCESS) {
        fmt::print("Failed to begin render with new shared image data\n");
        return false;
      }
      io.DisplaySize = ImVec2(image.width(), image.height());
    }
    if (data->ctx.begin(data->img, data->info) == BL_SUCCESS) {
      render_draw_list(
          data->ctx, data->draw_buffers[data->buffer++ % 2],
          as_rgba(ImGui::ColorConvertFloat4ToU32(data->clear_color)));
      return true;
    }
  }
  return false;
}

bool render_frame(ImDrawData const *draw_data, ImVec4 clear_color,
                  BLContextFlushFlags flags) {
  ZoneScoped;
  if (auto *context = static_cast<imblend_context *>(
          ImGui::GetIO().BackendRendererUserData)) {
    if (clear_color.x != IMX_NO_COLOR.x || clear_color.y != IMX_NO_COLOR.y ||
        clear_color.z != IMX_NO_COLOR.z || clear_color.w != IMX_NO_COLOR.w) {
      context->clear_color = clear_color;
    }
    process_draw_data(context->draw_buffers[context->buffer % 2], draw_data);
    return context->ctx.flush(flags) == BL_SUCCESS;
  }
  return false;
}

BLImage &add_texture() {
  if (auto *data = static_cast<imblend_context *>(
          ImGui::GetIO().BackendRendererUserData)) {
    return data->textures.emplace_back();
  }
  static BLImage invalid;
  invalid.reset();
  return invalid;
}

} // namespace imx
