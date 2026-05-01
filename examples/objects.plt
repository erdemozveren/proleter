var std = @import("std");

var obj:object = {
  "a": 1,
  "b": [1, 2, 3],
  "c": { "nested": true },
};
std.printf("ob.a = %d\nobj.b = %d\nobj.c.nested = %d\n",obj.a,obj.b[1],obj.c.nested);
var values: array = obj.values();
var keys  : array = obj.keys();
std.print("object has 'a' key = ",obj.has("a"));


