var std = @import("std");

func main() {
  var obj:object = {
    "a": 1,
    "b": [1, 2, 3],
    "c": { "nested": true }
  };
  std.printf("ob.a = %d\nobj.b = %d\nobj.c.nested = %d",obj.a,obj.b[1],obj.c.nested);
}
