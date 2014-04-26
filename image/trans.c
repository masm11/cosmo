#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/xpm.h>
#include <stdio.h>
#include <stdlib.h>

static void calc_half(
	double x10, double y10, double x20, double y20, double x30, double y30,
	double x11, double x21, double x31,
	double *a, double *b, double *e)
{
    *a = ((x11 - x31) * (y20 - y30) - (x21 - x31) * (y10 - y30)) / ((x10 - x30) * (y20 - y30) - (x20 - x30) * (y10 - y30));
    if (y10 - y30 < -1.0 || y10 - y30 >= 1.0)
	*b = ((x11 - x31) - (x10 - x30) * *a) / (y10 - y30);
    else
	*b = ((x21 - x31) - (x20 - x30) * *a) / (y20 - y30);
    *e = x11 - x10 * *a - y10 * *b;
}

static void calc_params(
	double x10, double y10, double x20, double y20, double x30, double y30,
	double x11, double y11, double x21, double y21, double x31, double y31,
	double *a, double *b, double *c, double *d, double *e, double *f)
{
    calc_half(x10, y10, x20, y20, x30, y30, x11, x21, x31, a, b, e);
    calc_half(x10, y10, x20, y20, x30, y30, y11, y21, y31, c, d, f);
}

int main(int argc, char **argv)
{
    /*
      変換式
      x1 = a * x0 + b * y0 + e;
      y1 = c * x0 + d * y0 + f;
      の係数 a, b, c, d, e, f を求める。
      
      データは、
      (x10,y10) -> (x11,y10)
      (x20,y20) -> (x21,y20)
      (x30,y30) -> (x31,y30)
      
      上側の式について、a, b, e を求める。
      
      x11 = a * x10 + b * y10 + e;
      x21 = a * x20 + b * y20 + e;
      x31 = a * x30 + b * y30 + e;
      
      x11 - x31 = (x10 - x30) * a + (y10 - y30) * b;
      x21 - x31 = (x20 - x30) * a + (y20 - y30) * b;
      
      (x11 - x31) * (y20 - y30) = (x10 - x30) * (y20 - y30) * a + (y10 - y30) * (y20 - y30) * b;
      (x21 - x31) * (y10 - y30) = (x20 - x30) * (y10 - y30) * a + (y20 - y30) * (y10 - y30) * b;
      
      (x11 - x31) * (y20 - y30) - (x21 - x31) * (y10 - y30) = ((x10 - x30) * (y20 - y30) - (x20 - x30) * (y10 - y30)) * a;
      
      a = ((x11 - x31) * (y20 - y30) - (x21 - x31) * (y10 - y30)) / ((x10 - x30) * (y20 - y30) - (x20 - x30) * (y10 - y30));
      
      **
      
      x11 - x31 = (x10 - x30) * a + (y10 - y30) * b;
      
      b = ((x11 - x31) - (x10 - x30) * a) / (y10 - y30);
      
      または、
      
      x21 - x31 = (x20 - x30) * a + (y20 - y30) * b;
      
      b = ((x21 - x31) - (x20 - x30) * a) / (y20 - y30);
      
      **
      
      e = x11 - x10 * a - y10 * b;
      
      **
      
      下側の式についても同様。
    */
    
    char *src_fname, *dst_fname;
    
    int x10, y10, x20, y20, x30, y30;
    int x11, y11, x21, y21, x31, y31;
    double a, b, c, d, e, f;
    
    int *ints[] = {
	&x10, &y10, &x20, &y20, &x30, &y30,
	&x11, &y11, &x21, &y21, &x31, &y31,
    };
    int i;
    
    for (i = 0; i < sizeof ints / sizeof ints[0]; i++)
	*ints[i] = atoi(argv[1 + i]);
    
    src_fname = argv[1 + i];
    dst_fname = argv[1 + i + 1];
    
    calc_params(
	    x11,  y11,  x21,  y21,  x31,  y31,
	    x10,  y10,  x20,  y20,  x30,  y30,
	    &a,  &b,  &c,  &d,  &e,  &f);
    
    printf("%lf %lf %lf\n", a, b, e);
    printf("%lf %lf %lf\n", c, d, f);
    
    printf("%d %d %lf %lf\n",
	    x11, y11,
	    a * x11 + b * y11 + e,
	    c * x11 + d * y11 + f);
    
    printf("%d %d %lf %lf\n",
	    x21, y21,
	    a * x21 + b * y21 + e,
	    c * x21 + d * y21 + f);
    
    printf("%d %d %lf %lf\n",
	    x31, y31,
	    a * x31 + b * y31 + e,
	    c * x31 + d * y31 + f);
    
    
    int er;
    
    XpmImage src, dst;
    XpmInfo srcinfo, dstinfo;
    if ((er = XpmReadFileToXpmImage(src_fname, &src, &srcinfo)) != 0) {
	fprintf(stderr, "%s: %s\n", src_fname, XpmGetErrorString(er));
	exit(1);
    }
    if ((er = XpmReadFileToXpmImage(src_fname, &dst, &dstinfo)) != 0) {
	fprintf(stderr, "%s: %s\n", src_fname, XpmGetErrorString(er));
	exit(1);
    }
    
    int sx, sy, dx, dy;
    
    for (dy = 0; dy < src.height; dy++) {
	for (dx = 0; dx < src.width; dx++) {
	    sx = a * dx + b * dy + e;
	    sy = c * dx + d * dy + f;
	    
	    if (sx < 0 || sx >= src.width ||
		    sy < 0 || sy >= src.height)
		dst.data[dy * dst.width + dx] = 0;
	    else
		dst.data[dy * dst.width + dx] = src.data[sy * src.width + sx];
	}
    }
    
    if ((er = XpmWriteFileFromXpmImage(dst_fname, &dst, &dstinfo)) != 0) {
	fprintf(stderr, "%s: %s\n", dst_fname, XpmGetErrorString(er));
	exit(1);
    }
    
}
