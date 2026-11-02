#pragma once

#include "jagfile.h"

// name taken from rs3
typedef struct {
    int frameCount;
    int *frames;
    int *iframes;
    int *delay;
    int replayoff; // = -1;
    int *walkmerge;
    bool stretches;  // = false;
    int priority;    // = 5;
    int righthand;   // = -1;
    int lefthand;    // = -1;
    int replaycount; // = 99;
    // Isolated compatibility metadata. Original225 movement behavior is unchanged.
    // A future gameplay adapter must validate interruption/replay semantics separately.
    int precedenceAnimating; // source474 opcode9, default -1
    int priorityWalking;     // source474 opcode10, default -1
    int replayMode;          // source474 opcode11, default2
    bool compatibilityMetadata;

} SeqType;

typedef struct {
    int count;
    SeqType **instances;
} SeqTypeData;

void seqtype_free_global(void);
void seqtype_unpack(Jagfile *config);
