#include "app/views/SongLayout.h"

#include <algorithm>
#include <cmath>

void SongLayout::layout (const std::vector<SongSpan>& spans, float width, float minWidthFloor, int rowLimit)
{
    blocks.clear();
    rows = 1;
    totalBars = 0;

    // all repeats of one part form one block
    size_t i = 0;
    while (i < spans.size())
    {
        Block b;
        b.part = spans[i].part;
        b.colour = spans[i].colour;
        b.shape = spans[i].shape;
        b.firstSpan = i;
        b.firstBar = spans[i].firstBar;
        while (i < spans.size() && spans[i].part == b.part)
            b.bars += spans[i++].numBars;
        b.endSpan = i;
        totalBars += b.bars;
        blocks.push_back (b);
    }
    if (blocks.empty() || totalBars == 0 || width <= 0.0f)
        return;

    // readable names first: a large minimum width, more rows; then narrower blocks; 0 = scale only
    for (float minWidth : { 96.0f, 72.0f, 52.0f, 36.0f, 0.0f })
    {
        if (minWidth < minWidthFloor)
            minWidth = minWidthFloor;
        for (int target = 1; target <= rowLimit; ++target)
        {
            if (pack (minWidth, 0.0f, width) > target)
                continue;   // even the minimum widths need more rows

            // largest scale (pixels per bar) that still fits into 'target' rows
            float lo = 0.0f, hi = (float) target * width / (float) totalBars;
            for (int k = 0; k < 18; ++k)
            {
                const float mid = (lo + hi) * 0.5f;
                if (pack (minWidth, mid, width) <= target)
                    lo = mid;
                else
                    hi = mid;
            }
            rows = pack (minWidth, lo, width);
            return;
        }
    }
    // more blocks than fit within the row limit: accept extra rows at the floor width
    rows = pack (minWidthFloor, 0.0f, width);
}

int SongLayout::pack (float minWidth, float scale, float width)
{
    int row = 0;
    float x = 0.0f;
    for (auto& b : blocks)
    {
        float w = std::max ({ 1.0f, minWidth, (float) b.bars * scale });
        if (x > 0.0f && x + w > width + 0.01f)
        {
            ++row;
            x = 0.0f;
        }
        w = std::min (w, width);
        b.row = row;
        b.x = x;
        b.width = w;
        x += w + gap;
    }
    return row + 1;
}

const SongLayout::Block* SongLayout::blockAt (double bar) const
{
    for (auto& b : blocks)
        if (b.contains (bar))
            return &b;
    return nullptr;
}

const SongLayout::Block* SongLayout::blockAt (float x, int row) const
{
    const Block* best = nullptr;
    float bestDist = 0.0f;
    for (auto& b : blocks)
    {
        if (b.row != row)
            continue;
        const float dist = x < b.x ? b.x - x : x > b.x + b.width ? x - (b.x + b.width) : 0.0f;
        if (best == nullptr || dist < bestDist)
        {
            best = &b;
            bestDist = dist;
        }
    }
    return best;
}
