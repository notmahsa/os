#include <assert.h>
#include "common.h"
#include "point.h"
#include <math.h>

void
point_translate(struct point *p, double x, double y)
{
	point_set(p, point_X(p) + x, point_Y(p) + y);
}

double
point_distance(const struct point *p1, const struct point *p2)
{
	return sqrt(pow(point_X(p2) - point_X(p1), 2) +
	            pow(point_Y(p2) - point_Y(p1), 2) * 1.0);;
}

int
point_compare(const struct point *p1, const struct point *p2)
{
	float euc1 = sqrt(point_X(p1)*point_X(p1) + point_Y(p1)*point_Y(p1));
	float euc2 = sqrt(point_X(p2)*point_X(p2) + point_Y(p2)*point_Y(p2));
	int out = 0;
	if (euc1 < euc2)
	    out = -1;
	else if (euc2 < euc1)
	    out = 1;
	return out;
}
