#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "spimage.h"

 
/* Reads a white space separated list of floating point values and stores them in a matrix of predetermined size */

Image * read_matrix(char * filename, int x, int y){
  FILE * f = fopen(filename,"r");
  unsigned int bufsiz = 100000;
  char * buffer = malloc(sizeof(char)*bufsiz);
  real * matrix = malloc(sizeof(real)*x*y);
  char *nptr,*endptr;
  int i = 0;
  Image * res = malloc(sizeof(Image));
  res->phased = 0;
  res->scaled = 0;
  res->image = matrix;
  res->intensities = matrix;
  res->amplitudes = NULL;
  res->r = NULL;
  res->c = NULL;
  res->mask = malloc(sizeof(real)*x*y);
  for(i =0 ;i<x*y;i++){
    res->mask[i] = 1;
  }
  res->detector = malloc(sizeof(Detector));
  res->shifted = 0;
  res->detector->size[0] = x;
  res->detector->size[1] = y;
  res->detector->image_center[0] = 0;
  res->detector->image_center[1] = 0;
  res->detector->lambda = -1;
  res->detector->pixel_size = -1;
  res->detector->detector_distance = -1;
  if(!f){
    perror("Error opening matrix");
    return NULL;
  }
  i = 0;
  while(fgets(buffer,bufsiz,f)){
    if(strlen(buffer) == bufsiz){
      printf("Please increase bufsiz or split your lines a bit!\n");
      exit(1);
    }
    nptr = buffer;
    endptr = NULL;
    while(1){
      matrix[i++] = strtod(nptr,&endptr);
      if(nptr == endptr){
	i--;
	break;
      }else{
	nptr = endptr;
      }
    }
  }
  fclose(f);
  if(i != x*y){
    printf("Error dimension don't match\n");
    printf("Expected %d numbers but read, %d\n",x*y,i);
  }else{
    printf("Read %d values out of %d expected\n",i,x*y);
  }
  return res;
}

int main(int argc, char ** argv){
  Image * res;
  if(argc != 5){
    printf("Usage: %s <matrix_file> <nx> <ny> <image.h5>\n",argv[0]);
    exit(0);
  }
  res = read_matrix(argv[1],atoi(argv[2]),atoi(argv[3]));
  write_img(res,argv[4],sizeof(real));  
  return 0;
}
