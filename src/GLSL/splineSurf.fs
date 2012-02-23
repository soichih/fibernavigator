#include lighting.fs

varying vec4 VaryingTexCoord0;

uniform int dimX, dimY, dimZ;
uniform float voxX, voxY, voxZ;

uniform int countTextures;

#include functions.fs

void main()
{
	/* Normalize the normal. A varying variable CANNOT
	 // be modified by a fragment shader. So a new variable
	 // needs to be created. */
	vec4 ambient = vec4(0.0);
	vec4 diffuse = vec4(0.0);
	vec4 specular = vec4(0.0);

	calculateLighting(-normal, gl_FrontMaterial.shininess, ambient, diffuse, specular);

	vec4 color = vec4(0.0);

	vec3 v =  VaryingTexCoord0.xyz;
	v.x = v.x / (float(dimX) * voxX);
    v.y = v.y / (float(dimY) * voxY);
    v.z = v.z / (float(dimZ) * voxZ);

    /*if ( type9 > 0 ) lookupTex( color, type9, tex9, threshold9, v, alpha9 );
    if ( type8 > 0 ) lookupTex( color, type8, tex8, threshold8, v, alpha8 );
    if ( type7 > 0 ) lookupTex( color, type7, tex7, threshold7, v, alpha7 );
    if ( type6 > 0 ) lookupTex( color, type6, tex6, threshold6, v, alpha6 );*/
    if ( type5 > 0 ) lookupTex( color, type5, tex5, threshold5, v, alpha5 );
    if ( type4 > 0 ) lookupTex( color, type4, tex4, threshold4, v, alpha4 );
    if ( type3 > 0 ) lookupTex( color, type3, tex3, threshold3, v, alpha3 );
    if ( type2 > 0 ) lookupTex( color, type2, tex2, threshold2, v, alpha2 );
    if ( type1 > 0 ) lookupTex( color, type1, tex1, threshold1, v, alpha1 );
    if ( type0 > 0 ) lookupTex( color, type0, tex0, threshold0, v, alpha0 );

    if (color.rgb == vec3(0.0)) discard;

	color.a = 1.0;

	//color = color * 0.8 + (ambient * color / 2.0) + (diffuse * color / 2.0) + (specular * color / 2.0);

	color = clamp(color, 0.0, 1.0);

	gl_FragColor = color;
}
