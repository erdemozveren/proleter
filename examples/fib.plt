var std: object = @import("std");

func fib_iter(n: int) int {
  if (n <= 1) {
    return n;
  }

  var a: int = 0;
  var b: int = 1;
  var i: int = 2;

  while (i <= n) {
    var next: int = a + b;
    a = b;
    b = next;
    i = i + 1;
  }

  return b;
}

var x: int = 10;
var res: int = fib_iter(x);
std.print("fib(" + x + ") = " + res);
