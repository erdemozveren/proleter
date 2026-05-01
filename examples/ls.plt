var fs:object = @import("fs");
var std:object = @import("std");

var list:array = fs.readDir(".");
var folders: array = [];
var files: array = [];
for (var i:int = 0;i < list.len;i++) {
  var fp : string = list.get(i);
  if(fs.isDir(fp)){
    folders.push(fp);
  }else {
    files.push(fp);
  }
}

std.ccolor(2,0);
std.println("./");
for (var i:int = 0;i < folders.len;i++) {
  std.println(folders[i]);
}

std.ccolor(4,0);
for (var i:int = 0;i < files.len;i++) {
  std.println(files[i]);
}
