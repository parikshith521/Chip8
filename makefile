all:
	g++ chip8.cpp -o chip8 `sdl2-config --cflags --libs`