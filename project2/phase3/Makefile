# 컴파일러 및 플래그 설정
CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = myshell

# 의존 파일
OBJS = myshell.o csapp.o

# 기본 타겟
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) -lpthread

myshell.o: myshell.c csapp.h
	$(CC) $(CFLAGS) -c myshell.c

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

# clean 명령 - 문제 수정
clean:
	-killall -9 $(TARGET) 2>/dev/null || true
	rm -f *.o $(TARGET) .nfs*