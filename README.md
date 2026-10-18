<h1 align="center">ghost-free-hum</h1>

<p align="center">A native PC port of Luigi's Mansion.</p>

<p align="center">
  <img alt="Language: C++17" src="https://img.shields.io/badge/Language-C%2B%2B17-00599C?style=flat-square&amp;logo=cplusplus&amp;logoColor=white">
  <img alt="Progress: Early development" src="https://img.shields.io/badge/Progress-Early%20development-e0a526?style=flat-square">
  <img alt="CPU: x86-64" src="https://img.shields.io/badge/CPU-x86--64-7755aa?style=flat-square">
  <img alt="OS: Windows" src="https://img.shields.io/badge/OS-Windows-0078D4?style=flat-square">
  <a href="#license"><img alt="License: Not specified" src="https://img.shields.io/badge/License-Not%20specified-lightgrey?style=flat-square"></a>
</p>

## About

I'm working on bringing the original GameCube Luigi's Mansion to PC. The goal is to keep the game looking and playing like the original, with configurable improvements coming later.

The port is written in C++ and currently runs on Windows using Direct3D 11. It uses assets imported from your own copy of the game. No game files are included.

## Progress

This is still an early work in progress, not a complete playable game.

- **Up and running:** boot, title screen, file selection, the opening sequence, and movement through the Foyer, Balcony, and Parlor.
- **Opening events:** the Parlor cutscene, E. Gadd's lab introduction and cameras, and the transition into training.

There is still a lot of the game left to port.

## Getting started

Things you'll need:
- Your own USA revision 0 ISO
- Device with Windows, x64
- ImGui files for building the project, not included currently with this project

Build the `ghost-free-hum` target in Visual Studio using **x64 Release**, with the C++ desktop development and CMake tools installed. Launch `ghost-free-hum.exe` and select your ISO when prompted. The first launch imports the assets; later launches use the imported package.

Some quick debugging keybinds:
- F1 will restart the game
- F2 will display some basic stats
- F3 + 1, 2, or 3 will skip ahead to certain parts of the game

## Future Goals

- **Multiple OS support:** Ideally, I would like this project to expand its support to Linux, Steam OS (Steam Deck), and eventually the switch (Horizon OS) but I lack some of the hardware to get this possible by myself
- **Multiple ISO support:** Currently, the project is based entirely on USA revision 0 ISO. I would really like to expand that support.

## AI assistance

I've used AI to help trace functions in Ghidra and work through how the original game behaves. Without this tool, this project wouldn't be possible for me. It's been a big part of understanding the game and bringing those functions into the port. 

I understand if this turns you away from this project.

