uniform sampler2D uScene;
varying vec2 vUV;
void main(){gl_FragColor=texture2D(uScene,vUV);}
