CC = gcc
CFLAGS = -Wall -O2 -I./include -I"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8\include"
LDFLAGS = -L"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8\lib\x64" -lOpenCL -lm

SRC = src/main.c src/image_io.c src/boundary.c src/gauss_template.c src/ocl_context.c src/mem_transfer.c src/entropy.c
OBJ = $(SRC:.c=.o)
EXEC = um_ocl

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	del /Q src\*.o $(EXEC).exe
