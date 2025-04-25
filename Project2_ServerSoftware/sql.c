#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdarg.h>

#define BLOCK_SIZE 256
#define MAX_TABLES 20
#define MAX_COLS 10
#define MAX_QUERY_LEN 1024
#define END_MARKER "XXXX"

// Data types
#define TYPE_CHAR 1
#define TYPE_SMALLINT 2
#define TYPE_INTEGER 3

// SQL commands
#define CMD_CREATE 1
#define CMD_INSERT 2
#define CMD_UPDATE 3
#define CMD_SELECT 4
#define CMD_DELETE 5

// SQL comparison operators
#define OP_EQUAL 1
#define OP_NOT_EQUAL 2
#define OP_GREATER 3
#define OP_LESS 4

// Structure to store column information
typedef struct {
    char name[32];
    int type;
    int size;
} Column;

// Structure to store table schema
typedef struct {
    char name[32];
    int num_columns;
    Column columns[MAX_COLS];
} TableSchema;

// Structure for WHERE clause condition
typedef struct {
    char column_name[32];
    int op;
    char value[256];
} Condition;

// Function declarations
int parse_sql_command(char *sql, int *command_type);
int execute_create(char *sql);
int execute_insert(char *sql);
int execute_update(char *sql);
int execute_select(char *sql);
int execute_delete(char *sql);
int find_table_schema(char *table_name, TableSchema *schema);
int create_new_block(int fd);
int read_block(int fd, int block_num, char *block);
int write_block(int fd, int block_num, char *block);
int find_free_block(int fd);
void send_http_response(char *content_type, char *body);
void send_error_response(char *error_msg);
char *strncasestr(const char *haystack, const char *needle);
int parse_condition(char *where_clause, Condition *condition);
int evaluate_condition(Condition *condition, char *record, TableSchema *schema);

//implementation of main
int main() {
    char *query_string = getenv("QUERY_STRING");
    
    if (query_string == NULL) {
        send_error_response("No SQL query provided");
        return 1;
    }
    
    // Decode URL-encoded query string
    char sql[MAX_QUERY_LEN];
    int i = 0, j = 0;
    
    while (query_string[i] && j < MAX_QUERY_LEN - 1) {
        if (query_string[i] == '+') {
            sql[j++] = ' ';
        } else if (query_string[i] == '%' && query_string[i+1] && query_string[i+2]) {
            // Handle URL encoding (e.g., %20 = space)
            char hex[3] = {query_string[i+1], query_string[i+2], 0};
            sql[j++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else {
            sql[j++] = query_string[i];
        }
        i++;
    }
    sql[j] = '\0';
    
    // Parse and execute SQL command
    int command_type;
    int result = parse_sql_command(sql, &command_type);
    
    if (result != 0) {
        send_error_response("Failed to parse SQL command");
        return 1;
    }
    
    switch (command_type) {
        case CMD_CREATE:
            result = execute_create(sql);
            break;
        case CMD_INSERT:
            result = execute_insert(sql);
            break;
        case CMD_UPDATE:
            result = execute_update(sql);
            break;
        case CMD_SELECT:
            result = execute_select(sql);
            break;
        case CMD_DELETE:
            result = execute_delete(sql);
            break;
        default:
            send_error_response("Unknown SQL command");
            return 1;
    }
    
    if (result != 0) {
        send_error_response("Error executing SQL command");
        return 1;
    }
    
    return 0;
}