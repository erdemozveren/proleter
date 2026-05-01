var std:object = @import("std");
var i1:object = @import("./test_module.plt");
var i2:object = @import("./test_module.plt");

// Modules only evaulated once, local random int variable in module's print should be same
i1.print("hey 1");
i2.print("hey 2");
