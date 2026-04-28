var std = @import("std");

func a(sayi:int) {
std.println("inside a");
}

func b(callback:int) {
    callback(123);
}

func main() {
b(a);
}
