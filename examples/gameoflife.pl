var std: object = @import("std");

func lifeRule(center: int, neighbors: int) int {
    // Standard Conway Life: B3/S23
    if (center == 1) {
        if (neighbors == 2) { return 1; }
        if (neighbors == 3) { return 1; }
        return 0;
    } 
    if (center != 1) {
        if (neighbors == 3) { return 1; }
        return 0;
    }
}

func idx(x: int, y: int, width: int) int {
    return y * width + x;
}

func main() {
    var width: int = 10;
    var height: int = 10;

    // 1D grid representing 10x10 board
    var grid: int[100] = [
        0,0,0,0,0,0,0,0,0,0,
        0,0,1,0,0,0,0,0,0,0,
        0,0,0,1,0,0,0,0,0,0,
        0,1,1,1,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,

        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0
    ];

    var next: int[100];

    var step: int = 0;

    while (true) {

        std.cclear();

        // ---- render ----
        var y: int = 0;
        while (y < height) {
            var x: int = 0;
            while (x < width) {
                if (grid[idx(x, y, width)] == 1) {
                    std.print("#");
                }
                if (grid[idx(x, y, width)] != 1) {
                    std.print(".");
                }
                x = x + 1;
            }
            std.print("\n");
            y = y + 1;
        }

        // ---- compute next generation (TOROIDAL) ----
        y = 0;
        while (y < height) {
            var x2: int = 0;
            while (x2 < width) {

                var neighbors: int = 0;
                var dy: int = -1;

                while (dy <= 1) {
                    var dx: int = -1;
                    while (dx <= 1) {

                        if (!(dx == 0 && dy == 0)) {

                            var nx: int = x2 + dx;
                            var ny: int = y + dy;

                            // --- TOROIDAL WRAP ---
                            if (nx < 0) {
                                nx = nx + width;
                            }
                            if (nx >= width) {
                                nx = nx - width;
                            }

                            if (ny < 0) {
                                ny = ny + height;
                            }
                            if (ny >= height) {
                                ny = ny - height;
                            }
                            // --------------------

                            neighbors = neighbors +
                                grid[idx(nx, ny, width)];
                        }

                        dx = dx + 1;
                    }
                    dy = dy + 1;
                }

                var i: int = idx(x2, y, width);
                next[i] = lifeRule(grid[i], neighbors);

                x2 = x2 + 1;
            }
            y = y + 1;
        }

        // ---- copy next -> grid ----
        var i2: int = 0;
        while (i2 < width * height) {
            grid[i2] = next[i2];
            i2 = i2 + 1;
        }

        std.sleep(120);
    }

    return 0;
}
