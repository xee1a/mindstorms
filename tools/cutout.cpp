// Cuts a product photo out of its plain white background into a transparent PNG.
//   cutout <in.jpg> <out.png> [preview.png]
//   e.g. cutout tools/source/nxt.jpg resources/images/nxt.png
// Steps: upscale 2x, flood-fill near-white from the borders as background, close the
// mask (fills bites where white plastic touches the white background), open it a
// little (removes JPEG speckle on the outline), blur it into an anti-aliased alpha,
// remove the white bleed from edge colours and crop to the subject.
// The optional preview composites the result on the app's dark card colour.

#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

constexpr int Upscale = 2;
constexpr int BackgroundMax = 14; // "darkness" below this, touching the border, is background
constexpr int CloseRadius = 28;   // px (after upscaling): bites narrower than 2x this get filled
constexpr int OpenRadius = 2;     // px: outline speckle smaller than this gets removed
constexpr float Feather = 1.1f;   // gaussian sigma of the alpha edge, px
constexpr int Inset = 3;          // px pulled in from the outline, past the JPEG's white bleed

int darkness(QRgb c) { return 255 - std::min({qRed(c), qGreen(c), qBlue(c)}); }

using Mask = std::vector<uint8_t>;

// Chamfer (3-4) distance from every pixel to the nearest pixel where mask == value.
std::vector<int> distanceTo(const Mask &mask, uint8_t value, int w, int h)
{
    constexpr int Inf = 1 << 28;
    std::vector<int> d(mask.size());
    for (size_t i = 0; i < mask.size(); ++i) d[i] = mask[i] == value ? 0 : Inf;
    const auto relax = [&](int x, int y, int nx, int ny, int cost) {
        if (nx < 0 || ny < 0 || nx >= w || ny >= h) return;
        int &v = d[size_t(y) * w + x];
        v = std::min(v, d[size_t(ny) * w + nx] + cost);
    };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            relax(x, y, x - 1, y, 3), relax(x, y, x, y - 1, 3);
            relax(x, y, x - 1, y - 1, 4), relax(x, y, x + 1, y - 1, 4);
        }
    for (int y = h - 1; y >= 0; --y)
        for (int x = w - 1; x >= 0; --x) {
            relax(x, y, x + 1, y, 3), relax(x, y, x, y + 1, 3);
            relax(x, y, x + 1, y + 1, 4), relax(x, y, x - 1, y + 1, 4);
        }
    return d;
}

Mask dilate(const Mask &m, int r, int w, int h)
{
    const auto d = distanceTo(m, 1, w, h);
    Mask out(m.size());
    for (size_t i = 0; i < m.size(); ++i) out[i] = d[i] <= 3 * r;
    return out;
}

Mask erode(const Mask &m, int r, int w, int h)
{
    const auto d = distanceTo(m, 0, w, h);
    Mask out(m.size());
    for (size_t i = 0; i < m.size(); ++i) out[i] = d[i] > 3 * r;
    return out;
}

std::vector<float> gaussianBlur(const Mask &m, float sigma, int w, int h)
{
    const int r = int(std::ceil(sigma * 3));
    std::vector<float> k(2 * r + 1);
    float sum = 0;
    for (int i = -r; i <= r; ++i) sum += k[i + r] = std::exp(-0.5f * i * i / (sigma * sigma));
    for (float &v : k) v /= sum;
    std::vector<float> tmp(m.size()), out(m.size());
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float s = 0;
            for (int i = -r; i <= r; ++i) s += k[i + r] * m[size_t(y) * w + std::clamp(x + i, 0, w - 1)];
            tmp[size_t(y) * w + x] = s;
        }
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float s = 0;
            for (int i = -r; i <= r; ++i) s += k[i + r] * tmp[size_t(std::clamp(y + i, 0, h - 1)) * w + x];
            out[size_t(y) * w + x] = s;
        }
    return out;
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc < 3) {
        std::fprintf(stderr, "usage: cutout <in> <out.png> [preview.png]\n");
        return 2;
    }
    QImage src(QString::fromLocal8Bit(argv[1]));
    if (src.isNull()) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 1;
    }
    // Pad with white so the closing never runs into the image border.
    const int pad = CloseRadius + 8;
    QImage img(src.size() * Upscale + QSize(2 * pad, 2 * pad), QImage::Format_ARGB32);
    img.fill(Qt::white);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(QRect(QPoint(pad, pad), src.size() * Upscale), src);
    }
    const int w = img.width();
    const int h = img.height();
    const auto at = [&](int x, int y) { return reinterpret_cast<const QRgb *>(img.constScanLine(y))[x]; };

    // 1. Foreground: everything not reachable from the border through near-white pixels.
    Mask fg(size_t(w) * h, 1);
    std::vector<int> stack;
    const auto push = [&](int x, int y) {
        if (x >= 0 && y >= 0 && x < w && y < h) stack.push_back(y * w + x);
    };
    for (int x = 0; x < w; ++x) push(x, 0), push(x, h - 1);
    for (int y = 0; y < h; ++y) push(0, y), push(w - 1, y);
    while (!stack.empty()) {
        const int i = stack.back();
        stack.pop_back();
        const int x = i % w, y = i / w;
        if (!fg[i] || darkness(at(x, y)) >= BackgroundMax) continue;
        fg[i] = 0;
        push(x + 1, y), push(x - 1, y), push(x, y + 1), push(x, y - 1);
    }

    // 2. Clean outline: close the bites, then knock off speckle.
    fg = erode(dilate(fg, CloseRadius, w, h), CloseRadius, w, h);
    fg = dilate(erode(fg, OpenRadius, w, h), OpenRadius, w, h);

    // 3. Anti-aliased alpha, kept inside the mask so no white rim survives outside it.
    const auto inner = erode(fg, Inset, w, h);
    auto alpha = gaussianBlur(inner, Feather, w, h);

    // 4. Un-mix white from semi-transparent pixels and crop.
    QImage out(w, h, QImage::Format_ARGB32);
    int minX = w, minY = h, maxX = -1, maxY = -1;
    for (int y = 0; y < h; ++y) {
        auto *o = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const float a = alpha[size_t(y) * w + x];
            if (a < 0.01f) {
                o[x] = qRgba(0, 0, 0, 0);
                continue;
            }
            const QRgb c = at(x, y);
            const auto unmix = [a](int v) { return std::clamp(int(std::lround((v - (1 - a) * 255) / a)), 0, 255); };
            o[x] = a >= 0.995f ? c : qRgba(unmix(qRed(c)), unmix(qGreen(c)), unmix(qBlue(c)), int(std::lround(a * 255)));
            minX = std::min(minX, x), maxX = std::max(maxX, x);
            minY = std::min(minY, y), maxY = std::max(maxY, y);
        }
    }
    out = out.copy(QRect(QPoint(minX - 2, minY - 2), QPoint(maxX + 2, maxY + 2)).intersected(out.rect()));
    if (!out.save(QString::fromLocal8Bit(argv[2]))) {
        std::fprintf(stderr, "cannot write %s\n", argv[2]);
        return 1;
    }
    std::printf("%s: %dx%d\n", argv[2], out.width(), out.height());

    if (argc > 3) {
        QImage preview(out.size() + QSize(80, 80), QImage::Format_ARGB32);
        preview.fill(QColor(0x14, 0x1b, 0x26));
        QPainter p(&preview);
        p.drawImage(40, 40, out);
        p.end();
        preview.save(QString::fromLocal8Bit(argv[3]));
    }
    return 0;
}
