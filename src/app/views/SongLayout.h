#pragma once

#include "app/Song.h"

#include <vector>

// Lays out the parts of a song as proportional blocks in up to maxRows rows, wrapped like text.
// Every block keeps a minimum width so its name stays readable. When even three rows are not
// enough, the minimum shrinks step by step and finally everything is scaled down to fit
// (only with several hundred parts would more than three rows be needed).
// Used by the song strip in the main window and by the arranger's sequence view.
struct SongLayout
{
    struct Block
    {
        int part = 0;
        int colour = 0;                      // SongSpan::colour (same source, same colour)
        int shape = 0;                       // SongSpan::shape
        size_t firstSpan = 0, endSpan = 0;   // spans [firstSpan, endSpan): all repeats of the part
        int firstBar = 0, bars = 0;
        int row = 0;
        float x = 0.0f, width = 0.0f;        // pixels within the row

        float xOf (double bar) const    { return x + (float) ((bar - firstBar) / (double) bars) * width; }
        double barAt (float px) const   { return firstBar + (double) ((px - x) / width) * bars; }
        bool contains (double bar) const { return bar >= firstBar && bar < firstBar + bars; }
    };

    static constexpr int maxRows = 3;
    static constexpr float gap = 3.0f;   // between blocks

    std::vector<Block> blocks;
    int rows = 1;
    int totalBars = 0;

    // minWidthFloor: never narrower than this (the caller scrolls instead); rowLimit: rows allowed
    void layout (const std::vector<SongSpan>& spans, float width, float minWidthFloor = 0.0f, int rowLimit = maxRows);

    const Block* blockAt (double bar) const;             // nullptr if outside
    const Block* blockAt (float x, int row) const;       // nearest block in the row, nullptr if the row is empty

private:
    int pack (float minWidth, float scale, float width);   // assigns rows/x/width, returns the rows used
};
