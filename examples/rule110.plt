var std: object = @import("std");

func rule110(left: int, center: int, right: int) int {
    // encode neighborhood as 3-bit number
    // idx = left*4 + center*2 + right
    var idx: int = left * 4 + center * 2 + right;

    // Rule 110 outputs 1 for:
    // 110 (6), 101 (5), 011 (3), 010 (2), 001 (1)
    if (idx == 6) {return 1;}
    if (idx == 5) {return 1;}
    if (idx == 3) {return 1;}
    if (idx == 2) {return 1;}
    if (idx == 1) {return 1;}

    return 0;
}

func printCell(i:int) {
  if(i==0) {
   std.print(" ");
  }else {
   std.print(1);
  }
}

func main() {
    var size: int = 30;
    var steps: int = 30;

    var current: int[20];
    var next: int[20];

    var i: int = 0;

    // initialize all cells to 0
    while (i < size) {
        current[i] = 0;
        i = i + 1;
    }

    // single 1 at the top right
    current[29] = 1;

    var step: int = 0;

    while (step < steps) {

        // print current generation
        i = 0;
        while (i < size) {
          if(current[i]==0) {
            std.print(" ");
          }else {
            std.print(1);
          }
          i = i + 1;
        }
        std.print("\n");

        // compute next generation
        i = 0;
        while (i < size) {
            var left: int = 0;
            var right: int = 0;

            if (i > 0) {
                left = current[i - 1];
            }

            if (i < size - 1) {
                right = current[i + 1];
            }

            next[i] = rule110(left, current[i], right);
            i = i + 1;
        }

        // copy next -> current
        i = 0;
        while (i < size) {
            current[i] = next[i];
            i = i + 1;
        }

        step = step + 1;
    }

    return 0;
}

main();
