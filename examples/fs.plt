var fs:object = @import("fs");
var std:object = @import("std");

func main() {
    fs.writeFile("out.txt", "Hello");
    fs.appendFile("out.txt", "\nWorld");
    //var text:string = fs.readFile("hello.txt");

    std.println(fs.exists("out.txt"));
    std.println(fs.isFile("out.txt"));
    std.println(fs.isDir("."));

    var files:array = fs.readDir(".");
    std.println(files);
    //fs.mkdir("test");
    fs.remove("out.txt");
    return 0;
}
