#include <stdlib.h>
#include <stdio.h>
#include <tiffio.h>
#include <hdf5.h>
#include <getopt.h>
#include "spimage.h"
#include "tiff2h5.h"

void set_defaults(Options * opt){
  opt->x_center = -1;
  opt->y_center = -1;
  opt->detector_distance = 50;
  opt->pixel_size = 0.020;
  opt->lambda = 32;
  opt->input[0] = 0;
  opt->output[0] = 0;
}


Options * parse_options(int argc, char ** argv){
  int c;
  static char help_text[] = 
"    Options description:\n\
    \n\
    -i: Input file\n\
    -o: Output file\n\
    -x: Center of image x coordinate (690)\n\
    -y: Center of image y coordinate (608)\n\
    -d: Distance to detector (in mm) (50)\n\
    -p: Pixel Size (in mm) (0.020)\n\
    -l: Wavelength (in nm) (32)\n\
";
  static char optstring[] = "x:y:d:p:l:hi:o:";
  Options * res = calloc(1,sizeof(Options));
  set_defaults(res);

  while(1){
    c = getopt(argc,argv,optstring);
    if(c == -1){
      break;
    }
    switch(c){
    case 'x':
      res->x_center = atof(optarg);
      break;
    case 'y':
      res->y_center = atof(optarg);
      break;
    case 'd':
      res->detector_distance = atof(optarg);
      break;
    case 'p':
      res->pixel_size= atof(optarg);
      break;
    case 'l':
      res->lambda = atof(optarg);
      break;
    case 'i':
      strcpy(res->input,optarg);
      break;
    case 'o':
      strcpy(res->output,optarg);
      break;
    case 'h':
      printf("%s",help_text);
      exit(0);
      break;
    default:
      printf ("?? getopt returned character code 0%o ??\n", c);
    }
  }
  return res;
}


int main(int argc, char ** argv){
  int nstrips;
  int stripsize;
  int i,x,y;
  unsigned char * img;
  float * tmpf;
  unsigned short * tmpi;
  unsigned short * tmpui;
  unsigned char * tmpuc;
  Options * opts;  
  char buffer[1024];
  int bpp = 4;
  int datatype = 0;
  int width,height;
  Image * out = malloc(sizeof(Image));
  Image * autocorrelation;
  Image * tmp;
  TIFF * tif;
  out->detector = malloc(sizeof(Detector));
  opts = malloc(sizeof(Options));
  set_defaults(opts);
  opts = parse_options(argc,argv);

  if(!opts->input[0] || !opts->output[0]){
    printf("Use %s -h for details on how to run this program\n",argv[0]);
    exit(0);
  }

  tif = TIFFOpen(opts->input, "r");
  if(TIFFGetField(tif,TIFFTAG_BITSPERSAMPLE,&bpp)){
    bpp /= 8;
  }
  if(!TIFFGetField(tif,TIFFTAG_SAMPLEFORMAT,&datatype)){
    if(bpp == 1){
      datatype = SAMPLEFORMAT_VOID;
    }else if(bpp == 2){
      datatype = SAMPLEFORMAT_UINT;
    }
  }
  TIFFPrintDirectory(tif,stdout,0);  
  
  if(!TIFFGetField(tif,TIFFTAG_IMAGELENGTH,&height)){
    perror("Could not get image height!\n");
    return 1;
  }
  if(!TIFFGetField(tif,TIFFTAG_IMAGEWIDTH,&width)){
    perror("Could not get image width!\n");
    return 1;
  }
  
  nstrips = TIFFNumberOfStrips(tif);
  stripsize = TIFFStripSize(tif);
  img = malloc(nstrips*stripsize);
  for(i = 0;i<nstrips;i++){
    TIFFReadEncodedStrip(tif,i,img+i*stripsize,stripsize);
  }
  TIFFClose(tif);
  
  /* Transpose image (Why?!?)*/
  out->image = sp_cmatrix_alloc(height,width);
  out->mask = sp_imatrix_alloc(height,width);
  if(datatype == SAMPLEFORMAT_UINT){
    tmpui = (unsigned short *)img;
    for(x = 0;x<width;x++){
      for(y = 0;y<height;y++){
	tmpui = (unsigned short *)(img+y*width*bpp+x*bpp);
	out->image->data[x*height+y] = *tmpui;
      }    
    }
  }else if(datatype == SAMPLEFORMAT_IEEEFP){
    for(x = 0;x<width;x++){
      for(y = 0;y<height;y++){
	tmpf = (float *)(img+y*width*bpp+x*bpp);
	out->image->data[x*height+y] = *tmpf;
      }    
    }
  }else if(datatype == SAMPLEFORMAT_VOID){
    for(x = 0;x<width;x++){
      for(y = 0;y<height;y++){
	tmpuc = img+y*width*bpp+x*bpp;
	out->image->data[x*height+y] = *tmpuc;
      }    
    }
  }else if(datatype == SAMPLEFORMAT_INT){
    for(x = 0;x<width;x++){
      for(y = 0;y<height;y++){
	tmpi = (unsigned short *)(img+y*width*bpp+x*bpp);
	out->image->data[x*height+y] = (*tmpi);
      }    
    }
  }
  for(i = 0;i<sp_cmatrix_size(out->image);i++){
    out->mask->data[i] = 1;
  }
  out->scaled = 0;
  out->phased = 0;
  out->shifted = 0;
  if(opts->x_center == -1){
    opts->x_center = width/2;
  }
  if(opts->y_center == -1){
    opts->y_center = height/2;
  }
  out->detector->image_center[0] = opts->x_center;
  out->detector->image_center[1] = opts->y_center;
  out->detector->pixel_size = opts->pixel_size;
  out->detector->detector_distance = opts->detector_distance;
  out->detector->lambda = opts->lambda;
  /* write HDF5 */
  sp_image_write(out,opts->output,sizeof(real));
  autocorrelation = bilinear_rescale(sp_image_shift(sp_image_fft(sp_image_shift(out))),256,256);
  
  write_png(autocorrelation,"autocorrelation.png",COLOR_JET|LOG_SCALE);
  write_vtk(autocorrelation,"autocorrelation.vtk");
  sprintf(buffer,"%s.png",opts->output);
  sp_image_write(out,buffer,COLOR_JET|LOG_SCALE);
  sprintf(buffer,"%s.vtk",opts->output);
  write_vtk(out,buffer);
  return 0;
}
