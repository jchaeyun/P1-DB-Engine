#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255
#define size_of_attribute(Struct, Attribute) sizeof(((Struct *)0)->Attribute) //(Struct*)0:null을 구조체 타입의 포인터로 강제 형변환(0번지에 가상의 구조체 만듦)
// 구조체의 인스턴스(실제 변수)를 생성하지 않고, 구조체 내 특정 멤버 변수의 자료형 크기(byte)를 알아낼 때 사용하는 C/C++의 아주 유명한 **관용적 표현
#define TABLE_MAX_PAGES 400

// 사이즈,오프셋,열/페이지 사이즈/페이지 당 열 개수/테이블 최대 열 개수
#define ID_SIZE size_of_attribute(Row, id)
#define USERNAME_SIZE size_of_attribute(Row, username)
#define EMAIL_SIZE size_of_attribute(Row, email)

#define ID_OFFSET 0
#define USERNAME_OFFSET (ID_OFFSET + ID_SIZE)
#define EMAIL_OFFSET (USERNAME_OFFSET + USERNAME_SIZE)

#define ROW_SIZE (ID_SIZE + USERNAME_SIZE + EMAIL_SIZE)
#define PAGE_SIZE 4096

/*b-트리 도입하면 이제 데이터베이스에서 행의 개수가 아닌 페이지 수를 저장하는 것이 더 합리적.
#define ROW_PER_PAGE (PAGE_SIZE / ROW_SIZE)
#define TABLE_MAX_ROWS (ROW_PER_PAGE * TABLE_MAX_PAGES)
*/

// 4kb 페이지 하나를 그냥 통으로 쓰는게 아니라 헤더와 바디로 나누어 씀

//  Common Node Header Layout
//  각 노드는 시작 부분 헤더에 자신의 노드 유형(중간 or 리프노드),루트 노드 여부,부모 노드를 가리키는 포인터(형제 노드 찾기 위해)를 저장함
#define NODE_TYPE_SIZE sizeof(uint8_t)                                                // 노드 유형 기록 크기
#define NODE_TYPE_OFFSET 0                                                            // 노드 유형 기록 위치
#define IS_ROOT_SIZE sizeof(uint8_t)                                                  // 루트 노드인지 크기
#define IS_ROOT_OFFSET NODE_TYPE_SIZE                                                 // 루트 노드인지 여부 기록 위치
#define PARENT_POINTER_SIZE sizeof(uint32_t)                                          // 부모 포인터 기록 크기
#define PARENT_POINTER_OFFSET (IS_ROOT_OFFSET + IS_ROOT_SIZE)                         // 부모 포인터 기록 위치
#define COMMON_NODE_HEADER_SIZE (NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE) // 전체 공통 헤더 크기

// Leaf Node Header Layout
// 리프 노드일 경우의 리프 노드 헤더
// 여기에 데이터(cell)가 몇 개 들어있는지 기록. 셀은 키/값 쌍임
// NEXT_LEAF(형제 노드) 포인터를 추가(수평 스캔 가능)
// 가장 오른쪽에 있는 리프 노드는 NEXT_LEAF 형제 노드가 없음을 나타내기 위해 0의 값을 가짐(페이지 0은 루트라서)
#define LEAF_NODE_NUM_CELLS_SIZE sizeof(uint32_t)                                                             // 데이터(cell) 개수의 크기
#define LEAF_NODE_NUM_CELLS_OFFSET COMMON_NODE_HEADER_SIZE                                                    // 데이터 개수 기록의 위치
#define LEAF_NODE_NEXT_LEAF_SIZE sizeof(uint32_t)                                                             // 형제 노드 포인터 크기
#define LEAF_NODE_NEXT_LEAF_OFFSET (LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE)                    // 형제 노드 포인터 기록의 위치
#define LEAF_NODE_HEADER_SIZE (COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE + LEAF_NODE_NEXT_LEAF_SIZE) // 리프 노드 헤더 크기

// Leaf Node Body Layout
// 리프 노드의 본체는 셀 배열(데이터 한 줄).
// 각 셀 배열은 키와 값(직렬화된 행)으로 구성됨 [key(id) + value(row data)]
#define LEAF_NODE_KEY_SIZE sizeof(uint32_t)                                   // 리프 노드의 키 크기
#define LEAF_NODE_KEY_OFFSET 0                                                // 리프 노드의 키 기록 위치
#define LEAF_NODE_VALUE_SIZE ROW_SIZE                                         // 값의 크기는 행의 크기와 같음
#define LEAF_NODE_VALUE_OFFSET (LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE)    // 값의 위치는 키 기록 위치+키 크기
#define LEAF_NODE_CELL_SIZE (LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE)       // 데이터(셀)의 크기는 키 크기+값 크기
#define LEAF_NODE_SPACE_FOR_CELLS (PAGE_SIZE - LEAF_NODE_HEADER_SIZE)         // 데이터(셀)를 위한 공간은 페이지에서 리프 노드 헤더를 제외한 만큼
#define LEAF_NODE_MAX_CELLS (LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE) // 데이터(셀) 최대 개수는-> 셀을 위한 공간/셀 사이즈

// 트리의 균형 유지를 위해, 리프노드가 꽉 찼을 때 반으로 쪼개기 위한 기준점
// 리프 노드가 N개의 셀을 저장할 수 있다면 분할 과정에서 두 노드 사이에 N+1개의 셀을 분배해야함
#define LEAF_NODE_RIGHT_SPLIT_COUNT ((LEAF_NODE_MAX_CELLS + 1) / 2)                          // (최대 저장 개수+새로 들어올 1개)/2->새로 만들어진 오른쪽 노드에 이만큼 데이터 저장
#define LEAF_NODE_LEFT_SPLIT_COUNT ((LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT) // 기존 노드에는 (전체 개수-오른쪽 개수)

// Internal Node Header Layout
// 내부 노드 헤더는 공통 헤더+키의 개수+가장 오른쪽 자식 노드의 페이지 번호
// b-tree 내부 노드에서 키가 n개면 자식 포인터는 n+1개. 앞의 n개는 키와 짝을 이루지만(바디에 저장됨)
// 맨 마지막 자식 rightmost child는 짝이 없어서 헤더에 저장
#define INTERNAL_NODE_NUM_KEYS_SIZE sizeof(uint32_t)                                                                       // 키의 개수(4바이트)->이 내부 노드가 현재 몇 개의 키(이정표)를 가지고 있는지 기록
#define INTERNAL_NODE_NUM_KEYS_OFFSET COMMON_NODE_HEADER_SIZE                                                              // 키의 개수 저장 위치(공통 헤더 바로 뒤 [Node Type(1)| Is Root(1)| Parent Ptr(4)] [여기])
#define INTERNAL_NODE_RIGHT_CHILD_SIZE sizeof(uint32_t)                                                                    // 가장 오른쪽 자식(rightmost child) 페이지 번호(4바이트)
#define INTERNAL_NODE_RIGHT_CHILD_OFFSET (INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE)                     // 가장 오른쪽 자식 위치:키 개수 뒤
#define INTERNAL_NODE_HEADER_SIZE (COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_RIGHT_CHILD_SIZE) // 네부 노드 헤더 전체 크기

// Internal Node Body Layout
// 실제 이정표인 '키-자식 페이지 번호'의 배열
// 내부 노드의 본체=셀의 배열 [ 자식 페이지 번호 | 키 값 ]->이 자식 페이지 번호로 가면, 이 키 값보다 작은 애들이 있어
#define INTERNAL_NODE_KEY_SIZE sizeof(uint32_t)                                     // 키 크기(4바이트 id)
#define INTERNAL_NODE_CHILD_SIZE sizeof(uint32_t)                                   // 자식 페이지 번호 크기(4바이트)
#define INTERNAL_NODE_CELL_SIZE (INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE) // 셀 하나 크기=자식 번호+키
#define INTERNAL_NODE_MAX_CELLS 3

#define INVALID_PAGE_NUM UINT32_MAX

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
    EXECUTE_DUPLICATE_KEY,
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

typedef struct
{
    int file_descriptor;  // 페이저가 읽어온 파일의 디스크립터
    uint32_t file_length; // 페이저가 읽어온 파일의 길이
    // b-tree는 루트 노드의 페이지 번호로 식별되므로 테이블 객체는 이 페이지 번호를 추적해야함
    uint32_t num_pages;           // 파일 크기/4KB(총 페이지 개수)
    void *pages[TABLE_MAX_PAGES]; // 캐시. 이미 읽은 페이지는 또 읽지 않고 메모리에서 바로 줌
    //->이 배열이 꽉찼을때 '해시 테이블+링크드 리스트' 형태의, LRU 로직대로 페이지 쫓아내는 기능 구현!!
} Pager;

typedef struct
{
    uint32_t root_page_num; // b-tree의 시작점->b-tree에선 루트 노드가 조개지고 합쳐지면서 0번이 아닐수도 있어서 계속 추적해야함
    /*uint32_t num_rows; // 데이터 몇번째 줄까지 썼나 위치 표시->이제 단순히 파일 길이로 계산 불가. b-tree는 데이터가 여러 페이지에 흩어져있고 중간에 삭제되고 합쳐짐*/
    Pager *pager; // pager 구조체. 테이블이 몇번째 데이터를 달라는 요청 받으면 페이저에게 시킴
} Table;

// table을 가리키는 손가락(일회용 도구)
typedef struct
{
    Table *table;      // 커서가 가리키는 테이블
    uint32_t page_num; // 현재 커서가 가리키는 노드의 page번호
    uint32_t cell_num; // 현재 커서가 가리키는 노드 내의 셀 번호
    bool end_of_table; // 테이블의 끝까지 다 봤는지 여부
} Cursor;

// 노드 유형 추적용
// 각 노드는 하나의 페이지에 대응됨
typedef enum
{
    NODE_INTERNAL, // 이정표.자식 노드가 저장된 페이지 번호를 저장해, 자식노드를 가리킴->db 검색 속도를 O(n)->O(logn)으로 만드는 핵심
    NODE_LEAF      // 실제 데이터(row)가 들어있음. 트리의 가장 바닥
} NodeType;

/* ============================================================ */
/* [중요] Forward Declarations (함수 원형 미리 알림)              */
/* 컴파일러에게 이런 함수들이 있다고 미리 알려줘야 순서 에러가 안 남. */
/* ============================================================ */

// 노드 정보 Getter/Setter
uint32_t *leaf_node_num_cells(void *node);
void *leaf_node_cell(void *node, uint32_t cell_num);
uint32_t *leaf_node_key(void *node, uint32_t cell_num);
void *leaf_node_value(void *node, uint32_t cell_num);
NodeType get_node_type(void *node);
void set_node_type(void *node, NodeType type);
bool is_node_root(void *node);
void set_node_root(void *node, bool is_root);
uint32_t *node_parent(void *node);
uint32_t *leaf_node_next_leaf(void *node);

// 내부 노드 관련 Getter/Setter
uint32_t *internal_node_num_keys(void *node);
uint32_t *internal_node_right_child(void *node);
uint32_t *internal_node_cell(void *node, uint32_t cell_num);
uint32_t *internal_node_child(void *node, uint32_t child_num);
uint32_t *internal_node_key(void *node, uint32_t key_num);
uint32_t get_node_max_key(Pager *pager, void *node);

// 초기화 및 유틸
void initialize_leaf_node(void *node);
void initialize_internal_node(void *node);
uint32_t get_unused_page_num(Pager *pager);
void print_tree(Pager *pager, uint32_t page_num, uint32_t indentaion_level);

// 탐색 및 삽입 로직
Cursor *leaf_node_find(Table *table, uint32_t page_num, uint32_t key);
Cursor *internal_node_find(Table *table, uint32_t page_num, uint32_t key);
uint32_t internal_node_find_child(void *node, uint32_t key);
Cursor *table_find(Table *table, uint32_t key);

void leaf_node_insert(Cursor *cursor, uint32_t key, Row *value);
void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, Row *value);
void internal_node_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num);
void internal_node_split_and_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num);
void update_internal_node_key(void *node, uint32_t old_key, uint32_t new_key);
void create_new_root(Table *table, uint32_t right_child_page_num);

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
    // getline은 입력 크기가 크면 알아서 공간을 더 할당함.
    // 그래서 버퍼의 주소를 저장한 곳의 주소와 할당해야하는 크기의 주소를 인자로 받음
    ssize_t read_bytes = getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);

    // 버퍼에 있는걸 못 읽었을때 강제 종료
    if (read_bytes <= 0)
    {
        printf("Error reading input\n");
        exit(EXIT_FAILURE);
    }

    // getline 실행 직후 메모리 상태: ['a', 'b', 'c', '\n', '\0']
    input_buffer->buffer[read_bytes - 1] = 0;    // 버퍼 마지막 엔터(줄바꿈 \n)를 \0(널문자,문자열의끝)으로 덮어써서 문자열 끝 표시
    input_buffer->input_length = read_bytes - 1; // 인풋 길이에서 엔터를 제외
}

// 프로그램 꺼지기 전, 메모리에 올라와 있는 페이지(캐시)를 디스크에 저장(flush)
void pager_flush(Pager *pager, uint16_t page_num)
{
    // 해당 페이지가 null이면(디스크에서 불러온 후 작성한게 아무것도 없음)
    if (pager->pages[page_num] == NULL)
    {
        printf("tried to flush null page\n");
        exit(EXIT_FAILURE);
    }

    // pwrite 함수:파일 커서 위치를 안 바꾸고 특정 위치에 데이터를 씀(시스템콜)
    // pwrite은 실패하면 -1를 반환,성공 시 실제로 쓴 바이트 수 반환
    // 인자 1: 파일 번호(fd)
    // 인자 2: 메모리 주소(여기에 있는 데이터를 가져가라)
    // 인자 3: 얼마나 쓸지(size)
    // 인자 4: 파일의 어느 위치에 쓸지
    // 항상 페이지 단위로 쓰기 때문에 한 페이지를 수정본(원래+수정한 데이터 포함)으로 덮어씀
    ssize_t bytes_written = pwrite(pager->file_descriptor, pager->pages[page_num], PAGE_SIZE, page_num * PAGE_SIZE);

    if (bytes_written == -1)
    {
        printf("Error seeking:%d \n", errno);
        exit(EXIT_FAILURE);
    }

    // 물리 디스크 동기화(ACID의 Durability 보장)
    // fsync(pager->file_descriptor);
}

// 1.페이지 캐시를 디스크에 저장 2.db 파일 닫음 3.pager,Table에 필요한 메모리 해제
void db_close(Table *table)
{
    Pager *pager = table->pager;

    // 0번부터 끝까지 돌면서, 메모리에 데이터가 있으면 무조건 디스크로 저장(Flush)
    // 이 방식이 'Buffer Pool' 관리의 기본
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    {
        if (pager->pages[i] == NULL)
        {
            continue;
        }
        pager_flush(pager, i); // 페이지 번호만 넘기면 알아서 4KB 저장
        free(pager->pages[i]);
        pager->pages[i] = NULL;
    }

    // 파일 닫기
    int result = close(pager->file_descriptor);
    if (result == -1)
    {
        printf("Error closing db file.\n");
        exit(EXIT_FAILURE);
    }

    // 페이지 메모리 해제 루프는 위에서 했으므로, 페이저/테이블만 해제
    free(pager);
    free(table);
}

// 페이지 가져오기 (캐시 확인->없으면 디스크에서 가져옴)
void *get_page(Pager *pager, uint32_t page_num)
{
    // 페이지 번호 유효성 검사
    if (page_num >= TABLE_MAX_PAGES)
    { // 만약 페이지 번호가 최대 페이지보다 크다면 에러
        printf("Tried to fetch page number out of bounds. %d>%d\n ", page_num, TABLE_MAX_PAGES);
        exit(EXIT_FAILURE);
    }

    // 해당 페이지가 페이저의 페이지 배열(캐시)에 없다면(캐시 미스!)
    if (pager->pages[page_num] == NULL)
    {
        //[캐시 미스] 메모리 공간 할당(새 페이지)
        void *page = malloc(PAGE_SIZE);

        // 파일에 저장된 페이지 총 개수 계산
        uint32_t num_pages = pager->file_length / PAGE_SIZE;

        // 만약 파일 길이가 한 페이지 사이즈보다 크면 한 페이지 더 갖고있음(자투리 페이지)
        if (pager->file_length % PAGE_SIZE)
        {
            num_pages += 1;
        }

        // 현재 파일이 가지고 있는 페이지 개수(db에는 페이지가 순서대로 저장된다고 가정)가 가져와야 하는 페이지 번호보다 많다면
        // 이전에 파일에 저장해둔 데이터이므로, '디스크'에서 데이터를 긁어와 메모리에 채움
        // 아니라면 if문 건너뜀(디스크를 안 읽음) 그냥 malloc으로 만든 깨끗한 페이지를 리턴.
        //->나중에 여기에 데이터 쓰고 flush(디스크에 저장)하면 파일 크기가 늘어남
        // 요청한 페이지가 파일 범위 안에 있다면 읽어온다
        if (page_num <= num_pages)
        {
            // lseek과 read 대신 pread를 씀
            // pread(파일 디스크립터,버퍼,읽을 크기,"읽을 위치(offset)"옴
            // 파일에서 페이지 번호*페이지 크기(주소)부터, '한 페이지'만큼 읽어서 page(버퍼)에 저장
            // 읽어들인 바이트 수 반환
            ssize_t bytes_read = pread(pager->file_descriptor, page, PAGE_SIZE, page_num * PAGE_SIZE);

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

        // 캐싱
        pager->pages[page_num] = page;

        if (page_num >= pager->num_pages)
        {
            pager->num_pages = page_num + 1;
        }
    }

    // 캐시 히트면 바로 반환(캐시인 pages 배열에 있는거 바로 꺼냄)
    return pager->pages[page_num];
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
    else if (strcmp(input_buffer->buffer, ".btree") == 0)
    {
        printf("Tree:\n");
        print_tree(table->pager, 0, 0);
        return META_COMMAND_SUCCESS;
    }
    else if (strcmp(input_buffer->buffer, ".constants") == 0)
    {
        printf("Constants:\n");
        return META_COMMAND_SUCCESS;
    }
    else
    { //.exit이 아닌 명령일때(일단 모르는 명령일 경우만 넣음)
        return META_UNRECOGNIZED_COMMAND;
    }
}

// Tokenizer
// 사용자가 입력한 문자열을 컴퓨터가 이해할 수 있는 구조체로 변환하는 파싱 단계
PrepareResult prepare_statement(InputBuffer *input_buffer, Statement *statement)
{
    // insert인 경우
    if (strncmp(input_buffer->buffer, "insert", 6) == 0)
    {
        statement->type = STATEMENT_INSERT;

        // sscanf는 printf의 반대.
        // printf:변수->문자열로 출력
        // sscanf:문자열->변수로 추출
        // 인풋 버퍼에 있는 문자열을 읽어서 "insert %d %s %s" 형식에 맞춰 데이터를 뽑아냄
        int args_assigned = sscanf(input_buffer->buffer, "insert %d %s %s", &(statement->row_to_insert.id), statement->row_to_insert.username, statement->row_to_insert.email);

        // 성공적으로 뽑아낸 변수의 개수가 3개보다 적으면 명령어 문법 오류
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

/*
커서
논리주소->물리주소

특정 행에 대해 메모리에서 어디를 읽고 써야하는지 알아냄(페이지+바이트 수 반환)
사용자가 '1번 페이지의 293바이트 뒤에 데이터 줘' 따위로 요청 안 할 수 있도록 복잡함을 숨겨주는 도구
*/

void *cursor_value(Cursor *cursor)
{

    // 1.현재 커서가 몇 번 페이지에 있는지 확인
    uint32_t page_num = cursor->page_num;

    // 2.페이저에게 해당 페이지를 내놓으라고 명령(없으면 파일에서 읽어옴)
    // 그 페이지의 '시작 주소'를 가져옴
    void *page = get_page(cursor->table->pager, page_num);

    // 3. 페이지 내부에서 cell_num번째 데이터가 시작되는 정확한 메모리 주소를 계산해서 반환
    return leaf_node_value(page, cursor->cell_num);
}

// 커서를 키0(최소 가능 키)으로 이동(select(풀스캔)할 때)
Cursor *table_start(Table *table)
{
    // 트리가 깊어져도 가장 왼쪽, 가장 깊은 곳의 시작점을 자동으로 찾아냄
    Cursor *cursor = table_find(table, 0);

    void *node = get_page(table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);
    cursor->end_of_table = (num_cells == 0);

    return cursor;
}

// 안내 데스크-내가 찾는 키가 몇 페이지, 몇 번째 셀에 있나요?
//  주어진 키를 트리에서 검색
//  없으면 삽입해야 하는 위치 반환
Cursor *table_find(Table *table, uint32_t key)
{
    // 루트 페이지 가져옴
    uint32_t root_page_num = table->root_page_num;
    void *root_node = get_page(table->pager, root_page_num);

    // 루트 노드가 리프노드일 때
    if (get_node_type(root_node) == NODE_LEAF)
    {
        // 리프노드 하나뿐이면 바로 데이터 찾음
        return leaf_node_find(table, root_page_num, key);
    }
    else // 내부 노드일 때
    {
        // 내부 노드 탐색 시작(수직 탐색->루트에서 리프까지)
        return internal_node_find(table, root_page_num, key);
    }
}

// 리프 노드는 이진 탐색으로 검색
Cursor *leaf_node_find(Table *table, uint32_t page_num, uint32_t key)
{
    void *node = get_page(table->pager, page_num);
    // 셀의 개수
    uint32_t num_cells = *leaf_node_num_cells(node);

    Cursor *cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = page_num;

    // 이진 탐색
    // 범위 0~끝
    uint32_t min_index = 0;
    uint32_t one_past_max_index = num_cells;

    while (one_past_max_index != min_index)
    {
        // 1.중간 지점 계산
        uint32_t index = (min_index + one_past_max_index) / 2;
        // 2.중간 지점의 키 값 읽기
        uint32_t key_at_index = *leaf_node_key(node, index);

        // 3. 데이터를 찾았다!
        if (key == key_at_index)
        {
            cursor->cell_num = index;
            return cursor;
        }
        // 4. 찾으려는 키가 중간값보다 작다-> 왼쪽 절반 탐색
        if (key < key_at_index)
        {
            one_past_max_index = index;
        } // 5.찾으려는 키가 중간값보다 크다-> 오른쪽 절반 탐색
        else
        {
            min_index = index + 1;
        }
    }

    // 못 찾은 경우.
    // 찾으려는 키는 없지만, 만약 넣는다면 그 키를 넣어야 할 자리를 반환
    cursor->cell_num = min_index;
    return cursor;
}

// 수직 탐색(루트->리프)
// 재귀적으로 호출되어 트리르 타고 내려감
Cursor *internal_node_find(Table *table, uint32_t page_num, uint32_t key)
{
    // 1.현재 내부 노드 정보를 읽음
    void *node = get_page(table->pager, page_num);

    // 3. 선택된 자식의 페이지 번호를 가져옴
    uint32_t child_index = internal_node_find_child(node, key);
    uint32_t child_num = *internal_node_child(node, child_index);

    void *child = get_page(table->pager, child_num);

    // 4.자식이 어떤 타입인지 확인(재귀 호출 분기점)
    switch ((get_node_type(child)))
    {
    case NODE_LEAF:
        // 목적지 노드 도착(리프노드라 데이터가 저장돼있고, 이제 키 찾고 커서 반환하면됨)
        return leaf_node_find(table, child_num, key);
    case NODE_INTERNAL:
        // 내부노드. 아직 더 내려가야함->다시 내부노드 탐색(재귀)
        return internal_node_find(table, child_num, key);
    }
}

uint32_t internal_node_find_child(void *node, uint32_t key)
{

    // 2.이진탐색:어느 자식 노드에게 내려가야할까?
    // 내부 노드의 키는 오른쪽 자식의 max key를 의미함
    uint32_t num_keys = *internal_node_num_keys(node);
    uint32_t min_index = 0;
    uint32_t max_index = num_keys;

    while (min_index != max_index)
    {
        // 중간 인덱스부터 봄
        uint32_t index = (min_index + max_index) / 2;
        // 현재 인덱스의 키 값(오른쪽 자식의 최대값)
        uint32_t key_to_right = *internal_node_key(node, index);

        // 찾는 키가 이 자식의 범위 안에 드는가?
        if (key_to_right >= key)
        {
            // 더 왼쪽(앞)의 자식을 확인해보자
            max_index = index;
        }
        else
        { // 더 오른쪽(뒤)의 자식을 확인해보자
            min_index = index + 1;
        }
    } // while 문 종료 시 min_index는 목표 키를 포함할 수 있는 자식의 인덱스가 됨

    return min_index;
}

// 커서 전진
void cursor_advance(Cursor *cursor)
{
    uint32_t page_num = cursor->page_num;
    void *node = get_page(cursor->table->pager, page_num);

    // 같은 페이지 내의 다음 셀로 이동
    cursor->cell_num += 1;

    // 만약 이 페이지의 데이터를 다 읽었다면?
    if (cursor->cell_num >= (*leaf_node_num_cells(node)))
    {
        // 1.다음 형제 노드가 몇 번 페이지?(포인터 읽기)
        uint32_t next_page_num = *leaf_node_next_leaf(node);

        // 2.형제가 없다면
        if (next_page_num == 0)
        { // 진짜 끝
            cursor->end_of_table = true;
        }
        else
        { // 3.형제가 있다면->그 페이지로 커서 이동
            cursor->page_num = next_page_num;
            cursor->cell_num = 0; // 새 페이지의 첫번쨰 데이터부터 시작
        }
    }
}
// 노드 타입 확인하는 함수
NodeType get_node_type(void *node)
{
    uint8_t value = *((uint8_t *)(node + NODE_TYPE_OFFSET));
    return (NodeType)value;
}

// 노드 타입을 설정하는 함수
void set_node_type(void *node, NodeType type)
{
    uint8_t value = type;
    *((uint8_t *)(node + NODE_TYPE_OFFSET)) = value;
}

// 리프노드 초기화하는 함수
void initialize_leaf_node(void *node)
{
    set_node_type(node, NODE_LEAF); // 리프 노드
    set_node_root(node, false);     // 루트 노드 아님
    *leaf_node_num_cells(node) = 0; // 해당 페이지의 cell 개수를  0으로 세팅->이 방은 비어있음을 의미
    *leaf_node_next_leaf(node) = 0; // 0은 형제노드 없다는 뜻
}

// row 프린트 함수
void print_row(Row *row)
{
    printf("%d %s %s\n", row->id, row->username, row->email);
}

/*
데이터 직렬화:
왜 필요한가?
C언어 구조체(struct)는 컴파일러마다 빈 공간(Padding)을 넣을 수 있어서,
구조체 그대로 파일에 쓰면 낭비가 심하고, 나중에 읽을 때 엉망이 될 수 있음.

메모리에 있는 Row 구조체-> 파일 저장용 바이트 배열

memcpy(받을 주소,복사할 원본 주소,복사할 크기). CPU 최적화가 되어있어 매우 빠름*/
void serialize_row(void *destination, Row *source)
{
    // 1.ID 복사:destination 시작 위치에 id를 넣음
    memcpy((char *)destination + ID_OFFSET, &(source->id), ID_SIZE);
    // 2.Username 복사:destination+4바이트 위치부터
    memcpy((char *)destination + USERNAME_OFFSET, source->username, USERNAME_SIZE);
    // 3.Email 복사:destination+37바이트 위치부터
    memcpy((char *)destination + EMAIL_OFFSET, source->email, EMAIL_SIZE);
}

// 바이트 덩어리(source)-> Row 구조체
void deserialize_row(Row *destination, void *source)
{
    // 1.ID 복원: source 위치에서 4바이트 읽어서 id에 넣음
    memcpy(&(destination->id), (char *)source + ID_OFFSET, ID_SIZE);
    // 2. Username 복원
    memcpy(destination->username, (char *)source + USERNAME_OFFSET, USERNAME_SIZE);
    // 3. Email 복원
    memcpy(destination->email, (char *)source + EMAIL_OFFSET, EMAIL_SIZE);
}

// Executor 어떤 명령인지 인자로 받음(구조체)
// insert 실행 함수

ExecuteResult execute_insert(Statement *statement, Table *table)
{
    // 용량 초과 체크
    void *node = get_page(table->pager, table->root_page_num);
    uint32_t num_cells = (*leaf_node_num_cells(node));

    // 1.삽입할 데이터 준비(파싱된 구조체)
    Row *row_to_insert = &(statement->row_to_insert);

    // 커서를 만들어서 테이블의 맨 끝으로 이동시킴
    // Cursor *cursor = table_end(table);

    // 2. 키로 id를 사용함
    uint32_t key_to_insert = row_to_insert->id;

    // 3.테이블에서 삽입할 위치를 찾아서 커서가 거길 가리키게 함
    // 그냥 맨 끝에 넣는게 아니라 key 값에 맞는 위치를 찾아옴
    // 이진탐색을 거쳐서 커서를 받아옴
    Cursor *cursor = table_find(table, key_to_insert);

    // 4.중복 키 검사
    // 커서가 가리키는 곳에 이미 데이터가 있고, 그 키가 내가 넣으려는 키와 같다면?
    if (cursor->cell_num < num_cells)
    {
        // 커서가 가리키는 셀의 키 위치
        uint32_t key_at_index = *leaf_node_key(node, cursor->cell_num);

        // 삽입하려는 위치의 키와 삽입할 키가 같다면 에러
        if (key_at_index == key_to_insert)
        {
            return EXECUTE_DUPLICATE_KEY; // "이미 있는 키입니다"
        }
    }

    // 5. 실제 삽입 진행
    // 트리에 노드가 하나만 있다고 가정하므로, 이 함수는 단순히 이 헬퍼 함수를 호출하면 됨
    leaf_node_insert(cursor, row_to_insert->id, row_to_insert);

    // 6.정리. 임시로 쓴 커서 해제
    free(cursor);
    return EXECUTE_SUCCESS; // 실행 성공!
}

// select 실행 함수(풀스캔)
ExecuteResult execute_select(Statement *statement, Table *table)
{
    // 1.커서를 만들고 맨 앞으로 이동
    Cursor *cursor = table_start(table);

    // 여기다 읽어온 데이터 저장할것임
    Row row;

    // 2.테이블의 끝에 도달할 때까지 반복
    while (!(cursor->end_of_table))
    {
        // 커서 위치의 데이터를 읽어서 Row 구조체로 복원
        deserialize_row(&row, cursor_value(cursor));

        // row 출력
        print_row(&row);

        // 3.다음 줄로 커서 이동
        cursor_advance(cursor);
    }

    // 임시로 쓴 커서 해제
    free(cursor);
    return EXECUTE_SUCCESS;
}

// 명령어 실행 함수
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

/*
<일단 파일 문만 열어 둠>

pager:하드웨어(디스크)와 유일하게 직접 대화함
-페이지 가져오기(read)
-4KB 덩어리 디스크에 저장하기(write)
-파일 크기가 몇바이트인지 확인
-캐싱:한번 읽은 페이지는 메모리에 들고 있다가, 또 달라고 하면 디스크 안 가고 바로 줌
*/
Pager *pager_open(const char *filename)
{
    // 1.파일 열기(시스템콜)
    // O_RDWR:읽기/쓰기 모드
    // O_CREAT:파일 없으면 새로 만듦
    // S_IWUSR|S_IRUSR:만든 파일의 권한(내 아이디로 읽고 쓰기 가능)
    int fd = open(filename, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);

    // 열기 실패 시
    if (fd == -1)
    {
        printf("Unable to open file\n");
        exit(EXIT_FAILURE);
    }

    // 2.파일 크기 측정
    // 커서를 맨 끝(SEEK_END)으로 보내면 그 위치가 곧 파일 크기(byte)
    // 이때 lseek은 ㄱㅊ
    off_t file_length = lseek(fd, 0, SEEK_END);

    // 3.Pager 구조체 생성
    Pager *pager = (Pager *)malloc(sizeof(Pager)); // 페이저 동적할당
    pager->file_descriptor = fd;                   // 페이저 구조체에 있는 파일 디스크립터,이제 페이저가 필요할 때마다 '디스크'에 있는 파일에 접근 가능(메모리에 올라온게 아님)
    pager->file_length = file_length;              // 페이저 구조체에 있는 파일 길이에 파일 크기 저장

    pager->num_pages = (file_length / PAGE_SIZE);

    if (file_length % PAGE_SIZE != 0)
    {
        printf("DB file is not a whole number of pages. Currupt file.\n");
        exit(EXIT_FAILURE);
    }

    // 4.페이지 캐시(페이지 배열) 초기화
    // 아직 아무 페이지도 안 읽었으니 전부 NULL로 초기화
    // get_page()가 호출되면 그때 그때 읽어옴
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    {
        pager->pages[i] = NULL;
    }

    return pager;
}

/*
테이블:사용자(SQL)와 대화함
-요청받은 데이터의 위치 계산
-페이저에게 명령(0번 페이지 내놔)
-페이저가 준 덩어리에서 정확히 데이터를 읽어냄

데이터베이스를 처음 열 때 데이터베이스 파일은 비어있으므로 페이지 0을 빈 리프 노드(루트 노드)로 초기화
*/
Table *db_open(const char *filename)
{
    // 1.Pager 고용
    Pager *pager = pager_open(filename);

    // 2.Table 구조체 생성
    // 아직 빈 껍데기
    Table *table = (Table *)malloc(sizeof(Table)); // 동적 할당

    // 3.의존성 주입(Table이 Pager를 소유함)
    //  테이블이 페이저를 통해 디스크에 접근 가능
    table->pager = pager;

    // 4.루트 페이지 설정 (중요)
    //  이 db의 루트는 무조건 0번 페이지라고 선언
    table->root_page_num = 0;

    // 5.신규 생성 파일 초기화(기존 파일이면 건너뜀)
    //  b 트리의 시작
    //  db 파일(디스크에 있음)이 비어있다면(프로그램 생전 처음 실행 or 파일 지우고 실행)
    //  0번 페이지를 가져와서 '너는 이제 b트리의 루트이자 리프노드다'라고 선언(initialize_leaf_node)
    if (pager->num_pages == 0)
    {
        //(1) 페이저에게 0번 페이지 메모리 달라고 함
        void *root_node = get_page(pager, 0);
        //(2) 그 메모리에 "여기는 리프 노드고, 데이터는 0개야" 라고 헤더를 씀
        initialize_leaf_node(root_node);
        //(3) 그 메모리에 "여기가 루트야"라고 표시함(나중에 split될 때 중요)
        set_node_root(root_node, true);
    }

    // 디스크에 저장해둔 파일이 있으니 0번부터 새로 만들지 말고 그거 읽어서 복구
    return table;
}

// 키,값 밑 메타 데이터에 접근하는 함수들.(단순 계산용)
// 모두 해당 값에 대한 포인터를 반환하므로 getter/setter 모두로 사용 가능

// 리프 노드의 헤더에 있는, 데이터(셀) 몇개인지 기록되어있는 주소 반환
uint32_t *leaf_node_num_cells(void *node)
{
    // 페이지 시작 주소(node)+오프셋
    // 4바이트 정수(개수)가 저장된 메모리 주소
    return node + LEAF_NODE_NUM_CELLS_OFFSET;
}

// 리프 노드의 n번째 셀의 위치 반환
void *leaf_node_cell(void *node, uint32_t cell_num)
{
    // 시작주소+헤더크기(건너뜀)+(n번째*셀 하나 크기)
    return node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
}

// 리프 노드의 키 위치 반환->이게 셀의 위치와 같음. 셀의 구조가 [ 키(id) | 값(row) ]니까
uint32_t *leaf_node_key(void *node, uint32_t cell_num)
{
    return leaf_node_cell(node, cell_num);
}

// 리프 노드의 값 위치 반환
// 셀 시작점(=키 위치)에서 키의 크기(4바이트)만큼 가면 값이 나옴
void *leaf_node_value(void *node, uint32_t cell_num)
{
    return leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
}

//[핵심 로직:정렬된 삽입] 단순히 데이터를 쓰는 게 아니라 자리를 만들고 씀!(정렬하며 끼워넣기)
// 리프 노드에 키/값 쌍을 삽입하는 함수
// 키/값이 삽입될 위치를 나타내는 커서를 입력으로 받음
void leaf_node_insert(Cursor *cursor, uint32_t key, Row *value)
{
    // 1.커서가 가리키는 페이지의 메모리 주소를 가져옴
    void *node = get_page(cursor->table->pager, cursor->page_num);

    // 2.현재 노드에 데이터가 몇 개 있는지 헤더를 읽어옴
    uint32_t num_cells = *leaf_node_num_cells(node);

    // 3.방이 꽉 찼는지 확인
    if (num_cells >= LEAF_NODE_MAX_CELLS)
    {
        // 꽉 찼으면 분할 함수 호출
        leaf_node_split_and_insert(cursor, key, value);
        return;
    }

    // 4. 자리 만들기(shifting)
    //  [중요] 새로운 셀을 위한 공간 확보를 위해 내가 넣으려는 위치(cursor->cell_num)보다 더 뒤에 있는 기존 셀들을 '오른쪽으로 한 칸' 이동(memcpy가 수행)
    //  b-tree 역할:페이지 내부에서 항상 키 순서대로 정렬 유지->검색 속도 획기적으로 빨라짐
    if (cursor->cell_num < num_cells)
    {
        // 새로운 셀 자리 만듦
        // 맨 끝의 셀부터 시작해서 커서가 가리키는 셀 직전까지 반복
        // (덮어쓰기 방지->넣으려는 위치보다 오른쪽에 있는것만 움직일꺼니까)
        for (uint32_t i = num_cells; i > cursor->cell_num; i--)
        {
            // i-1번째 데이터를 i번째로 복사->오른쪽으로 한 칸 밀기
            //  memcpy(복사받고싶은 위치,복사할 원본 위치,복사할 크기)
            memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1), LEAF_NODE_CELL_SIZE);
        }
    }

    // 5.데이터 쓰기
    *(leaf_node_num_cells(node)) += 1;                             // 셀 하나 늘어남
    *(leaf_node_key(node, cursor->cell_num)) = key;                // 키 기록
    serialize_row(leaf_node_value(node, cursor->cell_num), value); // 값을 직렬화해서 기록

    // 제대로 기록되었는지 디버깅용
    void *dest = leaf_node_value(node, cursor->cell_num);
    printf("--- DEBUG: Cell %d | Key Addr: %p | Value Addr: %p ---\n",
           cursor->cell_num,
           leaf_node_key(node, cursor->cell_num),
           dest);

    serialize_row(dest, value);
}

// 내부 노드 분할 함수
void internal_node_split_and_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num)
{
    // 1.기존 노드(old_node)와 그 노드가 가진 가장 큰 키(old_max)를
    // old_node가 꽉 차서 터지기 직전
    // old_max:나중에 부모에게 '내 최대값 이걸로 바뀌었어' 라고 보고하기 위해 미리 저장해둠
    uint32_t old_page_num = parent_page_num;
    void *old_node = get_page(table->pager, parent_page_num);
    uint32_t old_max = get_node_max_key(table->pager, old_node);

    void *child = get_page(table->pager, child_page_num);
    uint32_t child_max = get_node_max_key(table->pager, child);

    // 2.새로운 빈 페이지(new_node)를 하나 만듦->분할된 절반을 받을 노드
    uint32_t new_page_num = get_unused_page_num(table->pager);

    // 3.부모 찾기(중요!)->쪼개지고 나면 두 개의 노드를 관리해 줄 부모가 필요
    uint32_t splitting_root = is_node_root(old_node);

    void *parent;
    void *new_node;

    // 쪼개지는 노드가 루트라면
    if (splitting_root)
    {
        // 루트 위에는 부모가 없으므로 새로운 루트를 만들어야함

        //(신)루트->(구)루트와 새 노드를 자식으로 거느림
        // 트리 높이 1 증가
        create_new_root(table, new_page_num);
        // 새로운 루트가 부모
        parent = get_page(table->pager, table->root_page_num);

        //(구)루트->왼쪽 자식으로 강등
        old_page_num = *internal_node_child(parent, 0);
        old_node = get_page(table->pager, old_page_num);
    }
    else // 평범한 중간노드였다면
    {    // 원래 있던 부모를 그대로 모셔옴
        parent = get_page(table->pager, *node_parent(old_node));
        // 분할될 절반을 받을 새로운 노드
        new_node = get_page(table->pager, new_page_num);
        // 새로운 노드 초기화
        initialize_internal_node(new_node);
    }

    uint32_t *old_num_keys = internal_node_num_keys(old_node);

    // new_node도 내부 노드이므로 반드시 '가장 오른쪽 자식'을 하나 가져야함.
    // 루프 돌기 전 미리 보낸 것

    // 1.old_node의 가장 오른쪽 자식을 가져옴
    uint32_t cur_page_num = *internal_node_right_child(old_node);
    void *cur = get_page(table->pager, cur_page_num);

    // 2.그걸 새 노드에 넣음
    internal_node_insert(table, new_page_num, cur_page_num);
    // 3.자식에게 '너 부모 바뀜'이라고 알려줌
    *node_parent(cur) = new_page_num;
    // 4.old_node의 가장 오른쪽 자식 자리는 일단 비워둠
    *internal_node_right_child(old_node) = INVALID_PAGE_NUM;

    // old_node에 꽉 차있던 자식들(가장오른쪽자식제외)의 절반(큰 값)을 new_node로 이사시킴
    // 중간 지점부터 끝까지(절반보다 큰 값) 반복
    for (int i = INTERNAL_NODE_MAX_CELLS - 1; i > INTERNAL_NODE_MAX_CELLS / 2; i--)
    {
        // 1.자식을 가져옴
        cur_page_num = *internal_node_child(old_node, i);
        cur = get_page(table->pager, cur_page_num);

        // 2.새 노드(new_node)에 자식을 넣음
        internal_node_insert(table, new_page_num, cur_page_num);

        // 3.[중요] 자식에게 이제 너의 부모는 new_node라는 걸 알려줌
        *node_parent(cur) = new_page_num;

        // 4.old_node에서는 지움(키 개수 감소)
        (*old_num_keys)--;
    }

    // old_node에 남은 자식들 중 가장 큰 녀석(배열의 마지막)을 가장오른쪽자식 으로 승격(헤더로 감)
    *internal_node_right_child(old_node) = *internal_node_child(old_node, *old_num_keys - 1);
    // 키의 개수는 줄임
    (*old_num_keys)--;

    // 분할을 유발한 child 처리
    // 쪼개진 old_node의 최대값(max_after_split)보다 작으면 old로, 크면 new로 보냄
    uint32_t max_after_split = get_node_max_key(table->pager, old_node);
    uint32_t destination_page_num = child_max < max_after_split ? old_page_num : new_page_num;

    internal_node_insert(table, destination_page_num, child_page_num);
    *node_parent(child) = destination_page_num;

    // 1.부모에게 old_node의 최대값이 줄어든 걸 알림(업데이트)
    update_internal_node_key(parent, old_max, get_node_max_key(table->pager, old_node));

    // 2.루트 분할이 아니라면, 새로 생긴 new_node도 자식으로 추가
    if (!splitting_root)
    {
        internal_node_insert(table, *node_parent(old_node), new_page_num);
        *node_parent(new_node) = *node_parent(old_node);
    }
}

void internal_node_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num)
{
    void *parent = get_page(table->pager, parent_page_num);
    void *child = get_page(table->pager, child_page_num);
    uint32_t child_max_key = get_node_max_key(table->pager, child);
    uint32_t index = internal_node_find_child(parent, child_max_key);

    uint32_t original_num_keys = *internal_node_num_keys(parent);

    if (original_num_keys >= INTERNAL_NODE_MAX_CELLS)
    {
        internal_node_split_and_insert(table, parent_page_num, child_page_num);
        return;
    }

    uint32_t right_child_page_num = *internal_node_right_child(parent);

    if (right_child_page_num == INVALID_PAGE_NUM)
    {
        *internal_node_right_child(parent) = child_page_num;
        return;
    }

    void *right_child = get_page(table->pager, right_child_page_num);

    *internal_node_num_keys(parent) = original_num_keys + 1;

    if (child_max_key > get_node_max_key(table->pager, right_child))
    {
        *internal_node_child(parent, original_num_keys) = right_child_page_num;
        *internal_node_key(parent, original_num_keys) = get_node_max_key(table->pager, right_child);
        *internal_node_right_child(parent) = child_page_num;
    }
    else
    {
        for (uint32_t i = original_num_keys; i > index; i--)
        {
            void *destination = internal_node_cell(parent, i);
            void *source = internal_node_cell(parent, i - 1);
            memcpy(destination, source, INTERNAL_NODE_CELL_SIZE);
        }
        *internal_node_child(parent, index) = child_page_num;
        *internal_node_key(parent, index) = child_max_key;
    }
}

// 분할 함수
// 새로운 노드를 만들고 데이터 절반을 새로운 노드에 옮김. 삽입하려는 데이터를 둘 중 하나에 넣음
// 부모 노드를 새로 만들거나 업데이트
void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, Row *value)
{
    // 1.기존 방(꽉 참) 메모리 가져옴
    void *old_node = get_page(cursor->table->pager, cursor->page_num);

    uint32_t old_max = get_node_max_key(cursor->table->pager, old_node);

    // 2.새 방 번호를 할당받음(예:파일 맨 뒤에 새 페이지 추가)
    uint32_t new_page_num = get_unused_page_num(cursor->table->pager);
    // 3.새 방 메모리 가져와서 "여긴 리프노드야"라고 초기화
    void *new_node = get_page(cursor->table->pager, new_page_num);
    initialize_leaf_node(new_node);

    *node_parent(new_node) = *node_parent(old_node);

    // 새 노드의 형제노드는 기존 노드(old_node)의 형제 노드
    *leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
    // 기존 노드의 형제 노드는 새 노드(new_node)
    *leaf_node_next_leaf(old_node) = new_page_num;

    // 4.분배 루프(n+1개의 데이터를 두 노드에 나눔)
    //  기존의 키들과 새로운 키는 기존 노드(왼쪽)와 새로운 노드(오른쪽)에 고르게 분할되어야함
    //  i는 "가상의 정렬된 인덱스", 뒤에서부터 0까지 도는 역순 루프
    for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--)
    {
        void *destination_node;
        uint32_t index_within_node;

        //[판단 1] 어느 방으로 보낼까?
        // 절반보다 크면 새 방(오른쪽 노드)
        if (i >= LEAF_NODE_LEFT_SPLIT_COUNT)
        {
            // 오른쪽이 새 노드
            destination_node = new_node;
            // 예: i가 7이고 기준이 7이면, 새 방의 0번째(7-7) 칸으로 감
            index_within_node = i - LEAF_NODE_LEFT_SPLIT_COUNT; // 노드 안에서 인덱스
        }
        else // 절반보다 작으면 헌 방(왼쪽 노드)에 잔류
        {
            // 왼쪽이 기존 노드
            destination_node = old_node;
            // 예: i가 5면, 헌 방의 5번째 칸 그대로 유지
            index_within_node = i;
        }

        void *destination = leaf_node_cell(destination_node, index_within_node); // 노드의 (인덱스)번 째 셀 반환

        //[판단 2] 무슨 데이터를 넣을까?
        if (i == cursor->cell_num)
        { // i번째가 이번에 넣으려는 바로 그 '새 데이터'
            serialize_row(leaf_node_value(destination_node, index_within_node), value);
            *leaf_node_key(destination_node, index_within_node) = key;
        }
        else if (i > cursor->cell_num)
        { // i가 새로 삽입할 데이터 인덱스보다 크면-> 원래 자리(i-1)에서 가져옴(오른쪽으로 밀려난 효과)
            memcpy(destination, leaf_node_cell(old_node, i - 1), LEAF_NODE_CELL_SIZE);
        }
        else
        { // 새로 삽입할 데이터보다 작으면->원래 자리(i) 거 그대로 유지
            memcpy(destination, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
        }
    }
    // 5. 장부 정리(헤더 업데이트)
    //" 왼쪽 방엔 7개,오른쪽 방에는 7개 있어"라고 기록
    *(leaf_node_num_cells(old_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;
    *(leaf_node_num_cells(new_node)) = LEAF_NODE_RIGHT_SPLIT_COUNT;

    // 6.부모 노드 처리
    // 만약 쪼개진 노드가 "루트"였다면?
    if (is_node_root(old_node))
    {
        // 루트가 두 개일 순 없으니, 더 높은 새로운 루트를 만듦
        return create_new_root(cursor->table, new_page_num);
    }
    else // 루트 노드가 아니라면 부모노드를 업데이트 해야함(자식 포인터 갱신)
    {
        uint32_t parent_page_num = *node_parent(old_node);                   // 옛날 노드(왼쪽 노드?)의 부모 노드를 가져옴
        uint32_t new_max = get_node_max_key(cursor->table->pager, old_node); // 새 키는 자식 노드의 최대 키
        void *parent = get_page(cursor->table->pager, parent_page_num);

        update_internal_node_key(parent, old_max, new_max); // 예전 키를 새로운 키로 업데이트
        internal_node_insert(cursor->table, parent_page_num, new_page_num);
    }
}

uint32_t *node_parent(void *node)
{
    return node + PARENT_POINTER_OFFSET;
}

void update_internal_node_key(void *node, uint32_t old_key, uint32_t new_key)
{
    uint32_t old_child_index = internal_node_find_child(node, old_key);
    *internal_node_key(node, old_child_index) = new_key;
}
// getter와 setter

// 루트 노드 여부 반환
bool is_node_root(void *node)
{
    uint8_t value = *((uint8_t *)(node + IS_ROOT_OFFSET));
    return (bool)value;
}

// 루트 노드 여부를 설정
void set_node_root(void *node, bool is_root)
{
    uint8_t value = is_root;
    *((uint8_t *)(node + IS_ROOT_OFFSET)) = value;
}

// db 파일 끝에 새로운 페이지 추가
uint32_t get_unused_page_num(Pager *pager)
{
    return pager->num_pages;
}

// 형제 노드 위치 반환
uint32_t *leaf_node_next_leaf(void *node)
{
    return node + LEAF_NODE_NEXT_LEAF_OFFSET;
}
// 루트의 승격(promotion)
// 루트 페이지 번호(0번)는 절대 바뀌지 않는다는 원칙을 지키기 위한 트릭
//->진입점 고정되어 메타데이터에 별도로 루트 번호 저장할 필요 없음
// 기존 루트 페이지가 왼쪽 자식 페이지로 복사됨.->루트 페이지 재사용 가능
void create_new_root(Table *table, uint32_t right_child_page_num)
{
    // 1. 현재 루트(0번,꽉 참) 메모리 가져옴
    void *root = get_page(table->pager, table->root_page_num);
    // 2.방금 split으로 생긴 오른쪽 노드(새 노드) 메모리 가져옴
    void *right_child = get_page(table->pager, right_child_page_num);
    // 3.왼쪽 자식이 될 놈을 위해 새 페이지 할당 받음(아직 split하고 왼쪽 절반은 기존의 루트 페이지에 있음)
    uint32_t left_child_page_num = get_unused_page_num(table->pager);
    void *left_child = get_page(table->pager, left_child_page_num);

    // 기존 루트가 internal node였다면
    if (get_node_type(root) == NODE_INTERNAL)
    {
        initialize_internal_node(right_child);
        initialize_internal_node(left_child);
    }

    // 4.[트릭] 기존 루트 페이지의 내용을 왼쪽 자식 페이지로 복사
    // 0번 페이지에 있던 데이터를 새 페이지(left)로 옮김
    // 이제 데이터는 left_child와 right_child에 나눠져있음
    memcpy(left_child, root, PAGE_SIZE);
    // 이제 루트 노드가 아니라고 설정
    set_node_root(left_child, false);

    // 기존 루트가 leaf node가 아니라 internal node였을 때
    // 기존 루트의 자식 노드들의 부모노드를 left_child로 변경
    if (get_node_type(left_child) == NODE_INTERNAL)
    {
        void *child;
        for (int i = 0; i < *internal_node_num_keys(left_child); i++)
        {
            child = get_page(table->pager, *internal_node_child(left_child, i));
            *node_parent(child) = left_child_page_num;
        }
        child = get_page(table->pager, *internal_node_right_child(left_child));
        *node_parent(child) = left_child_page_num;
    }
    // 5.루트 페이지(0번) 재건축
    // 이제 0번 페이지는 데이터 저장소가 아니라 내부 노드가 됨
    initialize_internal_node(root); // 타입을 internal로 변경
    set_node_root(root, true);      //"내가 루트다" 설정

    // 6.루트 내용 채우기
    *internal_node_num_keys(root) = 1;                   // 루트 키는 1개
    *internal_node_child(root, 0) = left_child_page_num; // 왼쪽 자식 포인터 연결

    // 이정표(key) 설정:왼쪽 자식의 가장 큰 키값(max key)를 가져와서 기록
    // 이 키보다 작거나 같으면 왼쪽으로 가라는 뜻
    uint32_t left_child_max_key = get_node_max_key(table->pager, left_child); // 왼쪽 자식 노드 최대 키 구하기
    *internal_node_key(root, 0) = left_child_max_key;                         // 루트의 키는 왼쪽 자식 노드의 최대 키(루트 노드의 0번째 셀에 기록)

    // 오른쪽 자식 포인터 연결
    *internal_node_right_child(root) = right_child_page_num;

    *node_parent(left_child) = table->root_page_num;
    *node_parent(right_child) = table->root_page_num;
}

// 내부 노드에 대한 읽기 및 쓰기 함수

// 내부 노드 키의 개수
uint32_t *internal_node_num_keys(void *node)
{
    return node + INTERNAL_NODE_NUM_KEYS_OFFSET;
}

// 내부 노드의 오른쪽 자식 포인터
uint32_t *internal_node_right_child(void *node)
{
    return node + INTERNAL_NODE_RIGHT_CHILD_OFFSET;
}

// 내부 노드의 n번째 셀
uint32_t *internal_node_cell(void *node, uint32_t cell_num)
{
    return node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE;
}

uint32_t *internal_node_child(void *node, uint32_t child_num)
{
    uint32_t num_keys = *internal_node_num_keys(node);

    if (child_num > num_keys)
    {
        printf("Tried to access child_num %d >num_keys %d\n", child_num, num_keys);
        exit(EXIT_FAILURE);
    }
    else if (child_num == num_keys)
    {
        uint32_t *right_child = internal_node_right_child(node);
        if (*right_child == INVALID_PAGE_NUM)
        {
            printf("Tried to access right child of node, but was invalid page.\n");
            exit(EXIT_FAILURE);
        }
        return right_child;
    }
    else
    {
        uint32_t *child = internal_node_cell(node, child_num);
        if (*child == INVALID_PAGE_NUM)
        {
            printf("Tried to access child %d of node, but was invalid page.\n", child_num);
            exit(EXIT_FAILURE);
        }
        return child;
    }
}

// 내부 노드의 키 위치 반환
uint32_t *internal_node_key(void *node, uint32_t key_num)
{
    return (void *)internal_node_cell(node, key_num) + INTERNAL_NODE_CHILD_SIZE;
}

// 노드 타입에 따라 가장 큰 키 값을 읽는 위치를 다르게 처리하는 함수
uint32_t get_node_max_key(Pager *pager, void *node)
{
    if (get_node_type(node) == NODE_LEAF)
    {
        return *leaf_node_key(node, *leaf_node_num_cells(node) - 1);
    }
    void *right_child = get_page(pager, *internal_node_right_child(node));
    return get_node_max_key(pager, right_child);
}

void initialize_internal_node(void *node)
{
    set_node_type(node, NODE_INTERNAL);
    set_node_root(node, false);
    *internal_node_num_keys(node) = 0;

    *internal_node_right_child(node) = INVALID_PAGE_NUM;
}

// 관심있는 몇가지 상수를 출력하는 새로운 메타 명령어 추가
void print_constants()
{
    printf("ROW_SIZE:%lu\n", ROW_SIZE);
    printf("COMMON_NODE_HEADER_SIZE: %lu\n", COMMON_NODE_HEADER_SIZE);
    printf("LEAF_NODE_HEADER_SIZE: %lu\n", LEAF_NODE_HEADER_SIZE);
    printf("LEAF_NODE_CELL_SIZE: %lu\n", LEAF_NODE_CELL_SIZE);
    printf("LEAF_NODE_SPACE_FOR_CELLS: %lu\n", LEAF_NODE_SPACE_FOR_CELLS);
    printf("LEAF_NODE_MAX_CELLS: %lu\n", LEAF_NODE_MAX_CELLS);
}

void indent(uint32_t level)
{
    for (uint32_t i = 0; i < level; i++)
    {
        printf(" ");
    }
}

// 디버깅 및 시각화를 돕기 위한 b-tree를 시각화하는 메타 명령어
void print_tree(Pager *pager, uint32_t page_num, uint32_t indentaion_level)
{
    void *node = get_page(pager, page_num);
    uint32_t num_keys, child;

    switch (get_node_type(node))
    {
    case (NODE_LEAF):
        num_keys = *leaf_node_num_cells(node);
        indent(indentaion_level);
        printf("- leaf (size %d)\n", num_keys);
        for (uint32_t i = 0; i < num_keys; i++)
        {
            indent(indentaion_level + 1);
            printf("- %d\n", *leaf_node_key(node, i));
        }
        break;
    case (NODE_INTERNAL):
        num_keys = *internal_node_num_keys(node);
        indent(indentaion_level);
        printf("- internal (size %d)\n", num_keys);

        for (uint32_t i = 0; i < num_keys; i++)
        {
            child = *internal_node_child(node, i);
            print_tree(pager, child, indentaion_level + 1);

            indent(indentaion_level + 1);
            printf("-key %d\n", *internal_node_key(node, i));
        }
        child = *internal_node_right_child(node);
        print_tree(pager, child, indentaion_level + 1);

        break;
    }
}

/*
args:들어온 단어 개수(프로그램 실행 전 터미널에서 작성)
->터미널이 쉘 프로그램 실행, 입력하면 터미널이 쉘에게 글자들을 던져주고,
쉘은 입력한 문자를 공백 기준으로 쪼개서 배열을 만들고, 운영체제 커널에게 이 파일 열라고 명령을 내림.
(c의 main 프로그램에게 넘겨주며 실행시킴)
argv:들어온 단어 배열(실행 파일 이름(프로그램 자기자신),내가 넘기고 싶은 파일(여기선 db 파일 이름))
*/
int main(int args, char *argv[])
{
    // 1.개수 체크
    // 실행 파일 이름(argv[0]) 하나만 있고 파일 이름(argv[1])을 안 줘서 에러
    if (args < 2)
    {
        printf("Must supply a database filename\n"); // 파일 이름 써라
        exit(EXIT_FAILURE);                          // 프로그램 강제 종료
    }

    // 2.파일 이름 낚아채기
    // argv[0]은 프로그램 이름이니까 버리고 argv[1]에 있는 진짜 데이터(파일 이름)를 변수에 저장
    // c에서 문자열은 주소. argv는 그 문자열 주소들이 모여있는 배열
    // argv[1]는 "data.db"가 저장된 곳의 주소를 가리킴(char*)
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

        // 어떤 명령어인지 받아오고, statement 구조체에 데이터가 채워짐
        switch (prepare_statement(input_buffer, &statement))
        {

        // 아는 명령어일때
        // switch문 탈출(가장 가까운 반복문이나 switch문 탈출)
        case (PREPARE_SUCCESS):
            break;

        // 문법 오류일때
        //  while문 처음으로 돌아감(다시 입력 받으러 감)
        case (PREPARE_SYNTAX_ERROR):
            printf("Syntax error\n");
            continue;

        // 모르는 명령어일때
        // while문 처음으로 돌아감(switch문에는 continue가 없음. for,while문에만 있다)
        case (PREPARE_UNRECOGNIZED):
            printf("unrecognized '%s'\n", input_buffer->buffer);
            continue;
        }

        //
        switch (execute_statement(&statement, table))
        {
        case (EXECUTE_SUCCESS):
            printf("Executed.\n");
            break;
        case (EXECUTE_DUPLICATE_KEY):
            printf("Error Duplicate key.\n");
            break;
        case (EXECUTE_TABLE_FULL):
            printf("Error:Table full.\n");
            break;
        }
    }

    return 0;
}