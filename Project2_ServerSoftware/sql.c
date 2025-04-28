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

// data types
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

// structure to store column information
typedef struct
{
    char name[32];
    int type;
    int size;
} Column;

// structure to store table schema
typedef struct
{
    char name[32];
    int num_columns;
    Column columns[MAX_COLS];
} TableSchema;

// structure for WHERE clause condition
typedef struct
{
    char column_name[32];
    int op;
    char value[256];
} Condition;

// functions
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

#ifdef UNIT_TEST
/**
 * runs unit tests to verify functionality of database operations
 * tests with multipule CREATE TABLE, INSERT, SELECT, UPDATE, and DELETE operations
 * prints the results of each test to stdout
 */
void run_unit_tests()
{
    printf("running unit tests...\n");

    // test CREATE TABLE
    printf("test CREATE TABLE: ");
    char create_sql[] = "CREATE TABLE test_table (id smallint, name char(20), age int)";
    if (execute_create(create_sql) == 0)
    {
        printf("\nPASSED\n");
    }
    else
    {
        printf("\nFAILED\n");
    }

    // test INSERT
    printf("test INSERT: ");
    char insert_sql[] = "INSERT INTO test_table VALUES (1, 'John Doe', 30)";
    if (execute_insert(insert_sql) == 0)
    {
        printf("\nPASSED\n");
    }
    else
    {
        printf("\nFAILED\n");
    }

    // test SELECT
    printf("test SELECT: ");
    char select_sql[] = "SELECT * FROM test_table";
    if (execute_select(select_sql) == 0)
    {
        printf("\nPASSED\n");
    }
    else
    {
        printf("\nFAILED\n");
    }

    // test UPDATE
    printf("test UPDATE: ");
    char update_sql[] = "UPDATE test_table SET age = 35 WHERE id = 1";
    if (execute_update(update_sql) == 0)
    {
        printf("\nPASSED\n");
    }
    else
    {
        printf("\nFAILED\n");
    }

    // test DELETE
    printf("test DELETE: ");
    char delete_sql[] = "DELETE FROM test_table WHERE id = 1";
    if (execute_delete(delete_sql) == 0)
    {
        printf("\nPASSED\n");
    }
    else
    {
        printf("\nFAILED\n");
    }

    printf("unit tests completed\n");
}
#endif

/**
 * main entry for the application
 * in unit test mode, runs the unit tests
 * in normal CGI mode, processes a SQL query from the QUERY_STRING environment variable
 *
 * @return 0 on success, 1 on error
 */
int main(void)
{
#ifdef UNIT_TEST
    run_unit_tests();
    return 0;
#else
    // CGI processing
    char *query_string = getenv("QUERY_STRING");

    if (query_string == NULL)
    {
        send_error_response("No SQL query provided");
        return 1;
    }

    // URL-encoded query string
    char sql[MAX_QUERY_LEN];
    int i = 0, j = 0;

    while (query_string[i] && j < MAX_QUERY_LEN - 1)
    {
        if (query_string[i] == '+')
        {
            sql[j++] = ' ';
        }
        else if (query_string[i] == '%' && query_string[i + 1] && query_string[i + 2])
        {
            // Handle URL encoding (e.g., %20 = space)
            char hex[3] = {query_string[i + 1], query_string[i + 2], 0};
            sql[j++] = (char)strtol(hex, NULL, 16);
            i += 2;
        }
        else
        {
            sql[j++] = query_string[i];
        }
        i++;
    }
    sql[j] = '\0';

    // execute SQL command
    int command_type;
    int result = parse_sql_command(sql, &command_type);

    if (result != 0)
    {
        send_error_response("Failed to parse SQL command");
        return 1;
    }

    switch (command_type)
    {
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

    if (result != 0)
    {
        send_error_response("Error executing SQL command");
        return 1;
    }

    return 0;
#endif
}

/**
 * case-insensitive of strstr that searches for needle in haystack
 *
 * @param haystack string to search in
 * @param needle substring to search for
 * @return pointer to the first occurrence of needle in haystack, or NULL if not found
 */
char *strncasestr(const char *haystack, const char *needle)
{
    size_t needle_len = strlen(needle);

    while (*haystack)
    {
        if (strncasecmp(haystack, needle, needle_len) == 0)
        {
            return (char *)haystack;
        }
        haystack++;
    }

    return NULL;
}

/**
 * sends HTTP response with the specified content type and body
 *
 * @param content_type The HTTP content type (e.g., "text/plain")
 * @param body The response body content
 */
void send_http_response(char *content_type, char *body)
{
    printf("Content-Type: %s\r\n", content_type);
    printf("Content-Length: %zu\r\n\r\n", strlen(body));
    printf("%s", body);
    fflush(stdout);
}

/**
 * sends an HTTP error response with the specified error message.
 *
 * @param error_msg error message to include in the response
 */
void send_error_response(char *error_msg)
{
    char body[1024];
    sprintf(body, "SQL Error: %s", error_msg);
    send_http_response("text/plain", body);
}

/**
 * parse SQL command string and determines its type
 *
 * @param sql The SQL command string to parse
 * @param command_type Pointer to store the determined command type
 * @return 0 on success, -1 if command type is unknown
 */
int parse_sql_command(char *sql, int *command_type)
{
    if (strncasecmp(sql, "CREATE", 6) == 0)
    {
        *command_type = CMD_CREATE;
    }
    else if (strncasecmp(sql, "INSERT", 6) == 0)
    {
        *command_type = CMD_INSERT;
    }
    else if (strncasecmp(sql, "UPDATE", 6) == 0)
    {
        *command_type = CMD_UPDATE;
    }
    else if (strncasecmp(sql, "SELECT", 6) == 0)
    {
        *command_type = CMD_SELECT;
    }
    else if (strncasecmp(sql, "DELETE", 6) == 0)
    {
        *command_type = CMD_DELETE;
    }
    else
    {
        return -1; // base case
    }

    return 0;
}

/**
 * creates a new block in the database file.
 *
 * @param fd file descriptor of the database file
 * @return block number of the newly created block, or -1 on error
 */
int create_new_block(int fd)
{
    struct stat st;

    if (fstat(fd, &st) < 0)
    {
        return -1;
    }

    int block_num = st.st_size / BLOCK_SIZE;

    char block[BLOCK_SIZE];
    memset(block, '.', BLOCK_SIZE);
    strncpy(block + BLOCK_SIZE - 4, END_MARKER, 4);

    if (write_block(fd, block_num, block) < 0)
    {
        return -1;
    }

    return block_num;
}

/**
 * reads a block from the database file.
 *
 * @param fd file descriptor of the database file
 * @param block_num block number to read
 * @param block buffer to store the read block data
 * @return 0 on success, -1 on error
 */
int read_block(int fd, int block_num, char *block)
{
    off_t offset = block_num * BLOCK_SIZE;

    if (lseek(fd, offset, SEEK_SET) < 0)
    {
        return -1;
    }

    if (read(fd, block, BLOCK_SIZE) != BLOCK_SIZE)
    {
        return -1;
    }

    return 0;
}

/**
 * writes a block to the database file.
 *
 * @param fd file descriptor of the database file
 * @param block_num block number to write to
 * @param block block data to write
 * @return 0 on success, -1 on error
 */
int write_block(int fd, int block_num, char *block)
{
    off_t offset = block_num * BLOCK_SIZE;

    if (lseek(fd, offset, SEEK_SET) < 0)
    {
        return -1;
    }

    if (write(fd, block, BLOCK_SIZE) != BLOCK_SIZE)
    {
        return -1;
    }

    return 0;
}

/**
 * finds a free block in the database file.
 *
 * @param fd file descriptor of the database file
 * @return block number of a free block, or -1 if no free blocks found
 */
int find_free_block(int fd)
{
    struct stat st;

    if (fstat(fd, &st) < 0)
    {
        return -1;
    }

    int num_blocks = st.st_size / BLOCK_SIZE;
    char block[BLOCK_SIZE];

    for (int i = 0; i < num_blocks; i++)
    {
        if (read_block(fd, i, block) < 0)
        {
            continue;
        }

        // check if block is empty
        int empty = 1;
        for (int j = 0; j < BLOCK_SIZE - 4; j++)
        {
            if (block[j] != '.')
            {
                empty = 0;
                break;
            }
        }

        if (empty)
        {
            return i;
        }
    }

    return -1;
}

/**
 * finds the schema for a specified table.
 *
 * @param table_name name of the table to find
 * @param schema pointer to store the found schema
 * @return 0 on success, -1 if table not found
 */
int find_table_schema(char *table_name, TableSchema *schema)
{
    int schema_fd = open("schema.dat", O_RDONLY);
    if (schema_fd < 0)
    {
        return -1; // schema file DNE, so table DNE
    }

    char block[BLOCK_SIZE];
    int block_num = 0;

    while (1)
    {
        if (read_block(schema_fd, block_num, block) < 0)
        {
            close(schema_fd);
            return -1;
        }

        // look for table name in this block
        char *schema_str = block;
        char *table_str = strtok(schema_str, "|");

        while (table_str != NULL)
        {
            if (strcmp(table_str, table_name) == 0)
            {
                strcpy(schema->name, table_name);

                char *columns_str = strtok(NULL, ";");
                if (columns_str == NULL)
                {
                    close(schema_fd);
                    return -1;
                }

                char *columns[MAX_COLS];
                int num_columns = 0;

                char *column = strtok(columns_str, ",");
                while (column != NULL && num_columns < MAX_COLS)
                {
                    columns[num_columns++] = column;
                    column = strtok(NULL, ",");
                }

                schema->num_columns = num_columns;

                for (int i = 0; i < num_columns; i++)
                {
                    char *name = strtok(columns[i], ":");
                    char *type = strtok(NULL, "");

                    if (name != NULL && type != NULL)
                    {
                        strcpy(schema->columns[i].name, name);

                        if (strncmp(type, "char(", 5) == 0)
                        {
                            schema->columns[i].type = TYPE_CHAR;
                            schema->columns[i].size = atoi(type + 5);
                        }
                        else if (strcmp(type, "smallint") == 0)
                        {
                            schema->columns[i].type = TYPE_SMALLINT;
                            schema->columns[i].size = 4;
                        }
                        else if (strcmp(type, "int") == 0)
                        {
                            schema->columns[i].type = TYPE_INTEGER;
                            schema->columns[i].size = 8;
                        }
                    }
                }

                close(schema_fd);
                return 0;
            }

            table_str = strtok(NULL, "|");
        }

        // check if there's a next block
        char next_block[5];
        strncpy(next_block, block + BLOCK_SIZE - 4, 4);
        next_block[4] = '\0';

        if (strcmp(next_block, END_MARKER) == 0)
        {
            break;
        }

        block_num = atoi(next_block);
    }

    close(schema_fd);
    return -1;
}

/**
 * executes a CREATE TABLE SQL command
 *
 * @param sql CREATE TABLE SQL command string
 * @return 0 on success, -1 on error
 */
int execute_create(char *sql)
{
    char table_name[32];
    char *p = strstr(sql, "CREATE TABLE");

    if (p == NULL)
    {
        send_error_response("Invalid CREATE TABLE syntax");
        return -1;
    }

    p += 12; // skip "CREATE TABLE"

    // skip whitespace
    while (*p && isspace(*p))
        p++;

    // get table name
    int i = 0;
    while (*p && !isspace(*p) && *p != '(' && i < 31)
    {
        table_name[i++] = *p++;
    }
    table_name[i] = '\0';

    // check if table already exists
    TableSchema schema;
    if (find_table_schema(table_name, &schema) == 0)
    {
        send_error_response("table already exists");
        return -1;
    }

    // find opening parenthesis
    while (*p && *p != '(')
        p++;
    if (*p != '(')
    {
        send_error_response("invalid CREATE syntax: missing opening parenthesis");
        return -1;
    }
    p++; // skip '('

    // parse column definitions
    TableSchema new_schema;
    strcpy(new_schema.name, table_name);
    new_schema.num_columns = 0;

    while (*p && *p != ')')
    {
        // skip whitespace
        while (*p && isspace(*p))
            p++;

        // get column name
        i = 0;
        while (*p && !isspace(*p) && *p != ',' && i < 31)
        {
            new_schema.columns[new_schema.num_columns].name[i++] = *p++;
        }
        new_schema.columns[new_schema.num_columns].name[i] = '\0';

        // skip whitespace
        while (*p && isspace(*p))
            p++;

        // get column type
        if (strncasecmp(p, "char", 4) == 0)
        {
            new_schema.columns[new_schema.num_columns].type = TYPE_CHAR;
            p += 4; // skip "char"

            // parse size in char(N)
            while (*p && *p != '(')
                p++;
            if (*p != '(')
            {
                send_error_response("Invalid char type: missing size");
                return -1;
            }
            p++; // skip '('

            char size_str[10];
            i = 0;
            while (*p && isdigit(*p) && i < 9)
            {
                size_str[i++] = *p++;
            }
            size_str[i] = '\0';

            new_schema.columns[new_schema.num_columns].size = atoi(size_str);

            // skip to closing parenthesis
            while (*p && *p != ')')
                p++;
            if (*p != ')')
            {
                send_error_response("invalid char type: missing closing parenthesis");
                return -1;
            }
            p++; // skip ')'
        }
        else if (strncasecmp(p, "smallint", 8) == 0)
        {
            new_schema.columns[new_schema.num_columns].type = TYPE_SMALLINT;
            new_schema.columns[new_schema.num_columns].size = 4; // 4-byte fixed size
            p += 8;                                              // skip "smallint"
        }
        else if (strncasecmp(p, "int", 3) == 0 || strncasecmp(p, "integer", 7) == 0)
        {
            new_schema.columns[new_schema.num_columns].type = TYPE_INTEGER;
            new_schema.columns[new_schema.num_columns].size = 8; // 8-byte fixed size
            p += (strncasecmp(p, "int", 3) == 0) ? 3 : 7;        // skip "int" or "integer"
        }
        else
        {
            send_error_response("invalid column type");
            return -1;
        }

        new_schema.num_columns++;

        // skip to comma or closing parenthesis
        while (*p && isspace(*p))
            p++;
        if (*p == ',')
        {
            p++; // skip ','
        }
        else if (*p != ')')
        {
            send_error_response("invalid CREATE TABLE syntax: expected comma or closing parenthesis");
            return -1;
        }
    }

    if (new_schema.num_columns == 0)
    {
        send_error_response("no columns defined for table");
        return -1;
    }

    // create schema file if it doesn't exist
    int schema_fd = open("schema.dat", O_RDWR | O_CREAT, 0644);
    if (schema_fd < 0)
    {
        send_error_response("failed to create schema file");
        return -1;
    }

    // find a free block or create a new one
    int block_num = find_free_block(schema_fd);
    if (block_num < 0)
    {
        block_num = create_new_block(schema_fd);
        if (block_num < 0)
        {
            close(schema_fd);
            send_error_response("failed to create schema block");
            return -1;
        }
    }

    // write schema to block
    char block[BLOCK_SIZE];
    memset(block, '.', BLOCK_SIZE); // fill with dots for clarity in examples

    // format schema string: tablename|col1:type1,col2:type2,...;
    char schema_str[BLOCK_SIZE - 8];
    int pos = sprintf(schema_str, "%s|", new_schema.name);

    for (i = 0; i < new_schema.num_columns; i++)
    {
        if (i > 0)
        {
            pos += sprintf(schema_str + pos, ",");
        }

        if (new_schema.columns[i].type == TYPE_CHAR)
        {
            pos += sprintf(schema_str + pos, "%s:char(%d)",
                           new_schema.columns[i].name,
                           new_schema.columns[i].size);
        }
        else if (new_schema.columns[i].type == TYPE_SMALLINT)
        {
            pos += sprintf(schema_str + pos, "%s:smallint",
                           new_schema.columns[i].name);
        }
        else if (new_schema.columns[i].type == TYPE_INTEGER)
        {
            pos += sprintf(schema_str + pos, "%s:int",
                           new_schema.columns[i].name);
        }
    }

    schema_str[pos] = ';';
    schema_str[pos + 1] = '\0';

    // cp schema string to block
    strncpy(block, schema_str, strlen(schema_str));

    // next block pointer to END_MARKER
    strncpy(block + BLOCK_SIZE - 4, END_MARKER, 4);

    // block to file
    if (write_block(schema_fd, block_num, block) < 0)
    {
        close(schema_fd);
        send_error_response("failed to write schema block");
        return -1;
    }

    close(schema_fd);

    // create table data file
    char data_filename[64];
    sprintf(data_filename, "%s.dat", table_name);

    int data_fd = open(data_filename, O_RDWR | O_CREAT, 0644);
    if (data_fd < 0)
    {
        send_error_response("failed to create table data file");
        return -1;
    }

    // first block
    memset(block, '.', BLOCK_SIZE);
    strncpy(block + BLOCK_SIZE - 4, END_MARKER, 4);

    if (write_block(data_fd, 0, block) < 0)
    {
        close(data_fd);
        send_error_response("failed to initialize table data file");
        return -1;
    }

    close(data_fd);

    // send success response
    char response[256];
    sprintf(response, "table %s created successfully", table_name);
    send_http_response("text/plain", response);

    return 0;
}

/**
 * executes INSERT INTO SQL command
 *
 * @param sql INSERT INTO SQL command string
 * @return 0 on success, -1 on error
 */
int execute_insert(char *sql)
{
    char table_name[32];
    char *p = strstr(sql, "INSERT INTO");

    if (p == NULL)
    {
        send_error_response("invalid INSERT syntax");
        return -1;
    }

    p += 11; // skip "INSERT INTO"

    // skip whitespace
    while (*p && isspace(*p))
        p++;

    // get table name
    int i = 0;
    while (*p && !isspace(*p) && i < 31)
    {
        table_name[i++] = *p++;
    }
    table_name[i] = '\0';

    // verify table exists and get schema
    TableSchema schema;
    if (find_table_schema(table_name, &schema) != 0)
    {
        send_error_response("Table does not exist");
        return -1;
    }

    // look for VALUES keyword
    p = strstr(p, "VALUES");
    if (p == NULL)
    {
        send_error_response("Invalid INSERT syntax: missing VALUES keyword");
        return -1;
    }

    p += 6; // skip "VALUES"

    // find opening parenthesis
    while (*p && *p != '(')
        p++;
    if (*p != '(')
    {
        send_error_response("Invalid INSERT syntax: missing opening parenthesis");
        return -1;
    }
    p++; // skip '('

    // parse values
    char values[MAX_COLS][256];
    int value_count = 0;

    while (*p && *p != ')' && value_count < MAX_COLS)
    {
        // skip whitespace
        while (*p && isspace(*p))
            p++;

        if (*p == '\'' || *p == '"')
        {
            // string value
            char quote = *p;
            p++; // skip quote

            i = 0;
            while (*p && *p != quote && i < 255)
            {
                values[value_count][i++] = *p++;
            }
            values[value_count][i] = '\0';

            if (*p != quote)
            {
                send_error_response("invalid string value: missing closing quote");
                return -1;
            }
            p++; // skip closing quote
        }
        else
        {
            // num value
            i = 0;
            while (*p && *p != ',' && *p != ')' && !isspace(*p) && i < 255)
            {
                values[value_count][i++] = *p++;
            }
            values[value_count][i] = '\0';
        }

        value_count++;

        // skip whitespace
        while (*p && isspace(*p))
            p++;

        if (*p == ',')
        {
            p++; // Skip ','
        }
        else if (*p != ')')
        {
            send_error_response("invalid INSERT syntax: expected comma or closing parenthesis");
            return -1;
        }
    }

    if (value_count != schema.num_columns)
    {
        send_error_response("number of values does not match number of columns");
        return -1;
    }

    // table data file
    char data_filename[64];
    sprintf(data_filename, "%s.dat", table_name);

    int data_fd = open(data_filename, O_RDWR);
    if (data_fd < 0)
    {
        send_error_response("failed to open table data file");
        return -1;
    }

    // find the last block
    char block[BLOCK_SIZE];
    int block_num = 0;
    int last_block = 0;

    while (1)
    {
        if (read_block(data_fd, block_num, block) < 0)
        {
            close(data_fd);
            send_error_response("failed to read data block");
            return -1;
        }

        last_block = block_num;

        // check if there is next block
        char next_block[5];
        strncpy(next_block, block + BLOCK_SIZE - 4, 4);
        next_block[4] = '\0';

        if (strcmp(next_block, END_MARKER) == 0)
        {
            break; // last block
        }

        block_num = atoi(next_block);
    }

    // calculate record size
    int record_size = 0;
    for (i = 0; i < schema.num_columns; i++)
    {
        record_size += schema.columns[i].size;
    }

    // find pos to insert new record
    int pos = 0;
    while (pos < BLOCK_SIZE - 4 && block[pos] != '.')
    {
        pos++;
    }

    // check if there is enough space
    if (pos + record_size > BLOCK_SIZE - 4)
    {
        // new block
        int new_block_num = create_new_block(data_fd);
        if (new_block_num < 0)
        {
            close(data_fd);
            send_error_response("failed to create new data block");
            return -1;
        }

        // update current block to point to new block
        char next_block_str[5];
        sprintf(next_block_str, "%04d", new_block_num);
        strncpy(block + BLOCK_SIZE - 4, next_block_str, 4);

        if (write_block(data_fd, last_block, block) < 0)
        {
            close(data_fd);
            send_error_response("failed to update last block");
            return -1;
        }

        // new block for record
        memset(block, '.', BLOCK_SIZE);
        strncpy(block + BLOCK_SIZE - 4, END_MARKER, 4);
        pos = 0;
        last_block = new_block_num;
    }

    // format and insert record data
    char record[BLOCK_SIZE];
    int record_pos = 0;

    for (i = 0; i < schema.num_columns; i++)
    {
        if (schema.columns[i].type == TYPE_CHAR)
        {
            // pad with spaces
            int len = strlen(values[i]);
            int j;

            for (j = 0; j < schema.columns[i].size; j++)
            {
                if (j < len)
                {
                    record[record_pos++] = values[i][j];
                }
                else
                {
                    record[record_pos++] = ' ';
                }
            }
        }
        else if (schema.columns[i].type == TYPE_SMALLINT)
        {
            // 4-byte integer stored as fixed-width string
            sprintf(record + record_pos, "%04d", atoi(values[i]));
            record_pos += 4;
        }
        else if (schema.columns[i].type == TYPE_INTEGER)
        {
            // 8-byte integer stored as fixed-width string
            sprintf(record + record_pos, "%08d", atoi(values[i]));
            record_pos += 8;
        }
    }

    // cp record to block
    strncpy(block + pos, record, record_pos);

    // w block back to file
    if (write_block(data_fd, last_block, block) < 0)
    {
        close(data_fd);
        send_error_response("failed to write data block");
        return -1;
    }

    close(data_fd);

    // send success response
    char response[256];
    sprintf(response, "record inserted into table %s", table_name);
    send_http_response("text/plain", response);

    return 0;
}

/**
 * executes UPDATE SQL command
 *
 * @param sql UPDATE SQL command string
 * @return 0 on success, -1 on error
 */
int execute_update(char *sql)
{
    char table_name[32];
    char set_column[32];
    char set_value[256];
    Condition condition;
    int has_condition = 0;

    // [P]arse the UPDATE command
    char *p = strstr(sql, "UPDATE");
    if (p == NULL)
    {
        send_error_response("invalid UPDATE syntax");
        return -1;
    }

    p += 6; // skip "UPDATE"

    // skip whitespace
    while (*p && isspace(*p))
        p++;

    // get table name
    int i = 0;
    while (*p && !isspace(*p) && i < 31)
    {
        table_name[i++] = *p++;
    }
    table_name[i] = '\0';

    // verify table exists and get schema
    TableSchema schema;
    if (find_table_schema(table_name, &schema) != 0)
    {
        send_error_response("Table does not exist");
        return -1;
    }

    // look for SET keyword
    p = strstr(p, "SET");
    if (p == NULL)
    {
        send_error_response("Invalid UPDATE syntax: missing SET keyword");
        return -1;
    }

    p += 3; // skip "SET"

    // skip whitespace
    while (*p && isspace(*p))
        p++;

    // get column name to update
    i = 0;
    while (*p && !isspace(*p) && *p != '=' && i < 31)
    {
        set_column[i++] = *p++;
    }
    set_column[i] = '\0';

    // skip to = sign
    while (*p && *p != '=')
        p++;
    if (*p != '=')
    {
        send_error_response("invalid UPDATE syntax: missing = after column name");
        return -1;
    }
    p++; // skip '='

    // skip whitespace
    while (*p && isspace(*p))
        p++;

    // get value to set
    i = 0;
    if (*p == '\'' || *p == '"')
    {
        // string value
        char quote = *p;
        p++; // skip

        while (*p && *p != quote && i < 255)
        {
            set_value[i++] = *p++;
        }

        if (*p != quote)
        {
            send_error_response("invalid string value: missing closing quote");
            return -1;
        }
        p++; // skip closing
    }
    else
    {
        // num value
        while (*p && !isspace(*p) && *p != ';' && *p != 'W' && i < 255)
        {
            set_value[i++] = *p++;
        }
    }
    set_value[i] = '\0';

    // check for WHERE
    p = strstr(p, "WHERE");
    if (p != NULL)
    {
        p += 5; // skip "WHERE"
        has_condition = 1;

        // Skip whitespace
        while (*p && isspace(*p))
            p++;

        // get column name in condition
        i = 0;
        while (*p && !isspace(*p) && *p != '=' && *p != '<' && *p != '>' && *p != '!' && i < 31)
        {
            condition.column_name[i++] = *p++;
        }
        condition.column_name[i] = '\0';

        // skip whitespace
        while (*p && isspace(*p))
            p++;

        // get operator
        if (*p == '=')
        {
            condition.op = OP_EQUAL;
            p++;
        }
        else if (*p == '!' && *(p + 1) == '=')
        {
            condition.op = OP_NOT_EQUAL;
            p += 2;
        }
        else if (*p == '>')
        {
            condition.op = OP_GREATER;
            p++;
        }
        else if (*p == '<')
        {
            condition.op = OP_LESS;
            p++;
        }
        else
        {
            send_error_response("Invalid operator in WHERE clause");
            return -1;
        }

        // skip whitespace
        while (*p && isspace(*p))
            p++;

        // get condition value
        i = 0;
        if (*p == '\'' || *p == '"')
        {
            // str value
            char quote = *p;
            p++; // skip quote

            while (*p && *p != quote && i < 255)
            {
                condition.value[i++] = *p++;
            }

            if (*p != quote)
            {
                send_error_response("invalid string value: missing closing quote");
                return -1;
            }
        }
        else
        {
            // numeric value
            while (*p && !isspace(*p) && *p != ';' && i < 255)
            {
                condition.value[i++] = *p++;
            }
        }
        condition.value[i] = '\0';
    }

    // table data file
    char data_filename[64];
    sprintf(data_filename, "%s.dat", table_name);

    int data_fd = open(data_filename, O_RDWR);
    if (data_fd < 0)
    {
        send_error_response("failed to open table data file");
        return -1;
    }

    // find column index to update
    int col_idx = -1;
    for (i = 0; i < schema.num_columns; i++)
    {
        if (strcmp(schema.columns[i].name, set_column) == 0)
        {
            col_idx = i;
            break;
        }
    }

    if (col_idx == -1)
    {
        close(data_fd);
        send_error_response("column not found in table");
        return -1;
    }

    // find condition column index
    int cond_col_idx = -1;
    if (has_condition)
    {
        for (i = 0; i < schema.num_columns; i++)
        {
            if (strcmp(schema.columns[i].name, condition.column_name) == 0)
            {
                cond_col_idx = i;
                break;
            }
        }

        if (cond_col_idx == -1)
        {
            close(data_fd);
            send_error_response("condition column not found in table");
            return -1;
        }
    }

    // process blocks and update records
    char block[BLOCK_SIZE];
    int block_num = 0;
    int records_updated = 0;

    while (1)
    {
        if (read_block(data_fd, block_num, block) < 0)
        {
            break;
        }

        // calculate offset to start of first record
        int pos = 0;
        int updated_block = 0;

        while (pos < BLOCK_SIZE - 4)
        {
            // skip dots (empty space)
            if (block[pos] == '.')
            {
                pos++;
                continue;
            }

            // check if reached the end of records
            char test_byte = block[pos];
            if (test_byte == '\0' || test_byte == '.')
            {
                break;
            }

            // found a record
            int match = 1;
            if (has_condition)
            {
                // calc offset to condition column value
                int cond_offset = 0;
                for (i = 0; i < cond_col_idx; i++)
                {
                    cond_offset += schema.columns[i].size;
                }

                // condition column value
                char cond_value[256];
                int cond_size = schema.columns[cond_col_idx].size;
                strncpy(cond_value, block + pos + cond_offset, cond_size);
                cond_value[cond_size] = '\0';

                if (schema.columns[cond_col_idx].type == TYPE_CHAR)
                {
                    int j = cond_size - 1;
                    while (j >= 0 && cond_value[j] == ' ')
                    {
                        cond_value[j--] = '\0';
                    }
                }

                if (schema.columns[cond_col_idx].type == TYPE_SMALLINT ||
                    schema.columns[cond_col_idx].type == TYPE_INTEGER)
                {
                    // num comparison
                    int db_num = atoi(cond_value);
                    int cond_num = atoi(condition.value);

                    switch (condition.op)
                    {
                    case OP_EQUAL:
                        match = (db_num == cond_num);
                        break;
                    case OP_NOT_EQUAL:
                        match = (db_num != cond_num);
                        break;
                    case OP_GREATER:
                        match = (db_num > cond_num);
                        break;
                    case OP_LESS:
                        match = (db_num < cond_num);
                        break;
                    }
                }
                else
                {
                    // str comparison
                    switch (condition.op)
                    {
                    case OP_EQUAL:
                        match = (strcmp(cond_value, condition.value) == 0);
                        break;
                    case OP_NOT_EQUAL:
                        match = (strcmp(cond_value, condition.value) != 0);
                        break;
                    case OP_GREATER:
                        match = (strcmp(cond_value, condition.value) > 0);
                        break;
                    case OP_LESS:
                        match = (strcmp(cond_value, condition.value) < 0);
                        break;
                    }
                }
            }

            if (match)
            {
                int update_offset = 0;
                for (i = 0; i < col_idx; i++)
                {
                    update_offset += schema.columns[i].size;
                }

                if (schema.columns[col_idx].type == TYPE_CHAR)
                {
                    int char_size = schema.columns[col_idx].size;
                    memset(block + pos + update_offset, ' ', char_size);
                    int set_len = strlen(set_value);
                    strncpy(block + pos + update_offset, set_value, set_len < char_size ? set_len : char_size);
                }
                else if (schema.columns[col_idx].type == TYPE_SMALLINT)
                {
                    sprintf(block + pos + update_offset, "%04d", atoi(set_value));
                }
                else if (schema.columns[col_idx].type == TYPE_INTEGER)
                {
                    sprintf(block + pos + update_offset, "%08d", atoi(set_value));
                }

                records_updated++;
                updated_block = 1;
            }

            int record_size = 0;
            for (i = 0; i < schema.num_columns; i++)
            {
                record_size += schema.columns[i].size;
            }
            pos += record_size;
        }

        if (updated_block)
        {
            if (write_block(data_fd, block_num, block) < 0)
            {
                close(data_fd);
                send_error_response("failed to write updated block");
                return -1;
            }
        }

        char next_block[5];
        strncpy(next_block, block + BLOCK_SIZE - 4, 4);
        next_block[4] = '\0';

        if (strcmp(next_block, END_MARKER) == 0)
        {
            break; // no more blocks
        }

        block_num = atoi(next_block);
    }

    close(data_fd);

    char response[256];
    sprintf(response, "Updated %d record(s) in table %s", records_updated, table_name);
    send_http_response("text/plain", response);

    return 0;
}

/**
 * Executes a SELECT SQL command.
 *
 * @param sql The SELECT SQL command string
 * @return 0 on success, -1 on error
 * example: SELECT * FROM movies WHERE id = 1;
 *
 */
int execute_select(char *sql)
{
    char table_name[32];
    char *columns[MAX_COLS];
    int num_columns = 0;
    Condition condition;
    int has_condition = 0;

    char *p = strstr(sql, "SELECT");
    if (p == NULL)
    {
        send_error_response("invalid SELECT syntax");
        return -1;
    }

    p += 6; // skip "SELECT"

    // skip whitespace
    while (*p && isspace(*p))
        p++;

    // check if SELECT *
    int select_all = 0;
    if (*p == '*')
    {
        select_all = 1;
        p++; // skip '*'

        // skip to FROM
        while (*p && strncasecmp(p, "FROM", 4) != 0)
            p++;
    }
    else
    {
        // parse column list
        char *col_start = p;
        while (*p && strncasecmp(p, "FROM", 4) != 0)
            p++;

        if (strncasecmp(p, "FROM", 4) != 0)
        {
            send_error_response("Invalid SELECT syntax: missing FROM");
            return -1;
        }

        char save_char = *p;
        *p = '\0';

        char *token = strtok(col_start, ",");
        while (token != NULL && num_columns < MAX_COLS)
        {
            while (*token && isspace(*token))
                token++;

            columns[num_columns++] = token;

            token = strtok(NULL, ",");
        }

        *p = save_char;
    }

    p += 4; // skip "FROM"

    // skip whitespace
    while (*p && isspace(*p))
        p++;

    // get table name
    int i = 0;
    while (*p && !isspace(*p) && *p != ';' && *p != 'W' && i < 31)
    {
        table_name[i++] = *p++;
    }
    table_name[i] = '\0';

    TableSchema schema;
    if (find_table_schema(table_name, &schema) != 0)
    {
        send_error_response("Table does not exist");
        return -1;
    }

    p = strstr(p, "WHERE");
    if (p != NULL)
    {
        p += 5; // skip "WHERE"
        has_condition = 1;

        while (*p && isspace(*p))
            p++;

        i = 0;
        while (*p && !isspace(*p) && *p != '=' && *p != '<' && *p != '>' && *p != '!' && i < 31)
        {
            condition.column_name[i++] = *p++;
        }
        condition.column_name[i] = '\0';

        while (*p && isspace(*p))
            p++;

        if (*p == '=')
        {
            condition.op = OP_EQUAL;
            p++;
        }
        else if (*p == '!' && *(p + 1) == '=')
        {
            condition.op = OP_NOT_EQUAL;
            p += 2;
        }
        else if (*p == '>')
        {
            condition.op = OP_GREATER;
            p++;
        }
        else if (*p == '<')
        {
            condition.op = OP_LESS;
            p++;
        }
        else
        {
            send_error_response("invalid operator in WHERE clause");
            return -1;
        }

        while (*p && isspace(*p))
            p++;

        i = 0;
        if (*p == '\'' || *p == '"')
        {
            char quote = *p;
            p++; // skip quote

            while (*p && *p != quote && i < 255)
            {
                condition.value[i++] = *p++;
            }

            if (*p != quote)
            {
                send_error_response("invalid string value: missing closing quote");
                return -1;
            }
        }
        else
        {
            while (*p && !isspace(*p) && *p != ';' && i < 255)
            {
                condition.value[i++] = *p++;
            }
        }
        condition.value[i] = '\0';
    }

    char data_filename[64];
    sprintf(data_filename, "%s.dat", table_name);

    int data_fd = open(data_filename, O_RDONLY);
    if (data_fd < 0)
    {
        send_error_response("Failed to open table data file");
        return -1;
    }

    int selected_columns[MAX_COLS];
    int column_offsets[MAX_COLS];
    int num_selected = 0;

    if (select_all)
    {
        num_selected = schema.num_columns;
        for (i = 0; i < num_selected; i++)
        {
            selected_columns[i] = i;

            column_offsets[i] = 0;
            for (int j = 0; j < i; j++)
            {
                column_offsets[i] += schema.columns[j].size;
            }
        }
    }
    else
    {
        for (i = 0; i < num_columns; i++)
        {
            int col_idx = -1;
            for (int j = 0; j < schema.num_columns; j++)
            {
                char *col_name = columns[i];
                while (*col_name && isspace(*col_name))
                    col_name++;

                if (strcmp(col_name, schema.columns[j].name) == 0)
                {
                    col_idx = j;
                    break;
                }
            }

            if (col_idx == -1)
            {
                close(data_fd);
                send_error_response("column not found in table");
                return -1;
            }

            selected_columns[num_selected] = col_idx;

            column_offsets[num_selected] = 0;
            for (int j = 0; j < col_idx; j++)
            {
                column_offsets[num_selected] += schema.columns[j].size;
            }

            num_selected++;
        }
    }

    int cond_col_idx = -1;
    int cond_offset = 0;
    if (has_condition)
    {
        for (i = 0; i < schema.num_columns; i++)
        {
            if (strcmp(schema.columns[i].name, condition.column_name) == 0)
            {
                cond_col_idx = i;
                break;
            }
        }

        if (cond_col_idx == -1)
        {
            close(data_fd);
            send_error_response("condition column not found in table");
            return -1;
        }

        // calculate offset to condition column
        for (i = 0; i < cond_col_idx; i++)
        {
            cond_offset += schema.columns[i].size;
        }
    }

    // calculate record size
    int record_size = 0;
    for (i = 0; i < schema.num_columns; i++)
    {
        record_size += schema.columns[i].size;
    }

    char response[4096];
    char *resp_ptr = response;
    int resp_len = 0;

    resp_len += sprintf(resp_ptr + resp_len, "Results from table %s:\n", table_name);
    for (i = 0; i < num_selected; i++)
    {
        int col_idx = selected_columns[i];
        resp_len += sprintf(resp_ptr + resp_len, "%s", schema.columns[col_idx].name);
        if (i < num_selected - 1)
        {
            resp_len += sprintf(resp_ptr + resp_len, " | ");
        }
    }
    resp_len += sprintf(resp_ptr + resp_len, "\n");

    for (i = 0; i < num_selected; i++)
    {
        int col_idx = selected_columns[i];
        for (int j = 0; j < strlen(schema.columns[col_idx].name); j++)
        {
            resp_len += sprintf(resp_ptr + resp_len, "-");
        }
        if (i < num_selected - 1)
        {
            resp_len += sprintf(resp_ptr + resp_len, "-+-");
        }
    }
    resp_len += sprintf(resp_ptr + resp_len, "\n");

    // process blocks and retrieve records
    char block[BLOCK_SIZE];
    int block_num = 0;
    int records_found = 0;

    while (1)
    {
        if (read_block(data_fd, block_num, block) < 0)
        {
            break;
        }

        // process records in this block
        int pos = 0;

        while (pos <= BLOCK_SIZE - 4 - record_size)
        {
            while (pos < BLOCK_SIZE - 4 && (block[pos] == '.' || block[pos] == '\0'))
            {
                pos++;
            }

            if (pos > BLOCK_SIZE - 4 - record_size)
            {
                break;
            }

            // found a valid record, check if it matches the condition
            int match = 1;
            if (has_condition)
            {
                char cond_value[256];
                int cond_size = schema.columns[cond_col_idx].size;
                strncpy(cond_value, block + pos + cond_offset, cond_size);
                cond_value[cond_size] = '\0';

                if (schema.columns[cond_col_idx].type == TYPE_CHAR)
                {
                    int j = cond_size - 1;
                    while (j >= 0 && cond_value[j] == ' ')
                    {
                        cond_value[j--] = '\0';
                    }
                }

                // compare values based on type and operator
                if (schema.columns[cond_col_idx].type == TYPE_SMALLINT ||
                    schema.columns[cond_col_idx].type == TYPE_INTEGER)
                {
                    int db_num = atoi(cond_value);
                    int cond_num = atoi(condition.value);

                    switch (condition.op)
                    {
                    case OP_EQUAL:
                        match = (db_num == cond_num);
                        break;
                    case OP_NOT_EQUAL:
                        match = (db_num != cond_num);
                        break;
                    case OP_GREATER:
                        match = (db_num > cond_num);
                        break;
                    case OP_LESS:
                        match = (db_num < cond_num);
                        break;
                    }
                }
                else
                {
                    switch (condition.op)
                    {
                    case OP_EQUAL:
                        match = (strcmp(cond_value, condition.value) == 0);
                        break;
                    case OP_NOT_EQUAL:
                        match = (strcmp(cond_value, condition.value) != 0);
                        break;
                    case OP_GREATER:
                        match = (strcmp(cond_value, condition.value) > 0);
                        break;
                    case OP_LESS:
                        match = (strcmp(cond_value, condition.value) < 0);
                        break;
                    }
                }
            }

            if (match)
            {
                records_found++;

                for (i = 0; i < num_selected; i++)
                {
                    int col_idx = selected_columns[i];
                    int offset = column_offsets[i];
                    int size = schema.columns[col_idx].size;

                    char value[256];
                    strncpy(value, block + pos + offset, size);
                    value[size] = '\0';

                    if (schema.columns[col_idx].type == TYPE_CHAR)
                    {
                        int j = size - 1;
                        while (j >= 0 && value[j] == ' ')
                        {
                            value[j--] = '\0';
                        }
                    }

                    resp_len += sprintf(resp_ptr + resp_len, "%s", value);
                    if (i < num_selected - 1)
                    {
                        resp_len += sprintf(resp_ptr + resp_len, " | ");
                    }
                }
                resp_len += sprintf(resp_ptr + resp_len, "\n");
            }

            // move to next pos
            pos += record_size;
        }

        char next_block[5];
        strncpy(next_block, block + BLOCK_SIZE - 4, 4);
        next_block[4] = '\0';

        if (strcmp(next_block, END_MARKER) == 0)
        {
            break; // no more blocks
        }

        block_num = atoi(next_block);
    }

    close(data_fd);

    resp_len += sprintf(resp_ptr + resp_len, "%d record(s) found.\n", records_found);

    send_http_response("text/plain", response);

    return 0;
}

/**
 * Executes a DELETE SQL command.
 *
 * @param sql The DELETE SQL command string
 * @return 0 on success, -1 on error
 * example: DELETE FROM movies WHERE id = 1;
 */
int execute_delete(char *sql)
{
    char table_name[32];
    Condition condition;
    int has_condition = 0;

    char *p = strstr(sql, "DELETE FROM");
    if (p == NULL)
    {
        send_error_response("Invalid DELETE syntax");
        return -1;
    }

    p += 11; // skip "DELETE FROM"

    // skip whitespace
    while (*p && isspace(*p))
        p++;

    // get table name
    int i = 0;
    while (*p && !isspace(*p) && *p != ';' && *p != 'W' && i < 31)
    {
        table_name[i++] = *p++;
    }
    table_name[i] = '\0';

    // verify table exists and get schema
    TableSchema schema;
    if (find_table_schema(table_name, &schema) != 0)
    {
        send_error_response("Table does not exist");
        return -1;
    }

    // check for WHERE clause
    p = strstr(p, "WHERE");
    if (p != NULL)
    {
        p += 5; // skip "WHERE"
        has_condition = 1;

        // Skip whitespace
        while (*p && isspace(*p))
            p++;

        // get column name
        i = 0;
        while (*p && !isspace(*p) && *p != '=' && *p != '<' && *p != '>' && *p != '!' && i < 31)
        {
            condition.column_name[i++] = *p++;
        }
        condition.column_name[i] = '\0';

        // skip whitespace
        while (*p && isspace(*p))
            p++;

        // get operator
        if (*p == '=')
        {
            condition.op = OP_EQUAL;
            p++;
        }
        else if (*p == '!' && *(p + 1) == '=')
        {
            condition.op = OP_NOT_EQUAL;
            p += 2;
        }
        else if (*p == '>')
        {
            condition.op = OP_GREATER;
            p++;
        }
        else if (*p == '<')
        {
            condition.op = OP_LESS;
            p++;
        }
        else
        {
            send_error_response("Invalid operator in WHERE clause");
            return -1;
        }

        // skip whitespace
        while (*p && isspace(*p))
            p++;

        // get condition value
        i = 0;
        if (*p == '\'' || *p == '"')
        {
            // str value
            char quote = *p;
            p++; // skip quote

            while (*p && *p != quote && i < 255)
            {
                condition.value[i++] = *p++;
            }

            if (*p != quote)
            {
                send_error_response("invalid string value: missing closing quote");
                return -1;
            }
        }
        else
        {
            while (*p && !isspace(*p) && *p != ';' && i < 255)
            {
                condition.value[i++] = *p++;
            }
        }
        condition.value[i] = '\0';
    }

    char data_filename[64];
    sprintf(data_filename, "%s.dat", table_name);

    int data_fd = open(data_filename, O_RDWR);
    if (data_fd < 0)
    {
        send_error_response("Failed to open table data file");
        return -1;
    }

    int cond_col_idx = -1;
    int cond_offset = 0;
    if (has_condition)
    {
        for (i = 0; i < schema.num_columns; i++)
        {
            if (strcmp(schema.columns[i].name, condition.column_name) == 0)
            {
                cond_col_idx = i;
                break;
            }
        }

        if (cond_col_idx == -1)
        {
            close(data_fd);
            send_error_response("condition column not found in table");
            return -1;
        }

        // calc offset to condition column
        for (i = 0; i < cond_col_idx; i++)
        {
            cond_offset += schema.columns[i].size;
        }
    }

    // calculate record size
    int record_size = 0;
    for (i = 0; i < schema.num_columns; i++)
    {
        record_size += schema.columns[i].size;
    }

    // process blocks and delete records
    char block[BLOCK_SIZE];
    int block_num = 0;
    int records_deleted = 0;

    while (1)
    {
        if (read_block(data_fd, block_num, block) < 0)
        {
            break;
        }

        // process records in this block
        int pos = 0;
        int updated_block = 0;

        while (pos < BLOCK_SIZE - 4)
        {
            if (block[pos] == '.')
            {
                pos++;
                continue;
            }

            char test_byte = block[pos];
            if (pos >= BLOCK_SIZE - 4 || (test_byte == '\0' && pos % record_size == 0))
            {
                break;
            }

            int match = 1;
            if (has_condition)
            {
                char cond_value[256];
                int cond_size = schema.columns[cond_col_idx].size;
                strncpy(cond_value, block + pos + cond_offset, cond_size);
                cond_value[cond_size] = '\0';

                if (schema.columns[cond_col_idx].type == TYPE_CHAR)
                {
                    int j = cond_size - 1;
                    while (j >= 0 && cond_value[j] == ' ')
                    {
                        cond_value[j--] = '\0';
                    }
                }

                // compare values based on type and operator
                if (schema.columns[cond_col_idx].type == TYPE_SMALLINT ||
                    schema.columns[cond_col_idx].type == TYPE_INTEGER)
                {
                    int db_num = atoi(cond_value);
                    int cond_num = atoi(condition.value);

                    switch (condition.op)
                    {
                    case OP_EQUAL:
                        match = (db_num == cond_num);
                        break;
                    case OP_NOT_EQUAL:
                        match = (db_num != cond_num);
                        break;
                    case OP_GREATER:
                        match = (db_num > cond_num);
                        break;
                    case OP_LESS:
                        match = (db_num < cond_num);
                        break;
                    }
                }
                else
                {
                    switch (condition.op)
                    {
                    case OP_EQUAL:
                        match = (strcmp(cond_value, condition.value) == 0);
                        break;
                    case OP_NOT_EQUAL:
                        match = (strcmp(cond_value, condition.value) != 0);
                        break;
                    case OP_GREATER:
                        match = (strcmp(cond_value, condition.value) > 0);
                        break;
                    case OP_LESS:
                        match = (strcmp(cond_value, condition.value) < 0);
                        break;
                    }
                }
            }

            if (match)
            {
                memset(block + pos, '.', record_size);
                records_deleted++;
                updated_block = 1;
            }

            pos += record_size;
        }

        if (updated_block)
        {
            if (write_block(data_fd, block_num, block) < 0)
            {
                close(data_fd);
                send_error_response("Failed to write updated block");
                return -1;
            }
        }

        char next_block[5];
        strncpy(next_block, block + BLOCK_SIZE - 4, 4);
        next_block[4] = '\0';

        if (strcmp(next_block, END_MARKER) == 0)
        {
            break; // no more blocks
        }

        block_num = atoi(next_block);
    }

    close(data_fd);

    char response[256];
    sprintf(response, "Deleted %d record(s) from table %s", records_deleted, table_name);
    send_http_response("text/plain", response);

    return 0;
}