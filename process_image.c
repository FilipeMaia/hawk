#if defined(_MSC_VER) || defined(__MINGW32__)
#include <direct.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <math.h>
#include <hdf5.h>
#include <fftw3.h>
#include <getopt.h>
#include "spimage.h"
#include "process_image.h"


Image * centrosymetry_average(Image * img){
  int x,y;
  int xs,ys;
  int i,is;
  Image * out = sp_image_duplicate(img,SP_COPY_DATA|SP_COPY_MASK);
  real num;
  int den;
  for(x = 0;x<sp_cmatrix_cols(img->image);x++){
    xs = 2*img->detector->image_center[0]-x;
    if(xs >= 0 && xs < sp_cmatrix_cols(img->image)){
      for(y = 0;y<sp_cmatrix_rows(img->image);y++){
	ys = 2*img->detector->image_center[1]-y;
	if(ys >= 0 && ys < sp_cmatrix_rows(img->image)){
	  i = x*sp_cmatrix_rows(img->image)+y;
	  is = xs*sp_cmatrix_rows(img->image)+ys;
	  den = 0;
	  num = 0;
	  if(img->mask->data[i]){
	    den++;
	    num += img->image->data[i];
	  }
	  if(img->mask->data[is]){
	    den++;
	    num += img->image->data[is];
	  }
	  if(den){
	    out->image->data[i] = num/den;
	    out->mask->data[i] = 1;
	    out->image->data[is] = num/den;
	    out->mask->data[is] = 1;
	  }
	}
      }      
    }
  }
  return out;
}

void subtract_dark(Image * img, Image * dark){
  int i;
  for(i = 0;i<sp_cmatrix_size(img->image);i++){
    img->image->data[i] -= dark->image->data[i];
    if(creal(img->image->data[i]) < 0){
      img->image->data[i] = 0;
    }
  }
}

Image * speed_pad_image(Image * in){
  char buffer[1024];
  char buffer2[1024];
  char * home_dir;
  FILE * f;
  int size,time,opt_size;
  #if defined(_MSC_VER) || defined(__MINGW32__)
  home_dir = getenv("USERPROFILE");
#else
  home_dir = getenv("HOME");
#endif
  sprintf(buffer2,"%s/.uwrapc/fft_speed",home_dir);  
  f = fopen(buffer2,"r");
  if(!f){
    /* For some reason can't access benchmark file */
    return sp_image_duplicate(in,SP_COPY_DATA|SP_COPY_MASK);
  }
  while(fgets(buffer,1023,f)){
    sscanf(buffer,"%d\t%d\t%d\t",&size,&time,&opt_size);
    if(size == sp_cmatrix_cols(in->image)){
      /* we have a match */
      return zero_pad_image(in,opt_size,opt_size,1);
    }else if(size < sp_cmatrix_cols(in->image)){
      return sp_image_duplicate(in,SP_COPY_DATA|SP_COPY_MASK);
    }
  }
  return NULL;
}


real sum_square(Image * in, int x1, int y1, int x2, int y2){
  int i,j;
  real sum = 0;
  for(i = x1;i <= x2;i++){
    for(j = y1;j<=y2;j++){
      sum += in->image->data[i*sp_cmatrix_rows(in->image)+j];
    }
  }
  return sum;
}


real sum_square_edge(Image * in, int x1, int y1, int x2, int y2){
  int i,j;
  real sum = 0;
  for(i = x1;i <= x2;i++){
    for(j = y1;j<=y2;j++){
      if(i != x1 && i != x2){
	sum += in->image->data[i*sp_cmatrix_rows(in->image)+y1];
	sum += in->image->data[i*sp_cmatrix_rows(in->image)+y2];
	break;
      }
      sum += in->image->data[i*sp_cmatrix_rows(in->image)+j];
    }
  }
  return sum;
}


real max_square_edge(Image * in, int x1, int y1, int x2, int y2){
  int i,j;
  real max = 0;
  for(i = x1;i <= x2;i++){
    for(j = y1;j<=y2;j++){
      if(i != x1 && i != x2){
	if(max < creal(in->image->data[i*sp_cmatrix_rows(in->image)+y1])){
	  max = creal(in->image->data[i*sp_cmatrix_rows(in->image)+y1]);
	}else if(max < creal(in->image->data[i*sp_cmatrix_rows(in->image)+y2])){
	  max = creal(in->image->data[i*sp_cmatrix_rows(in->image)+y2]);
	}
	break;
      }
      if(max < creal(in->image->data[i*sp_cmatrix_rows(in->image)+j])){
	max = creal(in->image->data[i*sp_cmatrix_rows(in->image)+j]);
      }
    }
  }
  return max;
}


Image * limit_sampling(Image * img, real oversampling_factor, real cutoff){
  /* The space limiting criteria will be everything 10x smaller than the patterson cutoff */
  /* I'm gonna take the patterson of a blurred version of the diffraction pattern due to "hot pixels" and "blue spots" */
  Image * blur_pat = sp_image_patterson(gaussian_blur(img,5));
  Image * pat = sp_image_patterson(img);
  Image * resampled;
  Image * s_pat;
  Image * out;
  real max = 0;
  real current_max = 0;  
  int i;
  write_png(pat,"pat.png",COLOR_JET);
  max = sp_cmatrix_max(blur_pat->image,NULL);
  for(i = 0;;i++){
    current_max = max_square_edge(blur_pat,i,i,sp_cmatrix_cols(blur_pat->image)-i-1, sp_cmatrix_cols(blur_pat->image)-i-1);
    if(max * cutoff < current_max){
      break;
    }
  }
  sp_image_free(blur_pat);
  s_pat = sp_image_shift(pat);
  sp_image_free(pat);
  out = bilinear_rescale(img,(sp_cmatrix_cols(s_pat->image)/2-i)*oversampling_factor*2,(sp_cmatrix_cols(s_pat->image)/2-i)*oversampling_factor*2);
  resampled = sp_image_low_pass(s_pat,(sp_cmatrix_cols(s_pat->image)/2-i)*oversampling_factor);  
  sp_image_free(s_pat);
  return out;
}

Image * downsample(Image * img, real downsample_factor){
  int size_x = sp_cmatrix_cols(img->image)/downsample_factor;
  int size_y = sp_cmatrix_rows(img->image)/downsample_factor;
/*  Image * mask;
  Image * low_passed = low_pass_gaussian_filter(img,size);
  write_png(low_passed,"low_passed.png",COLOR_JET|LOG_SCALE);
  mask = sp_image_duplicate(low_passed,SP_COPY_DATA|SP_COPY_MASK);
  memcpy(mask->image,mask->mask,sp_cmatrix_size(mask->image)*sizeof(real));
  write_png(mask,"low_passed_mask.png",COLOR_JET|LOG_SCALE);
  sp_image_free(mask);*/

  Image * downsampled =  bilinear_rescale(img,size_x,size_y);
  write_png(downsampled,"downsampled.png",COLOR_JET|LOG_SCALE);
/*  mask = sp_image_duplicate(downsampled,SP_COPY_DATA|SP_COPY_MASK);
  memcpy(mask->image,mask->mask,sp_cmatrix_size(mask->image)*sizeof(real));
  write_png(mask,"downsampled_mask.png",COLOR_JET|LOG_SCALE);
  sp_image_free(low_passed);
  sp_image_free(mask);*/
  return downsampled;    
}


/* Search and mask overexposure */
void mask_overexposure(Image * img,real saturation){
  int i;

  for(i = 0;i<sp_cmatrix_size(img->image);i++){
    /* 16 bit detector apparently */
    /* mask over 20k */
    if(creal(img->image->data[i]) >= saturation){
      img->mask->data[i] = 0;
      img->image->data[i] = 0;
    }
  }  
}

/* Search and mask overexposure */
void remove_background(Image * img, Options * opt){
  int i;
  for(i = 0;i<sp_cmatrix_size(img->image);i++){
    img->image->data[i] -= opt->background;
    if(creal(img->image->data[i]) < 0){
      img->image->data[i] = 0;
      img->mask->data[i] = 0;
    }
  }  
}



/* Remove all signal deemed too small */
void remove_noise(Image * img, Image * noise){
  int i;
/*  Image * variance = image_local_variance(noise,rectangular_window(10,10,10,10,0));*/
  for(i = 0;i<sp_cmatrix_size(img->image);i++){
    if(creal(img->image->data[i] - noise->image->data[i]) < sqrt(img->image->data[i])){
      img->image->data[i] = 0;
    }else{
      img->image->data[i] -= noise->image->data[i];
    }
  }
}


/* Get amplitudes */
void intensity_to_amplitudes(Image * img){
  int i;
  img->scaled = 1;
  for(i = 0;i<sp_cmatrix_size(img->image);i++){
    if(img->mask->data[i]){
      img->image->data[i] = sqrt(img->image->data[i]);
    }else{
      img->image->data[i] = 0;
    }
  }  
}



Options * parse_options(int argc, char ** argv){
  int c;
  static char help_text[] = 
    "    Options description:\n\
    \n\
    -i: Input file\n\
    -o: Output file\n\
    -s: Saturation level of the detector (20000)\n\
    -a: Downsample by x times\n\
    -g: Background level of the detector (550)\n\
    -b: Beamstop size in pixels (55)\n\
    -m: Mask to mask out pixels\n\
    -p: Pad image to improve fft speed\n\
    -r: Maximum resolution to use in pixels (400)\n\
    -C: Use centrosymetry to assign values to missing data\n\
    -c: User set image center (300x300)\n\
    -d: Dark image file\n\
    -S: Do not shift quadrants\n\
    -n: Noise image file\n\
    -v: Produce lots of output files for diagnostic\n\
    -h: print this text\n\
";
  static char optstring[] = "c:Cx:s:b:r:hi:o:g:a:pd:m:vSn:";
  Options * res = calloc(1,sizeof(Options));
  set_defaults(res);

  while(1){
    c = getopt(argc,argv,optstring);
    if(c == -1){
      break;
    }
    switch(c){
    case 'a':
      res->oversampling_factor = atof(optarg);
      break;
    case 'g':
      res->background = atof(optarg);
      break;
    case 's':
      res->saturation = atof(optarg);
      break;
    case 'b':
      res->beamstop= atof(optarg);
      break;
    case 'r':
      res->resolution = atof(optarg);
      break;
    case 'x':
      res->cross_removal = atof(optarg);
      break;
    case 'i':
      strcpy(res->input,optarg);
      break;
    case 'o':
      strcpy(res->output,optarg);
      break;
    case 'n':
      strcpy(res->noise,optarg);
      break;
    case 'd':
      strcpy(res->dark,optarg);
      break;
    case 'm':
      strcpy(res->mask,optarg);
      break;
    case 'C':
      res->centrosymetry = 1;
      break;
    case 'c':
      res->user_center_x = atof(optarg);
      res->user_center_y = atof(strstr(optarg,"x")+1);
      res->centrosymetry = 1;
      break;
    case 'p':
      res->pad = 1;
      break;
    case 'v':
      res->verbose = 1;
      break;
    case 'S':
      res->shift_quadrants = 0;
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

void set_defaults(Options * opt){
  opt->background = 0;
  opt->saturation = 0;
  opt->beamstop = 0;
  opt->resolution = 0;
  opt->input[0] = 0;
  opt->dark[0] = 0;
  opt->noise[0] = 0;
  opt->output[0] = 0;
  opt->oversampling_factor = 0;
  opt->cross_removal = 0;
  opt->pad = 0;
  opt->mask[0] = 0;
  opt->verbose = 0;
  opt->centrosymetry = 0;
  opt->shift_quadrants = 1;
  opt->user_center_x = -1;
  opt->user_center_y = -1;
}


int main(int argc, char ** argv){
  Image * img;
  Image * out;
  Image * dark;
  Image * noise;
  Image * mask;
  Image * autocorrelation;
  sp_imatrix * kernel;

  Options * opts;  
  char buffer[1024] = "";
  char buffer2[1024] = "";
  int i;
  FILE * f;
  int tmp;
  real max =0;
  opts = malloc(sizeof(Options));
  set_defaults(opts);
#if defined(_MSC_VER) || defined(__MINGW32__)
  _getcwd(buffer,1023);
#else
  getcwd(buffer,1023);
#endif
  strcat(buffer,"> ");
  for(i = 0;i<argc;i++){
    strcat(buffer,argv[i]);
    strcat(buffer," ");
  }
  opts = parse_options(argc,argv);
  /* write log */
  sprintf(buffer2,"%s.log",opts->output);
  f = fopen(buffer2,"w");
  fprintf(f,"%s\n",buffer);
  fclose(f);
  if(!opts->input[0] || !opts->output[0]){
    printf("Use process_image -h for details on how to run this program\n");
    exit(0);
  }

  img = read_imagefile(opts->input);
  if(img->shifted){
    img = sp_image_shift(img);
  }
  if(opts->dark[0]){
    if(opts->verbose){
      write_png(img,"before_minus_dark.png",COLOR_JET|LOG_SCALE);
    }
    dark = read_imagefile(opts->dark);
    subtract_dark(img,dark);
    if(opts->verbose){
      write_png(img,"after_minus_dark.png",COLOR_JET|LOG_SCALE);
    }
  }
  if(opts->noise[0]){
    if(opts->verbose){
      write_png(img,"before_minus_noise.png",COLOR_JET|LOG_SCALE);
    }
    noise = read_imagefile(opts->noise);
    remove_noise(img,noise);
    if(opts->verbose){
      write_png(img,"after_minus_noise.png",COLOR_JET|LOG_SCALE);
    }
  }
  if(opts->mask[0]){
    mask = read_imagefile(opts->mask);
    if(sp_cmatrix_size(mask->image) != sp_cmatrix_size(img->image)){
      fprintf(stderr,"Mask file size different than image size\n");
      exit(1);
    }
    for(i = 0;i<sp_cmatrix_size(mask->image);i++){
      if(mask->image->data[i] == 0){
	img->mask->data[i] = 0;
      }
    }
    if(opts->verbose){
      write_mask_to_png(img,"after_apllying_mask.png",COLOR_GRAYSCALE);
    }
  }
/*  write_png(img,"really_before.png",COLOR_JET);*/

/*  write_vtk(img,"before.vtk");*/
/*  out =  make_unshifted_image_square(img);
  write_png(out,"square.png",COLOR_JET);
  write_mask_to_png(out,"square_mask.png",COLOR_JET);
  sp_image_free(img);
  img = sp_image_duplicate(out,SP_COPY_DATA|SP_COPY_MASK);
  sp_image_free(out);*/

  if(opts->saturation){
    mask_overexposure(img,opts->saturation); 
  }
  if(opts->verbose){
     write_mask_to_png(img,"after_saturation_mask.png",COLOR_JET);
  }


  if(opts->verbose){
    write_vtk(img,"before_smooth.vtk");
  }

  real value = 5;
  Image * soft_edge = sp_image_duplicate(img,SP_COPY_MASK|SP_COPY_DATA);
  sp_image_smooth_edges(soft_edge,soft_edge->mask,SP_GAUSSIAN,&value);
  if(opts->verbose){
    write_vtk(soft_edge,"after_smooth.vtk");
  }
  
  autocorrelation = sp_image_patterson(soft_edge);
  sp_image_adaptative_constrast_stretch(autocorrelation,20,20);
  write_vtk(autocorrelation,"autocorrelation.vtk");


  if(opts->oversampling_factor){
    out = downsample(img,opts->oversampling_factor);
    sp_image_free(img);
    img = sp_image_duplicate(out,SP_COPY_DATA|SP_COPY_MASK);
    sp_image_free(out);
  }
  if(opts->verbose){
    write_png(img,"after_resampling.png",COLOR_JET|LOG_SCALE);
    write_mask_to_png(img,"after_resampling_mask.png",COLOR_JET);
  }
  
  for(i = 0;i<sp_cmatrix_size(img->image);i++){
    if(img->mask->data[i] && creal(img->image->data[i]) > max){
      max = img->image->data[i];
    }
  }
  printf("max - %f\n",max);

  remove_background(img,opts);
  if(!img->scaled){
    intensity_to_amplitudes(img);
  }

  if(opts->user_center_x > 0){
    img->detector->image_center[0] = opts->user_center_x;
    img->detector->image_center[1] = opts->user_center_y;
  }else{
    find_center(img,&(img->detector->image_center[0]),&(img->detector->image_center[1]));
  }
  tmp = MIN(img->detector->image_center[0],(sp_cmatrix_cols(img->image)-img->detector->image_center[0]));
  tmp = MIN(tmp,img->detector->image_center[1]);
  tmp = MIN(tmp,(sp_cmatrix_rows(img->image)-img->detector->image_center[1]));
  printf("Minimum distance from center to image edge - %d\n",tmp);

  for(i = 0;i<sp_cmatrix_size(img->image);i++){
    if(sp_image_dist(img,i,SP_TO_CENTER) < opts->beamstop /*&& img->img[i] < 1000*/){
      img->image->data[i] = 0;
      img->mask->data[i] = 0;
    }
    if(sp_image_dist(img,i,SP_TO_AXIS) < opts->cross_removal){
      img->image->data[i] = 0;
      img->mask->data[i] = 0;
    }
  }
  if(opts->verbose){
    write_png(img,"after_beamstop.png",COLOR_JET);
  }

  if(opts->centrosymetry){
    out = centrosymetry_average(img);
    sp_image_free(img);
    img = out;
  }  
  if(opts->shift_quadrants){
    out = sp_image_shift(img);
    if(opts->verbose){
      write_png(out,"after_shift.png",COLOR_JET);
      write_mask_to_png(out,"after_shift_mask.png",COLOR_JET);
    }
  }else{
    out = sp_image_duplicate(img,SP_COPY_DATA|SP_COPY_MASK);
  }
  sp_image_free(img);
  if(opts->resolution){
    img = sp_image_low_pass(out, opts->resolution);
  }else{
    img = sp_image_duplicate(out,SP_COPY_DATA|SP_COPY_MASK);
  }
  if(opts->verbose){
    write_png(img,"after_shift_and_lim.png",COLOR_JET);
    write_mask_to_png(img,"after_shift_and_lim_mask.png",COLOR_JET);
  }

  if(opts->pad){
    out = speed_pad_image(img);
  }else{
    out = sp_image_duplicate(img,SP_COPY_DATA|SP_COPY_MASK);
  }

  for(i = 0;i<sp_cmatrix_size(img->image);i++){
    if(!img->mask->data[i]){
      img->image->data[i] = 0;
    }  
  }
  sp_image_write(out,opts->output,sizeof(real));

  /* also write the autocorrelation */
  for(i = 0;i<sp_cmatrix_size(out->image);i++){
    if(!out->mask->data[i]){
      out->image->data[i] *= out->image->data[i];
    }
  }
  return 0;
}
