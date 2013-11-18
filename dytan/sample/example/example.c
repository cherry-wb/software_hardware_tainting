#include <stdio.h>
#include <stdlib.h>

void DYTAN_tag(void *addr, size_t size, size_t id) {}
void DYTAN_display(void * addr, size_t size, char *fmt, ...) {}
void DYTAN_free(void *addr){}

int count = 7;

void prRandStr(int n){

	int i, seed;
	char *buffer;

	buffer = (char*) malloc (n);
	buffer = (char*) realloc(buffer, n);
	//buffer = (char*) calloc (n, sizeof(char));
	
	//DYTAN_tag(buffer,n,5);

	if (buffer == NULL) return;

	seed = count;
	srand(seed);

	DYTAN_display(buffer,n,"buffer before for");

	for(i=0; i<=n; i++){
		buffer[i] = rand()%26+'a';
	}
	buffer[n-1] = '\0';

	DYTAN_display(buffer,n,"buffer before free");
	free(buffer);
	//DYTAN_free(buffer);
	DYTAN_display(buffer,n,"buffer after DYTAN free");

	printf("Random string: %s\n", buffer);
}

int main(int argc, char** argv){
	prRandStr(atoi(argv[1]));
}
