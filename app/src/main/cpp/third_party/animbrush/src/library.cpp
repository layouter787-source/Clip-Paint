#include "../include/animbrush/library.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#include "../include/animbrush/dabbrush.hpp"

namespace ab {

// ---------------------------------------------------------------------------------------------
// Stamp bank
// ---------------------------------------------------------------------------------------------
static float tileNoise(float x, float y, int period, uint32_t seed) {
  int xi = (int)std::floor(x), yi = (int)std::floor(y);
  float xf = x - xi, yf = y - yi;
  auto wrap = [&](int i) { i %= period; return i < 0 ? i + period : i; };
  auto h = [&](int a, int b) { return hashf(hash2(wrap(a), wrap(b), seed)); };
  float u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf);
  return lerpf(lerpf(h(xi, yi), h(xi + 1, yi), u), lerpf(h(xi, yi + 1), h(xi + 1, yi + 1), u), v);
}

static Mask makeMask(const char* name, int n) {
  Mask m; m.name = name; m.w = m.h = n; m.a.assign((size_t)n * n, 0); return m;
}

// Histogram equalisation -> values uniformly distributed 0..255 (so grain thresholds behave predictably).
static void equalize(Mask& m) {
  size_t hist[256] = {0};
  for (uint8_t v : m.a) ++hist[v];
  uint8_t lut[256]; size_t cum = 0;
  for (int i = 0; i < 256; ++i) { cum += hist[i]; lut[i] = (uint8_t)std::min<size_t>(255, (cum * 255) / std::max<size_t>(1, m.a.size())); }
  for (auto& v : m.a) v = lut[v];
}

StampBank::StampBank() { makeBuiltins(); }
StampBank& StampBank::instance() { static StampBank b; return b; }

void StampBank::makeBuiltins() {
  const int N = 256;
  {  // paper tooth: fine isotropic grain
    Mask m = makeMask("@paper", N);
    for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) {
      float g = 0.45f * tileNoise(x / 3.2f, y / 3.2f, 80, 1) + 0.35f * tileNoise(x / 1.6f, y / 1.6f, 160, 2) +
                0.20f * tileNoise(x / 8.f, y / 8.f, 32, 3);
      m.a[(size_t)y * N + x] = (uint8_t)(clampf(g) * 255);
    }
    equalize(m);
    add(std::move(m));
  }
  {  // canvas weave
    Mask m = makeMask("@canvas", N);
    for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) {
      float px = std::fmod((float)x, 8.f) / 8.f, py = std::fmod((float)y, 8.f) / 8.f;
      bool over = (((x / 8) + (y / 8)) & 1) != 0;
      float tx = 0.5f + 0.5f * std::sin(px * kPi), ty = 0.5f + 0.5f * std::sin(py * kPi);
      float g = over ? 0.45f + 0.55f * tx : 0.45f + 0.55f * ty;
      g *= 0.85f + 0.15f * tileNoise(x / 2.f, y / 2.f, 128, 9);
      m.a[(size_t)y * N + x] = (uint8_t)(clampf(g) * 255);
    }
    add(std::move(m));
  }
  {  // rough: coarse blotchy tooth (watercolour paper, rough chalk)
    Mask m = makeMask("@rough", N);
    for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) {
      float g = 0.5f * tileNoise(x / 10.f, y / 10.f, 25, 4) + 0.3f * tileNoise(x / 4.f, y / 4.f, 64, 5) +
                0.2f * tileNoise(x / 1.5f, y / 1.5f, 170, 6);
      m.a[(size_t)y * N + x] = (uint8_t)(clampf(g) * 255);
    }
    equalize(m);
    add(std::move(m));
  }
  {  // white noise
    Mask m = makeMask("@noise", N);
    for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) m.a[(size_t)y * N + x] = (uint8_t)(hashf(hash2(x, y, 99)) * 255);
    add(std::move(m));
  }
  {  // fibers: short random hairs (charcoal / handmade paper / dry brush)
    Mask m = makeMask("@fibers", N);
    for (auto& v : m.a) v = 170;
    Rng r(1234);
    for (int i = 0; i < 900; ++i) {
      float x = r.uniform() * N, y = r.uniform() * N, a = r.uniform() * kTau, len = r.range(6, 22);
      uint8_t val = (uint8_t)r.range(20, 90);
      for (float t = 0; t < len; t += 0.6f) {
        int px = ((int)(x + std::cos(a) * t) % N + N) % N, py = ((int)(y + std::sin(a) * t) % N + N) % N;
        m.a[(size_t)py * N + px] = std::min<uint8_t>(m.a[(size_t)py * N + px], val);
      }
    }
    add(std::move(m));
  }
  {  // dots: regular dot pattern
    Mask m = makeMask("@dots", N);
    for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) {
      float fx = std::fmod((float)x, 8.f) - 3.5f, fy = std::fmod((float)y, 8.f) - 3.5f;
      float d = std::sqrt(fx * fx + fy * fy);
      m.a[(size_t)y * N + x] = (uint8_t)(clampf(1.f - (d - 2.2f)) * 255);
    }
    add(std::move(m));
  }
  {  // linen: crossing fine threads
    Mask m = makeMask("@linen", N);
    for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) {
      float a = 0.6f + 0.4f * std::sin(x * 1.9f + 6.f * tileNoise(x / 16.f, y / 16.f, 16, 11));
      float b = 0.6f + 0.4f * std::sin(y * 1.9f + 6.f * tileNoise(y / 16.f, x / 16.f, 16, 12));
      m.a[(size_t)y * N + x] = (uint8_t)(clampf(0.5f * (a + b) + 0.1f * hashf(hash2(x, y, 5))) * 255);
    }
    add(std::move(m));
  }
}

void StampBank::add(Mask m) {
  std::string n = m.name;
  masks_[n] = std::make_unique<Mask>(std::move(m));
}

const Mask* StampBank::get(const std::string& name) {
  auto it = masks_.find(name);
  return it == masks_.end() ? nullptr : it->second.get();
}

std::vector<std::string> StampBank::names() const {
  std::vector<std::string> v;
  for (auto& kv : masks_) v.push_back(kv.first);
  return v;
}

// Pack format (little endian): "ABPK" u32 version(1) u32 count, then per entry:
// char name[48] (NUL padded) u16 w u16 h, u8 data[w*h]
bool StampBank::loadPackFromMemory(const uint8_t* d, size_t size) {
  if (size < 12 || std::memcmp(d, "ABPK", 4) != 0) return false;
  auto rd32 = [&](size_t o) { return (uint32_t)d[o] | ((uint32_t)d[o + 1] << 8) | ((uint32_t)d[o + 2] << 16) | ((uint32_t)d[o + 3] << 24); };
  auto rd16 = [&](size_t o) { return (uint32_t)d[o] | ((uint32_t)d[o + 1] << 8); };
  uint32_t count = rd32(8);
  size_t o = 12;
  for (uint32_t i = 0; i < count; ++i) {
    if (o + 52 > size) return false;
    Mask m;
    char nm[49]; std::memcpy(nm, d + o, 48); nm[48] = 0; m.name = nm;
    m.w = (int)rd16(o + 48); m.h = (int)rd16(o + 50); o += 52;
    size_t n = (size_t)m.w * m.h;
    if (o + n > size) return false;
    m.a.assign(d + o, d + o + n); o += n;
    add(std::move(m));
  }
  return true;
}

bool StampBank::loadPack(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  return loadPackFromMemory(buf.data(), buf.size());
}

// ---------------------------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------------------------
BrushLibrary& BrushLibrary::instance() {
  static BrushLibrary lib;
  static bool init = false;
  if (!init) { init = true; registerAllBrushes(lib); }
  return lib;
}

void BrushLibrary::add(BrushInfo info, BrushFactory f) {
  auto it = index_.find(info.id);
  if (it != index_.end()) { entries_[it->second] = {std::move(info), std::move(f)}; return; }
  index_[info.id] = entries_.size();
  entries_.push_back({std::move(info), std::move(f)});
}

void BrushLibrary::addDab(BrushInfo info, DabBrushSpec spec) {
  BrushInfo copy = info;
  add(std::move(info), [copy, spec]() -> std::unique_ptr<Brush> { return std::make_unique<DabBrush>(copy, spec); });
}

std::vector<const BrushEntry*> BrushLibrary::byCategory(const std::string& cat) const {
  std::vector<const BrushEntry*> v;
  for (auto& e : entries_) if (e.info.category == cat) v.push_back(&e);
  return v;
}

std::vector<std::string> BrushLibrary::categories() const {
  std::vector<std::string> v;
  for (auto& e : entries_) if (std::find(v.begin(), v.end(), e.info.category) == v.end()) v.push_back(e.info.category);
  return v;
}

const BrushEntry* BrushLibrary::find(const std::string& id) const {
  auto it = index_.find(id);
  return it == index_.end() ? nullptr : &entries_[it->second];
}

std::unique_ptr<Brush> BrushLibrary::create(const std::string& id) const {
  const BrushEntry* e = find(id);
  if (!e) return nullptr;
  auto b = e->make();
  if (b) b->mutableInfo() = e->info;
  return b;
}

static std::string jesc(const std::string& s) {
  std::string o;
  for (unsigned char c : s) {
    switch (c) {
      case '"': o += "\\\""; break; case '\\': o += "\\\\"; break; case '\n': o += "\\n"; break;
      case '\t': o += "\\t"; break; case '\r': break;
      default: if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; } else o += (char)c;
    }
  }
  return o;
}

std::string BrushLibrary::catalogJson() const {
  std::ostringstream o;
  o << "{\n  \"brushes\": [\n";
  for (size_t i = 0; i < entries_.size(); ++i) {
    const BrushInfo& b = entries_[i].info;
    auto inst = entries_[i].make();
    o << "    {\"id\":\"" << jesc(b.id) << "\",\"name\":\"" << jesc(b.name) << "\",\"category\":\"" << jesc(b.category)
      << "\",\"eraser\":" << (b.isEraser ? "true" : "false") << ",\"animated\":" << (b.animated ? "true" : "false")
      << ",\"additive\":" << (b.additive ? "true" : "false") << ",\"defaultSize\":" << b.defaultSize
      << ",\"minSize\":" << b.minSize << ",\"maxSize\":" << b.maxSize << ",\"stamp\":\"" << jesc(b.stampName)
      << "\",\"description\":\"" << jesc(b.description) << "\",\"tags\":[";
    for (size_t t = 0; t < b.tags.size(); ++t) o << (t ? "," : "") << "\"" << jesc(b.tags[t]) << "\"";
    o << "],\"params\":[";
    if (inst) {
      const auto& ps = inst->params();
      for (size_t t = 0; t < ps.size(); ++t)
        o << (t ? "," : "") << "{\"key\":\"" << jesc(ps[t].key) << "\",\"label\":\"" << jesc(ps[t].label) << "\",\"def\":" << ps[t].def
          << ",\"min\":" << ps[t].min << ",\"max\":" << ps[t].max << "}";
    }
    o << "]}" << (i + 1 < entries_.size() ? "," : "") << "\n";
  }
  o << "  ]\n}\n";
  return o.str();
}

void registerAllBrushes(BrushLibrary& lib) {
  registerMediaBrushes(lib);
  registerMediaOilZip(lib);
  registerNatureBrushes(lib);
  registerLightBrushes(lib);
  registerStampBrushes(lib);
  registerEraserBrushes(lib);
  registerDecorFxBrushes(lib);
}

}  // namespace ab
