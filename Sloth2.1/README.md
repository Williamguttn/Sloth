
![_f558e1c0-3a09-4c3c-aca2-7f556df01d27](https://github.com/Williamguttn/Sloth/assets/69588728/c6ff4527-da60-43b6-8835-7650e972bd78)
[Generated with DALL-E 3]

# Overview
Sloth is a decent UCI chess engine made with C++, although significant parts of its source code are written in a C stylish code.

Sloth is a beginner project, and can therefore not be compared/matched against other powerful engines like Stockfish. It currently uses Hand Crafted 
Evaluation, but there are plans to implement NNUE evaluation in the future

# Rating
Sloth has not received a CCRL rating yet, however, it does play online every once in a while. You can check out its Lichess account here:
[SlothEngine on Lichess](https://lichess.org/@/SlothEngine)

# Build Instructions
Clang is recommended. For windows, you can run the ```build_windows.bat``` file found in ```/build```. For Linux, you can run:
```
chmod +x build_linux.sh
./build_linux.sh
```
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
**jimablett** & **tissatussa** - Porting the code and testing the engine