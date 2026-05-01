var std: object = @import("std");

func lifeRule(center: int, neighbors: int) int {
    // Standard Conway Life: B3/S23
    if (center == 1) {
        if (neighbors == 2) { return 1; }
        if (neighbors == 3) { return 1; }
        return 0;
    }
    if (neighbors == 3) { return 1; }
    return 0;
}

func main() {
    var width: int = 10;
    var height: int = 10;

    // 2D grid
    var grid: int[10][10] = [
        [0,0,0,0,0,0,0,0,0,0],
        [0,0,1,0,0,0,0,0,0,0],
        [0,0,0,1,0,0,0,0,0,0],
        [0,1,1,1,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0,0,0],

        [0,0,0,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0,0,0],
        [0,0,0,0,0,0,0,0,0,0]
    ];

    var next: int[10][10];

    while (true) {

        std.cclear();

        // ---- render ----
        var y: int = 0;
        while (y < height) {
            var x: int = 0;
            while (x < width) {
                if (grid[y][x] == 1) {
                    std.print("#");
                } else {
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

                            // toroidal wrap
                            if (nx < 0) { nx = nx + width; }
                            if (nx >= width) { nx = nx - width; }

                            if (ny < 0) { ny = ny + height; }
                            if (ny >= height) { ny = ny - height; }

                            neighbors = neighbors + grid[ny][nx];
                        }

                        dx = dx + 1;
                    }
                    dy = dy + 1;
                }

                next[y][x2] = lifeRule(grid[y][x2], neighbors);

                x2 = x2 + 1;
            }
            y = y + 1;
        }

        // ---- copy next -> grid ----
        y = 0;
        while (y < height) {
            var x3: int = 0;
            while (x3 < width) {
                grid[y][x3] = next[y][x3];
                x3 = x3 + 1;
            }
            y = y + 1;
        }

        std.sleep(120);
    }

    return 0;
}

main();
