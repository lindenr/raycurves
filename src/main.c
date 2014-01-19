#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>
#include <string.h>

#include "inc/vector.h"

#include "SDL/SDL.h"

SDL_Surface *gr_init ()
{
	SDL_Surface *screen = NULL;

	if (SDL_Init (SDL_INIT_VIDEO) < 0)
	{
		fprintf (stderr, "Error initialising SDL: %s\n", SDL_GetError ());
		exit (1);
	}
	
	screen = SDL_SetVideoMode (600, 600, 32, SDL_SWSURFACE);
	if (screen == NULL)
	{
		fprintf (stderr, "Error initialising video mode: %s\n", SDL_GetError ());
		exit (1);
	}

	/* Finish housekeeping */
	SDL_WM_SetCaption ("Raycurves", "Raycurves");

	return screen;
}

#define sc2(f1,f2) ((SC2){(f1),(f2)})
#define sc3(f1,f2,f3) ((SC3){(f1),(f2),(f3)})

typedef float SC2[2];
typedef float SC3[3];

typedef enum
{
	SC_NONE = 0,
	SC_POINTLIGHT,
	SC_PLANE,
	SC_SPHERE,
	SC_CAMERA,
	SC_RAY
} SC_OTYPE;

typedef struct _obPointlight
{
	SC_OTYPE type;
	SC3 pos;
	float lum;
} obPointlight;

typedef struct _obPlane
{
	SC_OTYPE type;
	SC3 pos, dir;
} obPlane;

typedef struct _obSphere
{
	SC_OTYPE type;
	SC3 pos;
	float rad;
} obSphere;

typedef struct _obCamera
{
	SC_OTYPE type;
	SC3 pos, dir;
	SC2 dim;
} obCamera;

typedef struct _obRay
{
	SC_OTYPE type;
	SC3 pos, dir;
} obRay;

typedef union _Object
{
	SC_OTYPE type;
	obPointlight pointlight;
	obPlane plane;
	obSphere sphere;
	obCamera camera;
	obRay ray;
} Object;

typedef struct _Scene
{
	Vector objs, cameras, lights;
} Scene;

Scene *sc_init ()
{
	Scene *sc = malloc(sizeof(*sc));
	sc->objs = v_dinit (sizeof(Object*));
	sc->cameras = v_dinit (sizeof(Object*));
	sc->lights = v_dinit (sizeof(Object*));
	return sc;
}

Object *sc_newobj (Scene *sc, SC_OTYPE type)
{
	Object *obj = malloc (sizeof(*obj));
	switch (type)
	{
		case SC_POINTLIGHT:
			obj->pointlight = (obPointlight) {type, {0.0, 0.0, 0.0}, 1.0};
			break;
		case SC_PLANE:
			obj->plane = (obPlane) {type, {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}};
			break;
		case SC_CAMERA:
			obj->camera = (obCamera) {type, {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 1.0}};
			break;
		case SC_RAY:
			obj->ray = (obRay) {type, {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}};
			break;
		case SC_SPHERE:
			obj->sphere = (obSphere) {type, {0.0, 0.0, 0.0}, 1.0};
			break;
		default:
			return NULL;
	}

	if (sc == NULL)
		return obj;

	v_push (sc->objs, &obj);
	if (type == SC_CAMERA)
		v_push (sc->cameras, &obj);
	else if (type == SC_POINTLIGHT)
		v_push (sc->lights, &obj);

	return obj;
}

float *ob_setpos (Object *obj, SC3 pos)
{
	switch (obj->type)
	{
		case SC_POINTLIGHT:
			return memcpy (obj->pointlight.pos, pos, sizeof(SC3));
		case SC_PLANE:
			return memcpy (obj->plane.pos, pos, sizeof(SC3));
		case SC_SPHERE:
			return memcpy (obj->sphere.pos, pos, sizeof(SC3));
		case SC_CAMERA:
			return memcpy (obj->camera.pos, pos, sizeof(SC3));
		case SC_RAY:
			return memcpy (obj->ray.pos, pos, sizeof(SC3));
		default:
			return NULL;
	}
}

float *ob_setlum (Object *obj, float lum)
{
	switch (obj->type)
	{
		case SC_POINTLIGHT:
			obj->pointlight.lum = lum;
			return &obj->pointlight.lum;
		default:
			return NULL;
	}
}

float *ob_setdir (Object *obj, SC3 dir)
{
	switch (obj->type)
	{
		case SC_PLANE:
			return memcpy (obj->plane.dir, dir, sizeof(SC3));
		case SC_CAMERA:
			return memcpy (obj->camera.dir, dir, sizeof(SC3));
		case SC_RAY:
			return memcpy (obj->ray.dir, dir, sizeof(SC3));
		default:
			return NULL;
	}
}

float *ob_setdim (Object *obj, SC2 dim)
{
	switch (obj->type)
	{
		case SC_CAMERA:
			return memcpy (obj->camera.dim, dim, sizeof(SC2));
		default:
			return NULL;
	}
}

float *ob_setrad (Object *obj, float rad)
{
	switch (obj->type)
	{
		case SC_SPHERE:
			obj->sphere.rad = rad;
			return &obj->sphere.rad;
		default:
			return NULL;
	}
}

void vm_cross (float *d, SC3 s1, SC3 s2)
{
	d[0] = s1[2] * s2[1] - s1[1] * s2[2];
	d[1] = s1[0] * s2[2] - s1[2] * s2[0];
	d[2] = s1[1] * s2[0] - s1[0] * s2[1];
}

float vm_mod (SC3 v)
{
	return sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

void vm_scale (float *d, SC3 v, float sc)
{
	d[0] = v[0] * sc;
	d[1] = v[1] * sc;
	d[2] = v[2] * sc;
}

void vm_sum (float *dest, SC3 s1, SC3 s2)
{	
	dest[0] = s1[0] + s2[0];
	dest[1] = s1[1] + s2[1];
	dest[2] = s1[2] + s2[2];
}

void vm_sub (float *dest, SC3 s1, SC3 s2)
{
	dest[0] = s1[0] - s2[0];
	dest[1] = s1[1] - s2[1];
	dest[2] = s1[2] - s2[2];
}

float vm_dot (SC3 s1, SC3 s2)
{
	return s1[0]*s2[0] + s1[1]*s2[1] + s1[2]*s2[2];
}

float gr_solvepointlight (Object *obj, SC3 point, SC3 normal, SC3 campos)
{ 
	SC3 from, from1;
	vm_sub (from, point, obj->pointlight.pos);
	vm_sub (from1, point, campos);
	if (vm_dot (from, normal)*vm_dot (from1, normal) < 0.0)
		return 0.0;
	float d2 = vm_dot (from, from);
	d2 /= vm_dot (normal, from) / (vm_mod(normal) * vm_mod(from));
	d2 /= obj->pointlight.lum;
	*((Uint32*)&d2) &= 0x7FFFFFFF;
	return d2;
}

float gr_solvelights (Scene *sc, SC3 point, SC3 normal, SC3 campos)
{
	Vector lights = sc->lights;
	int i;
	float d2 = 0.0;
	for (i = 0; i < lights->len; ++ i)
	{
		Object **obj = v_at (lights, i);
		float x = gr_solvepointlight (*obj, point, normal, campos);
		if (x != 0.0)
			d2 += 1.0 / x;
	}
	if (d2 == 0.0)
		return 0.0;
	return 255.0 / (1.0/d2 + 1);
}

void gr_solveplane (Object *obj, SC3 raypos, SC3 raydir, float *maxparam, float *maxpixel, Scene *sc, Object *camera)
{
	obPlane *plane = &obj->plane;
	SC3 v;
	vm_sub (v, plane->pos, raypos);
	//printf("%d\n", obj->type);
	//printf("%f %f %f\n\n", v[0], v[1], v[2]);
	float param = vm_dot (v, plane->dir);
	param /= vm_dot (raydir, plane->dir);
	if (param > 0.1 && param < *maxparam)
	{
		*maxparam = param;
		vm_scale (v, raydir, param);
		vm_sum (v, v, raypos);
		float br = gr_solvelights (sc, v, plane->dir, camera->camera.pos);
		maxpixel[0] = maxpixel[1] = maxpixel[2] = br;
	}
}

void gr_solvesphere (Object *obj, SC3 raypos, SC3 raydir, float *maxparam, float *maxpixel, Scene *sc, Object *camera)
{
	obSphere *sphere = &obj->sphere;
	float A = vm_dot (raydir, raydir);
	SC3 X, v;
	vm_sub (X, raypos, sphere->pos);
	float C = vm_dot (X, X) - sphere->rad * sphere->rad;
	float B = 2.0 * vm_dot (raydir, X);
	float disc = B*B - 4.0*A*C;
	if (disc < 0.0)
		return;
	float param = sqrt (disc) / (2.0 * A);
	float p1 = - B / (2.0*A) + param, p2 = - B / (2.0*A) - param;
	if (p1 > 0.1)
	{
		if (p1 < p2)
			param = p1;
		else if (p2 > 0.1)
			param = p2;
		else
			param = p1;
	}
	else if (p2 > 0.1)
		param = p2;
	else
		return;
	//printf("%f\n", param * vm_mod (raydir));
	if (param > 0.1 && param < *maxparam)
	{
		*maxparam = param;
		vm_scale (v, raydir, param);
		vm_sum (v, v, raypos);
		vm_sub (X, v, sphere->pos);
		float br = gr_solvelights (sc, v, X, camera->camera.pos);
		maxpixel[0] = maxpixel[1] = maxpixel[2] = br;
	}
}

void gr_solve (Object *obj, SC3 raypos, SC3 raydir, float *maxparam, float *maxpixel, Scene *sc, Object *camera)
{
	switch (obj->type)
	{
		case SC_PLANE:
			gr_solveplane (obj, raypos, raydir, maxparam, maxpixel, sc, camera);
			break;
		case SC_SPHERE:
			gr_solvesphere (obj, raypos, raydir, maxparam, maxpixel, sc, camera);
			break;
		default:
			break;
	}
}

void gr_render (SDL_Surface *screen, Scene *scene, Object *camera)
{
	Object *ray = sc_newobj (NULL, SC_RAY);
	ob_setpos (ray, camera->camera.pos);
	float *raydir = ob_setdir (ray, camera->camera.dir);

	int width = screen->w, height = screen->h;
	SC3 xdir = {1.0, 0.0, 0.0}, ydir = {0.0, 1.0, 0.0};
	SC3 hdir, vdir;

	vm_cross (hdir, raydir, ydir);
	vm_cross (vdir, raydir, xdir);
	//printf ("%f %f %f\n", hdir[0], hdir[1], hdir[2]);

	vm_scale (hdir, hdir, camera->camera.dim[0] / (vm_mod(hdir)*width));
	vm_scale (vdir, vdir, camera->camera.dim[1] / (vm_mod(vdir)*height));
	//printf ("%f %f %f\n", vdir[0], vdir[1], vdir[2]);

	SC3 curdir, curxdir, curydir;
	int x, y, i;
	SDL_LockSurface(screen);
	for (x = -width/2; x < width/2; ++ x)
	{
		vm_scale (curxdir, hdir, .5+x);
		vm_scale (curydir, vdir, .5-height/2);
		vm_sum (curdir, raydir, curxdir);
		vm_sum (curdir, curdir, curydir);
		for (y = 0; y < height; ++ y)
		{
			vm_sum (curdir, curdir, vdir);
			//printf ("%f %f %f\n", curdir[0], curdir[1], curdir[2]);
			float maxparam = 1000.0;
			SC3 maxpixel = {0.0, 0.0, 0.0};
			for (i = 0; i < scene->objs->len; ++ i)
			{
				Object **obj = v_at(scene->objs, i);
				gr_solve (*obj, ray->ray.pos, curdir, &maxparam, maxpixel, scene, camera);
			}
			//if (y == -height/2)
			//	printf("%d\n", (int)maxpixel[0]);
			Uint32 *p = (Uint32*) ((Uint8*) screen->pixels + y * screen->pitch + (x+height/2) * 4);
			*p = SDL_MapRGB (screen->format, maxpixel[0], maxpixel[1], maxpixel[2]);
			//printf ("%f %f %f\n", maxpixel[0], maxpixel[1], maxpixel[2]);
		}
	}
	SDL_UnlockSurface(screen);
	SDL_UpdateRect (screen, 0, 0, 0, 0);
}

int main (int argc, char *argv[])
{
	SDL_Surface *screen = gr_init ();
	Scene *scene = sc_init ();

	Object *light1 = sc_newobj (scene, SC_POINTLIGHT),
	       *light2 = sc_newobj (scene, SC_POINTLIGHT),
	       *plane = sc_newobj (scene, SC_PLANE),
		   *sphere1 = sc_newobj (scene, SC_SPHERE),
		   *sphere2 = sc_newobj (scene, SC_SPHERE),
		   *cam = sc_newobj (scene, SC_CAMERA);

	ob_setpos (light1, sc3(5.0, 5.0, 5.0));
	ob_setlum (light1, 250.0);

	ob_setpos (light2, sc3(0.0, 0.0, -0.99));
	ob_setlum (light2, 0.0);

	ob_setpos (plane, sc3(0.0, 0.0, -8.0));
	ob_setdir (plane, sc3(0.0, 0.0, 1.0));

	ob_setpos (sphere1, sc3(0.0, 0.0, -3.0));
	ob_setrad (sphere1, 2.0);

	ob_setpos (sphere2, sc3(0.0, -3.5, 3.0));
	ob_setrad (sphere2, 1.0);

	ob_setpos (cam, sc3(0.0, 0.0, 100.0));
	ob_setdir (cam, sc3(0.0, 0.0, -1.0));
	ob_setdim (cam, sc2(0.3, 0.3));

	int i;
	for (i = 0; i < 200; ++ i)
	{
		gr_render (screen, scene, cam);
		//ob_setpos (light1, sc3(10.0, 10.0, 5.0 -0.1 * i));
		ob_setpos (sphere2, sc3(0.0, -3.5+0.08*i, 3.0));
		//SDL_Delay(20);
	}

	//SDL_Delay(5000);

	exit (0);
}

