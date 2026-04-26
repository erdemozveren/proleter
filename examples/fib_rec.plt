var std: object = @import("std");

func fib_rec(n: int) int {
    if (n <= 1) {
        return n;
    }
    return fib_rec(n - 1) + fib_rec(n - 2);
}

func main() {
    var x: int = 10;
    var res: int = fib_rec(x);
    std.print("fib(" + x + ") = " + res);
}
