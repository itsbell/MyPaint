/**
 * @file FillCommand.cpp
 * @brief FillCommand 구현 (스캔라인 플러드 필)
 */
#include "pch.h"
#include "Core/FillCommand.h"

#include <algorithm>

#include "Core/ByteStream.h"
#include "Core/Surface.h"

namespace
{
/** @brief 비교할 때 쓰지 않는 상위 바이트를 버립니다. (GDI가 남기는 값이 일정하지 않을 수 있음) */
constexpr Pixel kColorMask = 0x00FFFFFF;
} // namespace

std::vector<FillCommand::Span> FillCommand::FindSpans(const Surface& surface) const
{
    std::vector<Span> spans;
    const int w = surface.Width();
    const int h = surface.Height();
    if (m_seed.x < 0 || m_seed.y < 0 || m_seed.x >= w || m_seed.y >= h)
    {
        return spans;
    }

    const Pixel* bits = surface.Bits();
    const Pixel target = bits[static_cast<size_t>(m_seed.y) * w + m_seed.x] & kColorMask;
    if (target == (ToPixel(m_color) & kColorMask))
    {
        return spans; // 이미 같은 색이면 바뀌는 것이 없습니다.
    }

    std::vector<uint8_t> visited(static_cast<size_t>(w) * h, 0);
    auto matches = [&](int x, int y) {
        const size_t i = static_cast<size_t>(y) * w + x;
        return visited[i] == 0 && (bits[i] & kColorMask) == target;
    };

    std::vector<CPoint> stack;
    stack.push_back(m_seed);
    while (!stack.empty())
    {
        const CPoint p = stack.back();
        stack.pop_back();
        if (!matches(p.x, p.y))
        {
            continue;
        }

        // 이 행에서 왼쪽/오른쪽 끝까지 넓힙니다.
        int left = p.x;
        while (left > 0 && matches(left - 1, p.y))
        {
            --left;
        }
        int right = p.x;
        while (right + 1 < w && matches(right + 1, p.y))
        {
            ++right;
        }
        std::fill(visited.begin() + static_cast<ptrdiff_t>(p.y) * w + left,
                  visited.begin() + static_cast<ptrdiff_t>(p.y) * w + right + 1, uint8_t{1});
        spans.push_back({p.y, left, right + 1});

        // 위아래 행에서 이어지는 구간마다 시작점 하나씩 넣습니다.
        for (const int ny : {p.y - 1, p.y + 1})
        {
            if (ny < 0 || ny >= h)
            {
                continue;
            }
            bool inRun = false;
            for (int x = left; x <= right; ++x)
            {
                const bool m = matches(x, ny);
                if (m && !inRun)
                {
                    stack.emplace_back(x, ny);
                }
                inRun = m;
            }
        }
    }
    return spans;
}

CRect FillCommand::ComputeBounds(const Surface& surface) const
{
    const std::vector<Span> spans = FindSpans(surface);
    if (spans.empty())
    {
        return CRect();
    }
    CRect r(spans.front().x0, spans.front().y, spans.front().x1, spans.front().y + 1);
    for (const Span& s : spans)
    {
        r.left = (std::min)(r.left, static_cast<LONG>(s.x0));
        r.right = (std::max)(r.right, static_cast<LONG>(s.x1));
        r.top = (std::min)(r.top, static_cast<LONG>(s.y));
        r.bottom = (std::max)(r.bottom, static_cast<LONG>(s.y + 1));
    }
    return r;
}

void FillCommand::Execute(Surface& surface) const
{
    const std::vector<Span> spans = FindSpans(surface);
    if (spans.empty())
    {
        return;
    }
    Pixel* bits = surface.Bits();
    const Pixel value = ToPixel(m_color);
    for (const Span& s : spans)
    {
        Pixel* row = bits + static_cast<size_t>(s.y) * surface.Width();
        std::fill(row + s.x0, row + s.x1, value);
    }
}

void FillCommand::Write(ByteWriter& writer) const
{
    writer.Write(static_cast<int32_t>(m_seed.x));
    writer.Write(static_cast<int32_t>(m_seed.y));
    writer.Write(static_cast<uint32_t>(m_color));
}

std::unique_ptr<FillCommand> FillCommand::Read(ByteReader& reader)
{
    const auto x = reader.Read<int32_t>();
    const auto y = reader.Read<int32_t>();
    const auto color = reader.Read<uint32_t>();
    return std::make_unique<FillCommand>(CPoint(x, y), color & 0x00FFFFFF);
}
