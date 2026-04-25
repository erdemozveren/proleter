var std:object = @import("std");
var process:object = @import("process");
func show_usage() {
    var m: object = process.memory_usage();
    std.printf("mem %f / %f (mb)\nrss: %f (mb)\n",m.used/1024/1024,m.total/1024/1024,m.rss/1024/1024);
}

func main() {
    std.println("---Before Allocation---");
    show_usage();
    var len:int = 30000000;
    var a: int[30000000];
    std.println("---After---");
    show_usage();
    return 0;
}

