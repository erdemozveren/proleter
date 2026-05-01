// This is a test module
// Only exported variables can be accessed from outside
// check test_module_import.plt
var std:object = @import("std");
var math:object = @import("math");
var i : int = math.randRange(1,100);

func print(str:string) {
  std.printf("print from test_module print,module local val is %d,  STR ARTG = %s\n",i,str);
}

export {
  "print":print
};
