#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <semaphore.h>

#define maxThreads 256

int max_squares;
char *** dead_end_boards; 
int numDeadBoards = 0;
pthread_t topParent;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;



/*
Board markers:

'.' = not visited
'S' = visited
'K' = knight

*/

//only used for debugging
void printBoard(char ** board, int x, int y) {
	//pthread_mutex_lock(&mutex);
	//printf("THREAD IS: %ld\n", pthread_self());
	printf("--------\n");
	for(int i = 0; i < y; i++) {
		for(int j = 0; j < x; j++) {
			printf("%c", board[i][j]);
		}
		printf("\n");
	}
	//pthread_mutex_unlock(&mutex);
}

void freeBoard(char ** board, int x, int y) {
	for(int i = 0; i < y; i++) {
		free(board[i]);
	}
	free(board);
}
	
char ** copyBoard(char ** board, int x, int y) {
	char ** newBoard = calloc(y, sizeof(char*));
	for(int i = 0; i < y; i++){
		newBoard[i] = calloc(x+1, sizeof(char));
		memcpy(newBoard[i], board[i], x);
	}
	return(newBoard);
}

int countBoard(char ** board, int x, int y) {
	int i, j;
	int spots = 0;
	for(i = 0; i < y; i++) {
		for(j = 0; j < x; j++) {
			if(board[i][j] == 'S') spots++;	
		}
	}
	return(spots);
}

void  findMoves(char ** board, int c, int r, const int x, const int y); 

struct doMoveArguments {
	char ** board;
	int xSpot;
	int ySpot;	
	int xBoard;
	int yBoard;
};

void * doMove(void * arguments) {
	struct doMoveArguments *args = arguments;
	
	pthread_mutex_lock(&mutex); 
	int c = args -> xSpot;
	int r = args -> ySpot; 
	args -> board[r][c] = 'K'; 
    	pthread_mutex_unlock(&mutex);	
	
	findMoves(args -> board, c, r, args -> xBoard, args -> yBoard);
	int * moves = (int*)malloc(sizeof(int));
	*moves = countBoard(args -> board, args -> xBoard, args -> yBoard);
	#ifndef NO_PARALLEL
	//freeBoard(args -> board, args -> xBoard, args -> yBoard);
	#endif
	#ifdef DEBUG_MODE
	//printBoard(args -> board, args -> xBoard, args -> yBoard);
	//printf("THREAD %ld IS EXITING IN doMOVE NOW!\n", pthread_self());
	#endif
	pthread_exit(moves);
}

/*
c and r are the column  and row that the knight is in respectively
x and y are the columns and rows of the board respectively
*/
void findMoves(char ** board, int c, int r, const int x, const int y) {
	/* 
	int to keep track of how many moves we have. Also tells us what spot in 
	checkSpots to put the next move into
	*/
	
	#ifdef DEBUG_MODE
		printf("THREAD %ld: current spot: r: %d - c: %d\n", \
		 pthread_self(), r, c);
		printBoard(board, x, y);
	#endif
			
    	pthread_mutex_lock(&mutex); //lock 1	
	int moves = 0; 
	int checkSpots[8][2]; //0 is the column, 1 is the row
	
	board[r][c] = 'S';

	//left 2 up 1
	if(c >= 2) { 
		if(r > 0) {
			if(board[r-1][c-2] == '.') {
				checkSpots[moves][0] = c-2;
				checkSpots[moves][1] = r-1;
				moves++;
			}
		}
	}

	//checking above the knight
	if(r >= 2) { 
		if(c > 0) {
			if(board[r-2][c-1] == '.') {
				checkSpots[moves][0] = c-1;
				checkSpots[moves][1] = r-2;
				moves++;
			}
		}
		if(c < x-1) {
			if(board[r-2][c+1] == '.') {
				checkSpots[moves][0] = c+1;
				checkSpots[moves][1] = r-2;
				moves++;
			}
		}
	}

	//checking to the right of the knight
	if(c < x-2) { 
		if(r > 0) {
			if(board[r-1][c+2] == '.') {
				checkSpots[moves][0] = c+2;
				checkSpots[moves][1] = r-1;
				moves++;
			}
		}
		if(r < y-1) {
			if(board[r+1][c+2] == '.') {
				checkSpots[moves][0] = c+2;
				checkSpots[moves][1] = r+1;
				moves++;
			}
		}
	}

	//checking to below the knight
	if(r < y-2) { 
		if(c < x-1) {
			if(board[r+2][c+1] == '.') {
				checkSpots[moves][0] = c+1;
				checkSpots[moves][1] = r+2;
				moves++;
			}
		}
		if(c > 0) {
			if(board[r+2][c-1] == '.') {
				checkSpots[moves][0] = c-1;
				checkSpots[moves][1] = r+2;
				moves++;
			}
		}
	}
	
	if(c >= 2) { //left 2, down 1
		if(r < y-1) {
			if(board[r+1][c-2] == '.') {
				checkSpots[moves][0] = c-2;
				checkSpots[moves][1] = r+1;
				moves++;
			}
		}
	}

	pthread_mutex_unlock(&mutex); //unlock 1

	int spots = countBoard(board, x, y);
	//printf("There are %d moves in moves\n", moves);
	if(moves == 0) {
		pthread_mutex_lock(&mutex); //lock 2
		if(spots > max_squares) max_squares = spots;
		if(spots != x * y) {
			printf("THREAD %ld: Dead end after move #%d\n", pthread_self(), spots);
		
			/*	
			dead_end_boards = realloc(dead_end_boards, sizeof(dead_end_boards) \
			 + (x * y * sizeof(char **)));
			*/
				
			
			dead_end_boards = realloc(dead_end_boards, \
			 (numDeadBoards+1)*x*y*sizeof(char**));
			
			#ifdef DEBUG_MODE
			//printf("numdeadboards is %d\n", numDeadBoards);
			for(int i = 0; i < numDeadBoards; i++) {
				if(dead_end_boards[i] == NULL) {
					printf("numDeadBoards is %d\n \
					 %d is the last deadboardnum\n", numDeadBoards, i);
				}
			}
			#endif
			#ifdef deadBoardDebugging
			printf("numdeadboards: %d\n", numDeadBoards);
			for(int i = 0; i < numDeadBoards; i++) {
				printf("%d VVV\n", i);
				printBoard(dead_end_boards[i], x, y);
				printf("--------\n");
			}
			#endif
			dead_end_boards[numDeadBoards] = copyBoard(board, x, y);
			numDeadBoards++;
		}
		else {
			printf("THREAD %ld: Sonny found a full knight's tour!\n", pthread_self());
		}
		pthread_mutex_unlock(&mutex); //unlock 2
		return;
	}
	else if(moves == 1) {
		struct doMoveArguments args;
		board[r][c] = 'S';
		pthread_mutex_lock(&mutex); //lock 3
		args.board = board;
		args.xSpot = checkSpots[0][0];
		args.ySpot = checkSpots[0][1];
		args.xBoard = x;
		args.yBoard = y;
		pthread_mutex_unlock(&mutex); //unlock 3
		doMove((void*)&args);
		freeBoard(args.board, args.xBoard, args.yBoard);
	}
	else { //more than 1 move possible
		int * maxSpots = (int*)malloc(sizeof(int));
		*maxSpots = 0;
		printf("THREAD %ld: %d moves possible after move #%d; creating threads...\n", \
		 pthread_self(), moves, spots);
		pthread_t tid[moves];
		int i;

		#ifdef DEBUG_MODE
		printf("Possible moves for %ld\n", pthread_self());
		for(i = 0; i < moves; i++) {
			printf("%d: c: %d -- r: %d\n", i, checkSpots[i][0], checkSpots[i][1]);
		}
		#endif

		int rc;
		int * retVal;
		//struct doMoveArguments args;
		
		pthread_mutex_lock(&mutex);
		board[r][c] = 'S';
		pthread_mutex_unlock(&mutex);
		
		//store all arguments so they can be freed later
		struct doMoveArguments allArgs[moves]; 
		for(i = 0; i < moves; i++) {
			
			pthread_mutex_lock(&mutex);
			allArgs[i].xSpot = checkSpots[i][0];
			allArgs[i].ySpot = checkSpots[i][1];
			allArgs[i].board = copyBoard(board, x, y); 	
			pthread_mutex_unlock(&mutex);
	
			allArgs[i].xBoard = x;
			allArgs[i].yBoard = y;
			rc = pthread_create(&tid[i], NULL, doMove, (void*)&allArgs[i]);
			if(rc != 0) {
				fprintf(stderr, "ERROR: could not make thread\n");
			}
			#ifdef DEBUG_MODE
			printf("THREAD %ld: created thread %ld\n", pthread_self(), tid[i]);
			printf("Newly created thread %ld will have coordinates r: %d, c: %d\n", tid[i], allArgs[i].ySpot, allArgs[i].xSpot);
			#endif
			#ifdef NO_PARALLEL
			rc = pthread_join(tid[i], (void**)&retVal);
			if(rc != 0) {
				fprintf(stderr, "ERROR: No thread %ld to join\n", tid[i]);
			}
			printf("THREAD %ld: Thread [%ld] joined (returned %d)\n", 
			 pthread_self(), tid[i], *retVal);
			if(*retVal > *maxSpots) *maxSpots = *retVal;
			freeBoard(allArgs[i].board, allArgs[i].xBoard, allArgs[i].yBoard);
			free(retVal);
			#endif 
		}
		#ifndef NO_PARALLEL
		for(i = 0; i < moves; i++) {
			rc = pthread_join(tid[i], (void**)&retVal);
			if(rc != 0) {
				fprintf(stderr, "ERROR: No thread %ld to join\n", tid[i]);
			}
			if(*retVal > *maxSpots) *maxSpots = *retVal;
			printf("THREAD %ld: Thread [%ld] joined (returned %d)\n", 
			 pthread_self(), tid[i], *retVal);	 
			free(retVal);
			freeBoard(allArgs[i].board, allArgs[i].xBoard, allArgs[i].yBoard);
		}
		#endif
		if(pthread_self() != topParent) {
			pthread_exit(maxSpots);
			printf("THREAD %ld IS EXITING  NOW!\n", pthread_self());
		}
		free(maxSpots);
	}
}

int inputError() {
	fprintf(stderr, "ERROR: Invalid argument(s)\n");
	fprintf(stderr, "USAGE: a.out <m> <n> [<x>]\n");
	return(EXIT_FAILURE);
}

int main(int argc, char ** argv) {

	topParent = pthread_self();
	
	if(argc != 3 && argc != 4) {
		return(inputError());	
	}

	int m = atoi(argv[1]);
	int n = atoi(argv[2]);	
	int x = 0;

	if(m <= 2 || n <= 2) {
		return(inputError());	
	}

	if(argc == 4) {
		x = atoi(argv[3]);	
		if(x > m*n) {
			return(inputError());	
		}
	}

	char ** board = calloc(m, sizeof(char *));
	int i;
	for(i = 0; i < m; i++) {
		board[i] = calloc(n+1, sizeof(char)); //n+1 for nill terminator
		int j;
		for(j = 0; j < n+1; j++) {
			board[i][j] = '.';
		}
	}
	board[0][0] = 'K'; 
	
	printf("THREAD %ld: Solving Sonny's knight's tour problem for a %dx%d board\n", \
	 pthread_self(), m, n);
	findMoves(board, 0, 0, n, m);	

	printf("THREAD %ld: Best solution(s) found visit %d squares (out of %d)\n", \
	 pthread_self(), max_squares, m*n);
	printf("THREAD %ld: Dead end boards:\n", pthread_self());
	for(i = 0; i < numDeadBoards; i++) {
		int spots = countBoard(dead_end_boards[i], n, m);
			for(int j = 0; j < m; j++) {
				if(spots >= x) {
					printf("THREAD %ld: ", pthread_self());
					if(j == 0) printf("> ");
					else printf("  ");
					for(int k = 0; k < n; k++) {
						printf("%c", dead_end_boards[i][j][k]);
					}
					printf("\n");
				}				
				free(dead_end_boards[i][j]);
			}
			free(dead_end_boards[i]);
	}
	
	freeBoard(board, n, m);
	free(dead_end_boards);
	return EXIT_SUCCESS;
}
