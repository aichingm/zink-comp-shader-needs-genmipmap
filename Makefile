.PHONY: default run-working run-failing run-hack
CC:=gcc
CC_FLAGS:=-std=gnu23 -pedantic -g -Wall -Wextra
CC_INCLS:=
CC_LIBS_DIR:=$$(pkg-config --libs glfw3 opengl glu glew)
CC_LIBS:=

CC_CMD:=$(CC) $(CC_FLAGS) $(CC_INCLS) $(CC_LIBS_DIR) $(CC_LIBS)

default: run-failing

run-working:
	$(CC_CMD) -DBIND main.c -o main
	MESA_LOADER_DRIVER_OVERRIDE=zink ./main

run-failing:
	$(CC_CMD) -DBINDLESS main.c -o main
	MESA_LOADER_DRIVER_OVERRIDE=zink ./main

run-hack:
	$(CC_CMD) -DBINDLESS -DHACK main.c -o main
	MESA_LOADER_DRIVER_OVERRIDE=zink ./main

