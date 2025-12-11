float linearize(float x)
{
   if (x <= 0.04045)
      return x / 12.92;
   else
      return pow((x + 0.055) / 1.055, 2.4);
}
vec4 rgba(float r, float g, float b, float a)
{
   return {linearize(r / 255), linearize(g / 255), linearize(b / 255), 1};
}
vec3 rgb(float r, float g, float b) {

   return {linearize(r / 255), linearize(g / 255), linearize(b / 255)};
}