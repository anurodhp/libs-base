/* GNU runtime type-encoding API on Apple's objc4: see GSObjC4Encoding.c.
 * Declarations and flag values as in libobjc2 v2.3 objc/encoding.h.
 */
#ifndef GSObjC4Encoding_h
#define GSObjC4Encoding_h
#include <stddef.h>
#include <objc/objc.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *objc_skip_type_qualifiers (const char *type);
const char *objc_skip_typespec(const char *type);
const char *objc_skip_argspec(const char *type);
size_t objc_sizeof_type(const char *type);
size_t objc_alignof_type(const char *type);
size_t objc_aligned_size(const char *type);
size_t objc_promoted_size(const char *type);
unsigned objc_get_type_qualifiers (const char *type);

struct objc_struct_layout
{
	const char *original_type;
	const char *type;
	const char *prev_type;
	unsigned int record_size;
	unsigned int record_align;
};

void objc_layout_structure (const char *type,
                            struct objc_struct_layout *layout);
BOOL objc_layout_structure_next_member(struct objc_struct_layout *layout);
void objc_layout_structure_get_info (struct objc_struct_layout *layout,
                                     unsigned int *offset,
                                     unsigned int *align,
                                     const char **type);

#define _F_CONST       0x01
#define _F_IN          0x01
#define _F_OUT         0x02
#define _F_INOUT       0x03
#define _F_BYCOPY      0x04
#define _F_BYREF       0x08
#define _F_ONEWAY      0x10
#define _F_GCINVISIBLE 0x20

#ifdef __cplusplus
}
#endif
#endif
