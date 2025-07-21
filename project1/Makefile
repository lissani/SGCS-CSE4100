# 컴파일러 설정
CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -g

# 소스 파일 목록
SRCS = main.c list.c hash.c debug.c hex_dump.c bitmap.c
OBJS = $(SRCS:.c=.o)  # .c 파일을 .o 파일로 변환
TARGET = testlib       # 실행 파일 이름

# 기본 빌드 규칙
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# 개별 .c 파일을 컴파일하여 .o 파일 생성
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 실행 규칙
run: $(TARGET)
	./$(TARGET)

# 정리 규칙: 빌드된 파일 삭제
clean:
	rm -f $(OBJS) $(TARGET)
