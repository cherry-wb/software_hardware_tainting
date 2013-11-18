#include <stdio.h>
#include <stdlib.h>

void DYTAN_tag(void *addr, size_t size, char *name) {}
void DYTAN_display(void * addr, size_t size, char *fmt, ...) {}

int count = 7;

void prRandStr(int n){
	
	DYTAN_display(&n, sizeof(n), "input");

	int i, seed,tmp;
	char *buffer;

	buffer = (char*) malloc (n);
	DYTAN_tag(buffer, n, "b");
	DYTAN_tag(&buffer, sizeof(char*), "b");
	DYTAN_display(buffer,n,"memory");
	DYTAN_display(&buffer,sizeof(char*),"pointer");

	if (buffer == NULL) return;

	seed = count;
	srand(seed);

	for(i=0; i<=n; i++){
		buffer[i] = rand()%26+ 'a';
		DYTAN_display(buffer+i, sizeof(buffer[i]),"memory");
		DYTAN_display(&buffer,sizeof(char*),"pointer");
	}
	buffer[n-1] = '\0';

	free(buffer);
	printf("Random string: %s\n", buffer);
}

int main(int argc, char** argv){
	prRandStr(atoi(argv[1]));
}
