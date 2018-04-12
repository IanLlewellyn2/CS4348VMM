#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fstream>

#define FRAME_SIZE 256
#define NUMBER_OF_FRAMES 128
#define TLB_SIZE 16
#define PAGE_TABLE_SIZE 128

struct pageTable{
	//first array is which logical memory page is in that frame
	//second array is valid/invalid bit 0 is invalid, 1 is valid
	//third array is the frame number
	//fourth array is for page replacement 
	//page replacement is done using second chance algorithm
	
	int page [PAGE_TABLE_SIZE];
	int validBit[PAGE_TABLE_SIZE];
	int frameNumber[PAGE_TABLE_SIZE];
	int secondChance[PAGE_TABLE_SIZE];
};

struct TLB{
	//page replacement is done using second chance algorithm
	
	int pageNumber[TLB_SIZE];
	int validBit[PAGE_TABLE_SIZE];
	int frameNumber[TLB_SIZE];
	int secondChance[TLB_SIZE];
};



pageTable myPageTable;
TLB myTLB;
int physicalMemory[NUMBER_OF_FRAMES][FRAME_SIZE]; //frames where data will be held after it is loaded
double totalHits = 0; //incremented every time an address is called, needed for percentages
double TLBHits = 0; //number of times there is a TLB hit
double PageFaults = 0; //number of times there is a page fault
int secondChanceIndex = 0; //where the second chance algorithm left off
int secondChanceIndexTLB = 0;

int addressTranslationPage(int);
int addressTranslationOffset(int);
int setUpPageTable();
int getValueFromAddress(int, FILE*);
int loadNewPage(int, FILE*);
int updateTLB(int, int);

using namespace std;


int main(int argc, char **argv)
{
	int addressRead;
	double percentages;
	ifstream input(argv[1]);
	if(argc != 2)
	{
		perror("problem with program arguments");
		return 1;
	}	
	
	
	FILE* data = fopen("BACKING_STORE.bin", "r"); //pointer for reading the binary file
	
	setUpPageTable(); //set valid/invalid bits to invalid to start off with
	
	//iterate through all addresses in given address file
	while(input >> addressRead)
	{
		getValueFromAddress(addressRead, data);
		totalHits++;
	}
	
	percentages = (TLBHits / totalHits) * 100;
	printf("The TLB hit rate is %f percent\n", percentages);
	percentages = (PageFaults / totalHits) * 100;
	printf("The Page fault rate is %f percent\n", percentages);
	
	int fileClose = fclose(data); //close the stream
	return 0;
}
int loadNewPage(int logicAddress, FILE* data)
{
	//read in page that logicAddress is located in
	//must find the next empty frame
	int pageNeeded = addressTranslationPage(logicAddress);
	int i = 0;
	char buffer[FRAME_SIZE];
	
	//find next open frame - if any still exist
	while(myPageTable.validBit[i] == 1 && i < PAGE_TABLE_SIZE) 
	{
		i++;
	}
	
	//no open frames - find next 0 bit for secondChance
	if(i > PAGE_TABLE_SIZE - 1)
	{
		
		i = secondChanceIndex; //start where we stopped before - round robin style
		while(myPageTable.secondChance[i] == 1 && i < PAGE_TABLE_SIZE)
		{
			i++;
		}
		
		//if we go to the end of the array without finding one, start over
		if(i == PAGE_TABLE_SIZE)
		{
			i = 0;
			while(myPageTable.secondChance[i] == 1 && i < PAGE_TABLE_SIZE)
			{
				i++;
			}
			//if all have a 1, just replace 0
			if(i == PAGE_TABLE_SIZE)
				i = 0;
				
		}
		//knock down second chance bit
		for(int i = 0; i < PAGE_TABLE_SIZE; i++)
			myPageTable.secondChance[i] = 0;
	}
	
	//read from file into this memory frame
	fseek(data, FRAME_SIZE * pageNeeded, SEEK_SET);
	fread(buffer, sizeof(char), FRAME_SIZE, data);
	
	for(int j = 0; j < FRAME_SIZE; j++)
		physicalMemory[i][j] = buffer[j];
	
	//update page table
	myPageTable.page[i] = pageNeeded;
	myPageTable.validBit[i] = 1; //set bit to valid
	myPageTable.frameNumber[i] = i;
	
	//update TLB
	updateTLB(pageNeeded, i);
	
	
	secondChanceIndex = i + 1;
	
	//return the frame that the page was placed in
	return i;
	
	
}
int updateTLB(int pageNumber, int frameNumber)
{
	int i = 0;
	
	//find next open frame - if any still exist
	while(myTLB.validBit[i] == 1 && i < TLB_SIZE) 
	{
		i++;
	}
	
	//no open frames - find next 0 bit for secondChance
	if(i > TLB_SIZE - 1)
	{
		
		i = secondChanceIndexTLB; //start where we stopped before - round robin style
		while(myTLB.secondChance[i] == 1 && i < TLB_SIZE)
		{
			i++;
		}
		
		//if we go to the end of the array without finding one, start over
		if(i == TLB_SIZE)
		{
			i = 0;
			while(myTLB.secondChance[i] == 1 && i < TLB_SIZE)
			{
				i++;
			}
			//if all have a 1, just replace 0
			if(i == TLB_SIZE)
				i = 0;
				
		}
		//knock down second chance bit
		for(int j = 0; j < TLB_SIZE; j++)
			myTLB.secondChance[j] = 0;
	}
	secondChanceIndexTLB = i + 1;
	
	myTLB.pageNumber[i] = pageNumber;
	myTLB.frameNumber[i] = frameNumber;
	myTLB.validBit[i] = 1; //set bit to valid
	
}

int getValueFromAddress(int logicAddress, FILE* data)
{
	int frameNumber = -1;
	int value;
	int offset;
	int i;
	int pageNumber;
	pageNumber = addressTranslationPage(logicAddress);
	//check TLB first
	//if not in TLB check page table
	//if not in page table must load into memory
	
	//TLB
	//walk through the TLB and see if the page is there
	for(int i = 0; i < TLB_SIZE; i++)
	{
		if(myTLB.pageNumber[i] == pageNumber)
		{
			frameNumber = myTLB.frameNumber[i];
			myTLB.secondChance[i] = 1;
			TLBHits++;
			break;
		}
	}
	//print value if found in TLB
	if(frameNumber != -1)
	{
		offset = addressTranslationOffset(logicAddress);
		value = physicalMemory[frameNumber][offset];
		printf("Virtual Address: %d Physical address: %d Value: %d\n", logicAddress, frameNumber * 256 + offset, value);
	}
	else
	{
		//PAGE TABLE	
		//walk through page table and see if page is already loaded
	
		for(int i = 0; i < NUMBER_OF_FRAMES; i++)
		{
			if(myPageTable.page[i] == pageNumber && myPageTable.validBit[i] == 1)
			{
				frameNumber = myPageTable.frameNumber[i];
				myPageTable.secondChance[i] = 1;
				break;
			}
		}
		
		
		//print value/physical address if found on page table, update TLB
		if(frameNumber != -1)
		{
			
			offset = addressTranslationOffset(logicAddress);
			value = physicalMemory[frameNumber][offset];
			updateTLB(pageNumber, frameNumber);
			printf("Virtual Address: %d Physical address: %d Value: %d\n", logicAddress, frameNumber * 256 + offset, value);
		}
		
		//LOAD NEW FRAME into memory, after it has been loaded find the value at [frame][offset]
		else
		{
			PageFaults++;
			offset = addressTranslationOffset(logicAddress);
			frameNumber = loadNewPage(logicAddress, data);
			value = physicalMemory[frameNumber][offset];
	
	
			printf("Virtual Address: %d Physical address: %d Value: %d\n", logicAddress, frameNumber * 256 + offset, value);
		}
	}
}

int setUpPageTable()
{
	for(int i = 0; i < PAGE_TABLE_SIZE; i++)
	{
		myPageTable.validBit[i] = 0;
		myPageTable.secondChance[i] = 0;
	}
	for(int i = 0; i < TLB_SIZE; i++)
	{
		myTLB.validBit[i] = 0;
		myTLB.secondChance[i] = 0;
	}
}

int addressTranslationPage(int address)
{
	if(address > 65535 || address < 0) //check to see if address is in range
	{
		perror("address not in range");
	}
	return address / FRAME_SIZE; //gets the page number due to integer division
}

int addressTranslationOffset(int address)
{
	if(address > 65535 || address < 0) //check to see if address is in range
	{
		perror("address not in range");
	}
	return address % FRAME_SIZE; //gets the offset - last 8 bits, 2^8 is 256
}
	
