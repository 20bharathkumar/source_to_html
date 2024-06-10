
#include <stdio.h>
#include <string.h>
#include "event.h"

#define SIZE_OF_SYMBOLS (sizeof(symbols))
#define SIZE_OF_OPERATORS (sizeof(operators))
#define WORD_BUFF_SIZE	100

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
static int event_data_idx=0;

static char word[WORD_BUFF_SIZE];
static int word_idx=0;


static char* res_kwords_data[] = {"const", "volatile", "extern", "auto", "register",
    "static", "signed", "unsigned", "short", "long", 
    "double", "char", "int", "float", "struct", 
    "union", "enum", "void", "typedef", ""
};

static char* res_kwords_non_data[] = {"goto", "return", "continue", "break", 
    "if", "else", "for", "while", "do", 
    "switch", "case", "default","sizeof", ""
};

static char operators[] = {'/', '+', '*', '-', '%', '=', '<', '>', '~', '&', ',', '!', '^', '|'};
static char symbols[] = {'(', ')', '{', '}',']','[', ':'};

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
    //state_sub = PSTATE_SUB_PREPROCESSOR_MAIN;
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
		//printf("key %c-n\n",ch);
		if((evptr = pstate_reserve_keyword_handler(fd, ch)) != NULL)
		    return evptr;
		break;
	    case PSTATE_NUMERIC_CONSTANT :
		//pevent_data.data[event_data_idx] = '\0';
		//printf("NEUMER %s-n\n",pevent_data.data);
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
	    if(event_data_idx) // we have regular exp in buffer first process that
	    {
		fseek(fd, -1L, SEEK_CUR); // unget chars
		set_parser_event(PSTATE_IDLE, PEVENT_REGULAR_EXP);
		return &pevent_data;
	    }
	    else
	    {
		pevent_data.data[event_data_idx++] = ch;
		state = PSTATE_ASCII_CHAR;
	    }
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
	    if(event_data_idx) // we have regular exp in buffer first process that
	    {
		fseek(fd, -1L, SEEK_CUR); // unget chars
		set_parser_event(PSTATE_IDLE, PEVENT_REGULAR_EXP);
		return &pevent_data;
	    }
	    else
	    {
		state = PSTATE_PREPROCESSOR_DIRECTIVE;
		pevent_data.data[event_data_idx++] = ch;
	    }
	    break;
	case '\"' :
	    if(event_data_idx) // we have regular exp in buffer first process that
	    {
		fseek(fd, -1L, SEEK_CUR); // unget chars
		set_parser_event(PSTATE_IDLE, PEVENT_REGULAR_EXP);
		//printf("data %s\n",pevent_data.data);
		return &pevent_data;
	    }
	    else
	    {
		state = PSTATE_STRING;
		pevent_data.data[event_data_idx++] = ch;
	    }
	    break;

	case '0' ... '9' : // detect numeric constant
		//printf("ch1111111111111111111111 %c\n",ch);
	    if(event_data_idx) // we have regular exp in buffer first process that
	    {
		fseek(fd, -1L, SEEK_CUR); // unget chars
		set_parser_event(PSTATE_IDLE, PEVENT_REGULAR_EXP);
		//printf("data %s\n",pevent_data.data);
		return &pevent_data;
	    }
	    else
	    {
		//printf("ch %c\n",ch);
		state = PSTATE_NUMERIC_CONSTANT;
		pevent_data.data[event_data_idx++] = ch;
	    }
	    break;

	case 'a' ... 'z' : // could be reserved key word
	    if(event_data_idx) // we have regular exp in buffer first process that
	    {
		fseek(fd, -1L, SEEK_CUR); // unget chars
		set_parser_event(PSTATE_IDLE, PEVENT_REGULAR_EXP);
		//printf("data %s\n",pevent_data.data);
		return &pevent_data;
	    }
	    else
	    {
		state = PSTATE_RESERVE_KEYWORD;
		pevent_data.data[event_data_idx++] = ch;
		//printf("ch = %c\n",ch);
	    }
	    break;
	default : // Assuming common text starts by default.
	    pevent_data.data[event_data_idx++] = ch;
	    break;
    }

    return NULL;
}
pevent_t * pstate_preprocessor_directive_handler(FILE *fd, int ch)
{
    int tch;
    switch(state_sub)
    {
	case PSTATE_SUB_PREPROCESSOR_MAIN :
	    return pstate_sub_preprocessor_main_handler(fd, ch);
	case PSTATE_SUB_PREPROCESSOR_RESERVE_KEYWORD :
	    return pstate_reserve_keyword_handler(fd, ch);
	case PSTATE_SUB_PREPROCESSOR_ASCII_CHAR :
	    return pstate_ascii_char_handler(fd, ch);
	default :
	    printf("unknown state\n");
	    state = PSTATE_IDLE;
    }

    return NULL;
}
pevent_t * pstate_sub_preprocessor_main_handler(FILE *fd, int ch)
{
    /* write a switch case here to detect several events here
     * This state is similar to Idle state with slight difference
     * in state transition.
     * return event data at the end of event
     * else return NULL
     */
    //printf("in main hand %c\n",ch);
    int pre_ch;
    switch(ch)
    {
	case '<'://for header files
	    event_data_idx = 0;
	    pevent_data.type = PSTATE_PREPROCESSOR_DIRECTIVE;
	    state = PSTATE_HEADER_FILE;
	    return &pevent_data;
	    break;
	case '"'://for user header file
		 //set_parser_event(PSTATE_HEADER_FILE,PEVENT_PREPROCESSOR_DIRECTIVE);
	    fseek(fd,-1L,SEEK_CUR);
	    event_data_idx = 0;
	    pevent_data.type = PSTATE_PREPROCESSOR_DIRECTIVE;
	    state = PSTATE_HEADER_FILE;
	    return &pevent_data;
	    break;
	case '0' ... '9'://for digits
	    pevent_data.data[event_data_idx] = '\0';
	    //printf("neu %s\n",pevent_data.data);
	    event_data_idx = 0;
	    pevent_data.type = PEVENT_PREPROCESSOR_DIRECTIVE;
	    state_sub = PSTATE_SUB_PREPROCESSOR_ASCII_CHAR;
	    return &pevent_data;
	case '\''://for characters
	    event_data_idx = 0;
	    pevent_data.type = PSTATE_PREPROCESSOR_DIRECTIVE;
	    state_sub = PSTATE_SUB_PREPROCESSOR_ASCII_CHAR;
	    return &pevent_data;
	    break;
	case 'a' ... 'z':/*when character appears*/
	    if(event_data_idx == 0)
	    {
		pevent_data.data[event_data_idx++] = ch;
		while((ch = fgetc(fd)) != '\n')
		{
		    if(is_symbol(ch))
		    {
			    pevent_data.data[event_data_idx] = '\0';
			    //printf("data %s\n",pevent_data.data);
			if(is_reserved_keyword(pevent_data.data) == RES_KEYWORD_DATA)
			{
			    set_parser_event(PSTATE_PREPROCESSOR_DIRECTIVE,PEVENT_RESERVE_KEYWORD);
			    fseek(fd, -1L, SEEK_CUR);
			    //state = PSTATE_PREPROCESSOR_DIRECTIVE;
			    pevent_data.property = RES_KEYWORD_DATA;
			    //printf("data %s",pevent_data.data);
			    return &pevent_data;
			}
			else if(is_reserved_keyword(pevent_data.data) == RES_KEYWORD_NON_DATA)
			{
			    set_parser_event(PSTATE_PREPROCESSOR_DIRECTIVE,PEVENT_RESERVE_KEYWORD);
			    fseek(fd, -1L, SEEK_CUR);
			    //state = PSTATE_PREPROCESSOR_DIRECTIVE;
			    pevent_data.property = RES_KEYWORD_NON_DATA;
			    //printf("data %s",pevent_data.data);
			    return &pevent_data;
			}
			else
			{
			    pevent_data.data[event_data_idx] = '\0';
			    set_parser_event(PSTATE_PREPROCESSOR_DIRECTIVE, PEVENT_PREPROCESSOR_DIRECTIVE);
			    //printf("preprocessor argc = %s-n\n",pevent_data.data);
			    fseek(fd, -1L, SEEK_CUR);
			    return &pevent_data;
			}

		    }
		    else
			pevent_data.data[event_data_idx++] = ch;
		}
		pevent_data.data[event_data_idx++] = ch;
		//pevent_data.type = PEVENT_PREPROCESSOR_DIRECTIVE;
		set_parser_event(PSTATE_PREPROCESSOR_DIRECTIVE, PEVENT_PREPROCESSOR_DIRECTIVE);
		//set_parser_event(PSTATE_PREPROCESSOR_DIRECTIVE, PEVENT_PREPROCESSOR_DIRECTIVE);
		//state_sub = PSTATE_SUB_PREPROCESSOR_RESERVE_KEYWORD;
		return &pevent_data;
	    }
	    else
		pevent_data.data[event_data_idx++] = ch;

	    break;
	case '\n'://for preprocessor directives
	    pevent_data.data[event_data_idx++] = ch;
	    set_parser_event(PSTATE_IDLE, PEVENT_PREPROCESSOR_DIRECTIVE);
	    return &pevent_data;
	    break;
	default:
	    pevent_data.data[event_data_idx++] = ch;
	    if(is_symbol(ch))
	    {
		set_parser_event(PSTATE_PREPROCESSOR_DIRECTIVE, PEVENT_PREPROCESSOR_DIRECTIVE);
		//state_sub = PSTATE_SUB_PREPROCESSOR_RESERVE_KEYWORD;
		return &pevent_data;
	    }
	    break;
    }
    return NULL;
}

pevent_t * pstate_header_file_handler(FILE *fd, int ch)
{
    /* write a switch case here to store header file name
     * return event data at the end of event
     * else return NULL
     */
    int pre_ch;
    switch(ch)
    {
	case '>':/*main preprocessor ends*/
	    //state = PSTATE_HEADER_FILE;
	    set_parser_event(PSTATE_IDLE, PEVENT_HEADER_FILE);
	    return &pevent_data;
	    break;
	case '"':/*user defined preprocessor ends here*/
	    pevent_data.data[event_data_idx++] = ch;
	    while((ch = fgetc(fd)) != '"')
	    {
		pevent_data.data[event_data_idx++] = ch;
	    }
	    pevent_data.data[event_data_idx++] = ch;
	    set_parser_event(PSTATE_IDLE, PEVENT_HEADER_FILE);
	    pevent_data.property = USER_HEADER_FILE;
	    return &pevent_data;
	    break;
	default:/*string continues*/
	    pevent_data.data[event_data_idx++] = ch;
	    break;
    }
    return NULL;
}
pevent_t * pstate_reserve_keyword_handler(FILE *fd, int ch)
{
    /* write a switch case here to store words
     * return event data at the end of event
     * else return NULL
     */
    //printf("daata key %c-n\n",ch);
    switch(ch)
    {
	case 'a' ... 'z':
	    pevent_data.data[event_data_idx++] = ch;
	    //printf("ch is --------------- %c\n",ch);
	    while((ch = fgetc(fd)) != '\n')
	    {
		if(is_operator(ch) || is_symbol(ch)|| ch == ';' || ch == ' ' || ch == '\t')
		{
		    pevent_data.data[event_data_idx] = '\0';
		    if(is_reserved_keyword(pevent_data.data) == RES_KEYWORD_DATA)
		    {
			set_parser_event(PSTATE_RESERVE_KEYWORD,PEVENT_RESERVE_KEYWORD);
			fseek(fd, -1L, SEEK_CUR);
			pevent_data.property = RES_KEYWORD_DATA;
			//printf("data %s",pevent_data.data);
			return &pevent_data;
		    }
		    else if(is_reserved_keyword(pevent_data.data) == RES_KEYWORD_NON_DATA)
		    {
			set_parser_event(PSTATE_IDLE,PEVENT_RESERVE_KEYWORD);
			fseek(fd, -1L, SEEK_CUR);
			pevent_data.property = RES_KEYWORD_NON_DATA;
			//printf("non data %s\n",pevent_data.data);
			return &pevent_data;
		    }
		    else
		    {
			pevent_data.data[event_data_idx] = ch;
			set_parser_event(PSTATE_IDLE, PEVENT_REGULAR_EXP);
			//printf("keyword argc = %s-n\n",pevent_data.data);
			fseek(fd, -1L, SEEK_CUR);
			return &pevent_data;
		    }
		}
		else
		{
		    pevent_data.data[event_data_idx++] = ch;
		    pevent_data.data[event_data_idx] = '\0';
		    //printf("answer %s\n",pevent_data.data);
		}
	    }
	    //when new line character occurs
	    pevent_data.data[event_data_idx] = '\0';
	    //printf("before argc1 = %s\n",pevent_data.data);
	    if(is_reserved_keyword(pevent_data.data) == RES_KEYWORD_DATA)
	    {
		set_parser_event(PSTATE_IDLE,PEVENT_RESERVE_KEYWORD);
		fseek(fd, -1L, SEEK_CUR);
		pevent_data.property = RES_KEYWORD_DATA;
		//printf("data %s",pevent_data.data);
		return &pevent_data;
	    }
	    else if(is_reserved_keyword(pevent_data.data) == RES_KEYWORD_NON_DATA)
	    {
		set_parser_event(PSTATE_IDLE,PEVENT_RESERVE_KEYWORD);
		fseek(fd, -1L, SEEK_CUR);
		pevent_data.property = RES_KEYWORD_NON_DATA;
		//printf("non data1 %s\n",pevent_data.data);
		return &pevent_data;
	    }
	    else
	    {
		pevent_data.data[event_data_idx++] = ch;
		set_parser_event(PSTATE_RESERVE_KEYWORD, PEVENT_REGULAR_EXP);
		//printf("keyword argc1 = %s-n\n",pevent_data.data);
		//fseek(fd, -1L, SEEK_CUR);
		return &pevent_data;
	    }
	    break;
	default:/*string continues*/
	    //if(event_data_idx)
	    //{
	    //printf("def out argc = %s-n\n",pevent_data.data);
	    if(is_operator(ch) || is_symbol(ch) || ch == '\n')
	    {
		pevent_data.data[event_data_idx] = '\0';
		if(is_reserved_keyword(pevent_data.data) == RES_KEYWORD_DATA)
		{
		    set_parser_event(PSTATE_RESERVE_KEYWORD,PEVENT_RESERVE_KEYWORD);
		    fseek(fd, -1L, SEEK_CUR);
		    pevent_data.property = RES_KEYWORD_DATA;
		    //printf("data %s",pevent_data.data);
		    return &pevent_data;
		}
		else if(is_reserved_keyword(pevent_data.data) == RES_KEYWORD_NON_DATA)
		{
		    set_parser_event(PSTATE_IDLE,PEVENT_RESERVE_KEYWORD);
		    fseek(fd, -1L, SEEK_CUR);
		    //state = PSTATE_IDLE;
		    pevent_data.property = RES_KEYWORD_NON_DATA;
		    printf("non data2 %s\n",pevent_data.data);
		    return &pevent_data;
		}
		else
		{
		    set_parser_event(PSTATE_IDLE, PEVENT_REGULAR_EXP);
		    //printf("default argc = %s-n\n",pevent_data.data);
		    fseek(fd, -1L, SEEK_CUR);
		    return &pevent_data;
		}
	    }
	    else if(ch == ' ' || ch == '\t')
	    {
		pevent_data.data[event_data_idx++] = ch;
		//printf("indx %d\n",event_data_idx);
		//pevent_data.data[event_data_idx] = '\0';
		set_parser_event(PSTATE_RESERVE_KEYWORD, PEVENT_REGULAR_EXP);
		//printf("is space = %s-n\n",pevent_data.data);
		return &pevent_data;
	    }
	    else
	    {
		pevent_data.data[event_data_idx++] = ch;
	    }
	    /* }
	       else
	       {
	       pevent_data.data[event_data_idx++] = ch;
	       }*/
	    break;
    }
    return NULL;

}

pevent_t * pstate_numeric_constant_handler(FILE *fd, int ch)
{
    /* write a switch case here to store digits
     * return event data at the end of event
     * else return NULL
     */
	    //printf("number %s-n\n",pevent_data.data);
	    pevent_data.data[event_data_idx] = '\0';
    switch(ch)
    {
	case '0' ... '9':
	    pevent_data.data[event_data_idx++] = ch;
	    while((ch = fgetc(fd)) != ';')
	    {
		if(is_operator(ch) || is_symbol(ch) || ch == ';')
		{
		    set_parser_event(PSTATE_NUMERIC_CONSTANT, PEVENT_NUMERIC_CONSTANT);
		    fseek(fd, -1L, SEEK_CUR);
		    return &pevent_data;
		}
		else
		{
		    pevent_data.data[event_data_idx++] = ch;
		}

	    }
	    if(event_data_idx)
	    {

		set_parser_event(PSTATE_NUMERIC_CONSTANT, PEVENT_NUMERIC_CONSTANT);
		fseek(fd, -1L, SEEK_CUR);
		return &pevent_data;
	    }
	    else
	    {
		pevent_data.data[event_data_idx++] = ch;
		set_parser_event(PSTATE_IDLE,PEVENT_REGULAR_EXP);
		return &pevent_data;
	    }
	    //	}
	    break;
	default:
	    if(is_operator(ch) || is_symbol(ch) || ch == ';')
	    {
		set_parser_event(PSTATE_IDLE, PEVENT_NUMERIC_CONSTANT);
		fseek(fd, -1L, SEEK_CUR);
		return &pevent_data;
	    }
	    else if(ch == ';')
	    {
		pevent_data.data[event_data_idx++] = ch;
		set_parser_event(PSTATE_IDLE,PEVENT_REGULAR_EXP);
		return &pevent_data;
	    }
	    else
	    {
		pevent_data.data[event_data_idx++] = ch;
	    }
	    break;
    }
    return NULL;
}
pevent_t * pstate_string_handler(FILE *fd, int ch)
{
    /* write a switch case here to store string
     * return event data at the end of event
     * else return NULL
     */
    switch(ch)
    {
	case '\"':
	    pevent_data.data[event_data_idx++] = ch;
	    set_parser_event(PSTATE_IDLE,PEVENT_STRING);
	    return &pevent_data;
	default:
	    pevent_data.data[event_data_idx++] = ch;
	    break;
    }
    return NULL;
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
    /* write a switch case here to store ASCII chars
     * return event data at the end of event
     * else return NULL
     */
    //printf("in ascii %c\n",ch);
    switch(ch)
    {
	case '\'':/*ASCII char ends here*/
	    pevent_data.data[event_data_idx++] = ch;
	    set_parser_event(PSTATE_IDLE, PEVENT_ASCII_CHAR);
	    state_sub = PSTATE_SUB_PREPROCESSOR_MAIN;
	    return &pevent_data;
	case '\n':
	    pevent_data.data[event_data_idx++] = ch;
	    set_parser_event(PSTATE_IDLE, PEVENT_NUMERIC_CONSTANT);
	    state_sub = PSTATE_SUB_PREPROCESSOR_MAIN;
	    //printf("num\n");
	    return &pevent_data;
	default:/*Char still continues*/
	    if(event_data_idx == 0)
	    {
		fseek(fd, -2L, SEEK_CUR);//move 2 steps back
		pevent_data.data[event_data_idx++] = fgetc(fd);//read char
		set_parser_event(PSTATE_IDLE, PEVENT_ASCII_CHAR);
		return &pevent_data;
	    }
	    else
	    {
		pevent_data.data[event_data_idx++] = ch;
		//set_parser_event(PSTATE_IDLE, PEVENT_REGULAR_EXP);
		//return &pevent_data;
	    }
	    break;
    }
    return NULL;
}
/**** End of file ****/
