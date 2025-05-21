
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "s2html_event.h"

#define SIZE_OF_SYMBOLS (sizeof(symbols))
#define SIZE_OF_OPERATORS (sizeof(operators))
#define WORD_BUFF_SIZE	100

int user_flag;
/********** Internal states and event of parser **********/
typedef enum
{
	PSTATE_IDLE,
	PSTATE_PREPROCESSOR_DIRECTIVE,
	PSTATE_SUB_PREPROCESSOR_MAIN,
	PSTATE_SUB_PREPROCESSOR_RESERVE_KEYWORD,
	PSTATE_SUB_PREPROCESSOR_ASCII_CHAR,
	PSTATE_HEADER_FILE,
	PSTATE_RESERVE_KEYWORD,
	PSTATE_NUMERIC_CONSTANT,
	PSTATE_STRING,
	PSTATE_SINGLE_LINE_COMMENT,
	PSTATE_MULTI_LINE_COMMENT,
	PSTATE_ASCII_CHAR
}pstate_e;

/********** global variables **********/

/* parser state variable */
static pstate_e state = PSTATE_IDLE;

/* sub state is used only in preprocessor state */
static pstate_e state_sub = PSTATE_SUB_PREPROCESSOR_MAIN;

/* event variable to store event and related properties */
static pevent_t pevent_data;
static int event_data_idx = 0;

static char word[WORD_BUFF_SIZE];
static int word_idx = 0;


static char* res_kwords_data[] = {"const", "volatile", "extern", "auto", "register",
   						   "static", "signed", "unsigned", "short", "long", 
						   "double", "char", "int", "float", "struct", 
						   "union", "enum", "void", "typedef","FILE",""
						  };

static char* res_kwords_non_data[] = {"goto", "return", "continue", "break", 
							   "if", "else", "for", "while", "do", 
							   "switch", "case", "default","sizeof", ""
							  };

static char operators[] = {'/', '+', '*', '-', '%', '=', '<', '>', '~', '&', ',', '!', '^', '|'};
static char symbols[] = {'(', ')', '{', '[', ':',';'};

/********** state handlers **********/
pevent_t * pstate_idle_handler(FILE *fd, int ch);
pevent_t * pstate_single_line_comment_handler(FILE *fd, int ch);
pevent_t * pstate_multi_line_comment_handler(FILE *fd, int ch);
pevent_t * pstate_numeric_constant_handler(FILE *fd, int ch);
pevent_t * pstate_string_handler(FILE *fd, int ch);
pevent_t * pstate_header_file_handler(FILE *fd, int ch);
pevent_t * pstate_ascii_char_handler(FILE *fd, int ch);
pevent_t * pstate_reserve_keyword_handler(FILE *fd, int ch);
pevent_t * pstate_preprocessor_directive_handler(FILE *fd, int ch);
pevent_t * pstate_sub_preprocessor_main_handler(FILE *fd, int ch);

/********** Utility functions **********/

/* function to check if given word is reserved key word */
static int is_reserved_keyword(char *word)
{
	int idx = 0;

	/* search for data type reserved keyword */
	while(*res_kwords_data[idx])
	{
		if(strcmp(res_kwords_data[idx++], word) == 0)
			return RES_KEYWORD_DATA;
	}

	idx = 0; // reset index
	/* search for non data type reserved key word */
	while(*res_kwords_non_data[idx])
	{
		if(strcmp(res_kwords_non_data[idx++], word) == 0)
			return RES_KEYWORD_NON_DATA;
	}

	return 0; // word did not match, return false
}

/* function to check symbols */
static int is_symbol(char c)
{
	int idx;
	for(idx = 0; idx < SIZE_OF_SYMBOLS; idx++)
	{
		if(symbols[idx] == c)
			return 1;
	}

	return 0;
}

/* function to check operator */
static int is_operator(char c)
{
	int idx;
	for(idx = 0; idx < SIZE_OF_OPERATORS; idx++)
	{
		if(operators[idx] == c)
			return 1;
	}

	return 0;
}

/* to set parser event */
static void set_parser_event(pstate_e s, pevent_e e)
{
	pevent_data.data[event_data_idx] = '\0';
	pevent_data.length = event_data_idx;
	event_data_idx = 0;
	state = s;
	pevent_data.type = e;
}


/************ Event functions **********/

/* This function parses the source file and generate 
 * event based on parsed characters and string
 */
pevent_t *get_parser_event(FILE *fd)
{
	int ch, pre_ch;
	pevent_t *evptr = NULL;
	/* Read char by char */
	while((ch = fgetc(fd)) != EOF)
	{
#ifdef DEBUG
	//	putchar(ch);
#endif
		switch(state)
		{
			case PSTATE_IDLE :
				if((evptr = pstate_idle_handler(fd, ch)) != NULL)
					return evptr;
				break;
			case PSTATE_SINGLE_LINE_COMMENT :
				if((evptr = pstate_single_line_comment_handler(fd, ch)) != NULL)
					return evptr;
				break;
			case PSTATE_MULTI_LINE_COMMENT :
				if((evptr = pstate_multi_line_comment_handler(fd, ch)) != NULL)
					return evptr;
				break;
			case PSTATE_PREPROCESSOR_DIRECTIVE :
				if((evptr = pstate_preprocessor_directive_handler(fd, ch)) != NULL)
					return evptr;
				break;
			case PSTATE_RESERVE_KEYWORD :
				if((evptr = pstate_reserve_keyword_handler(fd, ch)) != NULL)
					return evptr;
				break;
			case PSTATE_NUMERIC_CONSTANT :
				if((evptr = pstate_numeric_constant_handler(fd, ch)) != NULL)
					return evptr;
				break;
			case PSTATE_STRING :
				if((evptr = pstate_string_handler(fd, ch)) != NULL)
					return evptr;
				break;
			case PSTATE_HEADER_FILE :
				if((evptr = pstate_header_file_handler(fd, ch)) != NULL)
					return evptr;
				break;
			case PSTATE_ASCII_CHAR :
				if((evptr = pstate_ascii_char_handler(fd, ch)) != NULL)
					return evptr;
				break;
			default : 
				printf("unknown state\n");
				state = PSTATE_IDLE;
				break;
		}
	}

	/* end of file is reached, move back to idle state and set EOF event */
	set_parser_event(PSTATE_IDLE, PEVENT_EOF);

	return &pevent_data; // return final event
}


/********** IDLE state Handler **********
 * Idle state handler identifies
 ****************************************/

pevent_t * pstate_idle_handler(FILE *fd, int ch)
{
	
	int pre_ch;
	switch(ch)
	{
		case '\'' : // begining of ASCII char 
			state = PSTATE_ASCII_CHAR;
			pevent_data.data[event_data_idx++] = ch;
			break;

		case '/' :
			pre_ch = ch;
			if((ch = fgetc(fd)) == '*') // multi line comment
			{
				if(event_data_idx) // we have regular exp in buffer first process that
				{
					fseek(fd, -2L, SEEK_CUR); // unget chars
					set_parser_event(PSTATE_IDLE, PEVENT_REGULAR_EXP);
					return &pevent_data;
				}
				else //	multi line comment begin 
				{
#ifdef DEBUG	
					printf("Multi line comment Begin : /*\n");
#endif
					state = PSTATE_MULTI_LINE_COMMENT;
					pevent_data.data[event_data_idx++] = pre_ch;
					pevent_data.data[event_data_idx++] = ch;
				}
			}
			else if(ch == '/') // single line comment
			{
				if(event_data_idx) // we have regular exp in buffer first process that
				{
					fseek(fd, -2L, SEEK_CUR); // unget chars
					set_parser_event(PSTATE_IDLE, PEVENT_REGULAR_EXP);
					return &pevent_data;
				}
				else //	single line comment begin
				{
#ifdef DEBUG	
					printf("Single line comment Begin : //\n");
#endif
					state = PSTATE_SINGLE_LINE_COMMENT;
					pevent_data.data[event_data_idx++] = pre_ch;
					pevent_data.data[event_data_idx++] = ch;
				}
			}
			else // it is regular exp
			{
				pevent_data.data[event_data_idx++] = pre_ch;
				pevent_data.data[event_data_idx++] = ch;
			}
			break;
		case '#' :
				state = PSTATE_PREPROCESSOR_DIRECTIVE;
				pevent_data.data[event_data_idx++] = ch;
			break;
		case '\"' :
			state = PSTATE_STRING;
			pevent_data.data[event_data_idx++] = ch;
			break;

		case '0' ... '9' : // detect numeric constant
			state = PSTATE_NUMERIC_CONSTANT;
			pevent_data.data[event_data_idx++] = ch;
			break;

		case 'a' ... 'z' : // could be reserved key word
			state = PSTATE_RESERVE_KEYWORD;
			pevent_data.data[event_data_idx++] = ch;
			break;

		default : // Assuming common text starts by default.
			pevent_data.data[event_data_idx++] = ch;
			set_parser_event(PSTATE_IDLE,PEVENT_REGULAR_EXP);
			return &pevent_data;
			break;
	}
	return NULL;
}
pevent_t * pstate_preprocessor_directive_handler(FILE *fd, int ch)
{
	switch(ch)
	{
		case '\n' : pevent_data.data[event_data_idx++] = ch;
					set_parser_event(PSTATE_IDLE,PEVENT_PREPROCESSOR_DIRECTIVE);
					return &pevent_data;
		case '<' : 	ungetc(ch,fd);
					set_parser_event(PSTATE_HEADER_FILE,PEVENT_PREPROCESSOR_DIRECTIVE);
					return &pevent_data;
					break;
		case '0'...'9' : //pevent_data.data[event_data_idx++] = ch;
						set_parser_event(PSTATE_NUMERIC_CONSTANT,PEVENT_PREPROCESSOR_DIRECTIVE);
						ungetc(ch,fd);
						return &pevent_data;
		case '"' : set_parser_event(PSTATE_HEADER_FILE,PEVENT_PREPROCESSOR_DIRECTIVE);
					user_flag = 1;
					return &pevent_data;
					break;
		case '(' : set_parser_event(PSTATE_IDLE,PEVENT_PREPROCESSOR_DIRECTIVE);
					ungetc(ch,fd);
					return &pevent_data;
					break;
		default :  pevent_data.data[event_data_idx++] = ch;	
					break;
	}
	return NULL;
}

pevent_t * pstate_header_file_handler(FILE *fd, int ch)
{
    // If 'user_flag' is set, it indicates the start of a user-defined header file ("...").
    // We insert a double quote (") at the beginning.
    if(user_flag)
    {
        pevent_data.data[event_data_idx++] = '"';
        user_flag = 0;  // Reset the flag after marking the start.
    }

    // Handle special character '<' (beginning of a standard header file).
    if(ch == '<')
    {
        // Convert '<' to its HTML-safe equivalent "&lt;"
        pevent_data.data[event_data_idx++] = '&';
        pevent_data.data[event_data_idx++] = 'l';
        pevent_data.data[event_data_idx++] = 't';
    }
    // Handle special character '>' (end of a standard header file).
    else if(ch == '>')
    {
        // Convert '>' to its HTML-safe equivalent "&gt;"
        pevent_data.data[event_data_idx++] = '&';
        pevent_data.data[event_data_idx++] = 'g';
        pevent_data.data[event_data_idx++] = 't';

        // Mark parsing as complete and set event type to PEVENT_HEADER_FILE.
        set_parser_event(PSTATE_IDLE, PEVENT_HEADER_FILE);

        // Set property to indicate it was a standard header file.
        pevent_data.property = STD_HEADER_FILE;
        return &pevent_data;  // Return parsed event data.
    }
    // Handle user-defined header files enclosed in double quotes (").
    else if(ch == '"')
    {
        // Store the double quote character (").
        pevent_data.data[event_data_idx++] = ch;

        // Mark parsing as complete and set event type to PEVENT_HEADER_FILE.
        set_parser_event(PSTATE_IDLE, PEVENT_HEADER_FILE);

        // Set property to indicate it was a user-defined header file.
        pevent_data.property = USER_HEADER_FILE;
        return &pevent_data;  // Return parsed event data.
    }
    else
    {
        // Store regular characters inside the header file name.
        pevent_data.data[event_data_idx++] = ch;
    }

    // Return NULL to indicate parsing is still in progress.
    return NULL;
}
pevent_t * pstate_reserve_keyword_handler(FILE *fd, int ch)
{
    // Store the first character of the keyword
    pevent_data.data[event_data_idx++] = ch;

    // Read subsequent alphanumeric characters and underscores ('_') to form the keyword
    while(isalnum(ch = fgetc(fd)) || ch == '_')
    {
        pevent_data.data[event_data_idx++] = ch;
    }

    // Move file pointer back one position after reading the first non-matching character
    fseek(fd, -1L, SEEK_CUR);

    // Null-terminate the extracted keyword string
    pevent_data.data[event_data_idx] = '\0';

    // Check if the extracted string is a reserved keyword
    if(is_reserved_keyword(pevent_data.data) == RES_KEYWORD_DATA)
    {
        printf("DEBUG : %s\n", pevent_data.data);
        set_parser_event(PSTATE_IDLE, PEVENT_RESERVE_KEYWORD);
        pevent_data.property = RES_KEYWORD_DATA;
    }
    // Check if the extracted string is a reserved keyword (non-data type)
    else if(is_reserved_keyword(pevent_data.data) == RES_KEYWORD_NON_DATA)
    {
        printf("DEBUG_1 : %s\n", pevent_data.data);
        set_parser_event(PSTATE_IDLE, PEVENT_RESERVE_KEYWORD);
        pevent_data.property = RES_KEYWORD_NON_DATA;
    }
    // If not a reserved keyword, treat it as a regular expression
    else
    {
        set_parser_event(PSTATE_IDLE, PEVENT_REGULAR_EXP);
    }

    return &pevent_data;  // Return parsed event data
}

pevent_t * pstate_numeric_constant_handler(FILE *fd, int ch)
{
    // Store the first character of the numeric constant
    pevent_data.data[event_data_idx++] = ch;

    // If the first character is ';', it's not part of a numeric constant, so put it back
    if(ch == ';')
    {
        ungetc(ch, fd);  // Push the character back to the stream
        event_data_idx--; // Remove it from the stored data
    }

    // Read characters until a delimiter is found (';', '\n', ',', '(', ')')
    while((ch = fgetc(fd)) != ';' && ch != '\n' && ch != ',' && ch != '(' && ch != ')')
    {
        pevent_data.data[event_data_idx++] = ch;
    }

    // Move file pointer back by one position after reading the first non-matching character
    fseek(fd, -1L, SEEK_CUR);

    // Set the parser state to indicate a numeric constant has been processed
    set_parser_event(PSTATE_IDLE, PEVENT_NUMERIC_CONSTANT);

    // Return parsed numeric constant data
    return &pevent_data;
}


pevent_t * pstate_string_handler(FILE *fd, int ch)
{
    // If the first character is a double quote (starting a string)
    if(ch == '"')
    {
        pevent_data.data[event_data_idx++] = ch;  // Store the quote
        goto label;  // Jump to setting the parser event
    }

    // Store the first character if it's not a quote
    pevent_data.data[event_data_idx++] = ch;

    // Read and store characters until the closing double quote is found
    while((ch = fgetc(fd)) != '"')
    {
        pevent_data.data[event_data_idx++] = ch;
    }

    // Store the closing quote
    pevent_data.data[event_data_idx++] = ch;

label:
    // Set the parser state to indicate a string literal has been processed
    set_parser_event(PSTATE_IDLE, PEVENT_STRING);
    
    // Return parsed string data
    return &pevent_data;
}


pevent_t * pstate_single_line_comment_handler(FILE *fd, int ch)
{
	int pre_ch;
	switch(ch)
	{
		case '\n' : /* single line comment ends here */
#ifdef DEBUG	
			printf("\nSingle line comment end\n");
#endif
			pre_ch = ch;
			pevent_data.data[event_data_idx++] = ch;
			set_parser_event(PSTATE_IDLE, PEVENT_SINGLE_LINE_COMMENT);
			return &pevent_data;
		default :  // collect single line comment chars
			pevent_data.data[event_data_idx++] = ch;
			break;
	}

	return NULL;
}
pevent_t * pstate_multi_line_comment_handler(FILE *fd, int ch)
{
	int pre_ch;
	switch(ch)
	{
		case '*' : /* comment might end here */
			pre_ch = ch;
			pevent_data.data[event_data_idx++] = ch;
			if((ch = fgetc(fd)) == '/')
			{
#ifdef DEBUG	
				printf("\nMulti line comment End : */\n");
#endif
				pre_ch = ch;
				pevent_data.data[event_data_idx++] = ch;
				set_parser_event(PSTATE_IDLE, PEVENT_MULTI_LINE_COMMENT);
				return &pevent_data;
			}
			else // multi line comment string still continued
			{
				pevent_data.data[event_data_idx++] = ch;
			}
			break;
		case '/' :
			/* go back by two steps and read previous char */
			fseek(fd, -2L, SEEK_CUR); // move two steps back
			pre_ch = fgetc(fd); // read a char
			fgetc(fd); // to come back to current offset

			pevent_data.data[event_data_idx++] = ch;
			if(pre_ch == '*')
			{
				set_parser_event(PSTATE_IDLE, PEVENT_MULTI_LINE_COMMENT);
				return &pevent_data;
			}
			break;
		default :  // collect multi-line comment chars
			pevent_data.data[event_data_idx++] = ch;
			break;
	}

	return NULL;
}
pevent_t * pstate_ascii_char_handler(FILE *fd, int ch)
{
    // Store the first character in the parsing buffer
    pevent_data.data[event_data_idx++] = ch;

    // If the character is a single quote (indicating the end of a character literal)
    if(ch == '\'')
    {
        // Set the parser state to indicate an ASCII character literal has been processed
        set_parser_event(PSTATE_IDLE, PEVENT_ASCII_CHAR);
        return &pevent_data;  // Return the parsed character data
    }

    // Return NULL if the character literal is not yet fully parsed
    return NULL;
}
/**** End of file ****/
