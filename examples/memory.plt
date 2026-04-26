var std:object = @import("std");
var gc:object = @import("gc");

func show_usage() {
    var used: object = gc.allocated();
    var rss: object = gc.rss();
    std.printf("mem %f (mb)\nrss: %f (mb)\n",used/1024.0/1024.0,rss/1024.0/1024.0);
}

func main() {
    std.println("---Before Allocation---");
    show_usage();
    var a: int[6000000];
    std.println("---After---");
    show_usage();
    return 0;
}

