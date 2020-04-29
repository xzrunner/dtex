#pragma once

#include <limits>

#include <stdint.h>

namespace dtex
{

#ifndef IS_4TIMES
	#define IS_4TIMES(x) ((x) % 4 == 0)
#endif // IS_4TIMES

#ifndef IS_POT
	#define IS_POT(x) ((x) > 0 && ((x) & ((x) -1)) == 0)
#endif // IS_POT

struct Rect
{
    Rect() {
        MakeInvalid();
    }

	int16_t xmin, ymin;
	int16_t xmax, ymax;

    void MakeInvalid() {
        xmin = ymin = std::numeric_limits<int16_t>::max();
        xmax = ymax = std::numeric_limits<int16_t>::min();
    }

    bool IsValid() const {
        return xmin != std::numeric_limits<int16_t>::max()
            && ymin != std::numeric_limits<int16_t>::max()
            && xmax != std::numeric_limits<int16_t>::min()
            && ymax != std::numeric_limits<int16_t>::min();
    }
};

struct Quad
{
    Rect rect;
    bool rot = false;
};

}
