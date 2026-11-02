#ifndef R100_CRYPT_FIXTURES_H
#define R100_CRYPT_FIXTURES_H
/* Six recovered multipart sarcophagi. Their explicit lid animations must
 * replace live models without retaining the closed lid in a baked landscape
 * or rebuilding that landscape when the animation returns to its rest pose. */
static int r100_barrows_sarcophagus(int id){
    return id==3830||id==3831||id==3832||id==3844||id==3845||id==3846;
}
#endif
