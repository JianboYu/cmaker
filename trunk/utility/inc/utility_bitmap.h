#ifndef _UTILITY_BITMAP_H_
#define _UTILITY_BITMAP_H_

#include <os_typedefs.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* hbitmap;

/*bits num need algined to 8*/
hbitmap bitmap_create(uint32_t bits);
void bitmap_destroy(hbitmap bm);

void bitmap_set_bit(hbitmap bm, uint32_t idx, int32_t data);
void bitmap_clear(hbitmap bm);
int32_t bitmap_find_first_zero_bit(hbitmap bm);
int32_t bitmap_get_bit(hbitmap bm, uint32_t idx);
int32_t bitmap_is_clear(hbitmap bm);

#ifdef __cplusplus
} //extern "C"
#endif

#endif //_UTILITY_BITMAP_H_
