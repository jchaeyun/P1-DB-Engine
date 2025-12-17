//////
#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255
#define size_of_attribute(Struct, Attribute) sizeof(((Struct *)0)->Attribute) //(Struct*)0:null을 구조체 타입의 포인터로 강제 형변환(0번지에 가상의 구조체 만듦)
// 구조체의 인스턴스(실제 변수)를 생성하지 않고, 구조체 내 특정 멤버 변수의 자료형 크기(byte)를 알아낼 때 사용하는 C/C++의 아주 유명한 **관용적 표현
#define TABLE_MAX_PAGES 100

// 사이즈,오프셋,열/페이지 사이즈/페이지 당 열 개수/테이블 최대 열 개수
#define ID_SIZE size_of_attribute(Row, id)
#define USERNAME_SIZE size_of_attribute(Row, username)
#define EMAIL_SIZE size_of_attribute(Row, email)

#define ID_OFFSET 0
#define USERNAME_OFFSET (ID_OFFSET + ID_SIZE)
#define EMAIL_OFFSET (USERNAME_OFFSET + USERNAME_SIZE)

#define ROW_SIZE (ID_SIZE + USERNAME_SIZE + EMAIL_SIZE)
#define PAGE_SIZE 4096

#define ROW_PER_PAGE (PAGE_SIZE / ROW_SIZE)
#define TABLE_MAX_ROWS (ROW_PER_PAGE * TABLE_MAX_PAGES)

#include <stdint.h> //uint32_t(부호 없는 32비트 정수,컴퓨터마다 int 크기가 달라서 이렇게 씀)를 쓰기 위함
//////

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

// enum과 statement
typedef enum
{
    META_COMMAND_SUCCESS,
    META_UNRECOGNIZED_COMMAND
} MetaCommand;

typedef enum
{
    PREPARE_SUCCESS,
    PREPARE_SYNTAX_ERROR,
    PREPARE_UNRECOGNIZED

} PrepareResult;

typedef enum
{
    STATEMENT_SELECT,
    STATEMENT_INSERT
} StatementType;
////////////////
typedef enum
{
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL
} ExecuteResult;

typedef struct
{
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE];
    char email[COLUMN_EMAIL_SIZE];
} Row;

typedef struct
{
    StatementType type;
    Row row_to_insert; //->insert 명령어일 때만 씀
} Statement;

typedef struct
{
    uint32_t num_rows;            // 데이터 몇번째 줄까지 썼나 위치 표시
    void *pages[TABLE_MAX_PAGES]; // 한 테이블에 페이지 백개
} Table;

/////////////

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
// 어떤 메타명령인지 확인
MetaCommand do_meta_command(InputBuffer *input_buffer)
{
    if (strcmp(input_buffer->buffer, ".exit") == 0)
    { //.exit 명령일때 버퍼 메모리 회수하고 종료
        close_buffer(input_buffer);
        exit(EXIT_SUCCESS);
    }
    else
    { //.exit이 아닌 명령일때(일단 모르는 명령일 경우만 넣음)
        return META_UNRECOGNIZED_COMMAND;
    }
}

// Tokenizer
PrepareResult prepare_statement(InputBuffer *input_buffer, Statement *statement)
{
    // insert인 경우
    if (strncmp(input_buffer->buffer, "insert", 6) == 0)
    {
        statement->type = STATEMENT_INSERT;
        int args_assigned = sscanf(input_buffer->buffer, "insert %d %s %s", &(statement->row_to_insert.id), statement->row_to_insert.username, statement->row_to_insert.email);
        if (args_assigned < 3)
        {
            return PREPARE_SYNTAX_ERROR;
        }
        return PREPARE_SUCCESS;
    }
    // select인 경우
    if (strcmp(input_buffer->buffer, "select") == 0)
    {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }

    return PREPARE_UNRECOGNIZED;
}
// 특정 행에 대해 메모리에서 어디를 읽고 써야하는지 알아냄(페이지+바이트 수 반환)
void *slot_row(Table *table, uint32_t num_rows)
{
    uint32_t page_num = num_rows / ROW_PER_PAGE; // 어느 페이지에 써야할지
    void *page = table->pages[page_num];         // 해당 페이지를 가리키는 포인터 선언
    if (page == NULL)                            // 해당 페이지가 할당된 공간이 없다면
    {
        page = table->pages[page_num] = malloc(PAGE_SIZE); // 한 페이지 크기만큼 동적 할당
    }

    uint32_t row_num = num_rows % ROW_PER_PAGE; // 해당 페이지에서 줄 번호
    uint32_t byte_num = row_num * ROW_SIZE;     // 줄 번호->바이트 변환

    return (char *)page + byte_num; // 페이지 주소+바이트 주소 반환
}

/////////row 프린트 함수
void print_row(Row *row)
{
    printf("%d %s %s\n", row->id, row->username, row->email);
}
// 직렬화(압축)
// memcpy(받을 주소,복사할 원본 주소,복사할 크기)
void serialize_row(void *destination, Row *source)
{

    memcpy((char *)destination + ID_OFFSET, &(source->id), ID_SIZE); // row에 있던 id를 저 주소에 복사
    memcpy((char *)destination + USERNAME_OFFSET, source->username, USERNAME_SIZE);
    memcpy((char *)destination + EMAIL_OFFSET, source->email, EMAIL_SIZE);
}
// 역직렬화(압해)

// source:페이지 메모리(직렬화된 데이터가 들어있다)
// destination:Row 구조체.역직렬화할 곳
void deserialize_row(Row *destination, void *source)
{
    memcpy(&(destination->id), (char *)source + ID_OFFSET, ID_SIZE); // 해당 주소에 있던 것을 저 row 구조체의 id로 복사
    memcpy(destination->username, (char *)source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(destination->email, (char *)source + EMAIL_OFFSET, EMAIL_SIZE);
}

// Executor 어떤 명령인지 인자로 받음(구조체)
//  insert 실행 함수
ExecuteResult execute_insert(Statement *statement, Table *table)
{
    if (table->num_rows >= TABLE_MAX_ROWS)
    {
        return EXECUTE_TABLE_FULL;
    }

    Row *row_to_insert = &(statement->row_to_insert);               // 테이블에 복사할 Row 구조체
    serialize_row(slot_row(table, table->num_rows), row_to_insert); // 해당 주소에 있는 것들을 serialize 후 table로 복사하기
    table->num_rows++;                                              // 데이터 한줄 추가
    return EXECUTE_SUCCESS;                                         // 실행 성공!
}
// select 실행함수(풀스캔)
ExecuteResult execute_select(Statement *statement, Table *table)
{
    Row row;                                       // 여기다 읽어온 데이터 저장할것임
    for (uint32_t i = 0; i < table->num_rows; i++) // 한 줄 씩 읽어와서 row에 넣고 출력
    {
        deserialize_row(&row, slot_row(table, i)); // row에다 table i번째 줄의 데이터 복사
        print_row(&row);                           // row 출력
    }

    return EXECUTE_SUCCESS;
}
// 실행함수
ExecuteResult execute_statement(Statement *statement, Table *table)
{

    switch (statement->type)
    {
    case (STATEMENT_INSERT):
        execute_insert(statement, table);
        break;
    case (STATEMENT_SELECT):
        execute_select(statement, table);
        break;
    }

    return EXECUTE_SUCCESS;
}

// 테이블 생성 함수
Table *new_table()
{
    Table *table = (Table *)malloc(sizeof(Table));
    table->num_rows = 0;

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    {
        table->pages[i] = NULL;
    }

    return table;
}
// 테이블 삭제 함수
/////////////////////////////////
void close_table(Table *table)
{
    for (uint32_t i = 0; table->pages[i]; i++) // for (int i = 0; table->pages[i] != NULL; i++)
    {
        free(table->pages[i]); // 테이블 안 페이지들 먼저 free
    }
    free(table); // 그다음 테이블 free
}
int main(int args, char *argv[])
{
    //////////////테이블 생성
    Table *table = new_table();
    ////////////////
    // 버퍼 생성
    InputBuffer *input_buffer = new_input_buffer();

    while (true)
    {
        print_prompt();
        read_input(input_buffer);

        // 메타 명령인지 그냥 명령인지
        if (input_buffer->buffer[0] == '.')
        { // 버퍼에 들어온게 .으로 시작하는 메타명령일 경우
            switch (do_meta_command(input_buffer))
            {                            // 어떤 메타 명령인지 확인
            case (META_COMMAND_SUCCESS): // 아는 메타 명령일 경우
                continue;
            case (META_UNRECOGNIZED_COMMAND): // 모르는 메타 명령일 경우
                printf("unrecognized meta command '%s'\n", input_buffer->buffer);
                continue;
            }
        }
        // 메타 명령이 아닌 명령어일때
        Statement statement;
        switch (prepare_statement(input_buffer, &statement)) // 어떤 명령어인지 확인
        {
        case (PREPARE_SUCCESS): // 아는 명령어일때
            break;              // switch문 탈출(가장 가까운 반복문이나 switch문 탈출)
        ////////////////////////////
        case (PREPARE_SYNTAX_ERROR): // 문법 오류일때
            printf("Syntax error\n");
            continue;
        ////////////////////////////
        case (PREPARE_UNRECOGNIZED): // 모르는 명령어일때
            printf("unrecognized '%s'\n", input_buffer->buffer);
            continue; // while문 처음으로 돌아감(switch문에는 continue가 없음. for,while문에만 있다)
        }

        ////////////////////////////////
        switch (execute_statement(&statement, table))
        {
        case (EXECUTE_SUCCESS):
            printf("Executed.\n");
            break;
        case (EXECUTE_TABLE_FULL):
            printf("Error:Table full.\n");
            break;
        }
        ///////////////////////////////
    }
    ////////////////////////////////
    return 0;
}