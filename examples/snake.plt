var std: object = @import("std");
var math: object = @import("math");
var process: object = @import("process");

func drawWalls(width: int, height: int) {
  var x: int;
  var y: int;
  var line: string;

  std.ccolor(4, 4);

  // top wall
  std.cmove(1, 1);
  line = "";
  x = 1;
  while (x <= width) {
    line += "#";
    x++;
  }
  std.print(line);

  // bottom wall
  std.cmove(1, height);
  std.print(line);

  // side walls
  y = 2;
  while (y < height) {
    std.cmove(1, y);
    std.print("#");

    std.cmove(width, y);
    std.print("#");

    y++;
  }

  std.creset();
}

func main() {
  std.inputRaw(true);
  std.inputNonblocking(true);
  var startMsgShowed:int = 0;
  var gridWidth: int = 60;
  var gridHeight: int = 25;

  // keys
  var keyQ: int = 113;
  var keyW: int = 119;
  var keyS: int = 115;
  var keyA: int = 97;
  var keyD: int = 100;

  // direction (dx, dy)
  var dx: int = 1;
  var dy: int = 0;

  // snake
  var length: int = 3;
  var sx: int[3] = [10, 9, 8];
  var sy: int[3] = [10, 10, 10];

  // food
  var fx: int = math.randRange(2,gridWidth-1);
  var fy: int = math.randRange(2,gridHeight-1);

  while (true) {
    // Little hack for to feel vertical movements more accurate
    std.cclear();
    drawWalls(gridWidth+1,gridHeight+1);
    // draw food
    std.ccolor(1, 0);
    std.cmove(fx, fy);
    std.print("@");
    var key : int = keyD;
    // draw snake
    var i: int = 0;
    while (i < length) {
      std.cmove(sx[i], sy[i]);
      if (i == 0) {
	std.ccolor(3, 3);
        std.print("O");
      } else {
	std.ccolor(6, 6);
        std.print("#");
      }
      i++;
    }
    std.creset();
    std.cmove(0,0);
    if(!startMsgShowed){
        startMsgShowed = 1;
        std.cmove(3,4);
        std.print("Classic snake game, snake only moves when a key is pressed");
        std.cmove(3,5);
        std.print("Snake only moves when a key is pressed, Q to exit");
        std.cmove(11,6);
        std.print("W");
        std.cmove(9,7);
        std.print("A S D");
    }
    // input
    var newKey: int = std.getChar();
    if(newKey != -1 || newKey != key){
        key = newKey;
    }

    if (key == keyQ) {
      std.cclear();
      std.cmove(0, 0);
      std.println("Game Over!");
      process.exit();
    } else if (key == keyW && dy != 1) {
      dx = 0; dy = -1;
    } else if (key == keyS && dy != -1) {
      dx = 0; dy = 1;
    } else if (key == keyA && dx != 1) {
      dx = -1; dy = 0;
    } else if (key == keyD && dx != -1) {
      dx = 1; dy = 0;
    }

    // move body
    i = length - 1;
    while (i > 0) {
      sx[i] = sx[i - 1];
      sy[i] = sy[i - 1];
      i--;
    }

    // move head
    sx[0] = sx[0] + dx;
    sy[0] = sy[0] + dy;

    // wall collision
    if (sx[0] <= 1 || sx[0] > gridWidth || sy[0] <= 1 || sy[0] > gridHeight) {
      std.cclear();
      std.cmove(1, 1);
      std.println("You hit a wall!");
      process.exit();
    }

    // self collision
    i = 1;
    while (i < length) {
      if (sx[0] == sx[i] && sy[0] == sy[i]) {
        std.cclear();
        std.cmove(1, 1);
        std.println("You ate yourself!");
        process.exit();
      }
      i++;
    }

    // eat food
    if (sx[0] == fx && sy[0] == fy) {
      length++;
      sx[length - 1] = sx[length - 2];
      sy[length - 1] = sy[length - 2];
      fx = math.randRange(2,gridWidth-1);
      fy = math.randRange(2,gridHeight-1);
    }
    if(dy!=0){
      std.sleep(70);
    }else {
      std.sleep(50);
    }

  }
}

main();
