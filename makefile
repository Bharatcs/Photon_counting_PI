CC = g++
CFLAGS = -O2 -funroll-loops
SYS = -march=armv8-a -mfloat-abi=hard -mfpu=neon-fp-armv8 


all:main.cpp
	$(CC) -g -o main.o main.cpp  `pkg-config --cflags --libs opencv4` -lpthread -lwiringPi $(CFLAGS) $(SYS)
clean:
	rm main.o -rf

run: main.o
	raspividyuv  -w 1280 -h 720 -ex fixedfps -ISO 800 -fps 30 -ss 33333 -ag 12.0 --luma -t 0 -n -o - | /home/pi/main.o
