var fs:object = @import("fs");
var std:object = @import("std");

fs.writeFile("out.txt", "Hello");
fs.appendFile("out.txt", "\nWorld");
var text:string = fs.readFile("out.txt");

std.println(fs.exists("out.txt"));
std.println(fs.isFile("out.txt"));
std.println(fs.isDir("."));

var files:array = fs.readDir(".");
std.println(text);
std.println(files);

//fs.mkdir("test");
fs.remove("out.txt");
