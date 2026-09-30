#include "../src/lib/matrix_lerp.cpp"
#include <cassert>
#include <kf/platform/prelude.h>

int main()
{
    MATRIX from{}, to{};
    KfRenderState view{};
    for (std::size_t row = 0; row < from.m.size(); ++row)
        for (std::size_t column = 0; column < from.m[row].size(); ++column) {
            from.m[row][column] = -1000 + 100 * static_cast<int>(row) + static_cast<int>(column);
            to.m[row][column] = 2000 - 100 * static_cast<int>(row) - static_cast<int>(column);
        }
    for (s32 blend : {0, 1024, 2048, 4096}) {
        auto &output = view.lighting.color_matrix;
        output.t[0] = 11;
        output.t[1] = 22;
        output.t[2] = 33;
        lighting_set_color_matrix(view, &from, &to, blend);
        for (std::size_t row = 0; row < from.m.size(); ++row)
            for (std::size_t column = 0; column < from.m[row].size(); ++column)
                assert(output.m[row][column] ==
                       from.m[row][column] +
                           (((to.m[row][column] - from.m[row][column]) * blend) >> 12));
        assert(output.t[0] == 11 && output.t[1] == 22 && output.t[2] == 33);
        // Updating the active matrix in place must support aliased endpoints.
        const auto expected = output.m;
        output.m = from.m;
        lighting_set_color_matrix(view, &output, &to, blend);
        assert(output.m == expected);
        assert(output.t[0] == 11 && output.t[1] == 22 && output.t[2] == 33);
        output.m = to.m;
        lighting_set_color_matrix(view, &from, &output, blend);
        assert(output.m == expected);
        assert(output.t[0] == 11 && output.t[1] == 22 && output.t[2] == 33);
    }
}
