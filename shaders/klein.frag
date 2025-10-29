void mainImage( out vec4 o, vec2 p )
{
    o *= 0.;
	p /= iResolution.xy;
    float s = 0.,a,t,S,C;
    for (float i=-3.; i<= 3.; i++) {
        s += a = .5+.5*cos(t= fract(iTime)-i);  
        t *= .6*smoothstep(0.,1.,length(p-.5));     
	    o += a*texture(iChannel0,mat2(C=cos(t),S=sin(t),-S,C)*(p-.5)+.1*iTime);
    } 
    o /= s;
    o = 2.*o-.5;
}

