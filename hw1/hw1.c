#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define MAXSIZE 128

int genHash(char * buf) {
	int hash = 0;
	int i;
	for(i = 0; i < strlen(buf); i++) {
		hash += *(buf+i);
	}
	return hash;
}


void fillTable(FILE * file, char ** hashTable, int tableSize) {
	char * buf = calloc(1, MAXSIZE);
	while(fscanf(file, "%127s", buf) == 1) {
		char * head;
		#if DEBUGMODE
		printf("%s\n", buf);
		#endif
		for(head = buf; *head != '\0'; head++) {
			#if DEBUGMODE
			printf("%c\n", *head);
			#endif
			if(!isalnum(*head)) { 
				*head = ' ';
			}
		}
		char * broken = strtok(buf, " ");
		#if DEBUGMODE
		printf("%s\n", buf);
		#endif
		while(broken != NULL) {
			if(strlen(broken) >  1) {
				int hash = genHash(broken); 
				hash %= tableSize;
				#if debug_mode
				printf("Word: %s Hash: %d\n", broken, hash);	
				#endif
				if(!*(hashTable+hash)) {
					*(hashTable+hash) = calloc(strlen(broken)+1, sizeof(char));
					if(!*(hashTable+hash)) {
						fprintf(stderr, "ERROR: \
							error allocating memory with calloc");
						return;
					}
					strcpy(*(hashTable+hash), broken);
					printf("Word \"%s\" ==> %d (calloc)\n", broken, hash);
				}
				else {
					*(hashTable+hash) = realloc(*(hashTable+hash), \
							(strlen(broken)+1 * sizeof(char)));
					if(!*(hashTable+hash)) {
						fprintf(stderr, "ERROR: \
							error allocating memory with calloc");
						return;
					}
					strcpy(*(hashTable+hash), broken);
					printf("Word \"%s\" ==> %d (realloc)\n", broken, hash);
				}
				#if debug_mode
				printf("stored: %s\n", *(hashTable+hash));
				#endif
			}
			broken = strtok(NULL, " "); 
		}
	}
	free(buf);
}

int main(int argc, char ** argv) {
	//checking for errors in input
	if(argc != 3) {
		//fprintf(stderr, "Requires more arguments\n");
		fprintf(stderr, "ERROR: Usage: %s <table size> <input file>\n", *argv); 
		return EXIT_FAILURE;
	}
	int i;
	for(i = 0; i < strlen(*(argv+1)); i++) {
		if(!isdigit(*(*(argv+1)+i))) {
			fprintf(stderr, "ERROR: Not a valid number\n");
			return EXIT_FAILURE;
		}
	}
	int inputNum = atoi(*(argv+1));
	char * inputFile = *(argv+2);

	#if debug_mode
	printf("File name: %s\n", inputFile);
	printf("input num: %d\n", inputNum);
	#endif
     
	FILE * file = fopen(inputFile, "r");
	if(!file) {
		fprintf(stderr, "ERROR: Not a file\n");
		return EXIT_FAILURE;
	}
	char ** hashTable = calloc(inputNum, sizeof(char *));
	
	fillTable(file, hashTable, inputNum);
	fclose(file);	

	for(i = 0; i < inputNum; i++) {
		if(*(hashTable+i)) {
			printf("Cache index %d ==> \"%s\"\n", i, *(hashTable+i));
			free(*(hashTable+i));
		}
	}	
	free(hashTable);
}
