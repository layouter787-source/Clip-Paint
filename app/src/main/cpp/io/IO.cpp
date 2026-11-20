#include "IO.h"

#include <sys/stat.h>
#include <sys/types.h>

namespace ClipPaint {

// -------------------------------------------------------------- utilitarios

bool fileExists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0;
}

bool makeDirs(const std::string& path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur += path[i];
        if (path[i] == '/' && cur.size() > 1) ::mkdir(cur.c_str(), 0755);
    }
    ::mkdir(path.c_str(), 0755);
    return fileExists(path);
}

bool readTextFile(const std::string& path, std::string& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

bool writeTextFile(const std::string& path, const std::string& text) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    std::fclose(f);
    return ok;
}

// ---------------------------------------------------------------------- PNG

namespace {

uint32_t g_crcTable[256];
bool g_crcReady = false;

void initCrc() {
    if (g_crcReady) return;
    for (uint32_t n = 0; n < 256; ++n) {
        uint32_t c = n;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        g_crcTable[n] = c;
    }
    g_crcReady = true;
}

uint32_t crcUpdate(uint32_t c, const uint8_t* d, size_t n) {
    for (size_t i = 0; i < n; ++i) c = g_crcTable[(c ^ d[i]) & 0xFF] ^ (c >> 8);
    return c;
}

void putBE32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

bool writeChunk(FILE* f, const char type[4], const uint8_t* data, uint32_t len) {
    uint8_t hdr[8];
    putBE32(hdr, len);
    std::memcpy(hdr + 4, type, 4);
    uint32_t c = 0xFFFFFFFFu;
    c = crcUpdate(c, hdr + 4, 4);
    if (len) c = crcUpdate(c, data, len);
    c ^= 0xFFFFFFFFu;
    uint8_t tail[4];
    putBE32(tail, c);
    return std::fwrite(hdr, 1, 8, f) == 8 && (len == 0 || std::fwrite(data, 1, len, f) == len) &&
           std::fwrite(tail, 1, 4, f) == 4;
}

}  // namespace

bool writePng(const std::string& path, int w, int h, const uint8_t* rgba, int dpi) {
    initCrc();
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    bool ok = std::fwrite(sig, 1, 8, f) == 8;
    uint8_t ihdr[13];
    putBE32(ihdr, (uint32_t)w);
    putBE32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    ok = ok && writeChunk(f, "IHDR", ihdr, 13);
    if (dpi > 0) {  // pHYs: pixels por metro
        uint8_t phys[9];
        const uint32_t ppm = (uint32_t)(dpi / 0.0254 + 0.5);
        putBE32(phys, ppm);
        putBE32(phys + 4, ppm);
        phys[8] = 1;
        ok = ok && writeChunk(f, "pHYs", phys, 9);
    }

    const uint64_t rawSize = (uint64_t)(4 * (uint64_t)w + 1) * (uint64_t)h;
    const uint64_t blocks = (rawSize + 65534) / 65535;
    const uint64_t zSize = 2 + blocks * 5 + rawSize + 4;
    uint8_t lenBuf[8];
    putBE32(lenBuf, (uint32_t)zSize);
    std::memcpy(lenBuf + 4, "IDAT", 4);
    ok = ok && std::fwrite(lenBuf, 1, 8, f) == 8;
    uint32_t crc = crcUpdate(0xFFFFFFFFu, lenBuf + 4, 4);
    auto emit = [&](const uint8_t* d, size_t n) {
        crc = crcUpdate(crc, d, n);
        if (std::fwrite(d, 1, n, f) != n) ok = false;
    };
    const uint8_t zh[2] = {0x78, 0x01};
    emit(zh, 2);

    uint32_t a = 1, b = 0;  // adler32
    std::vector<uint8_t> blk;
    blk.reserve(65535);
    uint64_t remaining = rawSize;
    auto flushBlock = [&]() {
        const bool last = remaining == 0;
        const uint16_t len = (uint16_t)blk.size();
        const uint8_t bh[5] = {(uint8_t)(last ? 1 : 0), (uint8_t)(len & 0xFF), (uint8_t)(len >> 8),
                               (uint8_t)(~len & 0xFF), (uint8_t)((~len >> 8) & 0xFF)};
        emit(bh, 5);
        emit(blk.data(), blk.size());
        blk.clear();
    };
    auto pushRaw = [&](const uint8_t* d, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            a = (a + d[i]) % 65521u;
            b = (b + a) % 65521u;
        }
        size_t off = 0;
        while (off < n) {
            const size_t take = std::min(n - off, (size_t)65535 - blk.size());
            blk.insert(blk.end(), d + off, d + off + take);
            off += take;
            remaining -= take;
            if (blk.size() == 65535 || remaining == 0) flushBlock();
        }
    };
    const uint8_t filter0 = 0;
    for (int y = 0; y < h && ok; ++y) {
        pushRaw(&filter0, 1);
        pushRaw(rgba + (size_t)y * w * 4, (size_t)w * 4);
    }
    uint8_t ad[4];
    putBE32(ad, (b << 16) | a);
    emit(ad, 4);
    uint8_t tail[4];
    putBE32(tail, crc ^ 0xFFFFFFFFu);
    ok = ok && std::fwrite(tail, 1, 4, f) == 4;
    ok = ok && writeChunk(f, "IEND", nullptr, 0);
    std::fclose(f);
    if (!ok) { std::remove(tmp.c_str()); return false; }
    std::remove(path.c_str());
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

// ------------------------------------------------------------------ projeto

namespace {

constexpr uint32_t kMagic = 0x31445043;  // "CPD1"

struct Writer {
    FILE* f;
    bool ok = true;
    void raw(const void* d, size_t n) { if (n && std::fwrite(d, 1, n, f) != n) ok = false; }
    template <class T> void put(T v) { raw(&v, sizeof(T)); }
};

struct Reader {
    FILE* f;
    bool ok = true;
    void raw(void* d, size_t n) { if (n && std::fread(d, 1, n, f) != n) ok = false; }
    template <class T> T get() { T v{}; raw(&v, sizeof(T)); return v; }
};

void encodeTile(const uint8_t* px, std::vector<uint8_t>& out) {
    const uint32_t* p = reinterpret_cast<const uint32_t*>(px);
    int i = 0;
    const int n = TILE * TILE;
    while (i < n) {
        int run = 1;
        while (i + run < n && run < 128 && p[i + run] == p[i]) ++run;
        if (run >= 2) {
            out.push_back((uint8_t)(0x80 | (run - 1)));
            out.insert(out.end(), px + i * 4, px + i * 4 + 4);
            i += run;
        } else {
            int lit = 1;
            while (i + lit < n && lit < 128 && !(i + lit + 1 < n && p[i + lit] == p[i + lit + 1])) ++lit;
            out.push_back((uint8_t)(lit - 1));
            out.insert(out.end(), px + i * 4, px + i * 4 + lit * 4);
            i += lit;
        }
    }
}

bool decodeTile(const uint8_t* in, size_t size, uint8_t* px) {
    size_t pos = 0;
    int i = 0;
    const int n = TILE * TILE;
    while (i < n) {
        if (pos >= size) return false;
        const uint8_t c = in[pos++];
        if (c & 0x80) {
            const int cnt = (c & 0x7F) + 1;
            if (pos + 4 > size || i + cnt > n) return false;
            for (int k = 0; k < cnt; ++k) std::memcpy(px + (i + k) * 4, in + pos, 4);
            pos += 4;
            i += cnt;
        } else {
            const int cnt = c + 1;
            if (pos + (size_t)cnt * 4 > size || i + cnt > n) return false;
            std::memcpy(px + i * 4, in + pos, (size_t)cnt * 4);
            pos += (size_t)cnt * 4;
            i += cnt;
        }
    }
    return true;
}

}  // namespace

bool saveProject(const Document& doc, const std::string& path) {
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;
    Writer w{f};
    w.put<uint32_t>(kMagic);
    w.put<uint32_t>((uint32_t)doc.width());
    w.put<uint32_t>((uint32_t)doc.height());
    w.put<uint32_t>((uint32_t)doc.layerCount());
    w.put<uint32_t>((uint32_t)doc.state.active);
    w.put<float>(doc.paper.r); w.put<float>(doc.paper.g); w.put<float>(doc.paper.b); w.put<float>(doc.paper.a);
    w.put<uint8_t>(doc.paperVisible ? 1 : 0);
    std::vector<uint8_t> enc;
    for (const Layer& l : doc.state.layers) {
        w.put<uint16_t>((uint16_t)std::min<size_t>(l.name.size(), 200));
        w.raw(l.name.data(), std::min<size_t>(l.name.size(), 200));
        w.put<uint8_t>((uint8_t)((l.visible ? 1 : 0) | (l.locked ? 2 : 0) | (l.alphaLock ? 4 : 0)));
        w.put<float>(l.opacity);
        w.put<uint8_t>((uint8_t)l.blend);
        for (const TilePtr& t : l.tiles) {
            if (!t) { w.put<uint8_t>(0); continue; }
            enc.clear();
            encodeTile(t->px, enc);
            w.put<uint8_t>(1);
            w.put<uint32_t>((uint32_t)enc.size());
            w.raw(enc.data(), enc.size());
        }
    }
    std::fclose(f);
    if (!w.ok) { std::remove(tmp.c_str()); return false; }
    std::remove(path.c_str());
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

bool loadProject(Document& doc, const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    Reader r{f};
    if (r.get<uint32_t>() != kMagic) { std::fclose(f); return false; }
    const uint32_t w = r.get<uint32_t>(), h = r.get<uint32_t>();
    const uint32_t nLayers = r.get<uint32_t>(), active = r.get<uint32_t>();
    if (!r.ok || w < 16 || h < 16 || w > 8192 || h > 8192 || nLayers == 0 || nLayers > (uint32_t)kMaxLayers) {
        std::fclose(f);
        return false;
    }
    Document tmp;
    tmp.create((int)w, (int)h);
    tmp.paper.r = r.get<float>(); tmp.paper.g = r.get<float>();
    tmp.paper.b = r.get<float>(); tmp.paper.a = r.get<float>();
    tmp.paperVisible = r.get<uint8_t>() != 0;
    tmp.state.layers.clear();
    std::vector<uint8_t> enc;
    for (uint32_t li = 0; li < nLayers && r.ok; ++li) {
        Layer l = tmp.makeLayer("");
        const uint16_t nl = r.get<uint16_t>();
        if (nl > 200) { r.ok = false; break; }
        l.name.resize(nl);
        if (nl) r.raw(&l.name[0], nl);
        const uint8_t flags = r.get<uint8_t>();
        l.visible = flags & 1; l.locked = flags & 2; l.alphaLock = flags & 4;
        l.opacity = clampf(r.get<float>(), 0.0f, 1.0f);
        const uint8_t bl = r.get<uint8_t>();
        l.blend = bl < (uint8_t)Blend::Count ? (Blend)bl : Blend::Normal;
        for (size_t ti = 0; ti < l.tiles.size() && r.ok; ++ti) {
            if (r.get<uint8_t>() == 0) continue;
            const uint32_t sz = r.get<uint32_t>();
            if (sz == 0 || sz > TILE * TILE * 5) { r.ok = false; break; }
            enc.resize(sz);
            r.raw(enc.data(), sz);
            if (!r.ok) break;
            auto t = std::make_shared<Tile>();
            if (!decodeTile(enc.data(), sz, t->px)) { r.ok = false; break; }
            t->version = nextTileVersion();
            l.tiles[ti] = t;
        }
        tmp.state.layers.push_back(std::move(l));
    }
    std::fclose(f);
    if (!r.ok || tmp.state.layers.empty()) return false;
    tmp.state.active = clampi((int)active, 0, tmp.layerCount() - 1);
    tmp.touch();
    doc = std::move(tmp);
    doc.touch();
    return true;
}

}  // namespace ClipPaint
