#include "util.h"		// dnslookup()
#include <stdio.h>		// FILE* variables, etc.
#include <stdlib.h>		// atoi(), malloc(), etc.
#include <string.h> 	// strcpy(), etc.
#include <sys/time.h>	// timeval struct, gettimeofday()
#include <float.h>		// to control float precision
#include <unistd.h>		// stderr file descriptor
#include <pthread.h>
#include <semaphore.h>


#define BUFFER_SIZE 1024


///////////////////////
//                   //
//      STRUCTS      //
//                   //
//                   //
///////////////////////
typedef struct 
{
	char *targetFile;
} ParserWorkOrder;


///////////////////////
//                   //
//     FUNCTION      //
//   DECLARATIONS    //
//                   //
///////////////////////
int main(int argc, char* argv[]);
int startProgramTimer();
int insufficientArgsError();
int parseCommandLineArgs(int argc, char * argv[]);
int initializeBuffer();
int initializeMutexes();
int countElementsToProcess();
int initializeOutputFiles();
void parserFunc(ParserWorkOrder *workOrderPtr);
void removeWhiteSpace(char *str);
int addToBuffer(char *line);
int outputParserInfo();
void converterFunc();
int valueOfElementsToProcess();
int cleanup();
int stopProgramTimer();
int printBuffer();


///////////////////////
//                   //
// GLOBAL VARIABLES  //
//    TO MEASURE     //
// PROGRAM EXECUTION //
//       TIME        //
//                   //
///////////////////////
double executionTime;
struct timeval start, stop;


///////////////////////
//                   //
// GLOBAL USER INPUT //
//    AND PROGRAM    //
//      OUTPUT       //
//     VARIABLES     //
//                   //
///////////////////////
int numParsingThreads;		// How many parser threads?
int numConversionThreads;	// How many conversion threads?
char *parsingLog;			// Filename for parsing log
char *converterLog;			// Filename for converter log
int inputFilesCount;		// How many input files?
char **inputFiles;			// Filename(s) of input file(s)
int elementsToProcess;		// Total number of lines to process
FILE* outFileConverter;		// Output file; converter threads write here
FILE* outFileParser;		// Output file; parser threads write here


////////////////////////
//                    //
//   GLOBAL BUFFER,   //
//  SEMAPHORE, AND    //
//       MUTEX        //
//     VARIABLES      //
//                    //
////////////////////////
char **buffer; 						// The bounded buffer
int buffCount;	 					// Number of elements in buffer
sem_t empty;						// Number of empty slots in buffer
sem_t full;							// Number of full slots in buffer
pthread_mutex_t mutexBuffer;		// Mutex to protect buffer access
pthread_mutex_t mutexEltsToProc;	// Mutex to protect 'elementsToProcess'
pthread_mutex_t mutexOFP;			// Mutex to protect 'outFileParser'


///////////////////////
//                   //
//     FUNCTION      //
//    DEFINITIONS    //
//                   //
///////////////////////
int main(int argc, char* argv[]) 
{	
	if (argc < 6)
	{
		insufficientArgsError();
		return -1;
	}

	startProgramTimer();
	parseCommandLineArgs(argc, argv);
	initializeBuffer();
	initializeMutexes();
	countElementsToProcess();
	initializeOutputFiles();

	//
	// CREATE parser threads
	//	
	ParserWorkOrder *workOrderPtr;
	pthread_t parserTh[inputFilesCount];
	for (int i = 0; i < inputFilesCount; i++)
	{
		workOrderPtr = (ParserWorkOrder *)malloc(sizeof(ParserWorkOrder));
		
		workOrderPtr->targetFile = inputFiles[i];

		if (pthread_create(&(parserTh[i]), 
							NULL, 
							(void *) parserFunc, 
							(void *) workOrderPtr) != 0)
		{
	        perror("Failed to create parser thread\n");
	    }
	}

	//
	// CREATE converter threads
	//	
	pthread_t converterTh[numConversionThreads];
	for (int i = 0; i < numConversionThreads; i++)
	{
		if (pthread_create(&(converterTh[i]), 
							NULL, 
							(void *) converterFunc, 
							NULL) != 0)
		{
	        perror("Failed to create converter thread\n");
	    }	
	}

	//
	// JOIN all threads to main thread
	//	
	for (int i = 0; i < inputFilesCount; i++)
	{
		if (pthread_join(parserTh[i], NULL) != 0) 
		{
			perror("Failed to join parser thread\n");
		}
	}
	for (int i = 0; i < numConversionThreads; i++)
	{
		if (pthread_join(converterTh[i], NULL) != 0) 
		{
			perror("Failed to join converter thread\n");
		}
	}

	cleanup();
	stopProgramTimer();

	return 0;
}


int startProgramTimer()
{
	// Record program start time; this will be used to
	// 	calculate total program execution time.
	gettimeofday(&start, NULL);

	return 0;
}


int insufficientArgsError()
{
	printf("INPUT ERROR\n");
	printf("Program Arguments: ./multi-lookup ");
	printf("<# parsing threads> <# conversion threads> ");
	printf("<parsing log filename> <converter log filename> ");
	printf("[<datafile> ...]\n");

	return 0;
}


int parseCommandLineArgs(int argc, char * argv[])
{
	numParsingThreads = atoi(argv[1]);
	numConversionThreads = atoi(argv[2]);
	parsingLog = argv[3];
	converterLog = argv[4];
	inputFilesCount = argc - 5;
	inputFiles = malloc(sizeof(char*)*inputFilesCount);

	// Parse each input filename into 'inputFiles' array
	// 	Notice we need strSize = length of argv[] + 1 to 
	//		accomodate the null '\0' terminating character.
	int i, strSize;
	for (i = 0; i < inputFilesCount; i++)
	{
		strSize = strlen(argv[i + 5]) + 1;
		inputFiles[i] = malloc(sizeof(char) * strSize);
		strcpy(inputFiles[i], argv[i + 5]);
	}

	return 0;
}


int initializeBuffer()
{
	// Create buffer.
	//	We initialize all elements in buffer in preparation
	//		for realloc-ing during subsequent program execution.
	buffer = malloc(sizeof(char*) * BUFFER_SIZE);
	
	int i;
	for (i = 0; i < BUFFER_SIZE; i++)
	{
		buffer[i] = malloc(sizeof(char));
	}

	// Initially, the buffer is empty, so buffCount = 0.
	// 	The number of empty slots is BUFFER_SIZE, so empty = BUFFER_SIZE.
	// 	Likewise, the number of full slots is 0, so full = 0.
	buffCount = 0;
	sem_init(&empty, 0, BUFFER_SIZE);
	sem_init(&full, 0, 0);

	return 0;
}


int initializeMutexes()
{
	// Initialize mutexes to control acess to shared resources. 
	pthread_mutex_init(&mutexBuffer, NULL);
	pthread_mutex_init(&mutexEltsToProc, NULL);	
	pthread_mutex_init(&mutexOFP, NULL);

	return 0;
}


int countElementsToProcess()
{
	elementsToProcess = 0;

	for (int i = 0; i < inputFilesCount; i++)
	{
		FILE *fp;

		fp = fopen(inputFiles[i], "r");

		if (fp == NULL)
		{
			printf("Error opening %s in countElementsToProcess()\n",
					inputFiles[i]);
			exit(-1);
		}
				
		int numLines = 0;
		char c;
		do
		{
			c = fgetc(fp);
			if (c == '\n') numLines++;

		} while (c != EOF);

		fclose(fp);

		elementsToProcess += numLines;
	}

	return 0;
}


int initializeOutputFiles()
{
	// Initialize global output files.
	// 		outFileConverter: converter threads write here.
	//		outFileParser   : parser threads write here.
	//		Note that writes to these files should append to EOF,
	//			hence fopen() with "a". 
	outFileConverter = NULL;
	outFileConverter = fopen(converterLog, "a");
	if (outFileConverter == NULL)
	{
		perror("Unable to open 'outFileConverter'\n");
		exit(-1);
	}

	outFileParser = NULL;
	outFileParser = fopen(parsingLog, "a");
	if (outFileParser == NULL)
	{
		perror("Unable to open 'outFileParser'\n");
		exit(-1);
	}

	return 0;
}


void parserFunc(ParserWorkOrder *workOrderPtr)
{
	FILE *inFile = fopen(workOrderPtr->targetFile, "r");
	if (inFile == NULL)
	{
		perror("Unable to open file\n");
		exit(-1);
	}

	char *line = NULL;
	size_t len = 0;
	while (getline(&line, &len, inFile) != -1)
	{
		removeWhiteSpace(line);

		sem_wait(&empty);					// Wait for empty slots...
		pthread_mutex_lock(&mutexBuffer);	// Lock buffer
		addToBuffer(line);					// Add 'line' to buffer
		pthread_mutex_unlock(&mutexBuffer); // Unlock buffer
		sem_post(&full);					// Signal there are now full slots
	}

	fclose(inFile);
	free(line);
	free(workOrderPtr);

	outputParserInfo();
}


void removeWhiteSpace(char *str)
{
	int i = strlen(str) - 1;
	while (i >= 0)
	{
		if (str[i] == ' ' || str[i] == '\n' ||
			str[i] == '\t') i--;

		else break;
	}
	str[i + 1] = '\0';
}


int addToBuffer(char *line)
{
	buffer[buffCount] = (char *)realloc(buffer[buffCount],
										(sizeof(char)*strlen(line)) + 1);
	
	if (buffer[buffCount] == NULL)
	{
		perror("realloc() in addToBuffer() failed\n");
	}

	strcpy(buffer[buffCount], line);
	buffCount++;

	return 0;
}


int outputParserInfo()
{
	unsigned int threadID = pthread_self();

	pthread_mutex_lock(&mutexOFP);		// Lock 'outFileParser'
	fprintf(outFileParser, 
			"Thread %u serviced 1 file(s).\n", 
			threadID);
	pthread_mutex_unlock(&mutexOFP);	// Unlock 'outFileParser'

	return 0;
}


void converterFunc()
{
	char *ipAddr;
	ipAddr = malloc(sizeof(char)*17);

	while (valueOfElementsToProcess() > 0)
	{
		sem_wait(&full);					// Wait for full slots...
		pthread_mutex_lock(&mutexBuffer);	// Lock 'mutexBuffer'

		buffCount--;
		dnslookup(buffer[buffCount], ipAddr, 17);
		fprintf(outFileConverter, "%s, %s\n", buffer[buffCount], ipAddr);
		if (strcmp(ipAddr, "") == 0)
		{
			fprintf(stderr, "Domain name %s could not be resolved.\n",
					buffer[buffCount]);
		}

		pthread_mutex_unlock(&mutexBuffer);	// Unlock 'mutexBuffer'
		sem_wait(&empty);					// Signal new empty slot in buffer
	}
	
	free(ipAddr);
}


int valueOfElementsToProcess()
{
    int result;

    pthread_mutex_lock(&mutexEltsToProc);	// Lock 'mutexEltsToProc'

    result = elementsToProcess;
	elementsToProcess--;

    pthread_mutex_unlock(&mutexEltsToProc);	// Unlock 'mutexEltsToProc'

    return result;
}


int cleanup()
{
	int i;

	for (i = 0; i < inputFilesCount; i++)
	{
		free(inputFiles[i]);
	}
	free(inputFiles);

	for (i = 0; i < BUFFER_SIZE; i++)
	{
		free(buffer[i]);
	}
	free(buffer);

	sem_destroy(&empty);
	sem_destroy(&full);
	pthread_mutex_destroy(&mutexBuffer);
	pthread_mutex_destroy(&mutexEltsToProc);
	pthread_mutex_destroy(&mutexOFP);

	fclose(outFileConverter);
	fclose(outFileParser);

	return 0;
}


int stopProgramTimer()
{
	// Record program stop time, calculate total program execution
	// 	time, and output this value to stdout. 
	gettimeofday(&stop, NULL);
	executionTime  = stop.tv_sec - start.tv_sec;
	executionTime += ((double)stop.tv_usec-(double) start.tv_usec)/1000000;
	printf("Program executed in %g seconds.\n", executionTime);

	return 0;
}


int printBuffer()
{
	// Print contents of buffer
	for (int i = 0; i < buffCount; i++)
	{
		printf("buffer[%d]: %s\n", i, buffer[i]);
	}

	if (buffCount == 0)
	{
		printf("-- buffer is empty --\n");
	}

	printf("buffCount is %d\n", buffCount);

	return 0;
}
