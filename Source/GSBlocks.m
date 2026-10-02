/** Implementation of GSBlocks for GNUStep
   Copyright (C) 2011 Free Software Foundation, Inc.

   This file is part of the GNUstep Base Library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.
   
   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 31 Milk Street #960789 Boston, MA 02196 USA.

   */ 

#import "Foundation/NSObject.h"
#import "Foundation/NSArray.h"
#import "Foundation/NSDictionary.h"

#if defined(NeXT_RUNTIME)
/* Apple's objc4 has no _NSBlock class: libsystem_blocks only exports
 * zero-filled OBJC_MAX_CLASS_SIZE buffers (libclosure data.c:22-27) for the
 * runtime's block isa pointers, and expects the Foundation layer to build
 * classes into them in place with objc_initializeClassPair()
 * (objc4 runtime/objc-internal.h:56-60,83-89).  Do that here; without it any
 * message sent to a block (-retain from ARC, NSTimer's block initialiser) is
 * a message to a non-class isa.
 */
#include <stdlib.h>
#include <string.h>
extern void *_NSConcreteStackBlock[32];
extern void *_NSConcreteMallocBlock[32];
extern void *_NSConcreteGlobalBlock[32];
extern Class objc_initializeClassPair(Class, const char *, Class, Class);
/* The empty-collection singletons clang references for @[] and @{}. */
id __NSArray0__;
id __NSDictionary0__;
#endif

@interface GSBlock : NSObject
@end

@implementation GSBlock
+ (void) load
{
  unsigned int	methodCount;
  Method	*m = NULL;
  Method	*methods = class_copyMethodList(self, &methodCount);
  id		blockClass = objc_lookUpClass("_NSBlock");
  Protocol	*nscopying = NULL;

#if defined(NeXT_RUNTIME)
  __NSArray0__ = [[NSArray alloc] init];
  __NSDictionary0__ = [[NSDictionary alloc] init];
  if (nil == blockClass)
    {
      /* _NSBlock, then the three concrete classes built in place over
       * libclosure's isa buffers.  The methods go on _NSBlock before the
       * subclasses exist so their custom-RR flags are right from the start.
       */
      static const struct { const char *name; void **buf; } kinds[] = {
	{ "__NSStackBlock__", _NSConcreteStackBlock },
	{ "__NSMallocBlock__", _NSConcreteMallocBlock },
	{ "__NSGlobalBlock__", _NSConcreteGlobalBlock },
      };
      unsigned	i;

      blockClass = objc_allocateClassPair([NSObject class], "_NSBlock", 0);
      for (m = methods; NULL != m && NULL != *m; m++)
	{
	  class_addMethod(blockClass, method_getName(*m),
	    method_getImplementation(*m), method_getTypeEncoding(*m));
	}
      class_addProtocol(blockClass, objc_getProtocol("NSCopying"));
      objc_registerClassPair(blockClass);
      for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
	{
	  /* The metaclass buffer is OBJC_MAX_CLASS_SIZE too; it lives forever. */
	  void	*meta = calloc(32, sizeof(void *));
	  Class	c = objc_initializeClassPair(blockClass, kinds[i].name,
	    (Class)kinds[i].buf, (Class)meta);

	  if (Nil == c)
	    {
	      abort();
	    }
	  objc_registerClassPair(c);
	}
      free(methods);
      return;
    }
#endif

  /* If we don't have an _NSBlock class, we don't have blocks support in the
   * runtime, so give up.
   */
  if (nil == blockClass)
    {
      return;
    }

  /* Copy all of the methods in this class onto the block-runtime-provided
   * _NSBlock
   */
  for (m = methods; NULL != *m; m++)
    {
      class_addMethod(blockClass, method_getName(*m),
	method_getImplementation(*m), method_getTypeEncoding(*m));
    }
  nscopying = objc_getProtocol("NSCopying");
  class_addProtocol(blockClass, nscopying);
  free(methods);
}

- (id) copyWithZone: (NSZone*)aZone
{
  return _Block_copy(self);
}

- (id) copy
{
  return _Block_copy(self);
}

- (id) retain
{
  return _Block_copy(self);
}

- (oneway void) release
{
  _Block_release(self);
}
@end

