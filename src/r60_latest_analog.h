#pragma once
/* A multi-sample peek supplies Square's physical edge history. Remote input
 * hooks may replace only its first record, which sorting places in the past.
 * Fetch the current single record for analog state without changing any
 * button-history processing or overriding the authenticated remote filter. */
static void vita_latest_analog(SceCtrlData *destination){
 SceCtrlData latest;
 if(sceCtrlPeekBufferPositive(0,&latest,1)>0){destination->lx=latest.lx;destination->ly=latest.ly;destination->rx=latest.rx;destination->ry=latest.ry;}
}
