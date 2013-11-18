/* Sample implementation of wc utility. */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

void DYTAN_tag(void *addr, size_t size, char *name) {}
void DYTAN_display(void * addr, size_t size, char *fmt, ...) {}
//void DYTAN_set_trace(int flag){}

int main (int argc, char **argv)
{
  //DYTAN_set_trace(1);

  char c;
  int i;
  int count;
  int total_count;
  FILE *fp;

  DYTAN_display(&argc, sizeof(argc), "argc=%d", argc);

  total_count = 0;
  
  for (i = 1; i < argc; i++) {

    DYTAN_display(argv[i], strlen(argv[i])+1, "argv[%d]=%s", i, argv[i]);

    count = 0;
    
    fp = fopen (argv[i], "r");

    c = fgetc(fp);
    DYTAN_tag(&c, sizeof(c), argv[i]);

    while(c != EOF) {
      if(c == '\n') count++;

      c = fgetc(fp);
      //DYTAN_tag(&c, sizeof(c), argv[i]);
    }    
    fclose(fp);
    
    //DYTAN_display(&count, sizeof(count), "count=%d", count);
    printf("%s: %d\n", argv[i], count);

    total_count += count;
  }

  //DYTAN_display(&total_count, sizeof(total_count), "total_count=%d", total_count);
  printf("Total: %d\n", total_count);
  
  return 0;
}
