#include <stdio.h>
#include <stdlib.h>

void DYTAN_tag_pointer(void *addr, size_t size, size_t id) {}
void DYTAN_tag_memory(void *addr, size_t size, size_t id) {}
void DYTAN_free(void *addr){}
//void DYTAN_check(void *pointer, void *memory){}

int seed_value = 7;

void prRandStr(int n){

	int i, seed;
	char *buffer;
	buffer = (char*) malloc (n);
	buffer = (char*) realloc(buffer, n+n);
	//debugging info
	printf("pointer %p\n",&buffer);
	printf("memory %p\n",buffer);

	if (buffer == NULL) return;

	seed = seed_value;
	srand(seed);

	char new=0;
	for(i=0; i<=n+n; i++){
		buffer[i] = rand()%26+'a';
	}
	buffer[n+n-1] = '\0';

	free(buffer);

	printf("Random string: %s\n", buffer);
	//debugging info
        printf("pointer %p\n",&buffer);
        printf("memory %p\n",buffer);
}

int main(int argc, char** argv){
	prRandStr(atoi(argv[1]));
}
