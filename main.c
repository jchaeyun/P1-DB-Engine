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

#include <sys/stat.h>
// S_IRUSR: Stat Is Read USR (소유자만 읽기 권한 - Read)
// S_IWUSR: Stat Is Write USR (소유자만 쓰기 권한 - Write)
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
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

typedef enum
{
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL
} ExecuteResult;

typedef struct
{
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE + 1]; // C문자열은 null문자로 끝나야해서 1바이트 더 할당
    char email[COLUMN_EMAIL_SIZE + 1];
} Row;

typedef struct
{
    StatementType type;
    Row row_to_insert; //->insert 명령어일 때만 씀
} Statement;

//////////////////////
typedef struct
{
    int file_descriptor;          // 페이저가 읽어온 파일의 디스크립터
    uint32_t file_length;         // 페이저가 읽어온 파일의 길이
    void *pages[TABLE_MAX_PAGES]; // 캐시. 이미 읽은 페이지는 또 읽지 않고 메모리에서 바로 줌
} Pager;

typedef struct
{
    uint32_t num_rows; // 데이터 몇번째 줄까지 썼나 위치 표시
    Pager *pager;      // pager 구조체. 테이블이 몇번째 데이터를 달라는 요청 받으면 페이저에게 시킴
} Table;

typedef struct
{
    Table *table;
    uint32_t row_num; // 현재 위치한 줄
    bool end_of_table;
} Cursor; // table 내 위치를 나타내는 객체
/////////////////////

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
// 메모리에 올라와 있는 페이지를 디스크에 저장(flush)
void pager_flush(Pager *pager, uint16_t page_num, uint32_t size)
{
    if (pager->pages[page_num] == NULL) // 해당 페이지가 null이면
    {
        printf("tried to flush null page\n");
        exit(EXIT_FAILURE);
    }

    // pwrite은 실패하면 -1를 반환,성공 시 실제로 쓴 바이트 수 반환
    // 파일의 page_num * PAGE_SIZE 위치에 해당 페이지의 내용을 size만큼 적음
    // 항상 페이지 단위로 쓰기 때문에 한 페이지를 수정본(원래+수정한 데이터 포함)으로 덮어씀
    ssize_t bytes_written = pwrite(pager->file_descriptor, pager->pages[page_num], size, page_num * PAGE_SIZE);

    if (bytes_written == -1)
    {
        printf("Error seeking:%d \n", errno);
        exit(EXIT_FAILURE);
    }
}
// 1.페이지 캐시를 디스크에 저장 2.db 파일 닫음 3.pager,Table에 필요한 메모리 해제
void db_close(Table *table)
{
    Pager *pager = table->pager;
    uint32_t num_full_pages = table->num_rows / ROW_PER_PAGE; // 꽉 찬 페이지의 개수

    for (uint32_t i = 0; i < num_full_pages; i++)
    {
        if (pager->pages[i] == NULL)
        { // 아무것도 안 써있으면 다음 페이지로 넘어감
            continue;
        }
        pager_flush(pager, i, PAGE_SIZE); // 디스크에 저장
        free(pager->pages[i]);            // 해당 페이지 메모리 해제
        pager->pages[i] = NULL;           // 페이지 초기화(처음 상태로 되돌림)
    }

    // 남은 row 처리
    uint32_t num_additional_rows = table->num_rows % ROW_PER_PAGE;
    if (num_additional_rows > 0)
    {
        uint32_t page_num = num_full_pages; // 해당 페이지 번호
        if (pager->pages[page_num] != NULL)
        {                                                                 // 마지막 페이지가 비어있지 않다면
            pager_flush(pager, page_num, num_additional_rows * ROW_SIZE); // 디스크에 저장
            // 사실 원래는 페이지 단위로 데이터를 써야하는데 여기선 그냥 데이터 있는 만큼만 씀.
            // 1.파일 용량 줄이려고 2.읽을 때 get_page에서 부족한 부분은 0으로 채워주는 로직 있어서 안전

            free(pager->pages[page_num]);  // 페이지 메모리 해제
            pager->pages[page_num] = NULL; // 페이지 초기화
        }
    }

    int result = close(pager->file_descriptor); // 파일 연결 해제
    if (result == -1)
    {
        printf("Error closing db file.\n");
        exit(EXIT_FAILURE);
    }

    // 모든 페이지의 메모리 해제
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    {
        void *page = pager->pages[i];
        if (page)
        {
            free(page);
            pager->pages[i] = NULL;
        }
    }
    free(pager); // 페이저 메모리 해제
    free(table); // 테이블 메모리 해제
}

// 파일에서 읽어오기(페이저가 페이지 단위로)
void *get_page(Pager *pager, uint32_t page_num)
{
    if (page_num > TABLE_MAX_PAGES)
    { // 만약 페이지 번호가 최대 페이지보다 크다면
        printf("Tried to fetch page number out of bounds. %d>%d\n ", page_num, TABLE_MAX_PAGES);
        exit(EXIT_FAILURE);
    }

    // 해당 페이지가 비어있다면(캐시 미스!)
    if (pager->pages[page_num] == NULL)
    {
        void *page = malloc(PAGE_SIZE);                      // 캐시 미스났으니 새로 페이지 마련
        uint32_t num_pages = pager->file_length / PAGE_SIZE; // 현재 열린 파일이 가지고 있는 전체 쪽수 계산

        if (pager->file_length % PAGE_SIZE) // 만약 파일 길이가 한 페이지 사이즈보다 크면 한 페이지 더 갖고있음
        {
            num_pages += 1;
        }

        // 현재 파일이 가지고 있는 페이지 개수(db에는 페이지가 순서대로 저장된다고 가정)가 가져와야 하는 페이지 번호보다 많다면
        // 이전에 파일에 저장해둔 데이터이므로, '디스크'에서 데이터를 긁어와 메모리에 채움
        // 아니라면 if문 건너뜀(디스크를 안 읽음) 그냥 malloc으로 만든 깨끗한 페이지를 리턴.
        //->나중에 여기에 데이터 쓰고 flush(디스크에 저장)하면 파일 크기가 늘어남
        if (page_num <= num_pages) // 페이지 번호가 실제 파일 안에 존재하는 페이지냐?
        {
            // lseek과 read 대신 pread를 씀
            // pread(파일 디스크립터,버퍼,읽을 크기,"읽을 위치(offset)"옴
            ssize_t bytes_read = pread(pager->file_descriptor, page, PAGE_SIZE, page_num * PAGE_SIZE);
            // 파일에서 페이지 번호*페이지 크기(주소)부터, 한 페이지만큼 읽어서 page(버퍼)에 저장
            // 읽어들인 바이트 수 반환

            if (bytes_read == -1)
            { // 읽는거 실패 시
                printf("Error reading file: %d\n", errno);
                exit(EXIT_FAILURE);
            }

            // 파일 끝이라서 덜 읽혔을 때 (Partial Read)
            // 예: 페이지 크기는 4096인데, 파일 끝이라 100바이트만 읽힌 경우
            //  나머지 공간에는 쓰레기 값이 있을 수 있음.
            if (bytes_read < PAGE_SIZE)
            {
                // 읽고 남은 뒷부분을 0(NULL)으로 깔끔하게 초기화
                // page 포인터에서 bytes_read만큼 뒤로 간 위치부터 채움
                memset((char *)page + bytes_read, 0, PAGE_SIZE - bytes_read);
            }
        }
        pager->pages[page_num] = page;
    }

    // 캐시 히트면 바로 반환(캐시인 pages 배열에 있는거 바로 꺼냄)
    return pager->pages[page_num]; // 해당 페이지 반 pager에 있는 페이지에 저장환
}

// 버퍼 메모리 반환
void close_buffer(InputBuffer *input_buffer)
{
    free(input_buffer->buffer); // 버퍼 먼저 반환해야함
    free(input_buffer);         // 그 다음 구조체 반환!! 순서 중요. 거꾸로하면 구조체 공간 반환 시 버퍼를 반환할 주소를 잃어버려서 메모리 누수 일어남
}
// 어떤 메타명령인지 확인
MetaCommand do_meta_command(InputBuffer *input_buffer, Table *table)
{
    if (strcmp(input_buffer->buffer, ".exit") == 0)
    { //.exit 명령일때 버퍼 메모리 회수하고 종료
        close_buffer(input_buffer);
        db_close(table);
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
/////////////////////////////////////
// 특정 행에 대해 메모리에서 어디를 읽고 써야하는지 알아냄(페이지+바이트 수 반환)
void *cursor_value(Cursor *cursor)
{
    uint32_t row_num = cursor->row_num;
    uint32_t page_num = row_num / ROW_PER_PAGE; // 어느 페이지에 써야할지

    void *page = get_page(cursor->table->pager, page_num); // 페이저 구조체(db 파일과 연결됨)와 페이지 번호를 넘김
    uint32_t row_offset = row_num % ROW_PER_PAGE;          // 해당 페이지에서 줄 번호
    uint32_t byte_offset = row_offset * ROW_SIZE;          // 줄 번호->바이트 변환

    return (char *)page + byte_offset; // 페이지 주소+바이트 주소 반환
}

// 새 커서 생성
Cursor *table_start(Table *table)
{
    Cursor *cursor = (Cursor *)(sizeof(Cursor));
    cursor->table = table;
    cursor->row_num = 0;
    cursor->end_of_table = (table->num_rows == 0); // 이게뭐지;;

    return cursor;
}

// 커서 닫기...?
Cursor *table_end(Table *table)
{
    Cursor *cursor = (Cursor *)malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->row_num = table->num_rows;
    cursor->end_of_table = true;

    return cursor;
}

// 커서 이동시키기
void cursor_advance(Cursor *cursor)
{
    cursor->row_num += 1;
    if (cursor->row_num >= cursor->table->num_rows)
    {
        cursor->end_of_table = true;
    }
}

/////////////////////////////////
// row 프린트 함수
void print_row(Row *row)
{
    printf("%d %s %s\n", row->id, row->username, row->email);
}
// 직렬화(압축)
/*
데이터 직렬화:
왜 필요한가? C언어 구조체(struct)는 컴파일러마다 빈 공간(Padding)을 넣을 수 있어서,
구조체 그대로 파일에 쓰면 나중에 읽을 때 엉망이 될 수 있음.*/
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

    Row *row_to_insert = &(statement->row_to_insert); // 테이블에 복사할 Row 구조체
    Cursor *cursor = table_end(table);
    serialize_row(cursor_value(cursor), row_to_insert); // 해당 주소에 있는 것들을 serialize 후 table로 복사하기
    table->num_rows += 1;                               // 데이터 한줄 추가
    free(cursor);
    return EXECUTE_SUCCESS; // 실행 성공!
}
// select 실행함수(풀스캔)
ExecuteResult execute_select(Statement *statement, Table *table)
{
    Cursor *cursor = table_start(table);
    Row row; // 여기다 읽어온 데이터 저장할것임
    while (!(cursor->end_of_table))
    {
        deserialize_row(&row, cursor_value(cursor)); // row에다 table i번째 줄의 데이터 복사
        print_row(&row);
        cursor_advance(cursor); // row 출력
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

//////////////
// db파일 열고 페이저 할당하고 안에 있는거 초기화
Pager *pager_open(const char *filename)
{
    int fd = open(filename, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR); // 파일 열기
    if (fd == -1)
    { // 열기 실패 시
        printf("Unable to open file\n");
        exit(EXIT_FAILURE);
    }

    off_t file_length = lseek(fd, 0, SEEK_END); // 파일 크기 재는 용도. 이때 lseek은 ㄱㅊ

    Pager *pager = (Pager *)(sizeof(Pager)); // 페이저 동적할당
    pager->file_descriptor = fd;             // 페이저 구조체에 있는 파일 디스크립터,이제 페이저가 필요할 때마다 '디스크'에 있는 파일에 접근 가능(메모리에 올라온게 아님)
    pager->file_length = file_length;        // 페이저 구조체에 있는 파일 길이

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    { // pager 구조체에 있는 페이지들 하나씩 null로 초기화
        pager->pages[i] = NULL;
    }
    return pager;
}

// 1.db 파일 열기 2.페이저 데이터 구조 초기화 3.테이블 데이터 구조 초기화
Table *db_open(const char *filename)
{
    Pager *pager = pager_open(filename);               // 파일 열고 페이저에다 옮기는? 함수
    uint32_t num_rows = pager->file_length / ROW_SIZE; // 파일 크기를 보고 현재 몇 줄이 저장되어 있는지 계산해서 table 초기화시킴
    // 이전에 어디까지 저장했는지 알아야 새로운 데이터를 그 뒤에 이어서 쓸 수 있음. 그러나 아직 파일은 안 읽은 상태(진짜로 읽으면 느리니까)
    Table *table = (Table *)malloc(sizeof(Table)); // 테이블 생성
    table->pager = pager;                          // 파일 열고 만든 페이저를 테이블과 연결
    table->num_rows = num_rows;                    // row 개수도 대입

    return table;
}

///////////////////////

int main(int args, char *argv[])
{
    if (args < 2)
    {
        printf("Must supply a database filename\n");
        exit(EXIT_FAILURE);
    }
    char *filename = argv[1];
    Table *table = db_open(filename);

    // 버퍼 생성
    InputBuffer *input_buffer = new_input_buffer();

    while (true)
    {
        print_prompt();
        read_input(input_buffer);

        // 메타 명령인지 그냥 명령인지
        if (input_buffer->buffer[0] == '.')
        { // 버퍼에 들어온게 .으로 시작하는 메타명령일 경우
            switch (do_meta_command(input_buffer, table))
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