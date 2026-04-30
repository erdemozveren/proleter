var std : object = @import("std");
var process : object = @import("process");
func main() {
  std.cclear();
  std.println("This is a echo repl that just echos back what you write\nType exit or press Ctrl+c to exit");
  var str:string;
  while(true){
      std.print("> ");
      str = std.readLine();
      if(str == "exit"){
          std.println("Goodbye!");
          process.exit();
        }
      std.print(str+"\n");
    }
}
