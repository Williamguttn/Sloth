
  

![_f558e1c0-3a09-4c3c-aca2-7f556df01d27](https://github.com/Williamguttn/Sloth/assets/69588728/c6ff4527-da60-43b6-8835-7650e972bd78)

[Generated with DALL-E 3]

  

# Overview

Sloth is a decent UCI chess engine written in C++, although significant parts of its source code are written in a C stylish code.

  

Sloth is a beginner project, and can therefore not be compared/matched against powerful engines like Stockfish. Up until version 2.1, it utilized hand-crafted evaluation. But now, it is equipped with a NNUE that was trained on a dataset of 250M selfplay positions.

  

# Rating

Sloth has not received a CCRL rating yet, however, it does play online every once in a while. You can check out its Lichess account here:

[SlothComputer on Lichess](https://lichess.org/@/SlothComputer)
  

# Build Instructions

Clang++ is recommended. For windows, you can run the ```build_windows.bat``` file found in ```/build```. For Linux, there is a ```Makefile``` in the root of the project.

To compile with the NNUE embedded, add `EVALFILE=/path/to/eval.nnue`

## Example usage:
Linux:
```bash
# Build all archs
make

# Build just one
make avx2

#  Embed NNUE
make EVALFILE=/path/to/eval.nnue
```
Windows:
```batch
REM Build all archs
./build_windows.bat

REM Build just one
./build_windows.bat avx2

REM  Embed NNUE
./build_windows.bat EVALFILE=/path/to/eval.nnue
```
### Note:
- Python is used to embed the NNUE

# ARM

I have not been able to test Sloth on ARM devices. Makefiles are still available in ```/build```.

  

# What is this project based on?

Sloth is based on the [BBC chess engine](https://www.chessprogramming.org/BBC) made by

[Maksym Korzh (Code Monkey King)](https://www.chessprogramming.org/Maksim_Korzh). He, alongside his engine, has brought me into a whole new world of

chess. And for that, I will always be grateful. Without his knowledge, none of this would be possible.

  

# Thanks to:

[Code Monkey King](https://www.youtube.com/@chessprogramming591) <br>

[Bluefever Software](https://www.youtube.com/channel/UCFkfibjxPzrP0e2WIa8aJCg) <br>

[Chess Programming Wiki](https://www.chessprogramming.org/Main_Page) <br>

[Bullet](https://github.com/jw1912/bullet) <br>

**jimablett** & **tissatussa** - Porting the code and testing the engine