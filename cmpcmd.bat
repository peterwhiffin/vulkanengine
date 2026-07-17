@echo off
REM cmake -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -S . -B build/win/
cmake -G "Ninja" -D CMAKE_C_COMPILER=clang -S . -B build/win/
