var std: object = @import("std");

func main() {
  var x: int = 10;
  while (x > 0) {
      std.print(x+"\n");
      x--;
  }
  for(var i:int = 0;i<10;i++){
    std.print(i+"\n");
  }
  return 0;
}

