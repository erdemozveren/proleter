var std = @import("std");
var myarray: array = [1,2,3];
myarray.set(5,555);
myarray.push(99);
myarray[0] = 333;
std.println("len = ",myarray.len);
std.println("element at index 0 = ",myarray.get(0));
std.println("element at index 5 = ",myarray[5]);
