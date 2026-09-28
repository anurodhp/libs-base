/* GNU Objective-C runtime type-encoding API for Apple's objc4.
 *
 * gnustep-base parses type encodings with the GNU runtimes' functions
 * (objc_sizeof_type, objc_skip_typespec, objc_layout_structure, ...).
 * Apple's objc4 has no such API: Apple's Foundation uses its own
 * NSGetSizeAndAlignment. When gnustep-base is the Foundation on objc4
 * (GS_OBJC4_RUNTIME), these are that API, taken verbatim from the
 * GNUstep Objective-C runtime:
 *   https://github.com/gnustep/libobjc2  tag v2.3 (e877e782fb96)
 *   encoding2.c   lines 11-49, 83-395 and 495-575 (the type-encoding parser;
 *                 the method_* functions are left out, objc4 owns them)
 *   type_encoding_cases.h  (as GSObjC4TypeEncodingCases.h)
 * The only changes: OBJC_PUBLIC/PRIVATE markers and the includes.
 *
 * libobjc2's licence:
 * Copyright (c) 2009 David Chisnall
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <objc/runtime.h>
#include "GSObjC4Encoding.h"

#ifdef max
#	undef max
#endif

size_t objc_alignof_type (const char *type);

// It would be so nice if this works, but in fact it returns nonsense:
//#define alignof(x) __alignof__(x)
//
#define alignof(type) __builtin_offsetof(struct { const char c; type member; }, member)

const char *objc_skip_type_qualifiers (const char *type)
{
	static const char *type_qualifiers = "rnNoORVA";
	while('\0' != *type && strchr(type_qualifiers, *type))
	{
		type++;
	}
	return type;
}

static const char *sizeof_type(const char *type, size_t *size);

const char *objc_skip_typespec(const char *type)
{
	size_t ignored = 0;
	return sizeof_type(type, &ignored);
}

const char *objc_skip_argspec(const char *type)
{
	type = objc_skip_typespec(type);
	while(isdigit(*type)) { type++; }
	return type;
}


typedef const char *(*type_parser)(const char*, void*);

static int parse_array(const char **type, type_parser callback, void *context)
{
	// skip [
	(*type)++;
	int element_count = (int)strtol(*type, (char**)type, 10);
	*type = callback(*type, context);
	// skip ]
	(*type)++;
	return element_count;
}

static void parse_struct_or_union(const char **type, type_parser callback, void *context, char endchar)
{
	// Skip the ( and structure name
	do
	{
		(*type)++;
		// Opaque type has no =definition
		if (endchar == **type) { (*type)++; return; }
	} while('=' != **type);
	// Skip =
	(*type)++;

	while (**type != endchar)
	{
		// Structure elements sometimes have their names in front of each
		// element, as in {NSPoint="x"f"y"f} - We need to skip the type name
		// here.
		//
		// TODO: In a future version we should provide a callback that lets
		// users of this code get the field name
		if ('"'== **type)
		{

			do
			{
				(*type)++;
			} while ('"' != **type);
			// Skip the closing "
			(*type)++;
		}
		*type = callback(*type, context);
	}
	// skip }
	(*type)++;
}

static void parse_union(const char **type, type_parser callback, void *context)
{
	parse_struct_or_union(type, callback, context, ')');
}

static void parse_struct(const char **type, type_parser callback, void *context)
{
	parse_struct_or_union(type, callback, context, '}');
}

inline static void round_up(size_t *v, size_t b)
{
	if (0 == b)
	{
		return;
	}

	if (*v % b)
	{
		*v += b - (*v % b);
	}
}
inline static size_t max(size_t v, size_t v2)
{
	return v>v2 ? v : v2;
}

static const char *skip_object_extended_qualifiers(const char *type)
{
	if (*(type+1) == '?')
	{
		type++;
		if (*(type+1) == '<')
		{
			type += 2;
			while (*type != '>')
			{
				type++;
			}
		}
	}
	else if (type[1] == '"')
	{
		type += 2;
		while (*type != '"')
		{
			type++;
		}
	}
	return type;
}

static const char *sizeof_union_field(const char *type, size_t *size);

static const char *sizeof_type(const char *type, size_t *size)
{
	type = objc_skip_type_qualifiers(type);
	switch (*type)
	{
		// For all primitive types, we round up the current size to the
		// required alignment of the type, then add the size
#define APPLY_TYPE(typeName, name, capitalizedName, encodingChar) \
		case encodingChar:\
		{\
			round_up(size, (alignof(typeName) * 8));\
			*size += (sizeof(typeName) * 8);\
			return type + 1;\
		}
#define SKIP_ID 1
#define NON_INTEGER_TYPES 1
#include "GSObjC4TypeEncodingCases.h"
		case '@':
		{
			round_up(size, (alignof(id) * 8));
			*size += (sizeof(id) * 8);
			return skip_object_extended_qualifiers(type) + 1;
		}
		case '?':
		case 'v': return type+1;
		case 'j':
		{
			type++;
			switch (*type)
			{
#define APPLY_TYPE(typeName, name, capitalizedName, encodingChar) \
		case encodingChar:\
		{\
			round_up(size, (alignof(_Complex typeName) * 8));\
			*size += (sizeof(_Complex typeName) * 8);\
			return type + 1;\
		}
#include "GSObjC4TypeEncodingCases.h"
			}
		}
		case '{':
		{
			const char *t = type;
			parse_struct(&t, (type_parser)sizeof_type, size);
			size_t align = objc_alignof_type(type);
			round_up(size, align * 8);
			return t;
		}
		case '[':
		{
			const char *t = type;
			size_t element_size = 0;
			// FIXME: aligned size
			int element_count = parse_array(&t, (type_parser)sizeof_type, &element_size);
			(*size) += element_size * element_count;
			return t;
		}
		case '(':
		{
			const char *t = type;
			size_t union_size = 0;
			parse_union(&t, (type_parser)sizeof_union_field, &union_size);
			*size += union_size;
			return t;
		}
		case 'b':
		{
			// Consume the b
			type++;
			// Ignore the offset
			strtol(type, (char**)&type, 10);
			// Consume the element type
			type++;
			// Read the number of bits
			*size += strtol(type, (char**)&type, 10);
			return type;
		}
		case '^':
		{
			// All pointers look the same to me.
			*size += sizeof(void*) * 8;
			size_t ignored = 0;
			// Skip the definition of the pointeee type.
			return sizeof_type(type+1, &ignored);
		}
	}
	abort();
	return NULL;
}

static const char *sizeof_union_field(const char *type, size_t *size)
{
	size_t field_size = 0;
	const char *end = sizeof_type(type, &field_size);
	*size = max(*size, field_size);
	return end;
}

static const char *alignof_type(const char *type, size_t *align)
{
	type = objc_skip_type_qualifiers(type);
	switch (*type)
	{
		// For all primitive types, we return the maximum of the new alignment
		// and the old one
#define APPLY_TYPE(typeName, name, capitalizedName, encodingChar) \
		case encodingChar:\
		{\
			*align = max((alignof(typeName) * 8), *align);\
			return type + 1;\
		}
#define NON_INTEGER_TYPES 1
#define SKIP_ID 1
#include "GSObjC4TypeEncodingCases.h"
		case '@':
		{
			*align = max((alignof(id) * 8), *align);\
			return skip_object_extended_qualifiers(type) + 1;
		}
		case '?':
		case 'v': return type+1;
		case 'j':
		{
			type++;
			switch (*type)
			{
#define APPLY_TYPE(typeName, name, capitalizedName, encodingChar) \
		case encodingChar:\
		{\
			*align = max((alignof(_Complex typeName) * 8), *align);\
			return type + 1;\
		}
#include "GSObjC4TypeEncodingCases.h"
			}
		}
		case '{':
		{
			const char *t = type;
			parse_struct(&t, (type_parser)alignof_type, align);
			return t;
		}
		case '(':
		{
			const char *t = type;
			parse_union(&t, (type_parser)alignof_type, align);
			return t;
		}
		case '[':
		{
			const char *t = type;
			parse_array(&t, (type_parser)alignof_type, align);
			return t;
		}
		case 'b':
		{
			// Consume the b
			type++;
			// Ignore the offset
			strtol(type, (char**)&type, 10);
			// Alignment of a bitfield is the alignment of the type that
			// contains it
			type = alignof_type(type, align);
			// Ignore the number of bits
			strtol(type, (char**)&type, 10);
			return type;
		}
		case '^':
		{
			*align = max((alignof(void*) * 8), *align);
			// All pointers look the same to me.
			size_t ignored = 0;
			// Skip the definition of the pointeee type.
			return alignof_type(type+1, &ignored);
		}
	}
	abort();
	return NULL;
}

size_t objc_sizeof_type(const char *type)
{
	size_t size = 0;
	sizeof_type(type, &size);
	return size / 8;
}

size_t objc_alignof_type (const char *type)
{
	size_t align = 0;
	alignof_type(type, &align);
	return align / 8;
}

size_t objc_aligned_size(const char *type)
{
	size_t size  = objc_sizeof_type(type);
	size_t align = objc_alignof_type(type);
	return size + (size % align);
}

size_t objc_promoted_size(const char *type)
{
	size_t size = objc_sizeof_type(type);
	return size + (size % sizeof(void*));
}


unsigned objc_get_type_qualifiers (const char *type)
{
	unsigned flags = 0;
#define MAP(chr, bit) case chr: flags |= bit; break;
	do
	{
		switch (*(type++))
		{
			default: return flags;
			MAP('r', _F_CONST)
			MAP('n', _F_IN)
			MAP('o', _F_OUT)
			MAP('N', _F_INOUT)
			MAP('O', _F_BYCOPY)
			MAP('V', _F_ONEWAY)
			MAP('R', _F_BYREF)
		}
	} while (1);
}

// Note: The implementations of these functions is horrible.
void objc_layout_structure (const char *type,
                            struct objc_struct_layout *layout)
{
	layout->original_type = type;
	layout->type = 0;
}

static const char *layout_structure_callback(const char *type, struct objc_struct_layout *layout)
{
	size_t align = 0;
	size_t size = 0;
	const char *end = sizeof_type(type, &size);
	alignof_type(type, &align);
	//printf("Callback called with %s\n", type);
	if (layout->prev_type < type)
	{
		if (layout->record_align == 0)
		{
			layout->record_align = align;
			layout->type = type;
		}
	}
	else
	{
		size_t rsize = (size_t)layout->record_size;
		round_up(&rsize, align);
		layout->record_size = rsize + size;
	}
	return end;
}

BOOL objc_layout_structure_next_member(struct objc_struct_layout *layout)
{
	const char *end = layout->type;
	layout->record_size = 0;
	layout->record_align = 0;
	layout->prev_type = layout->type;
	const char *type = layout->original_type;
	parse_struct(&type, (type_parser)layout_structure_callback, layout);
	//printf("Calculated: (%s) %s %d %d\n", layout->original_type, layout->type, layout->record_size, layout->record_align);
	//printf("old start %s, new start %s\n", end, layout->type);
	return layout->type != end;
}

void objc_layout_structure_get_info (struct objc_struct_layout *layout,
                                     unsigned int *offset,
                                     unsigned int *align,
                                     const char **type)
{
	//printf("%p\n", layout);
	*type = layout->type;
	size_t off = layout->record_size / 8;
	*align= layout->record_align / 8;
	round_up(&off, (size_t)*align);
	*offset = (unsigned int)off;
}
