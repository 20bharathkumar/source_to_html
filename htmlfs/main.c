/*
NAME: BHARATHKUMAR K
DATE: 10/6/2024
DESCRIPTION: Source to HTML . in this project we are converting c source to to html code and displaying source code in browser with same colour as in vi editor
 */
#include <stdio.h>
#include "event.h"
#include "conv.h"
//#include "s2html_conv.c"
//#include "s2html_event.c"

/********** main **********/

int main (int argc, char *argv[])
{
	FILE * sfp, *dfp; // source and destination file descriptors 
	pevent_t *event;
	char dest_file[100];

	if(argc < 2)
	{
		printf("\nError ! please enter file name and mode\n");
		printf("Usage: <executable> <file name> \n");
		printf("Example : ./a.out abc.txt\n\n");
		return 1;
	}
#ifdef DEBUG
	printf("File to be opened : %s\n", argv[1]);
#endif

	/* opnen the file */

	if(NULL == (sfp = fopen(argv[1], "r")))
	{
		printf("Error! File %s could not be opened\n", argv[1]);
		return 2;
	}
	/* Check for output file */
	if (argc > 2)
	{
		sprintf(dest_file, "%s.html", argv[2]);
	}
	else
	{
		sprintf(dest_file, "%s.html", argv[1]);
	}
	/* open dest file */
	if (NULL == (dfp = fopen(dest_file, "w")))
	{
		printf("Error! could not create %s output file\n", dest_file);
		return 3;
	}

	/* write HTML starting Tags */
	html_begin(dfp, HTML_OPEN);

	/* Read from src file convert into html and write to dest file */

	do
	{
		event = get_parser_event(sfp);
		/* call sourc_to_html */
		source_to_html(dfp, event);
//		printf("In main : Event = %d\n", event);
	} while (event->type != PEVENT_EOF);

	/* Call start_or_end_conv function for ending the convertation */
	html_end(dfp, HTML_CLOSE);
	
	printf("\nOutput file %s generated\n", dest_file);
/* close file */
	fclose(sfp);
	fclose(dfp);

	return 0;
}
