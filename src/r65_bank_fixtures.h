#pragma once
/* Every configured alias of exact booth models1270/1153/1146, bank tables
 *1262/1229 and noticeboard2040 in the packaged client. */
static int r65_bank_hull_type(int id){return id==2213||id==2214||id==2215||id==3045;}
static int r65_bank_layers_type(int id){return r65_bank_hull_type(id)||id==590||id==591||id==961||id==962;}
/* pix3d.textureRaster chooses one of1,7/8,6/8,5/8 texture planes,
 * then shifts once at shade64. The old GL255-2*shade approximation
 * makes shade123almostblack rather than the original5/16brightness. */
static int r65_texture_brightness(int shade){
 if(shade<0||shade>127)return -1;
 int denominator=8<<(shade>>6),numerator=255*(8-((shade>>4)&3));
 return (numerator+denominator/2)/denominator;
}
