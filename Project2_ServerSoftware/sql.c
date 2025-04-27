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

// Implementation of strcasestr
char *strncasestr(const char *haystack, const char *needle) {
    size_t needle_len = strlen(needle);
    
    while (*haystack) {
        if (strncasecmp(haystack, needle, needle_len) == 0) {
            return (char *)haystack;
        }
        haystack++;
    }
    
    return NULL;
}

// Send HTTP response
void send_http_response(char *content_type, char *body) {
    printf("Content-Type: %s\r\n", content_type);
    printf("Content-Length: %zu\r\n\r\n", strlen(body));
    printf("%s", body);
    fflush(stdout);
}

// Send error response
void send_error_response(char *error_msg) {
    char body[1024];
    sprintf(body, "SQL Error: %s", error_msg);
    send_http_response("text/plain", body);
}

// Parse SQL command and determine its type
int parse_sql_command(char *sql, int *command_type) {
    if (strncasestr(sql, "CREATE", 6) == 0) {
        *command_type = CMD_CREATE;
    } else if (strncasestr(sql, "INSERT", 6) == 0) {
        *command_type = CMD_INSERT;
    } else if (strncasestr(sql, "UPDATE", 6) == 0) {
        *command_type = CMD_UPDATE;
    } else if (strncasestr(sql, "SELECT", 6) == 0) {
        *command_type = CMD_SELECT;
    } else if (strncasestr(sql, "DELETE", 6) == 0) {
        *command_type = CMD_DELETE;
    } else {
        return -1; // Unknown command
    }
    
    return 0;
}

// Create a new block in the file
int create_new_block(int fd) {
    struct stat st;
    
    if (fstat(fd, &st) < 0) {
        return -1;
    }
    
    int block_num = st.st_size / BLOCK_SIZE;
    
    char block[BLOCK_SIZE];
    memset(block, '.', BLOCK_SIZE);
    strcpy(block + BLOCK_SIZE - 4, END_MARKER);
    
    if (write_block(fd, block_num, block) < 0) {
        return -1;
    }
    
    return block_num;
}

// Read a block from file
int read_block(int fd, int block_num, char *block) {
    off_t offset = block_num * BLOCK_SIZE;
    
    if (lseek(fd, offset, SEEK_SET) < 0) {
        return -1;
    }
    
    if (read(fd, block, BLOCK_SIZE) != BLOCK_SIZE) {
        return -1;
    }
    
    return 0;
}

// Write a block to file
int write_block(int fd, int block_num, char *block) {
    off_t offset = block_num * BLOCK_SIZE;
    
    if (lseek(fd, offset, SEEK_SET) < 0) {
        return -1;
    }
    
    if (write(fd, block, BLOCK_SIZE) != BLOCK_SIZE) {
        return -1;
    }
    
    return 0;
}

// Find a free block in the file (marked as empty)
int find_free_block(int fd) {
    struct stat st;
    
    if (fstat(fd, &st) < 0) {
        return -1;
    }
    
    int num_blocks = st.st_size / BLOCK_SIZE;
    char block[BLOCK_SIZE];
    
    for (int i = 0; i < num_blocks; i++) {
        if (read_block(fd, i, block) < 0) {
            continue;
        }
        
        // Check if block is empty (contains only dots)
        int empty = 1;
        for (int j = 0; j < BLOCK_SIZE - 4; j++) {
            if (block[j] != '.') {
                empty = 0;
                break;
            }
        }
        
        if (empty) {
            return i;
        }
    }
    
    return -1; // No free blocks found
}

// Find a table schema in the schema file
int find_table_schema(char *table_name, TableSchema *schema) {
    int schema_fd = open("schema.dat", O_RDONLY);
    if (schema_fd < 0) {
        return -1; // Schema file doesn't exist, so table doesn't exist
    }
    
    char block[BLOCK_SIZE];
    int block_num = 0;
    
    while (1) {
        if (read_block(schema_fd, block_num, block) < 0) {
            close(schema_fd);
            return -1;
        }
        
        // Look for table name in this block
        char *schema_str = block;
        char *table_str = strtok(schema_str, "|");
        
        while (table_str != NULL) {
            if (strcmp(table_str, table_name) == 0) {
                // Found the table, parse schema
                strcpy(schema->name, table_name);
                
                // Parse columns
                char *columns_str = strtok(NULL, ";");
                if (columns_str == NULL) {
                    close(schema_fd);
                    return -1;
                }
                
                char *columns[MAX_COLS];
                int num_columns = 0;
                
                char *column = strtok(columns_str, ",");
                while (column != NULL && num_columns < MAX_COLS) {
                    columns[num_columns++] = column;
                    column = strtok(NULL, ",");
                }
                
                schema->num_columns = num_columns;
                
                for (int i = 0; i < num_columns; i++) {
                    char *name = strtok(columns[i], ":");
                    char *type = strtok(NULL, "");
                    
                    if (name != NULL && type != NULL) {
                        strcpy(schema->columns[i].name, name);
                        
                        if (strncmp(type, "char(", 5) == 0) {
                            schema->columns[i].type = TYPE_CHAR;
                            schema->columns[i].size = atoi(type + 5);
                        } else if (strcmp(type, "smallint") == 0) {
                            schema->columns[i].type = TYPE_SMALLINT;
                            schema->columns[i].size = 4;
                        } else if (strcmp(type, "int") == 0) {
                            schema->columns[i].type = TYPE_INTEGER;
                            schema->columns[i].size = 8;
                        }
                    }
                }
                
                close(schema_fd);
                return 0; // Success
            }
            
            table_str = strtok(NULL, "|");
        }
        
        // Check if there's a next block
        char next_block[5];
        strncpy(next_block, block + BLOCK_SIZE - 4, 4);
        next_block[4] = '\0';
        
        if (strcmp(next_block, END_MARKER) == 0) {
            break; // No more blocks
        }
        
        block_num = atoi(next_block);
    }
    
    close(schema_fd);
    return -1; // Table not found
}