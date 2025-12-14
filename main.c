#include <stdio.h> //Standard input output. printf 함수나 getline 함수 들어있음
#include <string.h>
#include <stdlib.h>    //standard library. malloc, free 등이 들어있음
#include <stdbool.h>   //true/false 쓰기 위해 필요(c에는 원래 없음)
#include <sys/types.h> //ssize_t(signed size type)을 쓰기 위함

// 버퍼,버퍼 크기,인풋 크기
typedef struct
{
    char *buffer;
    size_t buffer_length;
    ssize_t input_length;

} InputBuffer;

// 버퍼 생성 함수
InputBuffer *new_input_buffer()
{
    // 버퍼 공간 동적할당
    InputBuffer *input_buffer = (InputBuffer *)malloc(sizeof(InputBuffer));

    // 초기화
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;

    // 만든 버퍼 구조체 반환
    return input_buffer;
}

// 실행 시 나오는 프롬프트
void print_prompt()
{
    printf("db>> ");
}

// 버퍼에 들어간 내용 읽기
void read_input(InputBuffer *input_buffer)
{
    // 버퍼에 들어간 바이트 수 알아냄
    // getline은 입력 크기가 크면 알아서 공간을 더 할당함. 그래서 버퍼의 주소를 저장한 곳의 주소와 할당해야하는 크기의 주소를 인자로 받음
    ssize_t read_bytes = getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);

    // 버퍼에 있는걸 못 읽었을때 강제 종료
    if (read_bytes <= 0)
    {
        printf("Error reading input\n");
        exit(EXIT_FAILURE);
    }

    input_buffer->buffer[read_bytes - 1] = 0;    // 버퍼 마지막 엔터를 0으로 덮어써서 문자열 끝 표시
    input_buffer->input_length = read_bytes - 1; // 인풋 길이에서 엔터를 제외
}

// 버퍼 메모리 반환
void close_buffer(InputBuffer *input_buffer)
{
    free(input_buffer->buffer); // 버퍼 먼저 반환해야함
    free(input_buffer);         // 그 다음 구조체 반환!! 순서 중요. 거꾸로하면 구조체 공간 반환 시 버퍼를 반환할 주소를 잃어버려서 메모리 누수 일어남
}

int main(int args, char *argv[])
{
    // 버퍼 생성
    InputBuffer *input_buffer = new_input_buffer();

    while (true)
    {
        print_prompt();
        read_input(input_buffer);

        // 입력값이 .exit이면 종료하고 버퍼 공간 회수
        if (strcmp(input_buffer->buffer, ".exit") == 0)
        {
            close_buffer(input_buffer);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("Undefined request '%s'\n", input_buffer->buffer);
        }
    }

    return 0;
}