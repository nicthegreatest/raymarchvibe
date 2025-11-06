/*
* License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
* Created by bal-khan
*/

vec2	march(vec3 pos, vec3 dir);
vec3	camera(vec2 uv);
void	rotate(inout vec2 v, float angle);
vec3	calcNormal( in vec3 pos, float e, vec3 dir);
float	sdTorus( vec3 p, vec2 t, float phase );
float	mylength(vec2 p);
float	nrand( vec2 n );

float 	t;			// time
vec3	ret_col;	// torus color
vec3	h; 			// light amount
float	mind;
float	ming;
float	mint;
float	minl;

const vec3	teal   = vec3(0.3 , .7, .9);
const vec3	orange = vec3(0.95, .5, .1);

#define I_MAX		100.
#define E			0.0001
#define FAR			30.

/*
* Leon's mod polar from : https://www.shadertoy.com/view/XsByWd
*/

#define PI 3.14159
#define TAU PI*2.

vec2 modA (vec2 p, float count) {
    float an = TAU/count;
    float a = atan(p.y,p.x)+an*.5;
    a = mod(a, an)-an*.5;
    return vec2(cos(a),sin(a))*length(p);
}

// blackbody by aiekick : https://www.shadertoy.com/view/lttXDn

// -------------blackbody----------------- //

// return color from temperature 
//http://www.physics.sfasu.edu/astro/color/blackbody.html
//http://www.vendian.org/mncharity/dir3/blackbody/
//http://www.vendian.org/mncharity/dir3/blackbody/UnstableURLs/bbr_color.html

vec3 blackbody(float Temp)
{
	vec3 col = vec3(255.);
    col.x = 56100000. * pow(Temp,(-3. / 2.)) + 148.;
   	col.y = 100.04 * log(Temp) - 623.6;
   	if (Temp > 6500.) col.y = 35200000. * pow(Temp,(-3. / 2.)) + 184.;
   	col.z = 194.18 * log(Temp) - 1448.6;
   	col = clamp(col, 0., 255.)/255.;
    if (Temp < 1000.) col *= Temp/1000.;
   	return col;
}

// -------------blackbody----------------- //


void mainImage(out vec4 c_out, in vec2 f)
{
    t  = abs(iTime*.125); // my phone forces me to do this, idk why iTime is -iTime on phone
    vec3	col = vec3(0., 0., 0.);
	vec2 R = iResolution.xy,
          uv  = vec2(f-R/2.) / R.y;
	vec3	dir = camera(uv);
    vec3	pos = vec3(.0+cos(t*5.), .0+sin(t*5.)*.5, .0);

    pos.z = 20.*exp(-t*5.)+10.+.5*sin(t*10.);    
    h*=0.;
    vec2	inter = (march(pos, dir));
    float	id = (mind == ming ? 1. : 0.)+(mind == mint ? 2. : 0.)+(mind == minl ? 3. : 0.);
    if (inter.y <= FAR)
        col.xyz = ret_col*(1.-inter.x*.025);
    else
        col *= 0.;
    col += h;
	col *= clamp((1.5-length(uv)), .5, 1.);
    col *= clamp((1.1-length(uv)), .0, 1.);
    c_out =  vec4(col,1.0);
}

float	scene(vec3 p)
{  
    mind = 1e5;
    ming = 1e5;
    mint = 1e5;
	minl = 1e5;

    rotate(p.xz, 1.57-.15*iTime);
    rotate(p.yz, 1.57-.125*iTime);
    vec3 op = p;
    float	var = (atan(p.x, p.z)+ PI)/(TAU);
    var = var*40.;
    p.xz = modA(p.xz, 40.);
    p.xz -= vec2(8., .0);

    rotate(p.xy, iTime*.25*(mod(var-.5, 2.) <= 1. ? 1. : -1.));
    vec2 q = vec2(length(p.xy)-4.,p.z);
    mind = mylength(q)-.05;

    float vir = (atan(p.x, p.y)+ PI)/(TAU);
    var = vir*30.;
    p.xy = modA(p.xy, 30.)-vec2(.0,.0);
    p.xz -= vec2(4., .0);
    q = vec2(length(p.zx)-0.25, p.y-.0);
    ming = mylength(q)-.05;
    mind = min(mind, ming);

    float as = (mind == ming ? 1. : 0.);
    ret_col = step(as, .0)*vec3(.2, .7, .4) + step(1., as)*vec3(.5, .2, .8);
    rotate(p.xz, iTime*1.5*(mod(var, 2.) <= 1. ? 1. : -1.)*(mod(vir, 2.) <= 1. ? 1. : -1.));
    p.xz = modA(p.xz, 20.)-vec2(.0,-.0);
    p.xy -= vec2(.25, .0);
    q = vec2(length(p.xy)-.1, p.z);
    mint = mylength(q)-.02;
    mind = min(mind, mint);
    as = (mind == mint ? 1. : 0.);
    if (as  == 1.)
    ret_col = teal;
    
    // dodecahedron
    rotate(op.zx, iTime*.5);
    op.xz = modA(op.xz, 25.);
    op -= vec3(6., .0, 0.);
    op /= 1.732; //sqrt(3.)
    vec3	b = vec3(.075);
	minl = max(max(abs(op.x)+.5*abs(op.y)-b.x, abs(op.y)+.5*abs(op.z)-b.y), abs(op.z)+.5*abs(op.x)-b.z);
    b *= .95;
    minl = max(minl, 
           -max(max(abs(op.x)+.5*abs(op.y)-b.x, abs(op.y)-.5*abs(op.z)-b.y), abs(op.z)+.5*abs(op.x)-b.z)
               );
    minl = max(minl, 
           -max(max(abs(op.x)-.5*abs(op.y)-b.x, abs(op.y)+.5*abs(op.z)-b.y), abs(op.z)+.5*abs(op.x)-b.z)
               );
    minl = max(minl, 
           -max(max(abs(op.x)+.5*abs(op.y)-b.x, abs(op.y)+.5*abs(op.z)-b.y), abs(op.z)-.5*abs(op.x)-b.z)
               );
    // end dodecahedron

    mind = min(mind, minl);

    as = mind == minl ? 1. : 0.;

    if (as == 1.)
    ret_col = orange;
    
    h += .036125*orange/(pow(minl, 25.)+.5);

    return (mind);
}

vec2	march(vec3 pos, vec3 dir)
{
    vec2	dist = vec2(0.0, 0.0);
    vec3	p = vec3(0.0, 0.0, 0.0);
    vec2	s = vec2(0.0, 0.0);
	float dinamyceps = E;
	for (float i = -1.; i < I_MAX; ++i)
	{
		p = pos + dir * dist.y;
		dist.x = scene(p);
		dist.y += dist.x*1.;
        dinamyceps = -dist.x+(dist.y)/(1500.);
        // log trick from aiekick
	    if (log(dist.y*dist.y/dist.x/1e5)>0. || dist.x < dinamyceps || dist.y > FAR)
        {
            break;
        }
	    s.x++;
    }
    s.y = dist.y;
    return (s);
}

float	mylength(vec2 p)
{
	float	ret;
    
    p = p*p*p*p;
    p = p*p;
    ret = (p.x+p.y);
    ret = pow(ret, 1./8.);
    
    return ret;
}

// Utilities

void rotate(inout vec2 v, float angle)
{
	v = vec2(cos(angle)*v.x+sin(angle)*v.y,-sin(angle)*v.x+cos(angle)*v.y);
}

vec2	rot(vec2 p, vec2 ang)
{
	float	c = cos(ang.x);
    float	s = sin(ang.y);
    mat2	m = mat2(c, -s, s, c);
    
    return (p * m);
}

vec3	camera(vec2 uv)
{
    float		fov = 1.;
	vec3		forw  = vec3(0.0, 0.0, -1.0);
	vec3    	right = vec3(1.0, 0.0, 0.0);
	vec3    	up    = vec3(0.0, 1.0, 0.0);

    return (normalize((uv.x) * right + (uv.y) * up + fov * forw));
}

vec3 calcNormal( in vec3 pos, float e, vec3 dir)
{
    vec3 eps = vec3(e,0.0,0.0);

    return normalize(vec3(
           march(pos+eps.xyy, dir).y - march(pos-eps.xyy, dir).y,
           march(pos+eps.yxy, dir).y - march(pos-eps.yxy, dir).y,
           march(pos+eps.yyx, dir).y - march(pos-eps.yyx, dir).y ));
}
