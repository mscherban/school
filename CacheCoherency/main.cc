/*******************************************************
                          main.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include <fstream>
using namespace std;

#include "cache.h"

int main(int argc, char *argv[])
{
	
	ifstream fin;
	FILE * pFile;

	if(argv[1] == NULL){
		 printf("input format: ");
		 printf("./smp_cache <cache_size> <assoc> <block_size> <num_processors> <protocol> <trace_file> \n");
		 exit(0);
        }

	printf("===== 506 Personal information =====\n");
	printf("Michael Scherban\n");
	printf("Mscherb\n");
	printf("ECE492 Students? NO\n");


	/*****uncomment the next five lines*****/
	int cache_size = atoi(argv[1]);
	int cache_assoc= atoi(argv[2]);
	int blk_size   = atoi(argv[3]);
	int num_processors = atoi(argv[4]);/*1, 2, 4, 8*/
	int protocol   = atoi(argv[5]);	 /*0:MSI, 1:MESI, 2:Dragon*/
	char *fname =  (char *)malloc(20);
 	fname = argv[6];

	
	//****************************************************//
	printf("===== 506 SMP Simulator configuration =====\n");
	printf("L1_SIZE: %d\n", cache_size);
	printf("L1_ASSOC: %d\n", cache_assoc);
	printf("L1_BLOCKSIZE: %d\n", blk_size);
	printf("NUMBER OF PROCESSORS: %d\n", num_processors);
	printf("COHERENCE PROTOCOL: %s\n", (protocol == 0) ? "MSI" : (protocol == 1) ? "MESI" : "Dragon");
	printf("TRACE FILE: %s\n", fname);
	//****************************************************//

 
	//*********************************************//
        //*****create an array of caches here**********//
	//*********************************************//
	Bus bus(num_processors, protocol);
	Cache *Caches[8];
	for (int i = 0; i < num_processors; i++) {
		Caches[i] = new Cache(cache_size, cache_assoc, blk_size, protocol);
		Caches[i]->BusReg(&bus);
		bus.BusReg(Caches[i]);
	}
	

	pFile = fopen (fname,"r");
	if(pFile == 0)
	{   
		printf("Trace file problem\n");
		exit(0);
	}
	///******************************************************************//
	//**read trace file,line by line,each(processor#,operation,address)**//
	//*****propagate each request down through memory hierarchy**********//
	//*****by calling cachesArray[processor#]->Access(...)***************//
	///******************************************************************//
	char str[2];
	ulong addr;
	int proc;
	uchar op;
	while (fscanf(pFile, "%d %s %lx", &proc, str, &addr) != EOF) {
			op = str[0];
			//printf("proc(%d), %c, addr(%lx)\n", proc, op, addr);
			Caches[proc]->Access(addr, op);
	}
	fclose(pFile);

	//********************************//
	//print out all caches' statistics //
	//********************************//
	
	bus.BusTest();
	for (int i = 0; i < num_processors; i++) {
		//printf("============ Simulation results (Cache %d) ============\n", i);
		//Caches[i]->printStats();
		delete Caches[i];
	}
}
