var std:object = @import("std");
var gc:object = @import("gc");

func show_usage() {
  var used: double = gc.allocated();
  var rss: double = gc.rss();
  std.printf("mem %f (mb)\nrss: %f (mb)\n",used/1024.0/1024.0,rss/1024.0/1024.0);
}

std.println("---Before Allocation---");
show_usage();
var a: int[6000000];
std.println("---After---");
show_usage();
