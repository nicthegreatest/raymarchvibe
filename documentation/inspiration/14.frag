#define rotate(a) mat2(cos(a),-sin(a),sin(a),cos(a))

float lineWidth=7.0;
float tunneRotSpeed = 0.05;
float tunnelSpeed = 0.06;
float size=100.0;
vec3 objectStartPos = vec3(0, 0, -50.0);    
float camRange=1000.0;
float scale[8];
vec3 projPos[8];    

vec4 quads[6] = vec4[]( 
    vec4(0, 1, 2, 3), 
    vec4(4, 5, 6, 7), 
    vec4(3, 7, 4, 0), 
    vec4(6, 5, 1, 2), 
    vec4(0, 1, 5, 4), 
    vec4(2, 3, 7, 6));

vec3 edges[24];


void oval(vec2 screenCoord, vec2 center, float radius, float strokeThickness, vec4 strokeColor, vec4 fillColor, inout vec4 pixel)
{
  float dist= distance(screenCoord, center);

  if (dist<radius)
  { 
    if (dist<radius-strokeThickness)
    {
      pixel = fillColor;
    } else
    {
      pixel = strokeColor;
    }
  }
}


bool lineItersection(vec2 v1, vec2 v2, vec2 v3, vec2 v4) 
{    
  float bx = v2.x - v1.x;
  float by = v2.y - v1.y;
  float dx = v4.x - v3.x;
  float dy = v4.y - v3.y;

  float b_dot_d_perp = bx * dy - by * dx;

  if (b_dot_d_perp == 0.0) return false;

  float cx = v3.x - v1.x;
  float cy = v3.y - v1.y;

  float t = (cx * dy - cy * dx) / b_dot_d_perp;
  if (t < 0.0 || t > 1.0)  return false;

  float u = (cx * by - cy * bx) / b_dot_d_perp;
  if (u < 0.0 || u > 1.0)  return false;

  return true;
}


void Line(vec2 screenCoord, vec2 p1, vec2 p2, float thickness, vec4 color, inout vec4 pixel)
{

  float a = distance(p1, screenCoord);
  float b = distance(p2, screenCoord);
  float c = distance(p1, p2);

  if ( a >= c || b >=  c ) return;

  float p = (a + b + c) * 0.5;

  float dist = 2.0 / c * sqrt( p * ( p - a) * ( p - b) * ( p - c));

  if (dist<thickness)
  {
    pixel = mix(pixel, color, 1.0/max(1.0, dist*3.0));
  }
}



bool insideQuad(vec2 v1, vec2 v2, vec2 v3, vec2 v4, vec2 point)
{

  vec2 point2 = vec2(point.x-10000.0,point.y);
    
  int colCount = 0;

  if (lineItersection(point, point2, v1, v2))         
  {     colCount++;  }
  if (lineItersection(point, point2, v2, v3))
  {     colCount++;  }
  if (lineItersection(point, point2, v3, v4))
  {     colCount++;  }
  if (lineItersection(point, point2, v4, v1))
  {     colCount++;  }

  return (colCount==1);
}


void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
  float frame = float(iTime)*60.0;
  float FrameRad = radians(frame);
  float sinFrame = sin(FrameRad);
  float cosFrame = cos(FrameRad);
  float sinFrame2 = sinFrame*0.2;
  float cosFrame2 = cosFrame*2.2;
  vec2 center = (iResolution.xy/2.0)+vec2((cosFrame*sinFrame2)*250.0, (cosFrame-cosFrame2)*50.0);
  vec2 centerFragDist = center-fragCoord;
  vec3 halfRes = vec3(iResolution.x*0.5, iResolution.y*0.5, 0);   
  vec2 uvTunnel = fragCoord.xy / iResolution.xy;
  float angle = atan( centerFragDist.y, centerFragDist.x)*3.14;
  float dist = length(fragCoord-center);

  uvTunnel.x=1.0/(dist*0.0005);
  uvTunnel.y=angle;

  vec4 color = texture(iChannel3, uvTunnel*vec2(0.4, 3.0)+vec2(frame*tunnelSpeed, frame*tunneRotSpeed));
  color*=vec4(0.2, 0.4, 1.0, 1.0)*(2.0/(dist*0.01));   

  float rot = frame*0.02;
  vec2 uv = (fragCoord.xy / iResolution.xy);
  vec2 uv2 = (fragCoord.xy / iResolution.xy +vec2(frame*0.01, frame*0.003))*0.1;
  vec2 screenCoord = vec2(fragCoord.x, iResolution.y-fragCoord.y);

  vec3 camPos = vec3(0, 0, 90.0+cos(FrameRad)*50.0);
    
  vec3 verts[8] = vec3[](
    vec3(-1.0, -1.0, -1.0), 
    vec3(-1.0, -1.0, 1.0), 
    vec3(1.0, -1.0, 1.0), 
    vec3(1.0, -1.0, -1.0), 
    vec3(-1.0, 1.0, -1.0), 
    vec3(-1.0, 1.0, 1.0), 
    vec3(1.0, 1.0, 1.0), 
    vec3(1.0, 1.0, -1.0));
    
  for (int i=0; i<8; i++)
  {           
      // Y ROTATION    
      verts[i].xz *= rotate(rot); 
      // X ROTATION
      verts[i].yz *= rotate(rot*0.7);  
      // Z ROTATION
      verts[i].xy *= rotate(rot*0.2);  
      
      verts[i]+=objectStartPos;
  }

  for (int i=0; i<8; i++)
  {                            
    float camDistance = distance(verts[i], camPos);
    scale[i] = (camRange/camDistance)*0.1;
    projPos[i] = verts[i]-camPos;
    projPos[i]*=size*scale[i];    
    projPos[i]+= halfRes;
  }

  float range =  max(0.0, 1.0*sin(FrameRad));
 
  for (int i=0; i<6; i++)
  {
    vec3 center = (projPos[int(quads[i].x)]+projPos[int(quads[i].y)]+projPos[int(quads[i].z)]+projPos[int(quads[i].w)])/4.0;           
    edges[i*4 + 0]=projPos[int(quads[i].x)]+((center-projPos[int(quads[i].x)])*range);
    edges[i*4 + 1]=projPos[int(quads[i].y)]+((center-projPos[int(quads[i].y)])*range);
    edges[i*4 + 2]=projPos[int(quads[i].z)]+((center-projPos[int(quads[i].z)])*range);
    edges[i*4 + 3]=projPos[int(quads[i].w)]+((center-projPos[int(quads[i].w)])*range);            
  }

  for (int i=0; i<6; i++)
  {  
    if (insideQuad(edges[i*4 + 0].xy, edges[i*4 + 1].xy, edges[i*4 + 2].xy, edges[i*4 + 3].xy, screenCoord))
    { 
      vec2 center = (edges[i*4 + 0].xy+edges[i*4 + 1].xy+edges[i*4 + 2].xy+edges[i*4 + 3].xy)/4.0;                

      float minX = min(edges[i*4 + 0].x, min(edges[i*4 + 1].x, min(edges[i*4 + 2].x, edges[i*4 + 3].x)));
      float minY = min(edges[i*4 + 0].y, min(edges[i*4 + 1].y, min(edges[i*4 + 2].y, edges[i*4 + 3].y)));
      float maxX = max(edges[i*4 + 0].x, max(edges[i*4 + 1].x, max(edges[i*4 + 2].x, edges[i*4 + 3].x)));
      float maxY = max(edges[i*4 + 0].y, max(edges[i*4 + 1].y, max(edges[i*4 + 2].y, edges[i*4 + 3].y)));

      float width = maxX-minX;
      float height = maxY-minY;       
      float xDist = distance(minX, screenCoord.x)/width;
      float yDist = distance(minY, screenCoord.y)/height;

      color = (color+texture(iChannel2, vec2(xDist, 1.0-yDist)*0.1))*0.75;
      color-=0.2*((distance(center, screenCoord)/width));
    }
  }

  vec4 lineColor = color*3.0;

  for (int i=0; i<6; i++)      
  {    
    Line(screenCoord, edges[i*4 + 0].xy, edges[i*4 + 1].xy, lineWidth, lineColor, color);  
    Line(screenCoord, edges[i*4 + 1].xy, edges[i*4 + 2].xy, lineWidth, lineColor, color);  
    Line(screenCoord, edges[i*4 + 2].xy, edges[i*4 + 3].xy, lineWidth, lineColor, color);  
    Line(screenCoord, edges[i*4 + 3].xy, edges[i*4 + 0].xy, lineWidth, lineColor, color);
  }

  for (int i=0; i<24; i++)
  {       
    oval(screenCoord, edges[i].xy, 4.0, lineWidth, vec4(1.0), color*5.0, color);
  }

  vec2 sunPos = vec2(400.0+(cos(FrameRad)*300.0), 200.0+(sin(FrameRad)*100.0));      
  float sunDist = distance(screenCoord, sunPos)*0.02;

  fragColor = color + vec4(0.1/sunDist, 0.1/sunDist, 0.2/sunDist, 0);
}
