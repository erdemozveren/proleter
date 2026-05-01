var std: object = @import("std");
var math: object = @import("math");
var process: object = @import("process");

func drawBorder(width: int, height: int) {
  var x: int;
  var y: int;
  var line: string;

  std.ccolor(4, 4);

  line = "";
  x = 1;
  while (x <= width) {
    line += "#";
    x++;
  }

  std.cmove(1, 1);
  std.print(line);

  std.cmove(1, height);
  std.print(line);

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

func drawPipe(x: int, gapY: int, gapSize: int, height: int) {
  var y: int = 2;
  std.ccolor(2, 2);

  while (y < height) {
    if (!(y >= gapY && y < gapY + gapSize)) {
      std.cmove(x, y);
      std.print("|");
    }
    y++;
  }

  std.creset();
}

func drawGround(width: int, y: int) {
  var x: int = 2;
  std.ccolor(3, 3);

  while (x < width) {
    std.cmove(x, y);
    std.print("=");
    x++;
  }

  std.creset();
}

func gameOver(score: int, msg: string) {
  std.inputNonblocking(false);
  std.cclear();

  std.cmove(3, 3);
  std.println("Game Over!");
  std.cmove(3, 5);
  std.println(msg);
  std.cmove(3, 7);
  std.print("Score: ");
  std.println(score);
  std.cmove(3, 9);
  std.println("Press any key to exit...");
  process.exit();
  process.exit();
}

func main() {
  var width: int = 50;
  var height: int = 20;
  var groundY: int = height - 1;

  var keyQ: int = 113;
  var keySpace: int = 32;

  var scale: int = 100;

  var birdX: int = 12;
  var birdY: int = 1000;     // 10.00 cells
  var vel: int = 0;
  var gravity: int = 16;     // 0.18 cells/frame
  var flapPower: int = -75;  // -0.55 cells/frame
  var maxFall: int = 70;     // 0.70 cells/frame

  var pipeCount: int = 3;
  var pipeX: int[3] = [35, 52, 69];
  var pipeGapY: int[3] = [6, 8, 5];
  var pipeGapSize: int = 5;

  var score: int = 0;
  var started: int = 0;
  var showHelp: int = 1;

  std.inputRaw(1);
  std.inputNonblocking(1);

  while (true) {
    var key: int = std.getChar();

    if (key == keyQ) {
      std.input_restore();
      std.cclear();
      std.cmove(1, 1);
      process.exit();
      process.exit();
    }

    if (key == keySpace) {
      started = 1;
      showHelp = 0;
      vel = flapPower;
    }

    if (started) {
      vel = vel + gravity;
      if (vel > maxFall) {
        vel = maxFall;
      }
      birdY = birdY + vel;
    }

    var drawY: int = birdY / scale;

    var i: int = 0;
    while (i < pipeCount) {
      if (started) {
        pipeX[i] = pipeX[i] - 1;
      }

      if (pipeX[i] < 2) {
        pipeX[i] = width - 2;
        pipeGapY[i] = math.randRange(3, height - pipeGapSize - 3);
        score++;
      }

      i++;
    }

    if (drawY <= 1) {
      gameOver(score, "Bird hit the ceiling.");
    }

    if (drawY >= groundY) {
      gameOver(score, "Bird hit the ground.");
    }

    i = 0;
    while (i < pipeCount) {
      if (pipeX[i] == birdX) {
        if (!(drawY >= pipeGapY[i] && drawY < pipeGapY[i] + pipeGapSize)) {
          gameOver(score, "Bird hit a pipe.");
        }
      }
      i++;
    }

    std.cclear();
    drawBorder(width, height);
    drawGround(width, groundY);

    i = 0;
    while (i < pipeCount) {
      if (pipeX[i] > 1 && pipeX[i] < width) {
        drawPipe(pipeX[i], pipeGapY[i], pipeGapSize, groundY);
      }
      i++;
    }

    std.ccolor(1, 0);
    std.cmove(birdX, drawY);
    std.print("@");
    std.creset();

    std.cmove(3, 2);
    std.print("Score: ");
    std.print(score);

    if (showHelp) {
      std.cmove(3, 4);
      std.print("Flappy Bird");
      std.cmove(3, 5);
      std.print("SPACE = flap");
      std.cmove(3, 6);
      std.print("Q = quit");
    }

    std.sleep(50);
  }
}

main();
