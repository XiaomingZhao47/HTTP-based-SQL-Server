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
    if (strncasecmp(sql, "CREATE", 6) == 0) {
        *command_type = CMD_CREATE;
    } else if (strncasecmp(sql, "INSERT", 6) == 0) {
        *command_type = CMD_INSERT;
    } else if (strncasecmp(sql, "UPDATE", 6) == 0) {
        *command_type = CMD_UPDATE;
    } else if (strncasecmp(sql, "SELECT", 6) == 0) {
        *command_type = CMD_SELECT;
    } else if (strncasecmp(sql, "DELETE", 6) == 0) {
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
    strncpy(block + BLOCK_SIZE - 4, END_MARKER, 4);
    
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

// Execute CREATE TABLE SQL command
int execute_create(char *sql) {
    // Example: CREATE TABLE movies (id smallint, title char(30), length int);
    char table_name[32];
    char *p = strstr(sql, "CREATE TABLE");
    
    if (p == NULL) {
        send_error_response("Invalid CREATE TABLE syntax");
        return -1;
    }
    
    p += 12; // Skip "CREATE TABLE"
    
    // Skip whitespace
    while (*p && isspace(*p)) p++;
    
    // Get table name
    int i = 0;
    while (*p && !isspace(*p) && *p != '(' && i < 31) {
        table_name[i++] = *p++;
    }
    table_name[i] = '\0';
    
    // Check if table already exists
    TableSchema schema;
    if (find_table_schema(table_name, &schema) == 0) {
        send_error_response("Table already exists");
        return -1;
    }
    
    // Find opening parenthesis
    while (*p && *p != '(') p++;
    if (*p != '(') {
        send_error_response("Invalid CREATE TABLE syntax: missing opening parenthesis");
        return -1;
    }
    p++; // Skip '('
    
    // Parse column definitions
    TableSchema new_schema;
    strcpy(new_schema.name, table_name);
    new_schema.num_columns = 0;
    
    while (*p && *p != ')') {
        // Skip whitespace
        while (*p && isspace(*p)) p++;
        
        // Get column name
        i = 0;
        while (*p && !isspace(*p) && *p != ',' && i < 31) {
            new_schema.columns[new_schema.num_columns].name[i++] = *p++;
        }
        new_schema.columns[new_schema.num_columns].name[i] = '\0';
        
        // Skip whitespace
        while (*p && isspace(*p)) p++;
        
        // Get column type
        if (strncasecmp(p, "char", 4) == 0) {
            new_schema.columns[new_schema.num_columns].type = TYPE_CHAR;
            p += 4; // Skip "char"
            
            // Parse size in char(N)
            while (*p && *p != '(') p++;
            if (*p != '(') {
                send_error_response("Invalid char type: missing size");
                return -1;
            }
            p++; // Skip '('
            
            char size_str[10];
            i = 0;
            while (*p && isdigit(*p) && i < 9) {
                size_str[i++] = *p++;
            }
            size_str[i] = '\0';
            
            new_schema.columns[new_schema.num_columns].size = atoi(size_str);
            
            // Skip to closing parenthesis
            while (*p && *p != ')') p++;
            if (*p != ')') {
                send_error_response("Invalid char type: missing closing parenthesis");
                return -1;
            }
            p++; // Skip ')'
        } else if (strncasecmp(p, "smallint", 8) == 0) {
            new_schema.columns[new_schema.num_columns].type = TYPE_SMALLINT;
            new_schema.columns[new_schema.num_columns].size = 4; // 4-byte fixed size
            p += 8; // Skip "smallint"
        } else if (strncasecmp(p, "int", 3) == 0 || strncasecmp(p, "integer", 7) == 0) {
            new_schema.columns[new_schema.num_columns].type = TYPE_INTEGER;
            new_schema.columns[new_schema.num_columns].size = 8; // 8-byte fixed size
            p += (strncasecmp(p, "int", 3) == 0) ? 3 : 7; // Skip "int" or "integer"
        } else {
            send_error_response("Invalid column type");
            return -1;
        }
        
        new_schema.num_columns++;
        
        // Skip to comma or closing parenthesis
        while (*p && isspace(*p)) p++;
        if (*p == ',') {
            p++; // Skip ','
        } else if (*p != ')') {
            send_error_response("Invalid CREATE TABLE syntax: expected comma or closing parenthesis");
            return -1;
        }
    }
    
    if (new_schema.num_columns == 0) {
        send_error_response("No columns defined for table");
        return -1;
    }
    
    // Create schema file if it doesn't exist
    int schema_fd = open("schema.dat", O_RDWR | O_CREAT, 0644);
    if (schema_fd < 0) {
        send_error_response("Failed to create schema file");
        return -1;
    }
    
    // Find a free block or create a new one
    int block_num = find_free_block(schema_fd);
    if (block_num < 0) {
        block_num = create_new_block(schema_fd);
        if (block_num < 0) {
            close(schema_fd);
            send_error_response("Failed to create schema block");
            return -1;
        }
    }
    
    // Write schema to block
    char block[BLOCK_SIZE];
    memset(block, '.', BLOCK_SIZE); // Fill with dots for clarity in examples
    
    // Format schema string: tablename|col1:type1,col2:type2,...;
    char schema_str[BLOCK_SIZE - 8]; // Leave room for next block pointer
    int pos = sprintf(schema_str, "%s|", new_schema.name);
    
    for (i = 0; i < new_schema.num_columns; i++) {
        if (i > 0) {
            pos += sprintf(schema_str + pos, ",");
        }
        
        if (new_schema.columns[i].type == TYPE_CHAR) {
            pos += sprintf(schema_str + pos, "%s:char(%d)", 
                          new_schema.columns[i].name, 
                          new_schema.columns[i].size);
        } else if (new_schema.columns[i].type == TYPE_SMALLINT) {
            pos += sprintf(schema_str + pos, "%s:smallint", 
                          new_schema.columns[i].name);
        } else if (new_schema.columns[i].type == TYPE_INTEGER) {
            pos += sprintf(schema_str + pos, "%s:int", 
                          new_schema.columns[i].name);
        }
    }
    
    schema_str[pos] = ';';
    schema_str[pos + 1] = '\0';
    
    // Copy schema string to block
    strncpy(block, schema_str, strlen(schema_str));
    
    // Set next block pointer to END_MARKER
    strncpy(block + BLOCK_SIZE - 4, END_MARKER, 4);
    
    // Write block to file
    if (write_block(schema_fd, block_num, block) < 0) {
        close(schema_fd);
        send_error_response("Failed to write schema block");
        return -1;
    }
    
    close(schema_fd);
    
    // Create table data file
    char data_filename[64];
    sprintf(data_filename, "%s.dat", table_name);
    
    int data_fd = open(data_filename, O_RDWR | O_CREAT, 0644);
    if (data_fd < 0) {
        send_error_response("Failed to create table data file");
        return -1;
    }
    
    // Initialize first block
    memset(block, '.', BLOCK_SIZE);
    strncpy(block + BLOCK_SIZE - 4, END_MARKER, 4);
    
    if (write_block(data_fd, 0, block) < 0) {
        close(data_fd);
        send_error_response("Failed to initialize table data file");
        return -1;
    }
    
    close(data_fd);
    
    // Send success response
    char response[256];
    sprintf(response, "Table %s created successfully", table_name);
    send_http_response("text/plain", response);
    
    return 0;
}

// Execute INSERT INTO SQL command
int execute_insert(char *sql) {
    // Example: INSERT INTO movies VALUES (2, 'Lyle, Lyle, Crocodile', 100);
    char table_name[32];
    char *p = strstr(sql, "INSERT INTO");
    
    if (p == NULL) {
        send_error_response("Invalid INSERT syntax");
        return -1;
    }
    
    p += 11; // Skip "INSERT INTO"
    
    // Skip whitespace
    while (*p && isspace(*p)) p++;
    
    // Get table name
    int i = 0;
    while (*p && !isspace(*p) && i < 31) {
        table_name[i++] = *p++;
    }
    table_name[i] = '\0';
    
    // Verify table exists and get schema
    TableSchema schema;
    if (find_table_schema(table_name, &schema) != 0) {
        send_error_response("Table does not exist");
        return -1;
    }
    
    // Look for VALUES keyword
    p = strstr(p, "VALUES");
    if (p == NULL) {
        send_error_response("Invalid INSERT syntax: missing VALUES keyword");
        return -1;
    }
    
    p += 6; // Skip "VALUES"
    
    // Find opening parenthesis
    while (*p && *p != '(') p++;
    if (*p != '(') {
        send_error_response("Invalid INSERT syntax: missing opening parenthesis");
        return -1;
    }
    p++; // Skip '('
    
    // Parse values
    char values[MAX_COLS][256];
    int value_count = 0;
    
    while (*p && *p != ')' && value_count < MAX_COLS) {
        // Skip whitespace
        while (*p && isspace(*p)) p++;
        
        if (*p == '\'' || *p == '"') {
            // String value
            char quote = *p;
            p++; // Skip quote
            
            i = 0;
            while (*p && *p != quote && i < 255) {
                values[value_count][i++] = *p++;
            }
            values[value_count][i] = '\0';
            
            if (*p != quote) {
                send_error_response("Invalid string value: missing closing quote");
                return -1;
            }
            p++; // Skip closing quote
        } else {
            // Numeric value
            i = 0;
            while (*p && *p != ',' && *p != ')' && !isspace(*p) && i < 255) {
                values[value_count][i++] = *p++;
            }
            values[value_count][i] = '\0';
        }
        
        value_count++;
        
        // Skip whitespace
        while (*p && isspace(*p)) p++;
        
        if (*p == ',') {
            p++; // Skip ','
        } else if (*p != ')') {
            send_error_response("Invalid INSERT syntax: expected comma or closing parenthesis");
            return -1;
        }
    }
    
    if (value_count != schema.num_columns) {
        send_error_response("Number of values does not match number of columns");
        return -1;
    }
    
    // Open table data file
    char data_filename[64];
    sprintf(data_filename, "%s.dat", table_name);
    
    int data_fd = open(data_filename, O_RDWR);
    if (data_fd < 0) {
        send_error_response("Failed to open table data file");
        return -1;
    }
    
    // Find the last block
    char block[BLOCK_SIZE];
    int block_num = 0;
    int last_block = 0;
    
    while (1) {
        if (read_block(data_fd, block_num, block) < 0) {
            close(data_fd);
            send_error_response("Failed to read data block");
            return -1;
        }
        
        last_block = block_num;
        
        // Check if there's a next block
        char next_block[5];
        strncpy(next_block, block + BLOCK_SIZE - 4, 4);
        next_block[4] = '\0';
        
        if (strcmp(next_block, END_MARKER) == 0) {
            break; // This is the last block
        }
        
        block_num = atoi(next_block);
    }
    
    // Calculate record size
    int record_size = 0;
    for (i = 0; i < schema.num_columns; i++) {
        record_size += schema.columns[i].size;
    }
    
    // Find position to insert new record
    int pos = 0;
    while (pos < BLOCK_SIZE - 4 && block[pos] != '.') {
        pos++;
    }
    
    // Check if there's enough space in the current block
    if (pos + record_size > BLOCK_SIZE - 4) {
        // Need a new block
        int new_block_num = create_new_block(data_fd);
        if (new_block_num < 0) {
            close(data_fd);
            send_error_response("Failed to create new data block");
            return -1;
        }
        
        // Update current block to point to new block
        char next_block_str[5];
        sprintf(next_block_str, "%04d", new_block_num);
        strncpy(block + BLOCK_SIZE - 4, next_block_str, 4);
        
        if (write_block(data_fd, last_block, block) < 0) {
            close(data_fd);
            send_error_response("Failed to update last block");
            return -1;
        }
        
        // Use new block for record
        memset(block, '.', BLOCK_SIZE);
        strncpy(block + BLOCK_SIZE - 4, END_MARKER, 4);
        pos = 0;
        last_block = new_block_num;
    }
    
    // Format and insert record data
    char record[BLOCK_SIZE];
    int record_pos = 0;
    
    for (i = 0; i < schema.num_columns; i++) {
        if (schema.columns[i].type == TYPE_CHAR) {
            // Fixed-length char field, pad with spaces
            int len = strlen(values[i]);
            int j;
            
            for (j = 0; j < schema.columns[i].size; j++) {
                if (j < len) {
                    record[record_pos++] = values[i][j];
                } else {
                    record[record_pos++] = ' ';
                }
            }
        } else if (schema.columns[i].type == TYPE_SMALLINT) {
            // 4-byte integer stored as fixed-width string
            sprintf(record + record_pos, "%04d", atoi(values[i]));
            record_pos += 4;
        } else if (schema.columns[i].type == TYPE_INTEGER) {
            // 8-byte integer stored as fixed-width string
            sprintf(record + record_pos, "%08d", atoi(values[i]));
            record_pos += 8;
        }
    }
    
    // Copy record to block
    strncpy(block + pos, record, record_pos);
    
    // Write block back to file
    if (write_block(data_fd, last_block, block) < 0) {
        close(data_fd);
        send_error_response("Failed to write data block");
        return -1;
    }
    
    close(data_fd);
    
    // Send success response
    char response[256];
    sprintf(response, "Record inserted successfully into table %s", table_name);
    send_http_response("text/plain", response);
    
    return 0;
}

